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

#include <cmath>
#include <cosmo.h>
#include <cstdio>
#include <string>
#include <vector>

#include "llama.cpp/common/common.h"
#include "llama.cpp/common/sampling.h"
#include "llama.cpp/common/arg.h"
#include "llama.cpp/include/llama.h"
#include "llamafile/llamafile.h"

static bool eval_tokens(struct llama_context *ctx_llama, std::vector<llama_token> tokens,
                        int n_batch, int *n_past) {
    int N = (int)tokens.size();
    for (int i = 0; i < N; i += n_batch) {
        int n_eval = (int)tokens.size() - i;
        if (n_eval > n_batch)
            n_eval = n_batch;
        // Updated: llama_batch_get_one now takes 2 params (API simplified in llama.cpp)
        llama_batch batch = llama_batch_get_one(&tokens[i], n_eval);
        batch.pos[0] = *n_past;
        batch.seq_id[0][0] = 0;
        if (llama_decode(ctx_llama, batch))
            return false; // probably ran out of context
        *n_past += n_eval;
    }
    return true;
}

static bool eval_id(struct llama_context *ctx_llama, int id, int *n_past) {
    std::vector<llama_token> tokens;
    tokens.push_back(id);
    return eval_tokens(ctx_llama, tokens, 1, n_past);
}

static bool eval_string(struct llama_context *ctx_llama, const char *str, int n_batch, int *n_past,
                        bool add_bos) {
    std::string str2 = str;
    // Updated: llama_tokenize now requires vocab instead of context
    const struct llama_model *model = llama_get_model(ctx_llama);
    const struct llama_vocab *vocab = llama_model_get_vocab(model);

    // Tokenize with new API (7 params)
    std::vector<llama_token> embd_inp(str2.length() + (add_bos ? 1 : 0));
    int n = ::llama_tokenize(vocab, str2.c_str(), str2.length(), embd_inp.data(), embd_inp.size(), add_bos, true);
    if (n < 0) {
        embd_inp.resize(-n);
        n = ::llama_tokenize(vocab, str2.c_str(), str2.length(), embd_inp.data(), embd_inp.size(), add_bos, true);
    }
    embd_inp.resize(n);
    return eval_tokens(ctx_llama, embd_inp, n_batch, n_past);
}

int main(int argc, char **argv) {

    llamafile_check_cpu();
    ShowCrashReports();

    // Updated: log_disable() → FLAG_log_disable (API changed in llama.cpp)
    FLAG_log_disable = true;

    // Updated: gpt_params → common_params (renamed in llama.cpp)
    common_params params;
    params.n_ctx = 0;

    // Updated: gpt_params_parse → common_params_parse (renamed in llama.cpp)
    if (!common_params_parse(argc, argv, params, LLAMA_EXAMPLE_MAIN, nullptr))
        return 1;

    if (params.prompt.empty())
        params.prompt = "The";

    llama_backend_init();

    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = llamafile_gpu_layers(35);

    // Updated: params.model is now a struct with .path member
    llama_model *model = llama_model_load_from_file(params.model.path.c_str(), model_params);
    if (model == NULL)
        return 2;

    // Updated: llama_context_params_from_gpt_params → common_context_params_to_llama
    llama_context_params ctx_params = common_context_params_to_llama(params);

    // Updated: llama_new_context_with_model → llama_init_from_model
    llama_context *ctx = llama_init_from_model(model, ctx_params);
    if (ctx == NULL)
        return 3;

    printf("%s", params.prompt.c_str());
    int n_past = 0;

    // Updated: llama_add_bos_token → llama_vocab_get_add_bos (latest API)
    const struct llama_vocab *vocab = llama_model_get_vocab(model);
    bool add_bos = llama_vocab_get_add_bos(vocab);
    eval_string(ctx, params.prompt.c_str(), params.n_batch, &n_past, add_bos);

    // Updated: llama_sampling_* → common_sampler_* (API overhauled in llama.cpp)
    // Updated: params.sparams → params.sampling (renamed in common_params)
    struct common_sampler *ctx_sampling = common_sampler_init(model, params.sampling);
    for (;;) {
        llama_token id = common_sampler_sample(ctx_sampling, ctx, -1);
        common_sampler_accept(ctx_sampling, id, true);

        // Updated: llama_token_is_eog now requires vocab parameter
        if (llama_vocab_is_eog(vocab, id))
            break;

        // Updated: llama_token_to_piece with vocab parameter
        char buf[256];
        int n = llama_token_to_piece(vocab, id, buf, sizeof(buf), 0, true);
        if (n > 0) {
            printf("%.*s", n, buf);
        }
        fflush(stdout);
        if (!eval_id(ctx, id, &n_past))
            break;
    }
    printf("\n");

    // Updated: llama_sampling_free → common_sampler_free
    common_sampler_free(ctx_sampling);
    llama_free(ctx);

    // Updated: llama_free_model → llama_model_free (API renamed)
    llama_model_free(model);
    llama_backend_free();
}
