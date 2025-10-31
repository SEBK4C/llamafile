// -*- mode:c;indent-tabs-mode:nil;c-basic-offset:4;coding:utf-8 -*-
// vi: set et ft=c ts=4 sts=4 sw=4 fenc=utf-8 :vi
//
// Copyright 2023 Mozilla Foundation
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

#include "llama.cpp/ggml/include/ggml-metal.h"
#include "llamafile.h"
#include "log.h"
#include <assert.h>
#include <cosmo.h>
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <spawn.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

__static_yoink("llama.cpp/ggml/include/ggml.h");
__static_yoink("llamafile/llamafile.h");
__static_yoink("llama.cpp/ggml/src/ggml-impl.h");
__static_yoink("llama.cpp/ggml/include/ggml-alloc.h");
__static_yoink("llama.cpp/ggml/src/ggml-metal/ggml-metal.m");
__static_yoink("llama.cpp/ggml/include/ggml-metal.h");
__static_yoink("llama.cpp/ggml/src/ggml-common.h");
__static_yoink("llama.cpp/ggml/src/ggml-quants.h");
__static_yoink("llama.cpp/ggml/include/ggml-backend.h");
__static_yoink("llama.cpp/ggml/src/ggml-metal/ggml-metal.metal");
__static_yoink("llama.cpp/ggml/src/ggml-backend-impl.h");

static const struct Source {
    const char *zip;
    const char *name;
} srcs[] = {
    {"/zip/llama.cpp/ggml/include/ggml.h", "ggml.h"},
    {"/zip/llamafile/llamafile.h", "llamafile.h"},
    {"/zip/llama.cpp/ggml/src/ggml-impl.h", "ggml-impl.h"},
    {"/zip/llama.cpp/ggml/include/ggml-metal.h", "ggml-metal.h"},
    {"/zip/llama.cpp/ggml/include/ggml-alloc.h", "ggml-alloc.h"},
    {"/zip/llama.cpp/ggml/src/ggml-common.h", "ggml-common.h"},
    {"/zip/llama.cpp/ggml/src/ggml-quants.h", "ggml-quants.h"},
    {"/zip/llama.cpp/ggml/include/ggml-backend.h", "ggml-backend.h"},
    {"/zip/llama.cpp/ggml/src/ggml-metal/ggml-metal.metal", "ggml-metal.metal"},
    {"/zip/llama.cpp/ggml/src/ggml-backend-impl.h", "ggml-backend-impl.h"},
    {"/zip/llama.cpp/ggml/src/ggml-metal/ggml-metal.m", "ggml-metal.m"}, // must come last
};

// The new Metal API is simplified
static struct Metal {
    bool supported;
    atomic_uint once;
    ggml_backend_t (*backend_init)(void);
    bool (*backend_is_metal)(ggml_backend_t);
    void (*set_abort_callback)(ggml_backend_t, ggml_abort_callback, void *);
    ggml_backend_reg_t (*reg)(void);
    bool (*supports_family)(ggml_backend_t, int);
    void (*capture_next_compute)(ggml_backend_t);
} ggml_metal;

static const char *Dlerror(void) {
    const char *msg;
    msg = cosmo_dlerror();
    if (!msg)
        msg = "null dlopen error";
    return msg;
}

static bool FileExists(const char *path) {
    struct stat st;
    return !stat(path, &st);
}

static bool BuildMetal(const char *dso) {

    // extract source code
    char src[PATH_MAX];
    bool needs_rebuild = false;
    for (int i = 0; i < sizeof(srcs) / sizeof(*srcs); ++i) {
        llamafile_get_app_dir(src, PATH_MAX);
        if (!i && makedirs(src, 0755)) {
            perror(src);
            return false;
        }
        strlcat(src, srcs[i].name, sizeof(src));
        switch (llamafile_is_file_newer_than(srcs[i].zip, src)) {
        case -1:
            return false;
        case 0:
            break;
        case 1:
            needs_rebuild = true;
            if (!llamafile_extract(srcs[i].zip, src)) {
                return false;
            }
            break;
        default:
            __builtin_unreachable();
        }
    }

    // determine if we need to build
    if (!needs_rebuild) {
        switch (llamafile_is_file_newer_than(src, dso)) {
        case -1:
            return false;
        case 0:
            break;
        case 1:
            needs_rebuild = true;
            break;
        default:
            __builtin_unreachable();
        }
    }

    // compile dynamic shared object
    if (needs_rebuild || FLAG_recompile) {
        tinylog("building ggml-metal.dylib with xcode...\n", NULL);
        int fd;
        char tmpdso[PATH_MAX];
        strlcpy(tmpdso, dso, sizeof(tmpdso));
        strlcat(tmpdso, ".XXXXXX", sizeof(tmpdso));
        if ((fd = mkostemp(tmpdso, O_CLOEXEC)) != -1) {
            close(fd);
        } else {
            perror(tmpdso);
            return false;
        }
        char *args[] = {
            "cc",
            "-I.",
            "-O3",
            "-fPIC",
            "-shared",
            "-pthread",
            "-DNDEBUG",
            "-ffixed-x28", // cosmo's tls register
            "-DTARGET_OS_OSX",
            "-DGGML_MULTIPLATFORM",
            src,
            "-o",
            tmpdso,
            "-framework",
            "Foundation",
            "-framework",
            "Metal",
            "-framework",
            "MetalKit",
            NULL,
        };
        int pid, ws;
        llamafile_log_command(args);
        errno_t err = posix_spawnp(&pid, "cc", NULL, NULL, args, environ);
        if (err) {
            perror("cc");
            if (err == ENOENT) {
                tinylog("PLEASE RUN: xcode-select --install\n", NULL);
            }
            return false;
        }
        while (waitpid(pid, &ws, 0) == -1) {
            if (errno != EINTR) {
                perror("cc");
                return false;
            }
        }
        if (ws) {
            tinylog("compiler returned nonzero exit status\n", NULL);
            return false;
        }
        if (rename(tmpdso, dso)) {
            perror(dso);
            return false;
        }
    }

    return true;
}

