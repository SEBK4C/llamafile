// -*- mode:c++;indent-tabs-mode:nil;c-basic-offset:4;coding:utf-8 -*-
// vi: set et ft=cpp ts=4 sts=4 sw=4 fenc=utf-8 :vi
//
// Copyright 2024 Mozilla Foundation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "chatbot.h"

#include <cosmo.h>
#include <cstdio>
#include <signal.h>
#include <string>
#include <vector>

#include "llama.cpp/common/common.h"
#include "llama.cpp/common/arg.h"
#include "llama.cpp/ggml/include/ggml-cuda.h"
#include "llama.cpp/tools/mtmd/clip.h"
// #include "llama.cpp/tools/server/server.h" // TODO: Update when server integration is needed
#include "llamafile/color.h"
#include "llamafile/compute.h"
#include "llamafile/llama.h"
#include "llamafile/llamafile.h"  // For llamafile_has_gpu, llamafile_has_metal, llamafile_has, etc.
#include "llamafile/string.h"
#include "llamafile/version.h"  // For LLAMAFILE_VERSION_STRING

namespace lf {
namespace chatbot {

struct ServerArgs {
    int argc;
    char **argv;
};

common_params g_params;
clip_ctx *g_clip;
llama_model *g_model;
llama_context *g_ctx;
pthread_cond_t g_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
std::string g_listen_url;

std::string describe_compute(void) {
    // Updated: Check g_params.n_gpu_layers instead of querying model (API changed in llama.cpp)
    if (g_params.n_gpu_layers > 0 && llamafile_has_gpu()) {
        if (llamafile_has_metal()) {
            return "Apple Metal GPU";
        } else {
            std::vector<std::string> vec;
            int count = ggml_backend_cuda_get_device_count();
            for (int i = 0; i < count; i++) {
                char buf[128];
                ggml_backend_cuda_get_device_description(i, buf, sizeof(buf));
                vec.emplace_back(buf);
            }
            return join(vec, ", ");
        }
    } else {
        return llamafile_describe_cpu();
    }
}

std::string token_to_piece(const struct llama_context *ctx, llama_token token, bool special) {
    if (token == IMAGE_PLACEHOLDER_TOKEN)
        return "⁑";
    return llamafile_token_to_piece(ctx, token, special);
}

void on_server_listening(const char *host, int port) {
    pthread_mutex_lock(&g_lock);
    g_listen_url = format("http://%s:%d/", host, port);
    pthread_cond_signal(&g_cond);
    pthread_mutex_unlock(&g_lock);
}

void *server_thread(void *arg) {
    // TODO: Update server integration when llama.cpp/tools/server/server.h API is available
    // ServerArgs *sargs = (ServerArgs *)arg;
    // server_log_json = false;
    // g_server_background_mode = true;
    // g_server_force_llama_model = g_model;
    // g_server_on_listening = on_server_listening;
    // exit(server_cli(sargs->argc, sargs->argv));
    fprintf(stderr, "error: server mode not yet implemented in modernized llamafile\n");
    exit(1);
}

const char *tip() {
    if (g_params.verbosity)
        return "";
    return " (use the --verbose flag for further details)";
}

bool is_base_model() {

    // check if user explicitly passed --chat-template flag
    if (!g_params.chat_template.empty())
        return false;

    // check if gguf metadata has chat template. this should always be
    // present for "instruct" models, and never specified on base ones
    return llama_model_meta_val_str(g_model, "tokenizer.chat_template", 0, 0) == -1;
}

int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);

    // print logo
    logo(argv);

    // disable llamafile gpu initialization log messages
    if (!llamafile_has(argv, "--verbose"))
        FLAG_log_disable = true;

    // override defaults for some flags
    g_params.n_batch = 256; // for better progress indication
    // Updated: sparams is now sampling (API changed in llama.cpp)
    g_params.sampling.temp = 0; // don't believe in randomness by default
    g_params.prompt = DEFAULT_SYSTEM_PROMPT;

    // parse flags (sadly initializes gpu support as side-effect)
    print_ephemeral("loading backend...");
    llama_backend_init();
    // Updated: gpt_params_parse → common_params_parse (API changed in llama.cpp)
    if (!common_params_parse(argc, argv, g_params, LLAMA_EXAMPLE_MAIN, nullptr)) { // also loads gpu module
        fprintf(stderr, "error: failed to parse flags\n");
        exit(1);
    }
    clear_ephemeral();

