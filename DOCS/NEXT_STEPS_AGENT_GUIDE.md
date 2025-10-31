# Llamafile Modernization - Next Steps & Agent Guide

**Date:** October 31, 2025  
**Status:** 85% Complete - Core Integration Done, API Wrappers Need Updates  
**Target Audience:** AI Coding Agents (Claude, GPT-4, etc.)

---

## 🎯 Mission Overview

Complete the llamafile modernization by updating custom wrapper code to match the new llama.cpp API. The **hard architectural work is done** - cosmocc 4.0.2 is integrated, build system is modernized, and core llama.cpp compiles successfully.

**Remaining Work:** Update llamafile's chatbot and server wrapper code to use new llama.cpp API conventions.

**Estimated Time:** 4-8 agent hours across multiple focused tasks

---

## 📋 Task Breakdown for Agents

### AGENT TASK 1: Fix chatbot_main.cpp API Migration
**Priority:** CRITICAL  
**Complexity:** HIGH  
**Estimated Time:** 2-3 hours  
**File:** `llamafile/chatbot_main.cpp`

#### Context
This file initializes the chatbot system and integrates with the server. It has ~20 API incompatibilities with the new llama.cpp.

#### Specific Issues to Fix

##### 1.1 Missing Helper Functions
```cpp
// Line 52: Error - use of undeclared identifier 'llama_n_gpu_layers'
// RESEARCH NEEDED: Find new equivalent or remove
// Old: llama_n_gpu_layers(...)
// New: Check llama.cpp/include/llama.h for model metadata functions
```

**Agent Instructions:**
- Search `llama.cpp/include/llama.h` for functions related to GPU layers
- Likely renamed to `llama_model_*` or moved to context params
- Update line 52 accordingly

##### 1.2 llamafile Helper Functions
```cpp
// Lines 44, 52-53, 116, 151: Missing llamafile utility functions
// ERROR: 'llamafile_has_gpu', 'llamafile_has_metal', 'llamafile_has'
```

**Agent Instructions:**
- These are defined in `llamafile/llamafile.h`
- Add missing include: `#include "llamafile/llamafile.h"`
- Verify FLAG_* variables are accessible

##### 1.3 Server Integration (Currently Commented Out)
```cpp
// Line 29: #include "llama.cpp/tools/server/server.h" // COMMENTED OUT
// Lines 85-89: Server variables undefined
// - server_log_json
// - g_server_background_mode  
// - g_server_force_llama_model
// - g_server_on_listening
// - server_cli
```

**Agent Instructions:**
- **Option A (Simple):** Keep server integration disabled
  - Wrap server-related code in `#ifdef ENABLE_SERVER ... #endif`
  - Focus on CLI chatbot functionality first
  
- **Option B (Complete):** Integrate new server API
  - Read `llama.cpp/tools/server/server.cpp` to understand new API
  - Check if server initialization has changed
  - May require substantial refactoring
  - **Recommend Option A for now**

##### 1.4 Params Parsing
```cpp
// Line 127: Error - use of undeclared identifier 'gpt_params_parse'
// Old: gpt_params_parse(argc, argv, g_params)
// New: common_params_parse(?) or similar
```

**Agent Instructions:**
- Search `llama.cpp/common/common.h` for: `params_parse|parse_params|common_.*_parse`
- Likely `common_params_parse` or `gpt_params_parse` still exists
- Update function call with correct signature

##### 1.5 Model Initialization
```cpp
// Line 139: Error - 'llama_model_params_from_gpt_params' undeclared
// Line 140: Error - no member 'c_str' in 'common_params_model'
```

**Agent Instructions:**
1. Search for `llama_model_params_from_*` in `llama.cpp/common/common.h`
2. Check `common_params` struct definition for `.model` member type
3. The `.model` field might now be a string directly, not a struct
4. Update accordingly:
```cpp
// OLD (probably):
auto mparams = llama_model_params_from_gpt_params(g_params);
g_model = llama_load_model_from_file(g_params.model.c_str(), mparams);

// NEW (research needed):
auto mparams = llama_model_params_default(); // or from common_params
g_model = llama_load_model_from_file(g_params.model, mparams);
```

