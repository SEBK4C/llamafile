# CPU Backend Static Linking Issue

## Executive Summary

The CPU backend fails to link in static builds because the critical `ggml_backend_cpu_reg()` function is never compiled. This occurs due to a Makefile rule conflict where both `ggml-cpu.c` and `ggml-cpu.cpp` exist with the same base name, and Make's implicit rules prefer the `.c` file over the `.cpp` file.

**Status**: 🔴 **BLOCKING** - Application cannot start without CPU backend registration.

**Impact**: Complete failure to initialize the CPU backend, preventing model loading and inference.

---

## Problem Statement

### Linker Error
```
undefined reference to `ggml_backend_cpu_reg'
```

### Symptoms
- Build completes successfully, but linking fails
- The `ggml_backend_cpu_reg()` symbol is missing from the final binary
- Application cannot initialize CPU backend at runtime
- Models cannot be loaded (no available backends)

---

## Root Cause Analysis

### The Core Issue

Both `ggml-cpu.c` and `ggml-cpu.cpp` exist in the same directory:
- `llama.cpp/ggml/src/ggml-cpu/ggml-cpu.c` (3,588 lines)
- `llama.cpp/ggml/src/ggml-cpu/ggml-cpu.cpp` (682 lines)

Both files would produce the same object file: `o/$(MODE)/llama.cpp/ggml/src/ggml-cpu/ggml-cpu.o`

### Why This Causes Problems

1. **Make's Implicit Rule Precedence**
   - GNU Make has pattern rules in `build/rules.mk`:
     ```makefile
     o/$(MODE)/%.o: %.c $(COSMOCC)
     o/$(MODE)/%.o: %.cpp $(COSMOCC)
     ```
   - When Make sees a target `ggml-cpu.o`, it matches **both** patterns
   - GNU Make tries `.c` files **before** `.cpp` files in implicit rule resolution
   - Result: The `.c` file is compiled, producing `ggml-cpu.o`

2. **Source Exclusion Doesn't Work**
   - `BUILD.mk` attempts to exclude `ggml-cpu.c`:
     ```makefile
     LLAMA_CPP_GGML_SRCS_C := $(wildcard llama.cpp/ggml/src/*.c) \
         $(filter-out llama.cpp/ggml/src/ggml-cpu/ggml-cpu.c, $(wildcard llama.cpp/ggml/src/ggml-cpu/*.c))
     ```
   - However, Make's **implicit rules** still find and compile the `.c` file independently
   - The exclusion only affects explicit source lists, not pattern-matched rules

3. **Missing Function**
   - `ggml_backend_cpu_reg()` is **only** defined in `ggml-cpu.cpp` (line 669-680)
   - The `.c` file ends at line 3588 and contains only `ggml_cpu_init()`
   - When the `.c` file is compiled instead, the registration function is never compiled

### Verification

**Check which file was actually compiled:**
```bash
nm o/llama.cpp/ggml/src/ggml-cpu/ggml-cpu.o | grep -E "(ggml_backend_cpu_reg|ggml_cpu_init)"
```

**Current state:**
- ✅ `ggml_cpu_init` is present (from `.c` file)
- ❌ `ggml_backend_cpu_reg` is **missing** (should be from `.cpp` file)

**Check build log:**
```bash
grep "ggml-cpu\.\(c\|cpp\)" build.log | tail -1
```

**Expected (what we need):**
```
cosmocc ... -c -o o/.../ggml-cpu.o llama.cpp/ggml/src/ggml-cpu/ggml-cpu.cpp
```

**Actual (what's happening):**
```
cosmocc ... -c -o o/.../ggml-cpu.o llama.cpp/ggml/src/ggml-cpu/ggml-cpu.c
```

---

## Technical Details

### Function Location

**`ggml_backend_cpu_reg()`** is defined in:
- **File**: `llama.cpp/ggml/src/ggml-cpu/ggml-cpu.cpp`
- **Lines**: 669-680
- **Purpose**: Returns a pointer to the CPU backend registration structure
- **Usage**: Called by `ggml-backend-reg.cpp:219` during static initialization

```cpp
ggml_backend_reg_t ggml_backend_cpu_reg(void) {
    // init CPU feature detection
    ggml_cpu_init();

    static struct ggml_backend_reg ggml_backend_cpu_reg = {
        /* .api_version = */ GGML_BACKEND_API_VERSION,
        /* .iface       = */ ggml_backend_cpu_reg_i,
        /* .context     = */ NULL,
    };

    return &ggml_backend_cpu_reg;
}

