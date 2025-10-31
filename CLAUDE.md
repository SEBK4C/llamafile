# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Llamafile** is a system for distributing and running Large Language Models (LLMs) as single-file portable executables. It combines [llama.cpp](https://github.com/ggerganov/llama.cpp) with [Cosmopolitan Libc](https://github.com/jart/cosmopolitan) to create Actually Portable Executables (APE) that run on Linux, macOS, Windows, FreeBSD, OpenBSD, and NetBSD without modification or dependencies.

**Key Innovation:** Models + inference engine + runtime = single executable that works everywhere.

### Current State: 85% Modernization Complete

This repository is currently undergoing modernization to integrate:
- **Latest llama.cpp** (new modular structure with 100+ model architectures)
- **Cosmocc 4.0.2** (latest Cosmopolitan Libc compiler with C++23 support)

**Status:** Build system complete, core libraries compiling, ~20 API wrapper compilation errors remaining.

**Critical:** Read `/DOCS/` folder first for complete modernization status, task breakdown, and API migration guides.

## Build System

### Requirements
- **GNU Make 4.4+** - Use `gmake` on macOS (NOT the built-in make 3.81)
- **wget** or **curl** - For downloading toolchain
- **sha256sum** - Or cc will build it
- **unzip** - Available at https://cosmo.zip/pub/cosmos/bin/

### Building

```bash
# Standard build (downloads cosmocc 4.0.2 automatically)
gmake -j$(nproc)

# Clean build
gmake clean

# Full clean (including toolchain)
gmake distclean

# Install to system
sudo gmake install PREFIX=/usr/local

# Run tests
gmake check
```

**Important:** The Makefile will automatically download cosmocc 4.0.2 to `.cosmocc/4.0.2/` if not present.

### Build Configuration

Located in `build/config.mk`:
- **Compiler:** cosmocc/cosmoc++ (Cosmopolitan Libc GCC 14.1.0)
- **C++ Standard:** gnu++23
- **Optimization:** -O2 with -g for debugging
- **Target Arch:** x86_64-mtune=znver4 (AMD Zen 4)
- **Key Flags:** -DGGML_MULTIPLATFORM -DLLAMAFILE_DEBUG

### Hierarchical Build Structure

```
Makefile (root)
├── build/config.mk          # Toolchain configuration
├── build/rules.mk           # Compilation rules
├── third_party/BUILD.mk     # mbedtls, sqlite, stb, double-conversion
├── llamafile/BUILD.mk       # Core llamafile implementation
│   ├── llamafile/highlight/BUILD.mk
│   └── llamafile/server/BUILD.mk
├── llama.cpp/BUILD.mk       # LLM inference engine
│   └── tools/*/BUILD.mk     # main, server, quantize, imatrix, etc.
├── whisper.cpp/BUILD.mk     # Speech-to-text
├── stable-diffusion.cpp/BUILD.mk
└── localscore/BUILD.mk      # Benchmarking
```

Each BUILD.mk defines:
- `PKGS` - Package names
- `*_SRCS` - Source file wildcards
- `*_OBJS` - Object file targets
- Dependencies and build rules

## Architecture Overview

### Core Components