##### 1.6 Sampling API Migration
```cpp
// Line 121: Error - no member 'sparams' in 'common_params'
// Line 146: Error - unknown type 'llama_sampling_context'
// Line 213: Error - 'llama_sampling_sample' → 'llama_sampler_sample'
// Line 214: Error - 'llama_sampling_accept' undeclared
// Line 233: Error - 'llama_sampling_free' → 'llama_sampler_free'
```

**Agent Instructions:**
1. Read `llama.cpp/common/sampling.h` for new sampler API
2. Likely changes:
```cpp
// OLD:
llama_sampling_context *sampler = llama_sampling_init(g_params.sparams);
llama_token token = llama_sampling_sample(sampler, g_ctx, -1);
llama_sampling_accept(sampler, g_ctx, token, true);
llama_sampling_free(sampler);

// NEW (research exact signatures):
llama_sampler *sampler = llama_sampler_chain_init(...);
// Add samplers to chain
llama_token token = llama_sampler_sample(sampler, g_ctx, -1);
// Accept may have changed or be removed
llama_sampler_free(sampler);
```

3. Check `llama.cpp/examples/main/main.cpp` or `llama.cpp/tools/main/main.cpp` for reference implementation

##### 1.7 BOS Token Function
```cpp
// Line 101: Error - 'llama_should_add_bos_token' → 'llama_add_bos_token'
// Line 103: Error - cannot initialize llama_vocab* with llama_model*
```

**Agent Instructions:**
```cpp
// OLD:
if (llama_should_add_bos_token(g_model))
    tokens.push_back(llama_token_bos(g_model));

// NEW:
const struct llama_vocab *vocab = llama_model_get_vocab(g_model);
if (llama_add_bos_token(vocab))  // Check signature
    tokens.push_back(llama_vocab_bos(vocab));
```

##### 1.8 Token EOG Checking
```cpp
// Line 217: Error - no matching function 'llama_token_is_eog'
```

**Agent Instructions:**
```cpp
// OLD:
if (llama_token_is_eog(g_model, token))

// NEW:
const struct llama_vocab *vocab = llama_model_get_vocab(g_model);
if (llama_vocab_is_eog(vocab, token))
```

#### Validation Steps
- [ ] File compiles without errors
- [ ] Can build: `cd /path && gmake -j4`
- [ ] Verify chatbot can initialize (runtime test)

---

### AGENT TASK 2: Fix compute.cpp Include
**Priority:** HIGH  
**Complexity:** TRIVIAL  
**Estimated Time:** 5 minutes  
**File:** `llamafile/compute.cpp`

#### Issue
```
fatal error: 'llama.cpp/string.h' file not found
```

#### Agent Instructions
1. Open `llamafile/compute.cpp`
2. Find line with `#include "llama.cpp/string.h"`
3. Determine what this should be:
   - If it's the llamafile string utilities: `#include "llamafile/string.h"`
   - If it's C++ standard library: `#include <string>`
   - If it's llama.cpp common: Check what exists in `llama.cpp/common/`
4. Update the include
5. Verify file compiles

---

### AGENT TASK 3: Complete Build and Fix Cascading Errors
**Priority:** HIGH  
**Complexity:** MEDIUM  
**Estimated Time:** 1-2 hours  

#### Agent Instructions
1. After Task 1 & 2, attempt full build:
```bash
cd /Users/seb/.cursor/worktrees/llamafile/E19A7
gmake clean
gmake -j4 2>&1 | tee build.log
```

2. For each remaining error:
   - **Undefined identifiers:** Search for new name in llama.cpp
   - **Missing includes:** Add appropriate header
   - **Type mismatches:** Check if struct renamed or member changed
   - **Function signature changes:** Check llama.cpp/include/llama.h

3. Common patterns to watch for:
   - `gpt_*` → `common_*`
   - `llama_token_*` → `llama_vocab_*`
   - `llama_kv_cache_*` → `llama_memory_*`
   - `llama_sampling_*` → `llama_sampler_*`

4. Document any API changes you discover

#### Expected Issues
- **Server components:** May need API updates similar to chatbot
- **Tool executables:** Should compile once core library builds
- **Whisper.cpp, stable-diffusion.cpp:** Likely need similar include updates

---

### AGENT TASK 4: Server API Migration
**Priority:** MEDIUM  
**Complexity:** HIGH  
**Estimated Time:** 2-3 hours  
**Files:** `llamafile/server/*.cpp` (20+ files)

