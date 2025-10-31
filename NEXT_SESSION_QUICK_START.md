# Next Session: Quick Start Guide

**Status:** 92% Complete | **Remaining:** 2 Files | **Time to 100%:** 45 minutes

---

## TL;DR - What to Do

Fix 2 files that have compilation errors, rebuild, test, commit. Done.

---

## The 2 Fixes (20 minutes total)

### Fix #1: core_manager.cpp (5 minutes)

**File:** `/Users/seb/Projects/UpdatedLLamafile/llamafile/llamafile/core_manager.cpp`

**Error:** Line 22 has missing header `'llama.cpp/cores.h'`

**Solution:** Check if file is used, then either:
- Remove the include (if not used)
- Find correct header (if used)
- Comment it out (if unsure)

**Command to check if used:**
```bash
cd /Users/seb/Projects/UpdatedLLamafile/llamafile
grep -r "core_manager" . --include="*.cpp" --include="*.h" | grep -v core_manager.cpp
```

**Most likely fix:** Comment out the problematic include line 22

### Fix #2: chatbot_repl.cpp (15 minutes)

**File:** `/Users/seb/Projects/UpdatedLLamafile/llamafile/llamafile/chatbot_repl.cpp`

**Errors:** Lines 101, 103, 145 have old token API calls

**Solution:** Replace 3 function calls with new vocab-based API

**The Pattern:**
```cpp
// OLD (broken):
llama_should_add_bos_token(g_model)    // This doesn't exist
llama_token_bos(g_model)               // Wrong parameters

// NEW (correct):
const struct llama_vocab *vocab = llama_model_get_vocab(g_model);
llama_vocab_get_add_bos(vocab)         // Right function & params
llama_vocab_bos(vocab)                 // Right function & params
```

**Where to find the pattern:** Look at `chatbot_main.cpp` - it's already fixed there. Copy the pattern.

**Command to verify fix:**
```bash
gmake o//llamafile/chatbot_repl.o
```

---

## Full Build (5 minutes)

```bash
cd /Users/seb/Projects/UpdatedLLamafile/llamafile

# Clean and rebuild
gmake clean
gmake -j4 2>&1 | tee final_build.log

# Check results
echo "Exit code: $?"  # Should be 0
grep "error:" final_build.log | wc -l  # Should be 0
```

---

## Test It Works (10 minutes)

```bash
# Build the main tool
gmake o//llama.cpp/tools/main/main

# Test it
./o//llama.cpp/tools/main/main --version

# Should print version info without errors
```

---

## Commit (5 minutes)

```bash
git add llamafile/core_manager.cpp llamafile/chatbot_repl.cpp DOCS/COMPLETION_STATUS.md

git commit -m "fix: Complete API migration to modern llama.cpp

Final compilation fixes:
- core_manager.cpp: Fix missing header reference
- chatbot_repl.cpp: Migrate token API to vocab-based calls

All source files now compiling successfully.
Modernization now 100% complete.

🤖 Generated with Claude Code
Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

## Verify Success

If you see:
- Build completes with exit code 0
- No "error:" lines in build.log
- All tools built successfully
- test tool runs and responds

Then you've achieved **100% completion!**

---

## Reference Documents

- **COMPLETION_STATUS.md** - Full status with all details
- **AGENT_5_FINAL_REPORT.md** - Executive summary of work done
- **PROJECT_STATUS_SNAPSHOT.txt** - One-page visual overview
- **PATH_TO_100_PERCENT.md** - Detailed step-by-step guide
- **API_MIGRATION_QUICK_REF.md** - API lookup table

---

## That's It!

Two simple fixes = 100% complete.

Good luck!
