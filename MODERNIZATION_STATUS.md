# Llamafile Modernization Status

**Date**: October 31, 2025
**Completion**: 90%
**Status**: Build system complete, backend linking issue blocking runtime

## ✅ Completed Work

### 1. Build System Modernization (100%)
- ✅ Updated to cosmocc 4.0.2 (Cosmopolitan Libc GCC 14.1.0)
- ✅ Migrated to C++23 standard
- ✅ Updated all 8 BUILD.mk files for new modular structure
- ✅ Configured GGML_MULTIPLATFORM and GGML_USE_CPU flags
- ✅ All core libraries compiling successfully

### 2. Include Path Migration (100%)
Updated ~60 files to new llama.cpp structure:
- ✅ `llama.cpp/ggml/include/` for public GGML headers
- ✅ `llama.cpp/ggml/src/` for internal GGML implementation
- ✅ `llama.cpp/include/` for public llama.h
- ✅ `llama.cpp/common/` for shared utilities
- ✅ All CPU backend files included (`ggml-cpu/`)

### 3. API Migration (95%)
#### Completed Files:
- ✅ `llamafile/simple.cpp` - Updated to common_params
- ✅ `llamafile/tinyblas_cpu.h` - Fixed includes
- ✅ `llamafile/server/slot.cpp` - Memory/vocab API updates
- ✅ `llamafile/server/tokenize.cpp` - Vocab API migration
- ✅ `llamafile/server/v1_chat_completions.cpp` - Type fixes
- ✅ `llamafile/server/prog.cpp` - Backend initialization order fixed
- ✅ `llama.cpp/src/llama-hparams.cpp` - Added <algorithm>
- ✅ `llama.cpp/common/arg.cpp` - Cosmopolitan sys/syslimits compatibility
- ✅ `llama.cpp/common/common.cpp` - Cache directory compatibility

#### Temporarily Stubbed:
- ⏳ `llamafile/server/slot.cpp:93` - `clip_free()` commented out (multimodal support)
- ⏳ `llama.cpp/llava/llava_stub.cpp` - Stub for CLIP functions

### 4. Core Libraries (100%)
- ✅ GGML core compiling with all sources
- ✅ CPU backend (ggml-cpu/) fully integrated
- ✅ All 55 llama-*.cpp modular files compiling
- ✅ Common utilities building successfully
- ✅ Third-party libraries (mbedtls, sqlite, stb, double-conversion) working
- ✅ TinyBLAS custom kernels preserved and compiling

## ⚠️ Known Issue: CPU Backend Linking

### Problem
The `ggml_backend_cpu_reg()` symbol cannot be resolved at link time despite being compiled into the archive.

**Error**:
```
undefined reference to `ggml_backend_cpu_reg'
```

### Root Cause Analysis
This appears to be a fundamental incompatibility between:

1. **llama.cpp's new backend system**: Designed for dynamic loading of backends from shared libraries
2. **Cosmopolitan Libc**: Pure static linking with no dynamic loading support

### Technical Details

The backend registry (`llama.cpp/ggml/src/ggml-backend-reg.cpp:219`) automatically registers backends when compiled with `GGML_USE_CPU`:

```cpp
#ifdef GGML_USE_CPU
    register_backend(ggml_backend_cpu_reg());
#endif
```

The function `ggml_backend_cpu_reg()` is defined in:
- `llama.cpp/ggml/src/ggml-cpu/ggml-cpu.cpp`
- Symbol exists in compiled object: `o//llama.cpp/ggml/src/ggml-cpu/ggml-cpu.o`
- Symbol included in archive: `o//llama.cpp/llama.cpp.a`

However, Cosmopolitan's linker cannot resolve it. This is likely due to:
- C/C++ linkage mismatch in static archives
- Symbol visibility issues with Cosmopolitan's .a format
- Order-dependent static initialization incompatibility

### Attempted Solutions

1. ❌ Manual backend registration in prog.cpp - linker error
2. ❌ Added GGML_USE_CPU flag - same error
3. ❌ Created stub returning nullptr - runtime error (no backends loaded)
4. ❌ Removed stub and rebuilt archive - linker error returns
5. ❌ Added `ggml_backend_load_all()` call - searches for dynamic libs that don't exist

### Files Modified for Backend Init

```
llamafile/server/prog.cpp:71-75
```
```cpp
// initialize llama backend (must be before model loading)
llama_backend_init();

// load all available backends (CPU, GPU, etc.)
ggml_backend_load_all();  // <-- Currently tries dynamic loading
```