#### Context
The server components wrap llama.cpp's server API and provide llamafile-specific functionality.

#### Approach

**Phase 1: Identify Issues**
```bash
cd /Users/seb/.cursor/worktrees/llamafile/E19A7
gmake o//llamafile/server/main 2>&1 | grep "error:" > server_errors.txt
```

**Phase 2: Systematic Fixes**
For each error:
1. Check if it's the same pattern as chatbot fixes
2. Apply same migration strategy
3. Document any new patterns

**Phase 3: Server-Specific APIs**
- Check `llama.cpp/tools/server/server.cpp` for reference
- Server may have its own API changes
- May need to disable advanced features temporarily

#### Known Potential Issues
```cpp
// These may need updates:
- llama_chat_msg → llama_chat_message (already done)
- Sampling API (same as chatbot)
- Token handling (same as chatbot)
- KV cache → Memory API (same as chatbot)
```

---

### AGENT TASK 5: Multimodal (MTMD) Migration
**Priority:** MEDIUM  
**Complexity:** VERY HIGH  
**Estimated Time:** 4-6 hours  
**Files:** `llamafile/chatbot_eval.cpp` (image functions)

#### Context
The old llava API (`llava_image_embed_*`) has been completely replaced with mtmd (multimodal). This is currently stubbed out.

#### Research Phase

**Step 1: Understand Old API**
Current stubbed code in `chatbot_eval.cpp`:
```cpp
bool eval_image(const std::string_view binary) {
    // OLD CODE (removed):
    // llava_image_embed *image_embed;
    // image_embed = llava_image_embed_make_with_bytes(
    //     g_clip, FLAG_threads_batch, binary.data(), binary.size());
    // bool ok = eval_image_embed(image_embed);
    // llava_image_embed_free(image_embed);
}
```

**Step 2: Understand New API**
Read these files:
- `llama.cpp/tools/mtmd/mtmd.h` - Main multimodal API
- `llama.cpp/tools/mtmd/mtmd.cpp` - Implementation
- `llama.cpp/tools/mtmd/clip.h` - Image processing
- `llama.cpp/tools/mtmd/mtmd-cli.cpp` - Example usage

**Step 3: Map Old → New**

| Old llava API | New mtmd API | Notes |
|---------------|--------------|-------|
| `llava_image_embed` | `mtmd_image_tokens` | Different structure |
| `llava_image_embed_make_with_bytes` | `mtmd_*` functions | Check mtmd.h |
| `llava_image_embed_free` | Smart pointers? | Check memory management |
| `g_clip` (clip_ctx) | `mtmd_context` | Larger context struct |

**Step 4: Implementation**
1. Update `g_clip` type to `mtmd_context*`
2. Update initialization in `chatbot_main.cpp`
3. Reimplement `eval_image()` using mtmd API
4. Test with a multimodal model (e.g., LLaVA)

**Step 5: Testing**
- Requires multimodal model (LLaVA, MiniCPM-V, etc.)
- Test image → text generation
- Verify embeddings are correct

**Recommendation:** Start this task AFTER basic text-only chatbot works

---

### AGENT TASK 6: Update CI Configuration
**Priority:** LOW  
**Complexity:** LOW  
**Estimated Time:** 30 minutes  
**File:** `.github/workflows/ci.yml`

#### Issue
CI caches cosmocc toolchain based on `config.mk` hash. Now that we've updated to 4.0.2, the cache key will change.

#### Agent Instructions
1. Open `.github/workflows/ci.yml`
2. Find the cosmocc cache step (around line 24-36)
3. The cache key uses `hashFiles('**/config.mk')`
4. This will automatically invalidate on first run (good!)
5. **Optional:** Add a comment noting the cosmocc version
6. Test CI by pushing changes

#### Current CI Config
```yaml
- name: Cache cosmocc toolchain
  uses: actions/cache@v4
  with:
    path: |
      .cosmocc
      o/depend
      o/depend.test
    key: ${{ runner.os }}-build-${{ env.cache-name }}-${{ hashFiles('**/config.mk') }}
```

**Expected Behavior:** First run will download cosmocc 4.0.2, subsequent runs will use cache.

---

