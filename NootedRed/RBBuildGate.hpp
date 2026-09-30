#pragma once
#include <string.h>

static inline bool rbSupportedKernel(const char *version) {
    static const char wanted[] = "Darwin Kernel Version 25.4.0: Thu Mar 19 19:27:54 PDT 2026; root:xnu-12377.101.15~1/RELEASE_X86_64";
    return version && !strncmp(version,wanted,sizeof wanted);
}

static inline bool rbSupportedOSBuild(const char *build) {
    return build && !strncmp(build,"25E253",7);
}