1. **llamafile/** (17K LOC) - Core implementation
   - APE packaging and ZIP embedding (`zipalign.c`, `llamafile.c`)
   - GPU acceleration (`cuda.c`, `metal.c`, `gpu.c`)
   - Custom matrix kernels (`tinyblas*.cpp`, `sgemm*.cpp`, `iqk_mul_mat*.inc`)
   - Web server (`server/`)
   - Chatbot interface (`chatbot_*.cpp`)
   - Syntax highlighting (`highlight/` - 40+ languages)
   - Utilities (JSON, HTTP, database, threading)

2. **llama.cpp/** (200K+ LOC) - LLM inference (NEW MODULAR STRUCTURE)
   ```
   ggml/
   ├── include/         # Public GGML headers
   └── src/             # Implementation + backends
       ├── ggml-cpu/    # CPU backend (arch/x86/, arch/arm/)
       ├── ggml-cuda/   # NVIDIA CUDA
       ├── ggml-metal/  # Apple Metal
       └── 12+ other backends
   include/             # Public llama.h
   src/                 # 55 modular llama-*.cpp files
   common/              # Shared utilities
   tools/               # CLI tools (main, server, quantize, mtmd, etc.)
   ```

3. **whisper.cpp/** - Speech-to-text (Whisper model)

4. **stable-diffusion.cpp/** - Image generation (Stable Diffusion)

### Key Technologies

- **APE (Actually Portable Executable)**: Shell script + native binary that runs on 6 OSes
- **ZIP Embedding**: Models stored in ZIP with 64KB page alignment for direct mmap()
- **Runtime GPU Compilation**: Extracts GPU source, compiles on-demand with nvcc/hipcc/xcode
- **TinyBLAS**: Custom CPU matrix kernels (2-5x faster than standard BLAS)
- **Microarchitecture Dispatch**: Multiple optimized implementations (AVX, AVX2, AVX512, NEON)

### Performance Secret

Llamafile achieves 2-5x speedup through **84 custom matrix multiplication kernels** that:
- Unroll outer loops instead of inner loops
- Exploit LLM-specific patterns (alpha=1, beta=0, matrix A always transposed)
- Share register loads across multiple FP operations
- Achieve 810 gigaflops vs Intel MKL's 295 gigaflops on i9-14900K

**Critical:** Preserve these optimizations during any code changes.

## Important File Locations

### Include Paths (Post-Modernization)
```c
// GGML public headers
#include "llama.cpp/ggml/include/ggml.h"
#include "llama.cpp/ggml/include/ggml-backend.h"
#include "llama.cpp/ggml/include/ggml-cuda.h"
#include "llama.cpp/ggml/include/ggml-metal.h"

// GGML internal headers
#include "llama.cpp/ggml/src/ggml-impl.h"
#include "llama.cpp/ggml/src/ggml-quants.h"
#include "llama.cpp/ggml/src/ggml-common.h"

// Llama public headers
#include "llama.cpp/include/llama.h"

// Common utilities
#include "llama.cpp/common/common.h"
#include "llama.cpp/common/sampling.h"
#include "llama.cpp/common/base64.hpp"
```

### Key Source Files
- **llamafile/chatbot_main.cpp** - Main chatbot entry (⚠️ ~18 compilation errors)
- **llamafile/compute.cpp** - GPU backend initialization (⚠️ 1 error)
- **llamafile/chatbot_eval.cpp** - Prompt evaluation (⚠️ image support stubbed)
- **llamafile/cuda.c** - NVIDIA GPU runtime compilation
- **llamafile/metal.c** - Apple Metal GPU compilation
- **llamafile/zipalign.c** - ZIP packaging tool (500 LOC)

### Documentation
- **/DOCS/** - **READ THIS FIRST** for modernization status
  - `COMPLETION_STATUS.md` - What's done, what remains
  - `AGENT_HANDOFF.md` - Quick start guide
  - `NEXT_STEPS_AGENT_GUIDE.md` - Detailed task breakdown with code examples
  - `API_MIGRATION_QUICK_REF.md` - API change lookup table
  - `SPEC.txt` - Original requirements and performance explanation

## Common Development Tasks

### Creating a Llamafile

```bash
# Build the tooling
gmake o/$(MODE)/llamafile/zipalign

# Package a model
o/$(MODE)/llamafile/zipalign -j0 \
  output.llamafile \
  model.gguf \
  .args

# .args contains default CLI arguments (one per line)
```

### Running Tests

```bash
# Run all tests
gmake check

# Individual test binaries are in:
# o/$(MODE)/llamafile/*_test
# o/$(MODE)/llamafile/server/*_test
# o/$(MODE)/llamafile/highlight/*_test
```

### GPU Support Testing

```bash
# NVIDIA: Requires nvcc on PATH
# AMD: Requires hipcc on PATH
# Apple: Requires Xcode Command Line Tools

# Enable GPU offloading
./model.llamafile -ngl 9999

# Test specific GPU
./model.llamafile --gpu nvidia  # or amd, apple, auto
```

## Current Modernization Issues

### ⚠️ Known Compilation Errors (~20 total)

1. **compute.cpp** (1 error)
   - Line 24: Include path issue with `llama.cpp/string.h`
   - Fix: Change to `<cstring>` or `<string.h>`

2. **chatbot_main.cpp** (~18 errors)
   - Missing includes (`llamafile_has_gpu`, `llamafile_has_metal`)
   - API changes:
     - `gpt_params` → `common_params`
     - `gpt_params_parse` → new equivalent
     - `llama_model_params_from_gpt_params` → new API
   - Sampling API complete overhaul:
     - `llama_sampling_*` → `llama_sampler_*`
     - Context type changed
   - Token functions now require vocab parameter
   - Struct member access issues (`.sparams` doesn't exist)

3. **chatbot_eval.cpp** (stubbed, not error)
   - Image evaluation temporarily disabled
   - Needs migration from llava API to mtmd (multimodal) API

### API Migration Patterns

**Token Functions:**
```cpp
// OLD:
llama_token tok = llama_token_bos(model);

// NEW:
const struct llama_vocab *vocab = llama_model_get_vocab(model);
llama_token tok = llama_vocab_bos(vocab);
```

**Memory/KV Cache:**
```cpp
// OLD:
llama_kv_cache_seq_rm(ctx, seq_id, pos0, pos1);

// NEW:
llama_memory_t mem = llama_get_memory(ctx);
llama_memory_seq_rm(mem, seq_id, pos0, pos1);
```

**Batch API:**
```cpp
// OLD:
llama_batch batch = llama_batch_get_one(tokens, n_tokens, pos, seq_id);

// NEW:
llama_batch batch = llama_batch_get_one(tokens, n_tokens);
// Position and seq_id now auto-managed
```

See `/DOCS/API_MIGRATION_QUICK_REF.md` for complete reference.

## Development Guidelines

### When Making Changes

1. **Read /DOCS/ first** - Comprehensive modernization documentation
2. **Preserve performance** - Don't break the 84 custom matrix kernels
3. **Test on multiple platforms** - Linux, macOS, Windows minimum
4. **Check GPU support** - Both NVIDIA and AMD
5. **Verify portability** - Don't introduce OS-specific dependencies
6. **Reference llama.cpp upstream** - Use `llama.cpp/tools/main/main.cpp` as example

### File Editing Rules

- **Include paths**: Use new structure (ggml/include/, ggml/src/, common/, include/)
- **API calls**: Check API_MIGRATION_QUICK_REF.md before using llama.cpp functions
- **Build files**: Follow BUILD.mk pattern (PKGS, *_SRCS, *_OBJS)
- **Optimization flags**: Maintain microarchitecture-specific builds

### Testing Checklist

Before committing changes:
- [ ] Build succeeds: `gmake clean && gmake -j$(nproc)`
- [ ] Tests pass: `gmake check`
- [ ] Loads modern models (Llama 3.x, Mistral, Gemma, Phi)
- [ ] GPU offloading works (`-ngl 9999`)
- [ ] Performance not regressed (compare prompt/generation speed)
- [ ] Cross-platform (test on Linux + macOS minimum)

## Troubleshooting

### Build Failures

**"please use bin/make from cosmocc.zip rather than old xcode make"**
- Solution: Use `gmake` instead of `make` on macOS

**Missing cosmocc toolchain**
- Solution: `gmake cosmocc` to download it

**"nvcc not found" or "hipcc not found"**
- Info: GPU compilation will fall back to CPU (not an error)
- Solution: Install CUDA SDK or ROCm SDK if GPU needed

### Runtime Issues

**"cannot execute binary file"**
- Solution (Linux): Register APE loader
  ```bash
  sudo wget -O /usr/bin/ape https://cosmo.zip/pub/cosmos/bin/ape-$(uname -m).elf
  sudo chmod +x /usr/bin/ape
  sudo sh -c "echo ':APE:M::MZqFpD::/usr/bin/ape:' >/proc/sys/fs/binfmt_misc/register"
  ```

**"developer cannot be verified" (macOS)**
- Solution: System Settings → Privacy & Security → Allow

**GPU not detected**
- Check: `--gpu auto` flag
- NVIDIA/AMD: Pass `-ngl 999` to enable offloading
- Apple: Metal enabled by default, use `-ngl 0` to disable

## Code Reference Examples

### llama.cpp Upstream Examples

When updating API calls, reference these files from llama.cpp:
- **CLI tool**: `llama.cpp/tools/main/main.cpp`
- **Server**: `llama.cpp/tools/server/server.cpp`
- **Sampling**: `llama.cpp/common/sampling.cpp`
- **Parameters**: `llama.cpp/common/common.cpp`

### Llamafile Integration Examples

- **Model loading**: `llamafile/llamafile.c:llamafile_open_gguf()`
- **GPU init**: `llamafile/compute.cpp:llamafile_gpu_layers()`
- **Server API**: `llamafile/server/v1_chat_completions.cpp`
- **Tokenization**: `llamafile/llama.cpp:llamafile_tokenize()`

## Security Considerations

- **Sandboxing**: pledge() on OpenBSD, SECCOMP on Linux (CPU-only mode)
- **No network in CLI**: Main command cannot access network
- **No filesystem writes**: Except with `--prompt-cache` (non-readonly)
- **GPU mode**: Sandboxing disabled (requires system compiler access)

## Performance Notes

### Benchmarking

```bash
# Build benchmark tool
gmake o/$(MODE)/localscore/localscore

# Run benchmark
o/$(MODE)/localscore/localscore -m model.gguf
```

### Expected Performance

- **Prompt processing**: 2-5x faster than standard llama.cpp
- **Token generation**: Competitive with llama.cpp
- **Memory usage**: Similar to llama.cpp
- **GPU utilization**: Should be 90%+ with `-ngl 999`

### Performance Regression Signs

- Prompt processing slower than llama.cpp
- CPU usage lower than expected
- GPU not utilized despite `-ngl 999`
- Memory usage significantly higher

If detected: Profile with `perf` and compare against previous llamafile release.

## Related Documentation

- **README.md** - User-facing documentation
- **/DOCS/** - Complete modernization status and guides
- **llama.cpp/README.md** - Upstream llama.cpp documentation
- **llama.cpp/docs/** - Detailed technical documentation
- **llamafile/server/doc/** - Server API documentation

## Contact and Support

- **Issues**: https://github.com/Mozilla-Ocho/llamafile/issues
- **Discord**: https://discord.gg/YuMNeuKStr

## Project Status Summary

**Current State (85% Complete):**
- ✅ Cosmocc 4.0.2 integrated
- ✅ Build system modernized (8 BUILD.mk files)
- ✅ Include paths updated (~60 files)
- ✅ GGML + Llama core compiling
- ✅ GPU backends updated
- ⏳ ~20 compilation errors in chatbot_main.cpp and compute.cpp
- ❌ Multimodal (image) support temporarily stubbed

**Next Steps (4-8 hours estimated):**
1. Fix compute.cpp include (5 minutes)
2. Fix chatbot_main.cpp API calls (2-3 hours)
3. Complete build and test (1-2 hours)
4. Migrate multimodal support (2-4 hours)

**See /DOCS/NEXT_STEPS_AGENT_GUIDE.md for detailed task breakdown.**