static bool LinkMetal(const char *dso) {

    // runtime link dynamic shared object
    void *lib;
    lib = cosmo_dlopen(dso, RTLD_LAZY);
    if (!lib) {
        tinylog(Dlerror(), ": failed to load library\n", NULL);
        return false;
    }

    // import functions (updated for new simplified API)
    bool ok = true;
    ok &= !!(ggml_metal.backend_init = cosmo_dlsym(lib, "ggml_backend_metal_init"));
    ok &= !!(ggml_metal.backend_is_metal = cosmo_dlsym(lib, "ggml_backend_is_metal"));
    ok &= !!(ggml_metal.set_abort_callback = cosmo_dlsym(lib, "ggml_backend_metal_set_abort_callback"));
    ok &= !!(ggml_metal.reg = cosmo_dlsym(lib, "ggml_backend_metal_reg"));
    ok &= !!(ggml_metal.supports_family = cosmo_dlsym(lib, "ggml_backend_metal_supports_family"));
    ok &= !!(ggml_metal.capture_next_compute = cosmo_dlsym(lib, "ggml_backend_metal_capture_next_compute"));
    if (!ok) {
        tinylog(Dlerror(), ": not all symbols could be imported\n", NULL);
        return false;
    }

    // The new API doesn't require explicit linking
    return true;
}

static bool ImportMetalImpl(void) {

    // Ensure this is MacOS ARM64.
    if (!IsXnuSilicon()) {
        return false;
    }

    // Check if we're allowed to even try.
    switch (FLAG_gpu) {
    case LLAMAFILE_GPU_AUTO:
    case LLAMAFILE_GPU_APPLE:
        break;
    default:
        return false;
    }

    npassert(FLAGS_READY);

    // Get path of DSO.
    char dso[PATH_MAX];
    llamafile_get_app_dir(dso, PATH_MAX);
    strlcat(dso, "ggml-metal.dylib", sizeof(dso));
    if (FLAG_nocompile) {
        return LinkMetal(dso);
    }

    // Build and link Metal support DSO if possible.
    if (BuildMetal(dso)) {
        return LinkMetal(dso);
    } else {
        return false;
    }
}

static void ImportMetal(void) {
    if (ImportMetalImpl()) {
        ggml_metal.supported = true;
        tinylog("Apple Metal GPU support successfully loaded\n", NULL);
    } else if (FLAG_gpu == LLAMAFILE_GPU_APPLE) {
        tinyprint(2, "fatal error: support for --gpu ", llamafile_describe_gpu(),
                  FLAG_tinyblas ? " --tinyblas" : "",
                  " was explicitly requested, but it wasn't available\n", NULL);
        exit(1);
    }
}

bool llamafile_has_metal(void) {
    cosmo_once(&ggml_metal.once, ImportMetal);
    return ggml_metal.supported;
}

ggml_backend_t ggml_backend_metal_init(void) {
    if (!llamafile_has_metal())
        return 0;
    return ggml_metal.backend_init();
}

// Note: These functions were removed in new Metal API
// Keeping stubs for backward compatibility
ggml_backend_buffer_type_t ggml_backend_metal_buffer_type(void) {
    // The new API doesn't expose buffer_type directly
    // Users should use ggml_backend_metal_init() and get buffer type from backend
    return NULL;
}

ggml_backend_buffer_t ggml_backend_metal_buffer_from_ptr(void *data, size_t size,
                                                         size_t max_size) {
    // This function was removed in the new API
    return NULL;
}

bool ggml_backend_is_metal(ggml_backend_t backend) {
    if (!llamafile_has_metal())
        return 0;
    return ggml_metal.backend_is_metal(backend);
}

// Note: Removed functions - keeping stubs for backward compatibility
void ggml_backend_metal_set_n_cb(ggml_backend_t backend, int n_cb) {
    (void)backend;
    (void)n_cb;
    // This function was removed in the new API
}

void ggml_backend_metal_log_set_callback(ggml_log_callback log_callback, void *user_data) {
    (void)log_callback;
    (void)user_data;
    // This function was removed in the new API
}

ggml_backend_t ggml_backend_reg_metal_init(const char *params, void *user_data) {
    (void)params;
    (void)user_data;
    // In the new API, use ggml_backend_metal_init() directly
    if (!llamafile_has_metal())
        return NULL;
    return ggml_metal.backend_init();
}

// Define backward compatibility struct
struct ggml_metal_device_properties {
    char name[256];
    size_t totalGlobalMem;
};

void ggml_backend_metal_get_device_properties(ggml_backend_t backend, struct ggml_metal_device_properties *properties) {
    (void)backend;
    (void)properties;
    // This function was removed - new API doesn't expose device properties this way
}

void ggml_backend_metal_get_device_memory_usage(ggml_backend_t backend, float *used, float *total) {
    (void)backend;
    (void)used;
    (void)total;
    // This function was removed in the new API
}

bool ggml_backend_metal_supports_family(ggml_backend_t backend, int family) {
    if (!llamafile_has_metal())
        return 0;
    return ggml_metal.supports_family(backend, family);
}
