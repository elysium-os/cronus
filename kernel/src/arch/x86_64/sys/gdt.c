#include "gdt.h"

#include "common/log.h"

#define ACCESS_PRESENT (1 << 7)
#define ACCESS_DPL(DPL) (((DPL) & 0b11) << 5)
#define ACCESS_TYPE_TSS (9)
#define ACCESS_TYPE_CODE(CONFORM, READ) ((1 << 4) | (1 << 3) | ((CONFORM) << 2) | ((READ) << 1))
#define ACCESS_TYPE_DATA(DIRECTION, WRITE) ((1 << 4) | ((DIRECTION) << 2) | ((WRITE) << 1))
#define ACCESS_ACCESSED (1 << 0)

#define FLAG_GRANULARITY (1 << 7)
#define FLAG_DB (1 << 6)
#define FLAG_LONG (1 << 5)
#define FLAG_SYSTEM_AVL (1 << 4)

typedef struct {
    uint16_t limit;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t flags;
    uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    gdt_entry_t entry;
    uint32_t base_ext;
    uint8_t rsv0;
    uint8_t zero_rsv1;
    uint16_t rsv2;
} __attribute__((packed)) gdt_system_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdt_descriptor_t;

void gdt_load(gdt_descriptor_t *gdtr, uint64_t selector_code, uint64_t selector_data);

// clang-format off
static gdt_entry_t g_gdt[] = {
    {},
    {
        .limit = 0,
        .base_low = 0,
        .base_mid = 0,
        .access = ACCESS_PRESENT | ACCESS_DPL(0) | ACCESS_TYPE_CODE(0, 1),
        .flags = FLAG_LONG,
        .base_high = 0
    },
    {
        .limit = 0,
        .base_low = 0,
        .base_mid = 0,
        .access = ACCESS_PRESENT | ACCESS_DPL(0) | ACCESS_TYPE_DATA(0, 1),
        .flags = 0,
        .base_high = 0
    },
    {
        .limit = 0,
        .base_low = 0,
        .base_mid = 0,
        .access = ACCESS_PRESENT | ACCESS_DPL(3) | ACCESS_TYPE_DATA(0, 1),
        .flags = 0,
        .base_high = 0
    },
    {
        .limit = 0,
        .base_low = 0,
        .base_mid = 0,
        .access = ACCESS_PRESENT | ACCESS_DPL(3) | ACCESS_TYPE_CODE(0, 1),
        .flags = FLAG_LONG,
        .base_high = 0
    },
    {}, {} // TSS
};
// clang-format on

void x86_64_gdt_load() {
    gdt_descriptor_t gdtr;
    gdtr.limit = sizeof(g_gdt) - 1;
    gdtr.base = (uint64_t) &g_gdt;
    log(LOG_LEVEL_DEBUG, "GDT", "Loading GDT(%#lx, %#lx)", gdtr.base, (uint64_t) gdtr.limit);
    gdt_load(&gdtr, X86_64_GDT_SELECTOR_CODE64_RING0, X86_64_GDT_SELECTOR_DATA64_RING0);
}

void x86_64_gdt_load_tss(x86_64_tss_t *tss) {
    uint16_t tss_segment = sizeof(g_gdt) - 16;

    gdt_system_entry_t *entry = (gdt_system_entry_t *) ((uintptr_t) g_gdt + tss_segment);
    entry->entry.access = ACCESS_PRESENT | ACCESS_TYPE_TSS;
    entry->entry.flags = FLAG_SYSTEM_AVL | ((sizeof(x86_64_tss_t) >> 16) & 0b1111);
    entry->entry.limit = (uint16_t) sizeof(x86_64_tss_t);
    entry->entry.base_low = (uint16_t) (uint64_t) tss;
    entry->entry.base_mid = (uint8_t) ((uint64_t) tss >> 16);
    entry->entry.base_high = (uint8_t) ((uint64_t) tss >> 24);
    entry->base_ext = (uint32_t) ((uint64_t) tss >> 32);

    asm volatile("ltr %0" : : "m"(tss_segment));
}
