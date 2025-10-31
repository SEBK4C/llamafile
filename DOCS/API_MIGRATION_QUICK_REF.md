# llama.cpp API Migration Quick Reference

**Purpose:** Fast lookup for agents fixing compilation errors  
**Updated:** October 31, 2025

---

## Token Functions

```cpp
// Get vocab first
const struct llama_vocab *vocab = llama_model_get_vocab(model);

// OLD → NEW
llama_token_bos(model)           → llama_vocab_bos(vocab)
llama_token_eos(model)           → llama_vocab_eos(vocab)
llama_token_nl(model)            → llama_vocab_nl(vocab)
llama_token_cls(model)           → llama_vocab_cls(vocab)
llama_token_sep(model)           → llama_vocab_sep(vocab)
llama_token_pad(model)           → llama_vocab_pad(vocab)
llama_token_is_eog(model, tok)   → llama_vocab_is_eog(vocab, tok)
llama_token_is_control(model, t) → llama_vocab_is_control(vocab, t)
```

---

## Memory/KV Cache Functions

```cpp
// Get memory handle first
llama_memory_t mem = llama_get_memory(ctx);

// OLD → NEW
llama_kv_cache_clear(ctx)                    → llama_memory_clear(mem, true)
llama_kv_cache_seq_rm(ctx, seq, p0, p1)      → llama_memory_seq_rm(mem, seq, p0, p1)
llama_kv_cache_seq_add(ctx, seq, p0, p1, d)  → llama_memory_seq_add(mem, seq, p0, p1, d)
llama_kv_cache_seq_cp(ctx, src, dst, p0, p1) → llama_memory_seq_cp(mem, src, dst, p0, p1)
llama_kv_cache_seq_keep(ctx, seq)            → llama_memory_seq_keep(mem, seq)
llama_kv_cache_seq_div(ctx, seq, p0, p1, d)  → llama_memory_seq_div(mem, seq, p0, p1, d)
```

---

## Batch Functions

```cpp
// Simplified signature
llama_batch batch = llama_batch_get_one(tokens, n_tokens);
// No longer takes: pos, seq_id parameters
// Position and sequence are now auto-managed or set via batch struct members

// For manual control:
llama_batch batch = llama_batch_init(n_tokens_alloc, embd, n_seq_max);
// Then manually populate: batch.pos, batch.seq_id, etc.
```

---

## Sampling Functions

```cpp
// OLD:
llama_sampling_context *sampler = llama_sampling_init(sparams);
llama_token tok = llama_sampling_sample(sampler, ctx, idx);
llama_sampling_accept(sampler, ctx, tok, apply_grammar);
llama_sampling_free(sampler);

// NEW:
llama_sampler *sampler = llama_sampler_chain_init(...);
// Configure sampler chain...
llama_token tok = llama_sampler_sample(sampler, ctx, idx);
// Accept API changed - check common/sampling.h
llama_sampler_free(sampler);
```

**Note:** Sampling API substantially changed. Check `llama.cpp/common/sampling.h` for details.

---

## Timing/Performance Functions

```cpp
// OLD → NEW
llama_print_timings(ctx)    → llama_perf_context_print(ctx)
llama_reset_timings(ctx)    → llama_perf_context_reset(ctx)
                             → llama_perf_sampler_print(sampler)
                             → llama_perf_sampler_reset(sampler)
```

---

## Chat Functions

```cpp
// Type rename
llama_chat_msg → llama_chat_message

// Struct unchanged:
struct llama_chat_message {
    const char *role;
    const char *content;
};

// Template application changed signature:
// OLD:
std::string msg = llama_chat_apply_template(
    model, chat_template, chat_vector, add_assistant);

// NEW (requires buffer):
std::vector<llama_chat_message> chat = {...};
char buf[8192];
int32_t len = llama_chat_apply_template(
    chat_template,     // const char* (NULL for model default)
    chat.data(),       // const llama_chat_message*
    chat.size(),       // size_t n_msg
    add_assistant,     // bool
    buf,               // char* buffer
    sizeof(buf)        // int32_t length
);

// Handle buffer overflow:
if (len > sizeof(buf)) {
    std::string large_buf(len, '\0');
    llama_chat_apply_template(..., large_buf.data(), large_buf.size());
    msg = large_buf;
} else {
    msg = std::string(buf, len);
}
```

