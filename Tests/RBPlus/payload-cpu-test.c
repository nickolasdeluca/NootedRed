// Executes command emitters on synthetic CPU buffers. No Metal device or GPU submit.
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <mach-o/dyld.h>
#include <mach/mach.h>
#include <dlfcn.h>
typedef uint32_t *(*BlendEmitFn)(void *, uint32_t *, uint32_t *);
typedef uint32_t *(*BlendDeltaFn)(void *, uint32_t *, uint32_t *, uintptr_t, uint32_t *);
#include "rbplus-page-patch.h"

enum { StateWords = 0x144/4, BufferWords = 512, Cases = 20000 };
typedef struct {
    uint32_t ctx[64], state[StateWords], old[StateWords];
    uint32_t full[BufferWords], delta[BufferWords], oldAfter[StateWords];
    size_t fullWords, deltaWords;
    unsigned count;
} Case;
static unsigned seed = 0x5600;
static uint32_t next(void) { seed ^= seed<<13; seed ^= seed>>17; return seed ^= seed<<5; }
static uint32_t effective(const uint32_t *s) {
    uint32_t b = s[2], cc = s[10];
    if (b & (1u<<30)) {
        unsigned count = (b & (1u<<29)) ? 4 : 2;
        for (unsigned i=0; i<count; i++) {
            unsigned factor = (b >> (8*i)) & 31;
            if (factor >= 15 && factor <= 18) cc |= 1;
        }
    }
    return cc;
}
static size_t withoutColor(const uint32_t *src, size_t count, uint32_t *dst,
                           const uint32_t *state, bool patched, bool *seen) {
    size_t used=0;
    *seen=false;
    for (size_t i=0;i<count;) {
        uint32_t h=src[i];
        assert((h>>30)==3);
        size_t n=((h>>16)&0x3fff)+2;
        assert(n <= count-i);
        if (((h>>8)&255)==0x69 && src[i+1]==0x201) {
            assert(n==5 && !*seen);
            *seen=true;
            assert(src[i+2]==state[0x34/4] && src[i+4]==state[0x38/4]);
            assert(src[i+3]==(patched ? effective(state) : state[0x28/4]));
        } else {
            memcpy(dst+used,src+i,n*4); used+=n;
        }
        i+=n;
    }
    return used;
}
static void run(Case *c, BlendEmitFn full, BlendDeltaFn delta, bool compare, unsigned index) {
    uint32_t state[StateWords], old[StateWords], out[BufferWords], a[BufferWords], b[BufferWords];
    for (unsigned kind=0;kind<2;kind++) {
        memcpy(state,c->state,sizeof state); memcpy(old,c->old,sizeof old);
        for (unsigned i=0;i<BufferWords;i++) out[i]=0xdeadbeef;
        uint32_t *end = kind ? delta(c->ctx,state,old,c->count,out) : full(c->ctx,state,out);
        assert(end>=out && end<=out+BufferWords);
        size_t n=end-out;
        for (size_t i=n;i<BufferWords;i++) assert(out[i]==0xdeadbeef);
        assert(!memcmp(state,c->state,sizeof state));
        if (!compare) {
            memcpy(kind ? c->delta : c->full,out,sizeof out);
            if (kind) { c->deltaWords=n; memcpy(c->oldAfter,old,sizeof old); }
            else c->fullWords=n;
        } else {
            if (kind) assert(!memcmp(old,c->oldAfter,sizeof old));
            bool before, after;
            size_t an=withoutColor(kind ? c->delta : c->full,
                                  kind ? c->deltaWords : c->fullWords,a,state,false,&before);
            size_t bn=withoutColor(out,n,b,state,true,&after);
            if (an!=bn || memcmp(a,b,an*4)) {
                fprintf(stderr,"packet mismatch case=%u kind=%u original=%zu patched=%zu\n",index,kind,an,bn);
                abort();
            }
            bool needs = !kind || c->state[2]!=c->old[2] || before;
            assert(after==needs);
            if (kind && !after) assert(effective(state)==effective(c->old));
            // The only length change is an already-supported five-word packet.
            // Full and delta have distinct worst-case sizes in Apple's code.
            assert(n==(kind ? c->deltaWords : c->fullWords) + ((after && !before) ? 5 : 0));
        }
    }
}
static void install(unsigned char *p, const unsigned char *original,
                    const unsigned char *replacement, size_t n) {
    assert(!memcmp(p,original,n));
    uintptr_t page=(uintptr_t)p & ~(uintptr_t)4095;
    assert(((uintptr_t)p&4095)+n<=4096);
    kern_return_t kr=vm_protect(mach_task_self(),page,4096,false,VM_PROT_READ|VM_PROT_WRITE|VM_PROT_COPY);
    assert(kr==KERN_SUCCESS);
    memcpy(p,replacement,n);
    kr=vm_protect(mach_task_self(),page,4096,false,VM_PROT_READ|VM_PROT_EXECUTE);
    assert(kr==KERN_SUCCESS);
}
static void testPageMatcher(void) {
    unsigned char bytes[4096], expected[4096], wrong[4096];
    for (unsigned i=0;i<2;i++) {
        uint64_t offset=i ? RB_DELTA_PAGE_OFFSET : RB_FULL_PAGE_OFFSET;
        const unsigned char *page=i ? rbDeltaPage : rbFullPage;
        memcpy(bytes,page,sizeof bytes);
        assert(rbClassifyPage(bytes,sizeof bytes,offset)==RBPageOriginal);
        assert(rbPatchPage(bytes,sizeof bytes,offset)==RBPageOriginal);
        memcpy(expected,bytes,sizeof bytes);
        assert(rbPatchPage(bytes,sizeof bytes,offset)==RBPageAlreadyPatched);
        assert(!memcmp(bytes,expected,sizeof bytes));
        assert(rbPatchPage(bytes,4095,offset)==RBPageMismatch);
        assert(rbPatchPage(bytes,4097,offset)==RBPageMismatch);
        assert(rbPatchPage(bytes,4096,offset+4096)==RBPageMismatch);
        assert(rbPatchPage(NULL,4096,offset)==RBPageMismatch);
        for (unsigned state=0;state<2;state++) {
            const unsigned char *source=state ? expected : page;
            for (unsigned j=0;j<4096;j++) {
                memcpy(bytes,source,4096); bytes[j]^=0x80;
                memcpy(wrong,bytes,4096);
                assert(rbPatchPage(bytes,4096,offset)==RBPageMismatch);
                assert(!memcmp(bytes,wrong,4096));
            }
        }
    }
    puts("PASS: exact-page matching, 16384 single-byte corruptions, wrong offsets/sizes, idempotence");
}
int main(void) {
    testPageMatcher();
    const char *path="/System/Library/Extensions/AMDRadeonX5000MTLDriver.bundle/Contents/MacOS/AMDRadeonX5000MTLDriver";
    assert(dlopen(path,RTLD_LAZY|RTLD_LOCAL));
    unsigned char *base=NULL;
    for (uint32_t i=0;i<_dyld_image_count();i++)
        if (!strcmp(_dyld_get_image_name(i),path)) base=(void*)_dyld_get_image_header(i);
    assert(base);
    // The installed kext may have patched these pages already. Restore only
    // this process's private copies for the original-versus-patched CPU oracle.
    enum RBPageResult f=rbClassifyPage(base+RB_FULL_IMAGE_OFFSET-RB_FULL_WITHIN,4096,RB_FULL_PAGE_OFFSET);
    enum RBPageResult d=rbClassifyPage(base+RB_DELTA_IMAGE_OFFSET-RB_DELTA_WITHIN,4096,RB_DELTA_PAGE_OFFSET);
    assert(f!=RBPageMismatch && d!=RBPageMismatch);
    if (f==RBPageAlreadyPatched)
        install(base+RB_FULL_IMAGE_OFFSET,rbFullReplacement,rbFullOriginal,sizeof rbFullOriginal);
    if (d==RBPageAlreadyPatched)
        install(base+RB_DELTA_IMAGE_OFFSET,rbDeltaReplacement,rbDeltaOriginal,sizeof rbDeltaOriginal);
    BlendEmitFn full=(void*)(base+RB_FULL_IMAGE_OFFSET);
    BlendDeltaFn delta=(void*)(base+RB_DELTA_IMAGE_OFFSET);
    Case *cases=calloc(Cases,sizeof(Case)); assert(cases);
    for (unsigned i=0;i<Cases;i++) {
        Case *c=&cases[i];
        c->ctx[0xa0/4]=(i&1)*2; c->ctx[0xb0/4]=((i>>1)&1)*2;
        c->count=i%33;
        for (unsigned j=0;j<StateWords;j++) c->state[j]=next();
        // Systematically cover each five-bit factor field and both enable bits.
        unsigned field=(i/32)%4, factor=i%32;
        c->state[2] &= ~(31u<<(field*8));
        c->state[2] |= factor<<(field*8);
        c->state[2] = (c->state[2]&~0x60000000u) | (((i/128)%4)<<29);
        memcpy(c->old,c->state,sizeof c->old);
        switch ((i/512)%5) {
            case 0: break;
            case 1: c->old[2]^=0x00001f1f; break; // only blend changes
            case 2: c->old[10]^=0x100; break; // only color control changes
            case 3: c->old[2]^=0x60000000; break; // enable changes
            case 4: for(unsigned j=0;j<StateWords;j++) c->old[j]=next(); break;
        }
        run(c,full,delta,false,i);
    }
    install(base+RB_FULL_IMAGE_OFFSET,rbFullOriginal,rbFullReplacement,sizeof rbFullReplacement);
    install(base+RB_DELTA_IMAGE_OFFSET,rbDeltaOriginal,rbDeltaReplacement,sizeof rbDeltaReplacement);
    for (unsigned i=0;i<Cases;i++) run(&cases[i],full,delta,true,i);
    printf("PASS: %u synthetic states, full + delta, original versus patched; no GPU work\n",Cases);
    free(cases);
}
