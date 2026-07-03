// -*- mode:c++;indent-tabs-mode:nil;c-basic-offset:4;coding:utf-8 -*-
// vi: set et ft=cpp ts=4 sts=4 sw=4 fenc=utf-8 :vi
//
// Copyright 2024 Mozilla Foundation
// Copyright 2026 Mozilla.ai
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

#include "args.h"
#include "llamafile.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ftw.h>
#include <limits.h>
#include <string>
#include <unistd.h>
#include <vector>

namespace lf {

// Static storage for filtered argv (persists after function returns)
static std::vector<char*> g_filtered_argv;

// Helper: returns true if arg is a llamafile-specific flag (not recognized by llama.cpp)
static bool is_llamafile_flag(const char* arg) {
    return strcmp(arg, "--server") == 0 ||
           strcmp(arg, "--chat") == 0 ||
           strcmp(arg, "--cli") == 0 ||
           strcmp(arg, "--gpu") == 0 ||
           strcmp(arg, "--ascii") == 0 ||
           strcmp(arg, "--nologo") == 0 ||
           strcmp(arg, "--nothink") == 0 ||
           strcmp(arg, "--clear-all") == 0 ||
           strcmp(arg, "--version") == 0;
}


// ── hardware auto-tuning ────────────────────────────────────────────────
// The packaged .args carry only universal flags; everything workload- or
// backend-shaped is appended here per detected platform, and ONLY when the
// user (or .args) didn't set that flag already. Validated numbers:
// Metal draft n=2 / CUDA n=4 per this repo's benchmarks; f16 KV beats q8_0
// by ~16% on CUDA decode; CPU + draft-mtp can hit the upstream draft-probe
// assert, so speculation defaults off there.
static bool has_any(char** argv, std::initializer_list<const char*> names) {
    for (const char* n : names)
        if (llamafile_has(argv, n))
            return true;
    return false;
}

static std::vector<std::string> g_tuned_store;

static void tune(std::vector<char*>& out, char** argv,
                 std::initializer_list<const char*> names,
                 std::initializer_list<const char*> tokens) {
    if (has_any(argv, names))
        return;
    for (const char* t : tokens) {
        g_tuned_store.emplace_back(t);
        out.push_back(g_tuned_store.back().data());
    }
}

static void autotune_server_args(std::vector<char*>& out, char** argv) {
    if (getenv("LLAMAFILE_NO_AUTOTUNE"))
        return;
    const bool cuda  = llamafile_has_cuda() || llamafile_has_amd_gpu();
    const bool metal = llamafile_has_metal();
    const char* plat = cuda ? "CUDA" : metal ? "Metal" : "CPU";
    // reserve so .data() pointers survive future emplacements
    g_tuned_store.reserve(32);
    size_t before = out.size();
    if (cuda) {
        tune(out, argv, {"-np", "--parallel"},           {"-np", "1"});
        tune(out, argv, {"-c", "--ctx-size"},            {"-c", "131072"});
        tune(out, argv, {"-b", "--batch-size"},          {"-b", "2048"});
        tune(out, argv, {"-ub", "--ubatch-size"},        {"-ub", "256"});
        tune(out, argv, {"-ctk", "--cache-type-k"},      {"-ctk", "f16"});
        tune(out, argv, {"-ctv", "--cache-type-v"},      {"-ctv", "f16"});
        tune(out, argv, {"-sm", "--split-mode"},         {"-sm", "none"});
        tune(out, argv, {"--spec-type"},                 {"--spec-type", "draft-mtp"});
        tune(out, argv, {"-md", "--model-draft"},        {"-md", "/zip/mtp-gemma-4-12b-it-qat-q4_0.gguf"});
        tune(out, argv, {"--spec-draft-n-max"},          {"--spec-draft-n-max", "4"});
    } else if (metal) {
        tune(out, argv, {"-c", "--ctx-size"},            {"-c", "8192"});
        tune(out, argv, {"-b", "--batch-size"},          {"-b", "2048"});
        tune(out, argv, {"-ub", "--ubatch-size"},        {"-ub", "1024"});
        tune(out, argv, {"--no-mmproj-offload"},         {"--no-mmproj-offload"});
        tune(out, argv, {"--spec-type"},                 {"--spec-type", "draft-mtp"});
        tune(out, argv, {"-md", "--model-draft"},        {"-md", "/zip/mtp-gemma-4-12b-it-qat-q4_0.gguf"});
        tune(out, argv, {"--spec-draft-n-max"},          {"--spec-draft-n-max", "2"});
    } else {
        tune(out, argv, {"-c", "--ctx-size"},            {"-c", "8192"});
        tune(out, argv, {"-b", "--batch-size"},          {"-b", "2048"});
        tune(out, argv, {"-ub", "--ubatch-size"},        {"-ub", "512"});
        tune(out, argv, {"--spec-type"},                 {"--spec-type", "none"});
    }
    if (out.size() > before) {
        fprintf(stderr, "autotune: %s detected — applied tuned defaults (", plat);
        for (size_t i = before; i < out.size(); ++i)
            fprintf(stderr, "%s%s", i > before ? " " : "", out[i]);
        fprintf(stderr, ") — override any flag to disable it, LLAMAFILE_NO_AUTOTUNE=1 to disable all\n");
    }
}

// ── --clear-all ─────────────────────────────────────────────────────────
// Wipe every piece of state this server leaves on disk: the KV slot-save
// directory (which persists across restarts AND config changes — stale
// saves can mask config fixes) and the extracted app dir (~/.llamafile/v/*,
// re-extracted from the zip on next start). Startup then continues fresh.
static int rm_cb(const char* p, const struct stat*, int, struct FTW*) {
    return remove(p);
}

static void clear_all_state(char** argv) {
    const char* slot = "./.gemma4-kv";
    for (int i = 0; argv[i]; ++i)
        if (!strcmp(argv[i], "--slot-save-path") && argv[i + 1])
            slot = argv[i + 1];
    char app[PATH_MAX];
    llamafile_get_app_dir(app, sizeof(app));
    for (const char* dir : {slot, (const char*)app}) {
        if (!access(dir, F_OK)) {
            nftw(dir, rm_cb, 16, FTW_DEPTH | FTW_PHYS);
            fprintf(stderr, "--clear-all: removed %s\n", dir);
        }
    }
    fprintf(stderr, "--clear-all: done; continuing with a fresh start\n");
}

LlamafileArgs parse_llamafile_args(int argc, char** argv) {
    LlamafileArgs args;

    // Early GPU init must happen before we filter args
    // This reads --gpu and -ngl flags to set FLAG_gpu
    llamafile_early_gpu_init(argv);

    // Capture -p/--prompt value before filtering (needed for combined mode
    // where SERVER parsing excludes -p)
    // Note: Loop does not break early; if multiple -p flags are given,
    // the last occurrence wins (intentional for override flexibility)
    for (int i = 0; i < argc; ++i) {
        if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--prompt") == 0) && i + 1 < argc) {
            args.system_prompt = argv[i + 1];
        }
        if ((strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--model") == 0) && i + 1 < argc) {
            args.model_path = argv[i + 1];
        }
    }

    // Determine execution mode from flags
    // Priority: explicit flags override defaults
    if (llamafile_has(argv, "--server")) {
        args.mode = ProgramMode::SERVER;
    } else if (llamafile_has(argv, "--chat")) {
        args.mode = ProgramMode::CHAT;
    } else if (llamafile_has(argv, "--cli")) {
        args.mode = ProgramMode::CLI;
    } else {
        // AUTO mode: will run combined chat + server
        args.mode = ProgramMode::AUTO;
    }

    // Check verbose flag
    FLAG_verbose = llamafile_has(argv, "--verbose") ? 1 : 0;

    // Check --nothink flag (filters thinking/reasoning content in CLI mode)
    FLAG_nothink = llamafile_has(argv, "--nothink");

    // Check logo flags
    FLAG_nologo = llamafile_has(argv, "--nologo");
    FLAG_ascii = llamafile_has(argv, "--ascii");

    // Filter out llamafile-specific arguments
    // These are not recognized by llama.cpp and would cause errors
    g_filtered_argv.clear();

    for (int i = 0; i < argc; ++i) {
        const char* arg = argv[i];

        // Skip llamafile-specific flags
        if (is_llamafile_flag(arg)) {
            // --gpu takes a value argument, skip it too
            if (strcmp(arg, "--gpu") == 0 && i + 1 < argc) {
                ++i;
            }
            continue;
        }

        // Keep this argument
        g_filtered_argv.push_back(argv[i]);
    }

    // --clear-all: wipe slot-save KV dir + extracted app dir, then continue
    if (llamafile_has(argv, "--clear-all"))
        clear_all_state(argv);

    // Hardware auto-tuning (server mode): append per-platform defaults for
    // flags nobody set. GPU detection already ran via llamafile_early_gpu_init.
    if (args.mode == ProgramMode::SERVER || args.mode == ProgramMode::AUTO)
        autotune_server_args(g_filtered_argv, argv);

    // Null-terminate argv array (required by convention)
    g_filtered_argv.push_back(nullptr);

    args.llama_argc = static_cast<int>(g_filtered_argv.size()) - 1;
    args.llama_argv = g_filtered_argv.data();

    return args;
}

} // namespace lf
