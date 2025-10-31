#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#── vi: set noet ft=make ts=8 sw=8 fenc=utf-8 :vi ────────────────────┘

PKGS += LLAMA_CPP

# Collect all relevant source files from new structure
# Note: Exclude ggml-cpu/ggml-cpu.c because ggml-cpu/ggml-cpu.cpp provides the backend registration
LLAMA_CPP_GGML_SRCS_C := $(wildcard llama.cpp/ggml/src/*.c) \
	$(filter-out llama.cpp/ggml/src/ggml-cpu/ggml-cpu.c, $(wildcard llama.cpp/ggml/src/ggml-cpu/*.c))

LLAMA_CPP_GGML_SRCS_CPP := $(wildcard llama.cpp/ggml/src/*.cpp) \
	$(wildcard llama.cpp/ggml/src/ggml-cpu/*.cpp) \
	$(wildcard llama.cpp/ggml/src/ggml-cpu/arch/x86/*.cpp) \
	$(wildcard llama.cpp/ggml/src/ggml-cpu/arch/arm/*.cpp)

LLAMA_CPP_SRC_SRCS_CPP := $(wildcard llama.cpp/src/*.cpp)

LLAMA_CPP_COMMON_SRCS_CPP := $(wildcard llama.cpp/common/*.cpp)

# Collect headers
LLAMA_CPP_HDRS := $(wildcard llama.cpp/include/*.h) \
	$(wildcard llama.cpp/ggml/include/*.h) \
	$(wildcard llama.cpp/ggml/src/*.h) \
	$(wildcard llama.cpp/ggml/src/ggml-cpu/*.h) \
	$(wildcard llama.cpp/src/*.h) \
	$(wildcard llama.cpp/common/*.h)

# Combine all source files
LLAMA_CPP_SRCS_C = $(LLAMA_CPP_GGML_SRCS_C)
LLAMA_CPP_SRCS_CPP = $(LLAMA_CPP_GGML_SRCS_CPP) \
	$(LLAMA_CPP_SRC_SRCS_CPP) \
	$(LLAMA_CPP_COMMON_SRCS_CPP)

LLAMA_CPP_SRCS = $(LLAMA_CPP_SRCS_C) $(LLAMA_CPP_SRCS_CPP)

# Object files
LLAMA_CPP_SRCS_OBJS = \
	$(LLAMA_CPP_SRCS_C:%.c=o/$(MODE)/%.o) \
	$(LLAMA_CPP_SRCS_CPP:%.cpp=o/$(MODE)/%.o)

LLAMA_CPP_OBJS = \
	$(LLAMAFILE_OBJS) \
	$(LLAMA_CPP_SRCS_OBJS) \
	$(LLAMA_CPP_HDRS:%=o/$(MODE)/%.zip.o)

# Library target
o/$(MODE)/llama.cpp/llama.cpp.a: $(LLAMA_CPP_SRCS_OBJS)
	@mkdir -p $(@D)/.aarch64
	$(AR) $(ARFLAGS) $@ $^
	$(AR) $(ARFLAGS) $(dir $@)/.aarch64/$(notdir $@) $(foreach x,$^,$(dir $(x)).aarch64/$(notdir $(x)))

# Include tool BUILD.mk files
include llama.cpp/tools/main/BUILD.mk
# NOTE: llama.cpp/tools/server disabled - llamafile has its own server implementation
# include llama.cpp/tools/server/BUILD.mk
include llama.cpp/tools/imatrix/BUILD.mk
include llama.cpp/tools/quantize/BUILD.mk
include llama.cpp/tools/perplexity/BUILD.mk
include llama.cpp/tools/llama-bench/BUILD.mk
include llama.cpp/tools/mtmd/BUILD.mk

# Compiler flags for all llama.cpp objects
$(LLAMA_CPP_SRCS_OBJS): private CCFLAGS += \
	-DNDEBUG \
	-DGGML_MULTIPLATFORM \
	-DGGML_USE_LLAMAFILE \
	-DGGML_VERSION=\"1.0.0\" \
	-DGGML_COMMIT=\"modernized\"

# Add include paths for new structure
$(LLAMA_CPP_OBJS): private CPPFLAGS += \
	-Illama.cpp/include \
	-Illama.cpp/ggml/include \
	-Illama.cpp/ggml/src \
	-Illama.cpp/src \
	-Illama.cpp/common \
	-Illama.cpp/vendor

# Optimization flags for GGML core
o/$(MODE)/llama.cpp/ggml/src/ggml.o: private CCFLAGS += -O3 -mgcc

# Handle ggml-cpu dual-source conflict:
# - ggml-cpu.c contains CPU operation implementations
# - ggml-cpu.cpp contains backend registration (ggml_backend_cpu_reg)
# Both must be compiled, but Make's implicit rules prefer .c over .cpp
# Solution: Compile .c to ggml-cpu-impl.o and .cpp to ggml-cpu.o

# Compile .c file to separate object (CPU operations)
o/$(MODE)/llama.cpp/ggml/src/ggml-cpu/ggml-cpu-impl.o: llama.cpp/ggml/src/ggml-cpu/ggml-cpu.c $(COSMOCC)
	@mkdir -p $(@D)
	$(COMPILE.c) -Illama.cpp/ggml/include -Illama.cpp/ggml/src -Illama.cpp/ggml/src/ggml-cpu -o $@ $<

# Force .cpp file compilation (backend registration)
o/$(MODE)/llama.cpp/ggml/src/ggml-cpu/ggml-cpu.o: llama.cpp/ggml/src/ggml-cpu/ggml-cpu.cpp $(COSMOCC)
	@mkdir -p $(@D)
	$(COMPILE.cc) -Illama.cpp/ggml/include -Illama.cpp/ggml/src -Illama.cpp/ggml/src/ggml-cpu -o $@ $<

# Include the impl object in the archive
LLAMA_CPP_SRCS_OBJS += o/$(MODE)/llama.cpp/ggml/src/ggml-cpu/ggml-cpu-impl.o

# CPU-specific optimizations
o/$(MODE)/llama.cpp/ggml/src/ggml-cpu/ggml-cpu.o: private CCFLAGS += -O3 -mgcc
o/$(MODE)/llama.cpp/ggml/src/ggml-cpu/ggml-cpu-impl.o: private CCFLAGS += -O3 -mgcc
o/$(MODE)/llama.cpp/ggml/src/ggml-quants.o: private CXXFLAGS += -Os

# Architecture-specific files need parent dir includes
o/$(MODE)/llama.cpp/ggml/src/ggml-cpu/arch/x86/%.o: private CPPFLAGS += -Illama.cpp/ggml/src/ggml-cpu
o/$(MODE)/llama.cpp/ggml/src/ggml-cpu/arch/arm/%.o: private CPPFLAGS += -Illama.cpp/ggml/src/ggml-cpu

# Architecture-specific optimizations for x86
o/$(MODE)/llama.cpp/ggml/src/ggml-cpu/arch/x86/quants.o: private TARGET_ARCH += -Xx86_64-mtune=znver4

# Architecture-specific optimizations for ARM
o/$(MODE)/llama.cpp/ggml/src/ggml-cpu/arch/arm/quants.o: private TARGET_ARCH += -Xaarch64-march=armv8.2-a+fp16

# Size optimizations for some components
o/$(MODE)/llama.cpp/ggml/src/ggml-alloc.o \
o/$(MODE)/llama.cpp/ggml/src/ggml-backend.o \
o/$(MODE)/llama.cpp/common/sampling.o: \
	private CCFLAGS += -Os

# Dependency tracking
$(LLAMA_CPP_OBJS): llama.cpp/BUILD.mk

# Phony targets
.PHONY: o/$(MODE)/llama.cpp
o/$(MODE)/llama.cpp: \
	o/$(MODE)/llama.cpp/llama.cpp.a \
	o/$(MODE)/llama.cpp/main \
	o/$(MODE)/llama.cpp/server \
	o/$(MODE)/llama.cpp/imatrix \
	o/$(MODE)/llama.cpp/quantize \
	o/$(MODE)/llama.cpp/perplexity \
	o/$(MODE)/llama.cpp/llama-bench \
	o/$(MODE)/llama.cpp/llava
