#include <cassert>
#include <cstdio>
#include "RBBuildGate.hpp"
int main() {
    const char kernel[]="Darwin Kernel Version 25.4.0: Thu Mar 19 19:27:54 PDT 2026; root:xnu-12377.101.15~1/RELEASE_X86_64";
    // The old startup gate rejects an unpopulated build value on a supported
    // kernel. The fixed phase split can arm but cannot patch any pages yet.
    assert(!rbSupportedOSBuild(""));
    assert(rbSupportedKernel(kernel));
    assert(rbSupportedOSBuild("25E253"));
    assert(!rbSupportedOSBuild("25E252"));
    assert(!rbSupportedOSBuild("25E253x"));
    assert(!rbSupportedOSBuild("25E25"));
    assert(!rbSupportedOSBuild(nullptr));
    assert(!rbSupportedKernel(nullptr));
    assert(!rbSupportedKernel(""));
    char changed[sizeof kernel];
    for (unsigned i=0;i<sizeof kernel-1;i++) {
        memcpy(changed,kernel,sizeof kernel); changed[i]^=1;
        assert(!rbSupportedKernel(changed));
    }
    puts("PASS: delayed OS-build availability, exact kernel identity, wrong/truncated builds rejected");
}
