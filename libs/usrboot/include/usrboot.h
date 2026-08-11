#pragma once

#include <stddef.h>
#include <stdint.h>

typedef uint64_t ub_u64;
typedef uint64_t ub_addr64;
typedef uint64_t ub_off64;
typedef uint64_t ub_sz64;

#define USRBOOT_MAGIC UINT64_C(0x5f544f4f42525355)

struct usrboot_segment {
    ub_addr64 vaddr;
    ub_off64 off;
    ub_sz64 filesz;
    ub_sz64 memsz;
};

struct usrboot_header {
    ub_u64 magic;
    ub_sz64 body_size;
    ub_addr64 entry;
    struct usrboot_segment seg_rx;
    struct usrboot_segment seg_rw;
    struct usrboot_segment seg_ro;
};

#if defined(__cplusplus)
#define USRBOOT_STATIC_ASSERT(condition, message) static_assert(condition, message)
#else
#define USRBOOT_STATIC_ASSERT(condition, message) _Static_assert(condition, message)
#endif

USRBOOT_STATIC_ASSERT(sizeof(ub_u64) == 8, "usrboot integers must be 64-bit");
USRBOOT_STATIC_ASSERT(sizeof(struct usrboot_segment) == 32, "usrboot segment layout changed");
USRBOOT_STATIC_ASSERT(offsetof(struct usrboot_segment, vaddr) == 0,
                      "usrboot segment vaddr offset changed");
USRBOOT_STATIC_ASSERT(offsetof(struct usrboot_segment, off) == 8,
                      "usrboot segment off offset changed");
USRBOOT_STATIC_ASSERT(offsetof(struct usrboot_segment, filesz) == 16,
                      "usrboot segment filesz offset changed");
USRBOOT_STATIC_ASSERT(offsetof(struct usrboot_segment, memsz) == 24,
                      "usrboot segment memsz offset changed");

USRBOOT_STATIC_ASSERT(sizeof(struct usrboot_header) == 120, "usrboot header layout changed");
USRBOOT_STATIC_ASSERT(offsetof(struct usrboot_header, magic) == 0,
                      "usrboot header magic offset changed");
USRBOOT_STATIC_ASSERT(offsetof(struct usrboot_header, body_size) == 8,
                      "usrboot header body_size offset changed");
USRBOOT_STATIC_ASSERT(offsetof(struct usrboot_header, entry) == 16,
                      "usrboot header entry offset changed");
USRBOOT_STATIC_ASSERT(offsetof(struct usrboot_header, seg_rx) == 24,
                      "usrboot header RX offset changed");
USRBOOT_STATIC_ASSERT(offsetof(struct usrboot_header, seg_rw) == 56,
                      "usrboot header RW offset changed");
USRBOOT_STATIC_ASSERT(offsetof(struct usrboot_header, seg_ro) == 88,
                      "usrboot header RO offset changed");

#undef USRBOOT_STATIC_ASSERT
