# Llamafile Modernization - Agent Specifications

## Orchestrator Agent

**Role:** Project Manager & Integration Monitor
**Responsibility:** Track overall progress, resolve integration conflicts, ensure all agents' changes work together
**Tools:** Read-only access to all files, git status monitoring

### Tasks:
1. Monitor todo list completion across all agents
2. Identify integration conflicts between agents' changes
3. Verify build status after each agent completes work
4. Coordinate handoffs between dependent tasks
5. Ensure git commits are made at proper milestones

### Success Criteria:
- All agent tasks complete without conflicts
- Full build succeeds (`gmake -j4` exits 0)
- All tests pass
- Performance benchmarks within 10% of baseline

---

## Agent 1: Compilation Fix Specialist

**Focus:** chatbot_main.cpp API Migration
**Priority:** CRITICAL
**Estimated Time:** 2-3 hours

### Specific Responsibilities:
1. Add missing includes (`llamafile.h`, etc.)
2. Migrate params API (`gpt_params` → `common_params`)
3. Update sampling API (`llama_sampling_*` → `llama_sampler_*`)
4. Update token functions to use vocab parameter
5. Fix BOS token function calls
6. Fix EOG token checking

### Input Files:
- `llamafile/chatbot_main.cpp`
- `llamafile/chatbot.h`

### Reference Files:
- `llama.cpp/include/llama.h`
- `llama.cpp/common/common.h`
- `llama.cpp/common/sampling.h`
- `llama.cpp/tools/main/main.cpp` (example)

### Output:
- Modified `llamafile/chatbot_main.cpp` that compiles without errors
- Documentation of API changes made

### Validation:
```bash
gmake o//llamafile/chatbot_main.o
```

---

## Agent 2: Build System Validator

**Focus:** Full Build & Dependency Resolution
**Priority:** HIGH
**Estimated Time:** 1-2 hours
**Depends On:** Agent 1 completion

### Specific Responsibilities:
1. Attempt full clean build after Agent 1 completes
2. Identify all remaining compilation errors
3. Fix cascading include issues
4. Resolve linker errors
5. Ensure all BUILD.mk files are correct

### Tasks:
```bash
gmake clean
gmake -j4 2>&1 | tee build.log
# Analyze build.log for errors
# Fix systematic issues
# Repeat until clean build
```

### Success Criteria:
- `gmake -j4` completes without errors
- All binaries created in `o//` directory
- No undefined symbol errors

---

## Agent 3: Testing & Validation Specialist

**Focus:** Functional Testing
**Priority:** HIGH
**Estimated Time:** 2-3 hours
**Depends On:** Agent 2 completion

### Specific Responsibilities:
1. Create test llamafile with TinyLLama model
2. Test basic text generation (CPU mode)
3. Test GPU detection and offloading
4. Verify model loading works
5. Test interruption (Ctrl+C)
6. Validate output quality

### Test Plan:
```bash
# Test 1: Build llamafile
cp o//llama.cpp/main/main test.llamafile
o//llamafile/zipalign -j0 test.llamafile model.gguf .args

# Test 2: CPU generation
./test.llamafile -p "Once upon a time" -n 50 -ngl 0

# Test 3: GPU detection
./test.llamafile --version 2>&1 | grep -i "gpu\|cuda\|metal"

# Test 4: Model info
./test.llamafile -m model.gguf --help
```

### Success Criteria:
- Model loads without crashes
- Text generation produces coherent output
- No segfaults or memory errors
- GPU detected (if applicable)
- Performance is reasonable

---

## Agent 4: Performance Benchmarking Specialist

**Focus:** Performance Validation
**Priority:** MEDIUM
**Estimated Time:** 1-2 hours
**Depends On:** Agent 3 completion

### Specific Responsibilities:
1. Run llama-bench benchmarks
2. Compare performance vs baseline (if available)
3. Verify custom kernels are active
4. Check SIMD instruction usage
5. Measure prompt processing speed
6. Measure token generation speed