---

## Params Structs

```cpp
// Type rename
gpt_params → common_params

// Member changes (CHECK THESE):
g_params.sparams          → ? (Sampling params changed)
g_params.model            → ? (May be string directly now)
g_params.chat_template    → Still exists (std::string)
g_params.prompt           → Still exists (std::string)
g_params.n_batch          → Check if still exists

// Parsing function:
gpt_params_parse(...)     → common_params_parse(...) or similar (RESEARCH NEEDED)
```

---

## Model/Context Functions

```cpp
// Model params:
llama_model_params_from_gpt_params(...) → ? (RESEARCH NEEDED)
// Likely: llama_model_default_params() or similar

// Context params:
llama_context_params_from_gpt_params(...) → llama_context_default_params() or similar

// Model loading (likely unchanged):
llama_load_model_from_file(path, params)

// Context creation (likely unchanged):
llama_new_context_with_model(model, params)
```

---

## Type Changes

```cpp
// Opaque types (now void*):
llama_sampling_context  → llama_sampler_context_t (void**)
                        → llama_sampler (struct llama_sampler*)

// Memory handle:
(none)                  → llama_memory_t (opaque handle)
```

---

## CUDA/Metal API Changes

### CUDA
```cpp
// Removed functions (stubs provided in llamafile/cuda.c):
ggml_cuda_link(...)                          → REMOVED
ggml_backend_cuda_reg_devices()              → REMOVED (use get_device_count)
ggml_backend_cuda_get_device_properties(...) → REMOVED (compat stub exists)

// Still exists:
ggml_backend_cuda_init(device)
ggml_backend_cuda_get_device_count()
ggml_backend_cuda_get_device_memory(dev, free, total)
ggml_backend_cuda_get_device_description(dev, buf, size)
ggml_backend_cuda_buffer_type(device)
ggml_backend_cuda_host_buffer_type()

// New:
ggml_backend_cuda_reg()  → Returns backend registry
```

### Metal
```cpp
// Removed functions (stubs provided in llamafile/metal.c):
ggml_metal_link(...)                        → REMOVED
ggml_backend_metal_buffer_type()            → REMOVED (stub returns NULL)
ggml_backend_metal_buffer_from_ptr(...)     → REMOVED (stub returns NULL)
ggml_backend_metal_set_n_cb(...)            → REMOVED (stub does nothing)
ggml_backend_metal_log_set_callback(...)    → REMOVED (stub does nothing)
ggml_backend_metal_get_device_properties(...) → REMOVED (stub does nothing)
ggml_backend_metal_get_device_memory_usage(...) → REMOVED (stub does nothing)

// Still exists:
ggml_backend_metal_init()
ggml_backend_is_metal(backend)
ggml_backend_metal_supports_family(backend, family)

// New:
ggml_backend_metal_reg()  → Returns backend registry
ggml_backend_metal_set_abort_callback(...)
ggml_backend_metal_capture_next_compute(...)
```

---

## Include Path Quick Reference

