// Temporary stub for clip functions until multimodal support is migrated
#include <cstdint>
#include <cstddef>

struct clip_ctx;
struct block_q8_0;

// All stubs use C linkage
extern "C" {

// Clip stub - multimodal support temporarily disabled
void clip_free(clip_ctx* ctx) {
    // Stub - multimodal support temporarily disabled
    (void)ctx;
}

// Quantization stub
extern void quantize_row_q8_0_ref(const float * __restrict x, block_q8_0 * __restrict y, int64_t k);
void quantize_row_q8_0(const float * __restrict x, block_q8_0 * __restrict y, int64_t k) {
    quantize_row_q8_0_ref(x, y, k);
}

}