    // setup logging
    FLAG_log_disable = false;
    if (!g_params.verbosity) {
        // Updated: Just set FLAG_log_disable directly instead of calling log_disable()
        FLAG_log_disable = true;
    }

    print_ephemeral("loading model...");
    // Updated: llama_model_params_from_gpt_params → common_model_params_to_llama (API changed in llama.cpp)
    llama_model_params model_params = common_model_params_to_llama(g_params);
    // Updated: g_params.model is now a struct with .path member (API changed in llama.cpp)
    // Updated: llama_load_model_from_file → llama_model_load_from_file (renamed in llama.cpp)
    g_model = llama_model_load_from_file(g_params.model.path.c_str(), model_params);
    clear_ephemeral();
    if (g_model == NULL) {
        fprintf(stderr, "%s: failed to load model%s\n", g_params.model.path.c_str(), tip());
        exit(2);
    }
    // Updated: llama_n_ctx_train → llama_model_n_ctx_train (renamed in llama.cpp)
    if (g_params.n_ctx <= 0 || g_params.n_ctx > llama_model_n_ctx_train(g_model))
        g_params.n_ctx = llama_model_n_ctx_train(g_model);
    if (g_params.n_ctx < g_params.n_batch)
        g_params.n_batch = g_params.n_ctx;

    bool want_server = !llamafile_has(argv, "--chat") && !llamafile_has(argv, "--v2");
    if (want_server) {
        print_ephemeral("launching server...");
        pthread_t thread;
        pthread_attr_t attr;
        ServerArgs sargs = {argc, argv};
        pthread_mutex_lock(&g_lock);
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&thread, &attr, server_thread, &sargs);
        pthread_attr_destroy(&attr);
        pthread_cond_wait(&g_cond, &g_lock);
        pthread_mutex_unlock(&g_lock);
        clear_ephemeral();
    }

    if (!FLAG_nologo) {
        printf(BOLD "software" UNBOLD ": llamafile " LLAMAFILE_VERSION_STRING "\n" //
               BOLD "model" UNBOLD ":    %s\n",
               basename(g_params.model.path).c_str());
        if (is_base_model())
            printf(BOLD "mode" UNBOLD ":     RAW TEXT COMPLETION (base model)\n");
        printf(BOLD "compute" UNBOLD ":  %s\n", describe_compute().c_str());
        if (want_server)
            printf(BOLD "server" UNBOLD ":   %s\n", g_listen_url.c_str());
        printf("\n");
    }

    print_ephemeral("initializing context...");
    // Updated: llama_context_params_from_gpt_params → common_context_params_to_llama (API changed in llama.cpp)
    llama_context_params ctx_params = common_context_params_to_llama(g_params);
    // Updated: llama_new_context_with_model → llama_init_from_model (renamed in llama.cpp)
    g_ctx = llama_init_from_model(g_model, ctx_params);
    clear_ephemeral();
    if (!g_ctx) {
        fprintf(stderr, "error: failed to initialize context%s\n", tip());
        exit(3);
    }

    if (llama_model_has_encoder(g_model))
        fprintf(stderr, "warning: this model has an encoder\n");

    if (FLAG_mmproj) {
        print_ephemeral("initializing vision model...");
        // Updated: clip_model_load → clip_init (API changed in llama.cpp)
        clip_context_params clip_params;
        clip_params.use_gpu = (g_params.n_gpu_layers > 0);
        clip_params.verbosity = g_params.verbosity > 0 ? GGML_LOG_LEVEL_INFO : GGML_LOG_LEVEL_ERROR;
        auto clip_res = clip_init(FLAG_mmproj, clip_params);
        g_clip = clip_res.ctx_v;  // Use vision context
        clear_ephemeral();
        if (!g_clip) {
            fprintf(stderr, "%s: failed to initialize clip image model%s\n", FLAG_mmproj, tip());
            exit(4);
        }
    }

    repl();

    if (g_clip) {
        print_ephemeral("freeing vision model...");
        clip_free(g_clip);
        clear_ephemeral();
    }

    print_ephemeral("freeing context...");
    llama_free(g_ctx);
    clear_ephemeral();

    print_ephemeral("freeing model...");
    // Updated: llama_free_model → llama_model_free (renamed in llama.cpp)
    llama_model_free(g_model);
    clear_ephemeral();

    print_ephemeral("freeing backend...");
    llama_backend_free();
    clear_ephemeral();

    return 0;
}

} // namespace chatbot
} // namespace lf