### Benchmarks:
```bash
# Build benchmark tool
gmake o//llama.cpp/llama-bench/llama-bench

# Test prompt processing (512 tokens)
./o//llama.cpp/llama-bench/llama-bench \
  -m models/TinyLLama-v0.1-5M-F16.gguf \
  -p 512 -n 0 -ngl 0

# Test generation (128 tokens)
./o//llama.cpp/llama-bench/llama-bench \
  -m models/TinyLLama-v0.1-5M-F16.gguf \
  -p 0 -n 128 -ngl 0
```

### Success Criteria (from SPEC.txt):
- No performance regressions (within 10% of previous)
- Custom TinyBLAS kernels active
- Prompt processing: 2-5x faster than standard llama.cpp
- Token generation: Competitive with llama.cpp

---

## Agent 5: Llamafile Packaging Specialist

**Focus:** Create Production Llamafiles
**Priority:** MEDIUM
**Estimated Time:** 1 hour
**Depends On:** Agent 3 + Agent 4 completion

### Specific Responsibilities:
1. Inspect GGUF metadata for model architectures
2. Create .args files for each model with optimal parameters
3. Package all LM Studio models as llamafiles
4. Test each llamafile with sample prompts
5. Document model specifications

### Models to Package:
```
/Users/seb/.lmstudio/models/inclusionAI/Ring-mini-2.0-GGUF/Ring-mini-2.0-Q2_K.gguf
/Users/seb/.lmstudio/models/mradermacher/Huihui-Ling-mini-2.0-abliterated-i1-GGUF/Huihui-Ling-mini-2.0-abliterated.i1-IQ4_XS.gguf
/Users/seb/.lmstudio/models/lmstudio-community/granite-4.0-h-tiny-GGUF/granite-4.0-h-tiny-Q6_K.gguf
/Users/seb/.lmstudio/models/lmstudio-community/gemma-3n-E4B-it-text-GGUF/gemma-3n-E4B-it-Q4_K_M.gguf
```

### Process:
```bash
# For each model:
# 1. Inspect metadata
gguf-dump model.gguf | grep -i "arch\|type\|size"

# 2. Create .args file
cat > model.args << 'EOF'
-m
model.gguf
-ngl
999
--host
127.0.0.1
EOF

# 3. Package
cp o//llama.cpp/main/main model.llamafile
o//llamafile/zipalign -j0 model.llamafile model.gguf model.args

# 4. Test
./model.llamafile -p "Hello, how are you?" -n 20
```

### Success Criteria:
- All 4 models packaged as llamafiles
- Each llamafile runs independently
- Models generate coherent text
- Documentation created listing all models with specs

---

## Agent 7: Harmony Integration Specialist

**Focus:** OpenAI Harmony Framework Integration
**Priority:** MEDIUM
**Estimated Time:** 2-3 hours

### Specific Responsibilities:
1. Clone and integrate OpenAI Harmony (v0.3.4) from https://github.com/openai/harmony.git
2. Implement chat history renderer and parser from OpenAI
3. Implement prompt renderer from OpenAI
4. Fix bug where harmony crashes on very large prompts
5. Install openai-harmony==0.0.3

### Integration Points:
- Chat history management in chatbot
- Prompt formatting for LLM interaction
- OpenAI API compatibility layer

### Tasks:
```bash
# Clone Harmony
git clone https://github.com/openai/harmony.git third_party/harmony
cd third_party/harmony
git checkout v0.3.4

# Integrate with llamafile
# Create BUILD.mk for harmony
# Add harmony includes to chatbot
# Implement chat history renderer
# Implement prompt renderer
# Fix large prompt crash bug
```

### Success Criteria:
- Harmony library integrated and compiling
- Chat history rendering works
- Prompt rendering works
- Large prompts don't crash
- Tests pass

---

## Agent 6: Git Commit Coordinator

**Focus:** Version Control & Milestones
**Priority:** LOW
**Estimated Time:** 30 minutes
**Runs:** After each major milestone

### Specific Responsibilities:
1. Create git commits at specified milestones
2. Write descriptive commit messages
3. Ensure .gitignore is up to date
4. Tag important versions
5. Keep working tree clean

