// Experimental, opt-in 25E253/Ryzen 5600G Metal correction.
// Included only from NRed.cpp. Payload bytes execute in userspace, never here.
#pragma once
#include <Headers/kern_api.hpp>
#include <Headers/kern_util.hpp>
#include <NRed.hpp>
#include <libkern/OSAtomic.h>
#include <sys/vnode.h>
#include "rbplus-page-patch.h"
#include "RBBuildGate.hpp"

namespace RBPlus {
static mach_vm_address_t originalValidate {};
static const char *osBuild {};
static volatile UInt32 applied[2] {}, rejected[2] {}, badPath[2] {};

static bool matchesCache(const char *path) {
    return !strcmp(path,"/System/Volumes/Preboot/Cryptexes/OS/System/Library/dyld/dyld_shared_cache_x86_64h.03") ||
           !strcmp(path,"/System/Cryptexes/OS/System/Library/dyld/dyld_shared_cache_x86_64h.03") ||
           !strcmp(path,"/System/Library/dyld/dyld_shared_cache_x86_64h.03");
}

static void validatePage(vnode_t vp, memory_object_t pager, memory_object_offset_t offset,
                         const void *data, int *validated, int *tainted, int *nx) {
    FunctionCast(validatePage,originalValidate)(vp,pager,offset,data,validated,tainted,nx);
    if (offset!=0x10b7e000 && offset!=0x10b83000) return;
    // OS build can be populated after this plugin starts. Never patch until
    // it is the supported build, even when the kernel identity already matches.
    if (!rbSupportedOSBuild(osBuild)) return;
    // Installed x86_64 kernel stores 0xf for a fully validated page.
    // A nonzero validation result alone does not mean the hash matched.
    if (!vp || !data || !validated || !tainted || !nx ||
        *validated!=0xf || *tainted || *nx) return;
    const unsigned index=offset==0x10b7e000 ? 0 : 1;
    char path[1024];
    int pathSize=sizeof path;
    if (vn_getpath(vp,path,&pathSize)!=0) return;
    if (!matchesCache(path)) {
        if (OSCompareAndSwap(0,1,&badPath[index]))
            SYSLOG("NRedRB","rbplus-v3 skipped unexpected path at %llx: %s",offset,path);
        return;
    }
    const auto result=rbPatchPage(const_cast<void *>(data),PAGE_SIZE,offset);
    if (result==RBPageMismatch) {
        if (OSCompareAndSwap(0,1,&rejected[index]))
            SYSLOG("NRedRB","rbplus-v3 rejected exact page mismatch at %llx",offset);
    } else if (OSCompareAndSwap(0,1,&applied[index])) {
        SYSLOG("NRedRB","rbplus-v3 %s emitter page at %llx (%s)",index ? "delta" : "full",offset,
               result==RBPageOriginal ? "patched" : "already patched");
    }
}

// Persistent startup diagnostics; only used before hook registration, never
// from the page-validation callback (which must not allocate registry objects).
static void startupStatus(UInt32 value) {
    NRed::singleton().setProp32("NRedRBPlus-StartupStatus",value);
}
static void processPatcher(KernelPatcher &patcher) {
    startupStatus(1); // entered, opt-in not yet confirmed
    NRed::singleton().setProp32("NRedRBPlus-DiagnosticVersion",3);
    if (!checkKernelArgument("-NRedRBPlus")) return;
    startupStatus(2); // opt-in confirmed
    NRed::singleton().setProp32("NRedRBPlus-KernelMajor",static_cast<UInt32>(getKernelVersion()));
    NRed::singleton().setProp32("NRedRBPlus-KernelMinor",static_cast<UInt32>(getKernelMinorVersion()));
    NRed::singleton().setProp32("NRedRBPlus-DeviceID",NRed::singleton().getDeviceID());
    NRed::singleton().setProp32("NRedRBPlus-PCIRevision",NRed::singleton().getPciRevision());
    if (getKernelVersion()!= KernelVersion::Tahoe || getKernelMinorVersion()!=4 ||
        NRed::singleton().getDeviceID()!=0x1638 || NRed::singleton().getPciRevision()!=0xc9 || PAGE_SIZE!=4096) {
        startupStatus(10);
        SYSLOG("NRedRB","rbplus-v3 refused unsupported kernel or device");
        return;
    }
    const auto build=patcher.solveSymbol(KernelPatcher::KernelID,"_osversion");
    if (!build || patcher.getError()!=KernelPatcher::Error::NoError) {
        patcher.clearError();
        startupStatus(11);
        SYSLOG("NRedRB","rbplus-v3 cannot resolve OS build; disabled");
        return;
    }
    osBuild=reinterpret_cast<const char *>(build);
    UInt32 buildPrefix[2];
    memcpy(buildPrefix,osBuild,sizeof buildPrefix);
    NRed::singleton().setProp32("NRedRBPlus-EarlyBuild0",buildPrefix[0]);
    NRed::singleton().setProp32("NRedRBPlus-EarlyBuild1",buildPrefix[1]);
    const auto kernelVersion=patcher.solveSymbol(KernelPatcher::KernelID,"_version");
    if (!kernelVersion || patcher.getError()!=KernelPatcher::Error::NoError) {
        patcher.clearError();
        startupStatus(16);
        SYSLOG("NRedRB","rbplus-v3 cannot resolve fixed kernel identity; disabled");
        return;
    }
    if (!rbSupportedKernel(reinterpret_cast<const char *>(kernelVersion))) {
        startupStatus(17);
        SYSLOG("NRedRB","rbplus-v3 refused fixed kernel identity mismatch");
        return;
    }
    const auto entry=patcher.solveSymbol(KernelPatcher::KernelID,"_cs_validate_page");
    if (!entry || patcher.getError()!=KernelPatcher::Error::NoError) {
        patcher.clearError();
        startupStatus(13);
        SYSLOG("NRedRB","rbplus-v3 cannot resolve validation callback; disabled");
        return;
    }
    // Fail closed if another plugin already routed this function or ABI differs.
    static const UInt8 prologue[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x53,
                                  0x48,0x83,0xec,0x18,0x4c,0x89,0xcb,0x4d,0x89,0xc6};
    UInt32 observedPrologue[4];
    memcpy(observedPrologue,reinterpret_cast<const void *>(entry),sizeof observedPrologue);
    NRed::singleton().setProp32("NRedRBPlus-Prologue0",observedPrologue[0]);
    NRed::singleton().setProp32("NRedRBPlus-Prologue1",observedPrologue[1]);
    NRed::singleton().setProp32("NRedRBPlus-Prologue2",observedPrologue[2]);
    NRed::singleton().setProp32("NRedRBPlus-Prologue3",observedPrologue[3]);
    if (memcmp(reinterpret_cast<const void *>(entry),prologue,sizeof prologue)) {
        startupStatus(14);
        SYSLOG("NRedRB","rbplus-v3 refused callback prologue mismatch or existing hook");
        return;
    }
    KernelPatcher::RouteRequest route("_cs_validate_page",validatePage,originalValidate);
    if (!patcher.routeMultipleLong(KernelPatcher::KernelID,&route,1)) {
        patcher.clearError();
        startupStatus(15);
        SYSLOG("NRedRB","rbplus-v3 failed to route validation callback");
        return;
    }
    startupStatus(100); // routing reported success, not proof of page delivery
    SYSLOG("NRedRB","rbplus-v3 armed for 25E253 / 1638:c9; waiting for exact Metal pages");
}
}