### AGENT TASK 7: Test Basic Functionality
**Priority:** HIGH  
**Complexity:** MEDIUM  
**Estimated Time:** 1-2 hours  
**Prerequisites:** Tasks 1-3 complete (builds successfully)

#### Test Plan

##### 7.1 Build Verification
```bash
cd /Users/seb/.cursor/worktrees/llamafile/E19A7

# Clean build
gmake clean
gmake -j4

# Verify outputs
ls -lh o//llama.cpp/main/main
ls -lh o//llamafile/zipalign
```

**Expected:** All tools build without errors

##### 7.2 Create Test Llamafile
```bash
# Use the included TinyLLama model
cp ./models/TinyLLama-v0.1-5M-F16.gguf tinyllama.gguf

# Create args file
cat > .args << 'EOF'
-m
tinyllama.gguf
--host
127.0.0.1
-ngl
999
EOF

# Build llamafile
cp o//llama.cpp/main/main tinyllama.llamafile
o//llamafile/zipalign -j0 \
  tinyllama.llamafile \
  tinyllama.gguf \
  .args

# Make executable
chmod +x tinyllama.llamafile
```

##### 7.3 Test Execution
```bash
# Test 1: Basic prompt (CPU only)
./tinyllama.llamafile -e -p "Once upon a time" -n 50 -ngl 0

# Test 2: Help text
./tinyllama.llamafile --help

# Test 3: Model info
./tinyllama.llamafile -m tinyllama.gguf --version 2>&1 | head -20
```

**Expected Results:**
- ✅ Model loads
- ✅ Generates coherent text
- ✅ No crashes or segfaults
- ✅ GPU detection works (if applicable)

##### 7.4 Validation Checklist
- [ ] Model loads without errors
- [ ] Text generation works
- [ ] Performance seems reasonable
- [ ] No obvious crashes
- [ ] Can interrupt with Ctrl+C
- [ ] GPU detection (if on GPU-capable system)

---

### AGENT TASK 8: Performance Benchmarking
**Priority:** MEDIUM  
**Complexity:** MEDIUM  
**Estimated Time:** 1-2 hours  
**Prerequisites:** Task 7 complete

#### Objective
Verify that llamafile's performance characteristics are maintained per SPEC.txt requirements.

#### Benchmarks to Run

##### 8.1 Prompt Processing Speed
```bash
# Build llamafile with benchmark
cd /Users/seb/.cursor/worktrees/llamafile/E19A7
gmake o//llama.cpp/llama-bench/llama-bench

# Test prompt processing
./o//llama.cpp/llama-bench/llama-bench \
  -m models/TinyLLama-v0.1-5M-F16.gguf \
  -p 512 \
  -n 0 \
  -ngl 0

# Record tokens/second for prompt processing
```

##### 8.2 Token Generation Speed
```bash
# Test generation
./o//llama.cpp/llama-bench/llama-bench \
  -m models/TinyLLama-v0.1-5M-F16.gguf \
  -p 0 \
  -n 128 \
  -ngl 0

# Record tokens/second for generation
```

##### 8.3 Comparison Baseline
If you have the old llamafile build:
```bash
# Run same tests on old build
# Compare results
# Acceptable: Within 10% of previous performance
# Target: Same or better performance
```

##### 8.4 Success Criteria (from SPEC.txt)
- **No regressions:** Should be as fast or faster than old llamafile
- **Maintained optimizations:** Custom kernels should still be active
- **Platform detection:** Should auto-detect and use optimal kernels

#### What to Check
```bash
# Verify custom kernels are being used
# Look for "using" messages in output:
./tinyllama.llamafile -e -p "test" -n 1 2>&1 | grep -i "kernel\|blas\|optimization"

# Check CPU features detected
# Should show AVX, AVX2, FMA, etc. on capable systems
```

---

## 🔧 Reference Commands for Agents

### Build Commands
```bash
# Navigate to project
cd /Users/seb/.cursor/worktrees/llamafile/E19A7

# Clean build (required after config changes)
gmake clean

# Full build (use gmake, not make!)
gmake -j4

# Build specific target
gmake o//llama.cpp/main/main
gmake o//llamafile/server/main

# Verbose build (see full compile commands)
gmake V=1 -j1

# Check what would be built
gmake -n
```