### Milestones to Commit:
1. **compute.cpp fixed** (already done)
2. **chatbot_main.cpp compiles** (after Agent 1)
3. **Full build succeeds** (after Agent 2)
4. **Basic tests pass** (after Agent 3)
5. **Performance validated** (after Agent 4)
6. **Llamafiles created** (after Agent 5)

### Commit Message Format:
```
<type>: <short description>

<detailed explanation of changes>

- Change 1
- Change 2
- Change 3

Tested: <what was tested>
Performance: <any relevant metrics>

🤖 Generated with Claude Code
Co-Authored-By: Claude <noreply@anthropic.com>
```

---

## Agent Coordination Protocol

### Parallel Execution:
- **Phase 1:** Agent 1 (Compilation) works alone
- **Phase 2:** Agent 2 (Build) works alone (waits for Agent 1)
- **Phase 3:** Agent 3 (Testing) and Agent 4 (Performance) work in parallel (both wait for Agent 2)
- **Phase 4:** Agent 5 (Packaging) works alone (waits for Agent 3 + 4)
- **Throughout:** Agent 6 (Git) commits at milestones, Orchestrator monitors

### Communication:
Each agent reports status to Orchestrator:
- ✅ Task started
- 🔄 In progress (with percentage)
- ⚠️ Blocked (with reason)
- ✅ Task complete (with artifacts)
- ❌ Task failed (with error details)

### Conflict Resolution:
If agents modify overlapping files:
1. Orchestrator detects conflict
2. Pause both agents
3. Merge changes manually
4. Resume agents

### Integration Points:
1. After Agent 1: Verify chatbot_main.cpp compiles
2. After Agent 2: Verify full build succeeds
3. After Agent 3+4: Verify tests pass and performance OK
4. After Agent 5: Verify all llamafiles work
5. Final: Orchestrator runs full validation suite

---

## Orchestrator Validation Checklist

### After Each Agent Completes:
- [ ] Check agent's output artifacts exist
- [ ] Run agent's validation commands
- [ ] Check git status for unexpected changes
- [ ] Verify no new compilation errors introduced
- [ ] Update master todo list
- [ ] Trigger git commit (via Agent 6) if milestone reached

### Final Validation (All Agents Complete):
- [ ] Full clean build succeeds
- [ ] All tests pass
- [ ] Performance benchmarks pass
- [ ] All llamafiles created and functional
- [ ] Documentation updated
- [ ] Git history is clean and well-structured
- [ ] No TODOs left for critical issues

---

## Emergency Protocols

### If Agent Gets Stuck:
1. Orchestrator detects no progress for 30 minutes
2. Request agent to report current status
3. If blocked: Reassign task or adjust approach
4. If error: Collect error details and consult documentation

### If Build Breaks:
1. Identify which agent's changes caused breakage
2. Roll back that agent's changes
3. Analyze root cause
4. Fix issue
5. Resume agent with updated approach

### If Tests Fail:
1. Collect test output
2. Determine if issue is:
   - Compilation error (back to Agent 1/2)
   - Runtime error (Agent 3 investigates)
   - Performance regression (Agent 4 investigates)
   - Packaging issue (Agent 5 investigates)
3. Fix and retest

---

## Success Metrics

### Project Complete When:
- ✅ All 6 agents have completed tasks
- ✅ Full build succeeds (`gmake -j4` exits 0)
- ✅ All tests pass (Agent 3)
- ✅ Performance validated (Agent 4)
- ✅ All llamafiles created and tested (Agent 5)
- ✅ Git commits made at all milestones (Agent 6)
- ✅ Orchestrator validation checklist 100% complete

### Delivery Artifacts:
1. Fully compiled llamafile binaries in `o//` directory
2. 4 production llamafiles in `~/llamafiles/`
3. Test results and performance benchmarks
4. Updated documentation (COMPLETION_STATUS.md, etc.)
5. Clean git history with descriptive commits
6. No outstanding TODOs for critical issues

---

**Document Version:** 1.0
**Created:** 2025-10-31
**Purpose:** Coordinate parallel agent work on llamafile modernization
**Status:** READY FOR AGENT DEPLOYMENT