```cpp
// GGML Public Headers → ggml/include/
#include "llama.cpp/ggml/include/ggml.h"
#include "llama.cpp/ggml/include/ggml-backend.h"
#include "llama.cpp/ggml/include/ggml-alloc.h"
#include "llama.cpp/ggml/include/ggml-cuda.h"
#include "llama.cpp/ggml/include/ggml-metal.h"
#include "llama.cpp/ggml/include/ggml-cpu.h"
#include "llama.cpp/ggml/include/gguf.h"

// GGML Internal Headers → ggml/src/
#include "llama.cpp/ggml/src/ggml-impl.h"
#include "llama.cpp/ggml/src/ggml-common.h"
#include "llama.cpp/ggml/src/ggml-quants.h"
#include "llama.cpp/ggml/src/ggml-backend-impl.h"

// GGML CPU Headers → ggml/src/ggml-cpu/
#include "llama.cpp/ggml/src/ggml-cpu/vec.h"

// Llama Public Headers → include/
#include "llama.cpp/include/llama.h"
#include "llama.cpp/include/llama-cpp.h"

// Common Utilities → common/
#include "llama.cpp/common/common.h"
#include "llama.cpp/common/sampling.h"
#include "llama.cpp/common/base64.hpp"
#include "llama.cpp/common/log.h"

// Tools → tools/
#include "llama.cpp/tools/mtmd/clip.h"
#include "llama.cpp/tools/server/server.h"
```

---

## Error Message → Solution Mapping

| Error Message | Solution |
|---------------|----------|
| `use of undeclared identifier 'llama_token_bos'` | Use `llama_vocab_bos(llama_model_get_vocab(model))` |
| `use of undeclared identifier 'llama_kv_cache_seq_rm'` | Use `llama_memory_seq_rm(llama_get_memory(ctx), ...)` |
| `use of undeclared identifier 'llama_sampling_init'` | New sampler API - check `common/sampling.h` |
| `unknown type name 'llama_sampling_context'` | Use `llama_sampler*` or `llama_sampler_context_t` |
| `unknown type name 'gpt_params'` | Use `common_params` |
| `unknown type name 'llava_image_embed'` | Use `mtmd_image_tokens` (complex migration) |
| `member access into incomplete type 'gpt_params'` | Add `#include "llama.cpp/common/common.h"` |
| `no matching function for call to 'llama_batch_get_one'` | Signature changed: only takes `(tokens, n_tokens)` now |
| `fatal error: 'llama.cpp/X.h' file not found` | Update include path - see table above |
| `use of undeclared identifier 'llama_print_timings'` | Use `llama_perf_context_print(ctx)` |

---

## 🔗 Quick Links for Research

### Search Commands
```bash
# Find function in llama.cpp
grep -r "^LLAMA_API.*function_name" llama.cpp/include/

# Find struct definition
grep -r "struct.*name\|typedef.*name" llama.cpp/include/

# Find usage example
grep -r "function_name" llama.cpp/tools/main/
grep -r "function_name" llama.cpp/examples/

# Check if function deprecated
grep "DEPRECATED.*function_name" llama.cpp/include/llama.h
```

### File Locations Cheat Sheet
```
llama.cpp/
├── include/llama.h              → Main API definitions ⭐
├── common/common.h              → Params, utilities ⭐
├── common/sampling.h            → Sampling API ⭐
├── tools/main/main.cpp          → Reference CLI implementation ⭐
├── tools/server/server.cpp      → Reference server implementation
├── tools/mtmd/                  → Multimodal API
├── ggml/include/ggml.h          → GGML tensor operations
└── ggml/include/ggml-backend.h  → Backend abstraction
```

---

## 🎯 For Agents: Start Here

### First Time Reading This?
1. Read COMPLETION_STATUS.md for overall context
2. Read the specific AGENT TASK assigned to you
3. Use this document for quick API lookups
4. Follow the workflow template
5. Report back using the communication template

### Starting a Task?
1. Understand current state (build and check errors)
2. Research solution (use search commands)
3. Implement fix (apply migration patterns)
4. Verify fix (compile and test)
5. Document (add comments and update status)

### Stuck?
1. Check "Troubleshooting Guide" in NEXT_STEPS_AGENT_GUIDE.md
2. Search llama.cpp examples for reference
3. Read the actual header file (llama.h is authoritative)
4. Look for DEPRECATED markers (they tell you what to use instead)
5. If all else fails, stub it out with TODO and move on

---

**This is a living document. Update it as you discover new API changes!**