### Debugging Compilation Errors
```bash
# Find all compilation errors
gmake -j4 2>&1 | grep "error:" > errors.txt

# Find specific error type
gmake -j4 2>&1 | grep "undefined reference"
gmake -j4 2>&1 | grep "fatal error"

# Compile single file with full output
gmake o//llamafile/chatbot_main.o V=1
```

### Search Commands
```bash
# Find function definitions
grep -r "^LLAMA_API.*function_name" llama.cpp/include/

# Find struct definitions
grep -r "struct.*struct_name" llama.cpp/

# Find all uses of old API
grep -r "old_function_name" llamafile/

# Check if header exists
find llama.cpp -name "header.h"
```

---

## 📚 Key Reference Files for Agents

### API References
- `llama.cpp/include/llama.h` - Main llama API
- `llama.cpp/include/llama-cpp.h` - C++ wrapper
- `llama.cpp/ggml/include/ggml.h` - GGML tensor operations
- `llama.cpp/ggml/include/ggml-backend.h` - Backend abstraction
- `llama.cpp/common/common.h` - Common utilities and params
- `llama.cpp/common/sampling.h` - Sampling API

### Implementation Examples
- `llama.cpp/tools/main/main.cpp` - CLI tool reference implementation
- `llama.cpp/tools/server/server.cpp` - Server reference implementation
- `llama.cpp/examples/simple/simple.cpp` - Minimal example
- `llama.cpp/tests/test-*.cpp` - Test files showing API usage

### Build System
- `build/config.mk` - Main configuration
- `build/rules.mk` - Build rules
- `Makefile` - Top-level makefile
- `*/BUILD.mk` - Per-component build files

### Documentation
- `SPEC.txt` - Modernization specification
- `MODERNIZATION_PROGRESS.md` - Detailed progress log
- `COMPLETION_STATUS.md` - Current status summary
- `llama.cpp/README.md` - Upstream documentation

---

## 🎓 Common API Migration Patterns

### Pattern 1: Token Functions Need Vocab
```cpp
// OLD:
llama_token tok = llama_token_bos(model);

// NEW:
const struct llama_vocab *vocab = llama_model_get_vocab(model);
llama_token tok = llama_vocab_bos(vocab);
```

### Pattern 2: KV Cache → Memory
```cpp
// OLD:
llama_kv_cache_seq_rm(ctx, seq_id, pos0, pos1);
llama_kv_cache_seq_add(ctx, seq_id, pos0, pos1, delta);

// NEW:
llama_memory_t mem = llama_get_memory(ctx);
llama_memory_seq_rm(mem, seq_id, pos0, pos1);
llama_memory_seq_add(mem, seq_id, pos0, pos1, delta);
```

### Pattern 3: Batch Creation Simplified
```cpp
// OLD:
llama_batch batch = llama_batch_get_one(tokens, n_tokens, pos, seq_id);

// NEW:
llama_batch batch = llama_batch_get_one(tokens, n_tokens);
// Position and seq_id are now auto-managed or set via batch fields
```

### Pattern 4: Sampling API Overhaul
```cpp
// OLD:
llama_sampling_context *ctx = llama_sampling_init(sparams);
llama_token tok = llama_sampling_sample(ctx, lctx, idx);
llama_sampling_accept(ctx, lctx, tok, apply_grammar);
llama_sampling_free(ctx);

// NEW: (Research exact API in common/sampling.h)
llama_sampler *sampler = llama_sampler_chain_init(...);
// Add samplers to chain
llama_token tok = llama_sampler_sample(sampler, lctx, idx);
llama_sampler_free(sampler);
```

### Pattern 5: Params Struct Rename
```cpp
// OLD:
gpt_params params;
params.model = "path/to/model";

// NEW:
common_params params;
params.model = "path/to/model";
// NOTE: Check if .model is now string directly or still a struct
```

---

## 🐛 Troubleshooting Guide for Agents

### Issue: "use of undeclared identifier"
**Solution:**
1. Search llama.cpp/include/ for the identifier
2. Check if renamed (common pattern: add prefix like `common_` or `llama_`)
3. Check if moved to different header
4. Check if removed (look for DEPRECATED markers)

### Issue: "incomplete type"
**Solution:**
1. Missing `#include` for struct definition
2. Add full include instead of forward declaration
3. Common fix: `#include "llama.cpp/common/common.h"`