GGML_BACKEND_DL_IMPL(ggml_backend_cpu_reg)
```

### Backend Registration Flow

1. **Static Initialization** (`ggml-backend-reg.cpp:184-221`):
   ```cpp
   ggml_backend_registry() {
       #ifdef GGML_USE_CPU
           register_backend(ggml_backend_cpu_reg());  // <-- Calls missing function
       #endif
   }
   ```

2. **Runtime Requirement**:
   - `llama_backend_init()` creates a static `ggml_backend_registry` instance
   - Constructor calls `ggml_backend_cpu_reg()` to register the CPU backend
   - If function is missing → linker error at build time
   - If function exists but isn't called → no backends available at runtime

### Why Both Files Exist

In llama.cpp's CMake build system (`ggml/src/ggml-cpu/CMakeLists.txt:25-26`):
- Both files are explicitly included in the build
- CMake can handle multiple source files with different extensions
- The `.c` file contains CPU operation implementations
- The `.cpp` file contains C++ backend registration interface

For static linking (Make-based builds), we need **both**:
- The CPU operations from `.c` file
- The backend registration from `.cpp` file

**However**, the current Makefile can only build one object file per base name.

---

## Current State

### Build Configuration

**File**: `llama.cpp/BUILD.mk`

**Current approach** (lines 7-9):
```makefile
# Note: Exclude ggml-cpu/ggml-cpu.c because ggml-cpu/ggml-cpu.cpp provides the backend registration
LLAMA_CPP_GGML_SRCS_C := $(wildcard llama.cpp/ggml/src/*.c) \
    $(filter-out llama.cpp/ggml/src/ggml-cpu/ggml-cpu.c, $(wildcard llama.cpp/ggml/src/ggml-cpu/*.c))
```

**Problem**: This exclusion doesn't prevent Make's implicit rules from compiling the `.c` file.

### Object File Status

**Location**: `o/llama.cpp/ggml/src/ggml-cpu/ggml-cpu.o`

**Contents**:
- ✅ Compiled from `ggml-cpu.c` (verified via build log)
- ✅ Contains `ggml_cpu_init()` symbol
- ❌ Missing `ggml_backend_cpu_reg()` symbol

**Size**: ~246 KB (consistent with `.c` file compilation)

### Archive Status

**Location**: `o/$(MODE)/llama.cpp/llama.cpp.a`

**Contents**:
- Contains `ggml-cpu.o` (from `.c` file)
- Missing `ggml_backend_cpu_reg` symbol
- Linker cannot resolve reference from `ggml-backend-reg.cpp`

---

## Solution Approaches

### Option 1: Explicit Rule Override (RECOMMENDED)

Create an explicit rule that forces Make to use the `.cpp` file:

**In `llama.cpp/BUILD.mk`**, add after line 83:

```makefile
# Force ggml-cpu.cpp to be compiled instead of ggml-cpu.c
# This overrides Make's implicit rule preference for .c files
o/$(MODE)/llama.cpp/ggml/src/ggml-cpu/ggml-cpu.o: llama.cpp/ggml/src/ggml-cpu/ggml-cpu.cpp $(COSMOCC)
	@mkdir -p $(@D)
	$(COMPILE.cc) -o $@ $<
```

**Pros**:
- ✅ Simple, explicit solution
- ✅ Overrides implicit rules
- ✅ Preserves existing structure
- ✅ Minimal changes

**Cons**:
- ⚠️ We lose the CPU operations from `.c` file (unless merged)

**Status**: ⚠️ **PARTIAL** - Compiles `.cpp` but loses `.c` functionality

---

### Option 2: Rename and Build Both Files

Rename one file and compile both:

**Step 1**: Rename `ggml-cpu.c` → `ggml-cpu-impl.c`

**Step 2**: Update `BUILD.mk`:
```makefile
# Include both files with different names
LLAMA_CPP_GGML_SRCS_C := $(wildcard llama.cpp/ggml/src/*.c) \
    $(wildcard llama.cpp/ggml/src/ggml-cpu/*.c) \
    llama.cpp/ggml/src/ggml-cpu/ggml-cpu-impl.c

LLAMA_CPP_GGML_SRCS_CPP := $(wildcard llama.cpp/ggml/src/*.cpp) \
    $(wildcard llama.cpp/ggml/src/ggml-cpu/*.cpp) \
    ...
```

**Pros**:
- ✅ Both files compiled
- ✅ All functionality preserved
- ✅ No conflicts

**Cons**:
- ⚠️ Requires renaming upstream file (or patching)
- ⚠️ May need to update includes/references

**Status**: ✅ **COMPLETE** - But requires upstream changes

---

### Option 3: Merge Registration into .c File

Move `ggml_backend_cpu_reg()` into the `.c` file with `extern "C"`:

**In `ggml-cpu.c`**, add at the end:
```c
#ifdef __cplusplus
extern "C" {
#endif

ggml_backend_reg_t ggml_backend_cpu_reg(void) {
    ggml_cpu_init();
    // ... implementation ...
}

#ifdef __cplusplus
}
#endif
```

**Pros**:
- ✅ Single source file
- ✅ No build conflicts
- ✅ All functionality in one place

**Cons**:
- ⚠️ Requires C++ compilation of `.c` file (may need rename to `.cpp`)
- ⚠️ Mixes C and C++ code
- ⚠️ Requires understanding of both files' dependencies

**Status**: ⚠️ **COMPLEX** - Requires careful refactoring

---

### Option 4: Separate Object Files with Different Names

Compile both files to differently-named objects, then link:

**In `BUILD.mk`**, add explicit rules:
```makefile
# Compile .c file to ggml-cpu-impl.o
o/$(MODE)/llama.cpp/ggml/src/ggml-cpu/ggml-cpu-impl.o: llama.cpp/ggml/src/ggml-cpu/ggml-cpu.c $(COSMOCC)
	@mkdir -p $(@D)
	$(COMPILE.c) -o $@ $<

# Compile .cpp file to ggml-cpu.o (for registration)
o/$(MODE)/llama.cpp/ggml/src/ggml-cpu/ggml-cpu.o: llama.cpp/ggml/src/ggml-cpu/ggml-cpu.cpp $(COSMOCC)
	@mkdir -p $(@D)
	$(COMPILE.cc) -o $@ $<

# Add impl object to archive
LLAMA_CPP_GGML_SRCS_OBJS += o/$(MODE)/llama.cpp/ggml/src/ggml-cpu/ggml-cpu-impl.o
```

**Pros**:
- ✅ Both files compiled separately
- ✅ No naming conflicts
- ✅ All functionality preserved
- ✅ No upstream changes needed

**Cons**:
- ⚠️ More complex build rules
- ⚠️ Need to ensure both objects are included in archive

**Status**: ✅ **RECOMMENDED** - Best balance of completeness and feasibility

---

## Recommended Solution: Option 4

### Implementation Steps

1. **Add explicit rules to `llama.cpp/BUILD.mk`** (after line 83):

```makefile
# Handle ggml-cpu dual-source conflict:
# - ggml-cpu.c contains CPU operation implementations
# - ggml-cpu.cpp contains backend registration (ggml_backend_cpu_reg)
# Both must be compiled, but Make's implicit rules prefer .c over .cpp
# Solution: Compile .c to ggml-cpu-impl.o and .cpp to ggml-cpu.o

# Compile .c file to separate object (CPU operations)
o/$(MODE)/llama.cpp/ggml/src/ggml-cpu/ggml-cpu-impl.o: llama.cpp/ggml/src/ggml-cpu/ggml-cpu.c $(COSMOCC)
	@mkdir -p $(@D)
	$(COMPILE.c) -o $@ $<

# Force .cpp file compilation (backend registration)
o/$(MODE)/llama.cpp/ggml/src/ggml-cpu/ggml-cpu.o: llama.cpp/ggml/src/ggml-cpu/ggml-cpu.cpp $(COSMOCC)
	@mkdir -p $(@D)
	$(COMPILE.cc) -o $@ $<

# Include the impl object in the archive
LLAMA_CPP_SRCS_OBJS += o/$(MODE)/llama.cpp/ggml/src/ggml-cpu/ggml-cpu-impl.o
```

2. **Update source exclusion** (line 8-9) - remove the filter-out since we're handling it explicitly:

```makefile
LLAMA_CPP_GGML_SRCS_C := $(wildcard llama.cpp/ggml/src/*.c) \
    $(filter-out llama.cpp/ggml/src/ggml-cpu/ggml-cpu.c, $(wildcard llama.cpp/ggml/src/ggml-cpu/*.c))
```

Keep the exclusion to prevent double-compilation via implicit rules.

3. **Verify build**:
```bash
make clean
make -j$(nproc) 2>&1 | tee build.log
```

4. **Verify symbols**:
```bash
# Check that both objects exist
ls -lh o/llama.cpp/ggml/src/ggml-cpu/ggml-cpu*.o

# Verify ggml_backend_cpu_reg exists
nm o/llama.cpp/ggml/src/ggml-cpu/ggml-cpu.o | grep ggml_backend_cpu_reg

# Verify ggml_cpu_init exists
nm o/llama.cpp/ggml/src/ggml-cpu/ggml-cpu-impl.o | grep ggml_cpu_init

# Check archive contains both
nm o/llama.cpp/llama.cpp.a | grep -E "(ggml_backend_cpu_reg|ggml_cpu_init)"
```

5. **Test linking**:
```bash
make llamafile
# Should complete without linker errors
```

---

## Testing & Verification

### Build Verification

```bash
# Clean build
make clean

# Build with verbose output
make -j$(nproc) 2>&1 | tee build.log

# Check which file was compiled for ggml-cpu.o
grep "ggml-cpu\.\(c\|cpp\)" build.log | grep "ggml-cpu\.o"

# Should show:
# - ggml-cpu.cpp → ggml-cpu.o
# - ggml-cpu.c → ggml-cpu-impl.o
```

### Symbol Verification

```bash
# Check object files
nm o/llama.cpp/ggml/src/ggml-cpu/ggml-cpu.o | grep ggml_backend_cpu_reg
# Expected: T ggml_backend_cpu_reg (or similar)

nm o/llama.cpp/ggml/src/ggml-cpu/ggml-cpu-impl.o | grep ggml_cpu_init
# Expected: T ggml_cpu_init

# Check archive
nm o/llama.cpp/llama.cpp.a | grep -E "(ggml_backend_cpu_reg|ggml_cpu_init)"
# Expected: Both symbols present
```

### Runtime Verification

```bash
# Build llamafile
make llamafile

# Run with verbose backend output (if available)
./llamafile --help
# Should not show "no backends available" errors

# Test model loading
./llamafile -m models/TinyLLama-v0.1-5M-F16.gguf -p "Hello"
# Should successfully load and run
```

---

## Related Issues

### CMake vs Make Differences

**CMake** (`ggml/src/ggml-cpu/CMakeLists.txt`):
- Explicitly lists both files in `GGML_CPU_SOURCES`
- Compiles each file independently
- No naming conflicts

**Make** (current `BUILD.mk`):
- Uses pattern rules (`%.o: %.c` and `%.o: %.cpp`)
- Implicit rule resolution causes conflicts
- Requires explicit handling

### Why This Only Affects Static Builds

- **Dynamic builds**: Backends loaded from `.so` files at runtime
- **Static builds**: All symbols must be resolved at link time
- **Cosmopolitan**: Pure static linking, no dynamic loading

The `GGML_BACKEND_DL_IMPL` macro in `ggml-cpu.cpp:682` is for dynamic loading scenarios. In static builds, we rely on direct function calls.

---

## Next Steps

### Immediate Actions

1. ✅ **Document the issue** (this file)
2. ⏳ **Implement Option 4** (explicit rules for both files)
3. ⏳ **Test build and linking**
4. ⏳ **Verify runtime behavior**
5. ⏳ **Update MODERNIZATION_STATUS.md** when resolved

### Future Considerations

1. **Upstream Alignment**: Consider proposing changes to llama.cpp to better support static builds
2. **Build System**: Evaluate if similar issues exist for other backends (CUDA, Metal, etc.)
3. **Documentation**: Update build documentation with static linking requirements

---

## References

- **Backend Registration**: `llama.cpp/ggml/src/ggml-backend-reg.cpp:219`
- **CPU Backend Implementation**: `llama.cpp/ggml/src/ggml-cpu/ggml-cpu.cpp:669-682`
- **Build Rules**: `build/rules.mk:8-26`
- **LLama.cpp Build Config**: `llama.cpp/BUILD.mk:7-14`
- **CMake Equivalent**: `llama.cpp/ggml/src/ggml-cpu/CMakeLists.txt:25-26`

---

## Change Log

| Date | Action | Details |
|------|--------|---------|
| 2024-10-31 | Issue identified | Build compiles `.c` instead of `.cpp` |
| 2024-10-31 | Documentation created | This file |
| TBD | Solution implemented | Option 4 (explicit rules) |
| TBD | Issue resolved | Linking succeeds, runtime verified |

---

## Appendix: Make Implicit Rule Resolution

GNU Make's rule resolution order (when multiple patterns match):

1. **Explicit rules** (highest priority)
2. **Pattern rules** (tried in order of definition)
3. **Built-in rules** (for common file types)

For `%.o: %.c` vs `%.o: %.cpp`:
- Both are pattern rules
- When a target `x.o` is needed and both `x.c` and `x.cpp` exist:
  - Make tries rules in definition order
  - If both patterns are defined, Make prefers the **first matching source file** it finds
  - Source file search order is typically alphabetical or filesystem order
  - `.c` files often come before `.cpp` in search results

**Solution**: Explicit rules override pattern rules, so defining an explicit rule for the `.cpp` file ensures it's used.

