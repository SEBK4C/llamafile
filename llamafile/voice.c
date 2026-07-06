// -*- mode:c;indent-tabs-mode:nil;c-basic-offset:4;coding:utf-8 -*-
// vi: set et ft=c ts=4 sts=4 sw=4 fenc=utf-8 :vi
#define _COSMO_SOURCE // makedirs()
//
// Baked-in voice: if the zip carries a TTS server APE and a Kokoro GGUF,
// extract both to the app dir and spawn the server on a loopback port.
// The web UI probes /tts (reverse-proxied by server-http.cpp) and shows
// the read-aloud controls only when this answers — so on builds without
// the voice payload nothing changes.
//
// Opt-out: LLAMAFILE_NO_VOICE=1.

#include "llamafile.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define VOICE_ZIP_APE "/zip/tts-server.ape"
#define VOICE_ZIP_GGUF "/zip/kokoro.gguf"
#define VOICE_PORT "8078"

static pid_t g_voice_pid = -1;

static void voice_kill(void) {
    if (g_voice_pid > 0) {
        // the supervisor runs in its own process group with the TTS server;
        // signal the group so neither outlives us
        kill(-g_voice_pid, SIGTERM);
    }
}

// Returns the loopback port the voice server listens on, or 0 if no voice
// payload is present / spawn failed. Idempotent.
int llamafile_voice_start(void) {
    static int port = -1;
    if (port != -1)
        return port;
    port = 0;
    if (getenv("LLAMAFILE_NO_VOICE"))
        return port;
    // External sidecar mode (source-tree serve.sh route, or builds without
    // the baked payload): LLAMAFILE_TTS_PORT names a loopback port where a
    // TTS server (TTS.cpp tts-server or kokoro_server.py) already runs; the
    // /tts reverse proxy then works identically to the baked path.
    const char *env = getenv("LLAMAFILE_TTS_PORT");
    if (env && *env) {
        int p = atoi(env);
        if (p > 0 && p < 65536) {
            fprintf(stderr, "voice: proxying /tts to 127.0.0.1:%d "
                            "(LLAMAFILE_TTS_PORT)\n", p);
            port = p;
            return port;
        }
        fprintf(stderr, "warning: ignoring invalid LLAMAFILE_TTS_PORT=%s\n", env);
    }
    struct stat st;
    if (stat(VOICE_ZIP_APE, &st) || stat(VOICE_ZIP_GGUF, &st))
        return port; // no baked voice in this build
    char app[PATH_MAX], ape[PATH_MAX], gguf[PATH_MAX];
    llamafile_get_app_dir(app, sizeof(app));
    if (makedirs(app, 0755)) {
        fprintf(stderr, "voice: could not create %s\n", app);
        return port;
    }
    snprintf(ape, sizeof(ape), "%stts-server.ape", app);
    snprintf(gguf, sizeof(gguf), "%skokoro.gguf", app);
    if (!llamafile_extract(VOICE_ZIP_APE, ape) || !llamafile_extract(VOICE_ZIP_GGUF, gguf)) {
        fprintf(stderr, "voice: failed to extract baked TTS payload\n");
        return port;
    }
    chmod(ape, 0755);
    // Supervise: a crashed TTS server must not mean voice is gone until the
    // next full restart, so run it under a respawn loop (2s backoff). APE
    // binaries bootstrap reliably under sh on every unix. The loop gets its
    // own process group so voice_kill() can take out loop + server together.
    // TWO instances: VOICE_PORT = bulk lane, VOICE_PORT+1 = priority lane
    // for a turn's first words (synthesis is serial per instance; a long
    // chunk must never block the next reply's opening words). Each runs
    // under the watchdog script, which respawns on death AND on wedge (a
    // hung worker still LISTENS but never answers — liveness checks miss it).
    char wd[PATH_MAX];
    snprintf(wd, sizeof(wd), "%svoice-watchdog.sh", app);
    int have_wd = llamafile_extract("/zip/voice-watchdog.sh", wd);
    // Both branches are tied to OUR pid (F25): atexit cleanup never runs on
    // SIGKILL, which leaked watchdog/respawn loops and stacked orphans on
    // the voice ports; the loops now check the parent every cycle.
    char cmd[1024];
    if (have_wd) {
        snprintf(cmd, sizeof(cmd),
                 "( /bin/sh \"$3\" \"$1\" \"$2\" %s %d ) & "
                 "( /bin/sh \"$3\" \"$1\" \"$2\" %d %d ) & "
                 "wait", VOICE_PORT, (int) getpid(), atoi(VOICE_PORT) + 1, (int) getpid());
    } else {
        snprintf(cmd, sizeof(cmd),
                 "NP=\"nice -n -5\"; command -v nice >/dev/null 2>&1 || NP=\"\"; P=%d; "
                 "( while kill -0 $P 2>/dev/null; do $NP /bin/sh \"$1\" -mp \"$2\" --port %s & C=$!; "
                 "while kill -0 $C 2>/dev/null && kill -0 $P 2>/dev/null; do sleep 2; done; "
                 "kill -0 $P 2>/dev/null || kill $C 2>/dev/null; sleep 2; done ) & "
                 "( while kill -0 $P 2>/dev/null; do $NP /bin/sh \"$1\" -mp \"$2\" --port %d & C=$!; "
                 "while kill -0 $C 2>/dev/null && kill -0 $P 2>/dev/null; do sleep 2; done; "
                 "kill -0 $P 2>/dev/null || kill $C 2>/dev/null; sleep 2; done ) & "
                 "wait", (int) getpid(), VOICE_PORT, atoi(VOICE_PORT) + 1);
    }
    char *argv[] = {"/bin/sh", "-c", cmd, "g4voice", ape, gguf, wd, (char *)0};
    extern char **environ;
    pid_t pid;
    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attr, 0);
    int err = posix_spawn(&pid, "/bin/sh", 0, &attr, argv, environ);
    posix_spawnattr_destroy(&attr);
    if (err) {
        fprintf(stderr, "voice: failed to spawn TTS server: %s\n", strerror(err));
        return port;
    }
    g_voice_pid = pid;
    atexit(voice_kill);
    port = atoi(VOICE_PORT);
    fprintf(stderr, "voice: baked Kokoro TTS starting on 127.0.0.1:%d (read-aloud "
                    "controls appear in the web UI once it is ready; "
                    "LLAMAFILE_NO_VOICE=1 disables)\n", port);
    return port;
}