### Issue: "no member named 'X'"
**Solution:**
1. Struct definition changed
2. Read the struct definition in llama.cpp
3. Update member access or use new member name
4. Example: `sparams` might now be `sampling_params` or `sampler`

### Issue: "cannot initialize parameter of type X with type Y"
**Solution:**
1. Function signature changed
2. Read function declaration in llama.h
3. Update call site with correct types
4. Common: `model*` → `vocab*`, `sampling_context*` → `sampler*`

### Issue: Build hangs or runs forever
**Solution:**
```bash
# Kill the build
Ctrl+C

# Single-threaded build for better error messages
gmake -j1

# Build single file to isolate issue
gmake o//path/to/file.o V=1
```

---

## 📖 Agent Workflow Template

For each assigned task:

### 1. Understand Current State
```bash
# Read the task description above
# Check referenced files
# Understand what's broken and why
```

### 2. Research the Solution
```bash
# Search llama.cpp for new API
grep -r "function_name" llama.cpp/include/
grep -r "struct.*Type" llama.cpp/

# Read reference implementations
cat llama.cpp/tools/main/main.cpp | grep -A 10 "function_name"

# Check common utilities
cat llama.cpp/common/common.h | grep -A 5 "helper"
```

### 3. Implement the Fix
```bash
# Edit file(s)
# Apply pattern from "Common API Migration Patterns"
# Update all related call sites
```

### 4. Verify the Fix
```bash
# Compile the file
gmake o//path/to/file.o

# If successful, try building dependent components
gmake o//component/

# Finally, attempt full build
gmake -j4
```

### 5. Document Changes
```bash
# Add comments explaining non-obvious changes
// Updated: llama_old_func → llama_new_func (API changed in llama.cpp)

# Update progress docs if major milestone
echo "✅ Completed Task X" >> COMPLETION_STATUS.md
```

---

## 🎯 Success Criteria

### Task Complete When:
- [ ] All compilation errors resolved
- [ ] Full build succeeds: `gmake -j4` exits with code 0
- [ ] All tools build: main, server, quantize, imatrix, perplexity, llama-bench
- [ ] Basic llamafile can be created
- [ ] Test llamafile runs and generates text
- [ ] No obvious performance regressions

### Quality Checklist:
- [ ] Code compiles cleanly (no errors)
- [ ] Warnings minimized (acceptable: deprecation warnings)
- [ ] API changes documented in code comments
- [ ] Backward compatibility stubs where needed
- [ ] TODOs added for future work (e.g., mtmd migration)

---

## 🔍 Debugging Tips

### When Stuck on an API Change
1. **Check if deprecated:** Look for DEPRECATED macro in llama.h
2. **Search examples:** `grep -r "function_name" llama.cpp/examples/`
3. **Check git history:** The function might have migration notes in commits
4. **Read tests:** `llama.cpp/tests/` often show correct usage
5. **Check common helpers:** Many utilities moved to `common/common.h`

### When Build Fails Mysteriously
1. **Clean build:** `gmake clean && gmake -j1` (single-threaded for clear errors)
2. **Check dependencies:** `gmake -n | grep "prerequisite"`
3. **Verify includes:** All BUILD.mk files present?
4. **Check syntax:** Sometimes it's just a missing semicolon
5. **Incremental build:** Build one component at a time

### When API Signature Unclear
1. **Read the header:** Truth is in `llama.cpp/include/llama.h`
2. **Check implementation:** `llama.cpp/src/llama.cpp` for behavior
3. **Find usage:** `grep -r "function" llama.cpp/tools/` for examples
4. **Test incrementally:** Build a minimal test case

---

## 📞 Key Information for Agents

### Build Environment
- **OS:** macOS (darwin 25.0.0)
- **Architecture:** ARM64 (Apple Silicon)
- **Compiler:** cosmocc 4.0.2 (GCC 14.1.0 based)
- **Make:** GNU Make 4.4.1 (`gmake` command)
- **C++ Standard:** C++23 (`-std=gnu++23`)

### Important Paths
- **Project Root:** `/Users/seb/.cursor/worktrees/llamafile/E19A7`
- **Cosmocc:** `.cosmocc/4.0.2/`
- **Build Output:** `o//` directory
- **llama.cpp:** `llama.cpp/` (integrated, not submodule)

