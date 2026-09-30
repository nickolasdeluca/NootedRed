// Pure matcher shared by the CPU tests, process-local experiment, and kext.
// Caller must supply a validated 4096-byte executable page from the exact cache.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "rbplus-payload.h"

enum RBPageResult { RBPageMismatch, RBPageOriginal, RBPageAlreadyPatched };
static inline enum RBPageResult rbClassifyPage(const void *data, size_t size,
                                               uint64_t offset) {
    if (!data || size!=4096) return RBPageMismatch;
    const unsigned char *original, *replacement;
    size_t within, length;
    if (offset==RB_FULL_PAGE_OFFSET) {
        original=rbFullPage; replacement=rbFullReplacement;
        within=RB_FULL_WITHIN; length=sizeof rbFullReplacement;
    } else if (offset==RB_DELTA_PAGE_OFFSET) {
        original=rbDeltaPage; replacement=rbDeltaReplacement;
        within=RB_DELTA_WITHIN; length=sizeof rbDeltaReplacement;
    } else return RBPageMismatch;
    const unsigned char *bytes=(const unsigned char *)data;
    if (memcmp(bytes,original,within) ||
        memcmp(bytes+within+length,original+within+length,size-within-length))
        return RBPageMismatch;
    if (!memcmp(bytes+within,original+within,length)) return RBPageOriginal;
    if (!memcmp(bytes+within,replacement,length)) return RBPageAlreadyPatched;
    return RBPageMismatch;
}
static inline enum RBPageResult rbPatchPage(void *data, size_t size, uint64_t offset) {
    enum RBPageResult result=rbClassifyPage(data,size,offset);
    if (result==RBPageOriginal) {
        if (offset==RB_FULL_PAGE_OFFSET)
            memcpy((unsigned char *)data+RB_FULL_WITHIN,rbFullReplacement,sizeof rbFullReplacement);
        else
            memcpy((unsigned char *)data+RB_DELTA_WITHIN,rbDeltaReplacement,sizeof rbDeltaReplacement);
    }
    return result;
}
