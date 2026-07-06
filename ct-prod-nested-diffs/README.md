# CT prod nested-submodule state (snapshot 2026-07-07)

The nested submodules (llama.cpp @ 71d223c, whisper.cpp @ 2eeeba5,
stable-diffusion.cpp @ baf7eda) carry live modifications on CT that had
drifted BEYOND the tracked *.patches dirs (notably llama.cpp
tools/server/server.cpp embed-sidecar-era changes and llama.cpp/BUILD.mk).

To reconstruct the exact CT prod tree for each submodule:
1. `git checkout` the gitlink commit (pristine, no apply-patches.sh)
2. `git apply ct-prod-nested-diffs/<name>.tracked.diff`
3. copy `ct-prod-nested-diffs/<name>.untracked/*` into the submodule root