### Key Variables
- **MODE:** Build mode (default: release)
- **PREFIX:** Install prefix (default: /usr/local)
- **COSMOCC:** Toolchain path

### Compiler Flags (for context)
```makefile
CXXFLAGS = -frtti -std=gnu++23
CCFLAGS = -O2 -g -fexceptions -ffunction-sections -fdata-sections -mclang
CPPFLAGS_ = -iquote. -mcosmo -DGGML_MULTIPLATFORM -Wno-attributes -DLLAMAFILE_DEBUG
TARGET_ARCH = -Xx86_64-mtune=znver4
```

---

## 🎬 Recommended Agent Execution Order

### Phase 1: Get It Building (Priority 1)
1. **AGENT TASK 2** (5 min) - Fix compute.cpp
2. **AGENT TASK 1** (2-3 hr) - Fix chatbot_main.cpp  
3. **AGENT TASK 3** (1-2 hr) - Complete build, fix cascading errors

**Goal:** Achieve `gmake -j4` success

### Phase 2: Verify It Works (Priority 2)
4. **AGENT TASK 7** (1-2 hr) - Test basic functionality
5. **AGENT TASK 8** (1-2 hr) - Performance benchmarking

**Goal:** Working llamafile that generates text

### Phase 3: Complete Features (Priority 3)
6. **AGENT TASK 4** (2-3 hr) - Server API migration
7. **AGENT TASK 6** (30 min) - Update CI

**Goal:** Full feature parity

### Phase 4: Advanced Features (Priority 4)
8. **AGENT TASK 5** (4-6 hr) - Multimodal (mtmd) migration

**Goal:** Image support restored

---

## 📊 Estimated Total Remaining Time

| Phase | Tasks | Time Estimate |
|-------|-------|---------------|
| Phase 1 (Critical) | Tasks 1-3 | 3-5 hours |
| Phase 2 (Validation) | Tasks 7-8 | 2-4 hours |
| Phase 3 (Complete) | Tasks 4, 6 | 2-4 hours |
| Phase 4 (Advanced) | Task 5 | 4-6 hours |
| **TOTAL** | **8 tasks** | **11-19 hours** |

**Parallelization Possible:** Tasks 4 and 6 can be done independently after Phase 1.

---

## 💬 Agent Communication Template

When reporting back to user:

```markdown
## Task [NUMBER]: [NAME] - [STATUS]

### Completed:
- ✅ [Specific fix 1]
- ✅ [Specific fix 2]

### Issues Encountered:
- 🔴 [Issue description]
  - Resolution: [What you did]

### Changes Made:
- Modified: [file1, file2, ...]
- Added: [new files if any]
- Removed: [deprecated code]

### Next Steps:
- [ ] [Remaining work for this task]

### Build Status:
- Compilation: [SUCCESS/ERRORS]
- If errors: [Number] remaining
- If success: [What now works]
```

---

## 🚨 Critical Gotchas

### 1. DON'T Use System Make
```bash
# WRONG:
make -j4

# RIGHT:
gmake -j4
```
macOS `make` is version 3.81 (too old). Must use `gmake` from Homebrew.

### 2. ALWAYS Clean After Config Changes
```bash
# After modifying BUILD.mk or config.mk:
gmake clean
gmake -j4
```

### 3. Include Order Matters
```cpp
// WRONG:
#include <string>
#include "llama.cpp/include/llama.h"  // May define conflicting macros

// BETTER:
#include "llama.cpp/include/llama.h"
#include <string>
```

### 4. Forward Declarations Insufficient
```cpp
// WRONG (causes "incomplete type" errors):
struct common_params;  // Forward declaration
void func(common_params &p) {
    p.member = value;  // ERROR! Type incomplete
}

// RIGHT:
#include "llama.cpp/common/common.h"  // Full definition
void func(common_params &p) {
    p.member = value;  // Works
}
```

### 5. Check for DEPRECATED Markers
```cpp
// In llama.h:
DEPRECATED(LLAMA_API llama_token llama_token_bos(...), "use llama_vocab_bos instead");

// This means the function exists but will be removed
// Use the new API now to avoid future breakage
```

---

## 📝 Code Style Guidelines