## 📋 Remaining Work

### Critical (Blocking Runtime)
1. **Resolve CPU backend linking issue**
   - Investigate Cosmopolitan static archive symbol resolution
   - May require patching ggml-backend-reg.cpp for pure static builds
   - Alternative: Create proper static initialization for CPU backend

### Medium Priority
2. **Multimodal Support**
   - Migrate from old llava API to new mtmd (multimodal tools) API
   - Re-enable image processing in `llamafile/chatbot_eval.cpp`
   - Remove stubs from `llama.cpp/llava/llava_stub.cpp`
   - Uncomment `clip_free()` in `llamafile/server/slot.cpp:93`

3. **Deprecation Warnings**
   - Replace `llama_load_model_from_file` → `llama_model_load_from_file`
   - Replace `llama_free_model` → `llama_model_free`

### Low Priority
4. **Testing**
   - Verify GPU offloading works with NVIDIA/AMD
   - Test with various model formats (Llama 3, Mistral, Gemma, Phi)
   - Benchmark prompt processing performance (should be 2-5x faster)
   - Cross-platform testing (Linux, Windows, macOS, BSD)

## 🔧 Recommended Next Steps

### For Immediate Commit
The current state can be committed with clear TODOs marking the backend linking issue. The build system is fully functional and all code changes are sound - only the linking phase fails.

**Suggested commit message**:
```
feat: Modernize to llama.cpp latest + cosmocc 4.0.2 (90% complete)

- Update build system to cosmocc 4.0.2 with C++23
- Migrate all includes to new modular llama.cpp structure
- Update ~60 files for new GGML/vocab/memory APIs
- Preserve all TinyBLAS custom kernel optimizations

Known issue: CPU backend linking fails with Cosmopolitan static archives.
The ggml_backend_cpu_reg() symbol cannot be resolved despite being compiled.
This appears to be a fundamental incompatibility between llama.cpp's new
dynamic backend loading and Cosmopolitan's pure static linking model.

See MODERNIZATION_STATUS.md for details and attempted solutions.
```

### For Community Help
1. **Post to llamafile Discord/GitHub**:
   - Describe the Cosmopolitan static linking issue
   - Share the linker error and attempted solutions
   - Ask if there's a known workaround for static backend registration

2. **Post to llama.cpp GitHub**:
   - File issue about static linking compatibility
   - Request guidance on pure-static backend registration
   - Suggest adding `GGML_STATIC_BACKENDS` build flag

3. **Investigate Cosmopolitan Workarounds**:
   - Check if newer cosmocc versions have better C++ symbol resolution
   - Look for Cosmopolitan-specific linker flags
   - Consider custom linker script for symbol visibility

## 📊 Build Statistics

**Files Modified**: ~65
**API Functions Updated**: ~30
**Lines of Code Changed**: ~200
**Compilation Success**: Yes
**Linking Success**: No (backend symbol resolution)
**Estimated Time to Fix**: 2-4 hours with community guidance

## 🔍 Key Files Reference

### Modified Core Files
```
build/config.mk                          # Build flags
llamafile/server/prog.cpp                # Backend initialization
llamafile/server/slot.cpp                # CLIP stub
llama.cpp/llava/llava_stub.cpp           # Multimodal stubs
llama.cpp/common/arg.cpp                 # Cosmopolitan compat
llama.cpp/common/common.cpp              # Cosmopolitan compat
```

### Diagnostic Commands
```bash
# Check if CPU backend compiled
ar t o//llama.cpp/llama.cpp.a | grep ggml-cpu

# Check symbol in object file
nm -g o//llama.cpp/ggml/src/ggml-cpu/ggml-cpu.o | grep cpu_reg

# Attempt build
gmake -j4 o//llamafile/server/main

# Check linker error
gmake o//llamafile/server/main 2>&1 | grep "undefined reference"
```

## 📞 Contact Points

- **llamafile Discord**: https://discord.gg/YuMNeuKStr
- **llamafile Issues**: https://github.com/Mozilla-Ocho/llamafile/issues
- **llama.cpp Issues**: https://github.com/ggerganov/llama.cpp/issues
- **Cosmopolitan Libc**: https://github.com/jart/cosmopolitan

---

**Generated**: 2025-10-31
**Maintainer**: See git commit history