### Comments for API Changes
```cpp
// Good - Explains WHY:
// Updated: llama_kv_cache_seq_rm → llama_memory_seq_rm (API renamed in llama.cpp)
llama_memory_t mem = llama_get_memory(ctx);
llama_memory_seq_rm(mem, 0, pos, -1);

// Bad - No context:
llama_memory_seq_rm(mem, 0, pos, -1);
```

### TODOs for Future Work
```cpp
// Good - Actionable:
// TODO: Migrate to mtmd API - see llama.cpp/tools/mtmd/mtmd.h for new image processing

// Bad - Vague:
// TODO: Fix this
```

### Backward Compatibility Stubs
```cpp
// Good - Explains deprecation:
// Note: ggml_cuda_link was removed in llama.cpp
// Keeping stub for backward compatibility with existing code
void ggml_cuda_link(...) {
    // Function no longer needed in new API
}

// Bad - No explanation:
void ggml_cuda_link(...) {}
```

---

## 🎁 Bonus Tasks (If Time Permits)

### B1: Add Architecture Detection Logging
Enhance debugging by logging which kernels are selected:
```cpp
// In relevant kernel selection code:
tinylog("Using optimized AVX512 kernel for Zen 4\n");
```

### B2: Update Man Pages
If function signatures changed significantly:
- Update `llama.cpp/main/main.1`
- Update `llamafile/server/main.1`

### B3: Create Migration Guide
Document for users upgrading from old llamafile:
- What changed
- New flags (if any)
- Performance notes
- Breaking changes

### B4: Optimize Build Times
- Review BUILD.mk for unnecessary dependencies
- Consider ccache integration
- Document parallel build best practices

---

## 🏁 Final Checklist for Project Completion

### Compilation
- [ ] `gmake -j4` completes successfully
- [ ] All warnings reviewed (deprecations acceptable)
- [ ] No errors in build output
- [ ] All tools built: main, server, quantize, imatrix, perplexity, bench

### Functionality  
- [ ] Can create llamafile with zipalign
- [ ] Test llamafile runs
- [ ] Text generation works
- [ ] GPU detection works (on capable hardware)
- [ ] Server mode works (if implemented)
- [ ] Quantization tools work

### Performance (SPEC.txt Requirements)
- [ ] No performance regressions vs old llamafile
- [ ] Custom kernels still active
- [ ] SIMD optimizations detected and used
- [ ] Prompt processing speed maintained
- [ ] Token generation speed maintained

### Compatibility
- [ ] Loads modern model architectures (Llama 3, Mistral, Qwen, etc.)
- [ ] All quantization formats work (Q4_K_M, Q5_K_M, IQ variants, etc.)
- [ ] Cross-platform build possible (even if only tested on macOS)
- [ ] APE binary format maintained

### Documentation
- [ ] All progress documents updated
- [ ] Breaking changes documented
- [ ] Known issues listed
- [ ] Future work (mtmd) clearly marked

---

## 💾 State Snapshot for Next Agent

### What's Working
```bash
# Core library compiles:
✅ o//llama.cpp/llama.cpp.a

# Components compiling:
✅ GGML (ggml/src/*.c, ggml/src/*.cpp)
✅ Llama (src/*.cpp)
✅ Common (common/*.cpp)
✅ Llamafile core (most llamafile/*.c, llamafile/*.cpp)
✅ Highlight system
✅ Most chatbot files
```

### What's Blocked
```bash
# Needs API fixes:
❌ llamafile/chatbot_main.cpp (~20 errors)
❌ llamafile/compute.cpp (1 error - trivial)
❌ Possibly some server files
❌ Tools not attempted yet

# Stubbed for later:
⏸️ Image/multimodal (eval_image functions)
⏸️ Server integration in chatbot
```

### Build Command to Test
```bash
cd /Users/seb/.cursor/worktrees/llamafile/E19A7
gmake -j4 2>&1 | tee build.log
```

### Expected Next Error
After fixing chatbot_main.cpp and compute.cpp, likely next errors will be in:
- Server components
- Tool executables  
- Whisper.cpp integration
- Stable-diffusion.cpp integration

---

**Document Version:** 1.0  
**Last Updated:** October 31, 2025  
**Status:** READY FOR NEXT AGENT SESSION  
**Estimated Completion:** 85% → 100% in 11-19 agent hours

