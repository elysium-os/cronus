#include "heap.h"

#include "common/assert.h"
#include "common/log.h"
#include "common/panic.h"
#include "common/spinlock.h"
#include "lib/list.h"
#include "memory/pmm.h"

#define INITIAL_SIZE_PAGES 10
#define MIN_ENTRY_SIZE 8
#if __ENV_DEVELOPMENT
#define HEAP_PROTECTION true
#endif

typedef struct {
#if HEAP_PROTECTION
    uint64_t prot;
#endif
    size_t size;
    bool free;
    list_element_t list_elem;
} heap_entry_t;

static spinlock_t g_lock = SPINLOCK_INIT;
static list_t g_entries = LIST_INIT_CIRCULAR(g_entries);

#if HEAP_PROTECTION
static void update_prot(heap_entry_t *entry) {
    entry->prot = ~((uintptr_t) entry + entry->size);
}

static uint64_t get_prot(heap_entry_t *entry) {
    return (uint64_t) (uintptr_t) entry + entry->size + entry->prot + 1;
}
#endif

void heap_initialize(vm_address_space_t *address_space, size_t size) {
    void *addr = vm_map_anon(address_space, NULL, size, (vm_protection_t) {.read = true, .write = true}, VM_CACHE_STANDARD, VM_FLAG_NONE);
    log(LOG_LEVEL_DEBUG, "HEAP", "Initialized at address %#lx with size %#lx", (uintptr_t) addr, size);
    ASSERT(addr != NULL);

    heap_entry_t *entry = (heap_entry_t *) addr;
    entry->size = size - sizeof(heap_entry_t);
    entry->free = true;
#if HEAP_PROTECTION
    update_prot(entry);
#endif
    list_prepend(&g_entries, &entry->list_elem);
}

void *heap_alloc_align(size_t size, size_t alignment) {
    ASSERT(size > 0);
    log(LOG_LEVEL_DEBUG, "HEAP", "alloc(size: %#lx, alignment: %#lx)", size, alignment);
    ipl_t previous_ipl = spinlock_acquire(&g_lock);
    LIST_FOREACH(&g_entries, elem) {
        heap_entry_t *entry = LIST_CONTAINER_GET(elem, heap_entry_t, list_elem);
        if(!entry->free) continue;
#if HEAP_PROTECTION
        ASSERT(get_prot(entry) == 0);
#endif

        uintptr_t mod = ((uintptr_t) entry + sizeof(heap_entry_t)) % alignment;
        uintptr_t offset = alignment - mod;
        if(mod == 0) offset = 0;

    check_fit:
        if(entry->size < offset + size) continue;
        if(offset == 0) goto aligned;
        if(offset < sizeof(heap_entry_t) + MIN_ENTRY_SIZE) {
            offset += alignment;
            goto check_fit;
        }

        heap_entry_t *new = (heap_entry_t *) ((uintptr_t) entry + offset);
        new->free = true;
        new->size = entry->size - offset;
        list_append(&entry->list_elem, &new->list_elem);
        entry->size = offset - sizeof(heap_entry_t);
#if HEAP_PROTECTION
        update_prot(new);
        update_prot(entry);
#endif
        entry = new;

    aligned:
        entry->free = false;
        if(entry->size - size > sizeof(heap_entry_t) + MIN_ENTRY_SIZE) {
            heap_entry_t *overflow = (heap_entry_t *) ((uintptr_t) entry + sizeof(heap_entry_t) + size);
            overflow->free = true;
            overflow->size = entry->size - size - sizeof(heap_entry_t);
            list_append(&entry->list_elem, &overflow->list_elem);
            entry->size = size;
#if HEAP_PROTECTION
            update_prot(overflow);
            update_prot(entry);
#endif
        }

        spinlock_release(&g_lock, previous_ipl);

        size_t address = (uintptr_t) entry + sizeof(heap_entry_t);
        log(LOG_LEVEL_DEBUG, "HEAP", "alloc success (address: %#lx)", address);
        return (void *) address;
    }
    panic("HEAP: Out of memory");
}

void *heap_alloc(size_t size) {
    return heap_alloc_align(size, 1);
}

void heap_free(void *address) {
    if(address == NULL) return;
    ipl_t previous_ipl = spinlock_acquire(&g_lock);
    heap_entry_t *entry = (heap_entry_t *) (address - sizeof(heap_entry_t));
#if HEAP_PROTECTION
    ASSERT(get_prot(entry) == 0);
#endif
    entry->free = true;
    if(LIST_NEXT(&entry->list_elem) != &g_entries) {
        heap_entry_t *next = LIST_CONTAINER_GET(LIST_NEXT(&entry->list_elem), heap_entry_t, list_elem);
        if(next->free) {
            entry->size += sizeof(heap_entry_t) + next->size;
            list_delete(&next->list_elem);
#if HEAP_PROTECTION
            update_prot(entry);
#endif
        }
    }
    if(LIST_PREVIOUS(&entry->list_elem) != &g_entries) {
        heap_entry_t *prev = LIST_CONTAINER_GET(LIST_PREVIOUS(&entry->list_elem), heap_entry_t, list_elem);
        if(prev->free) {
            prev->size += sizeof(heap_entry_t) + entry->size;
            list_delete(&entry->list_elem);
#if HEAP_PROTECTION
            update_prot(prev);
#endif
        }
    }
    spinlock_release(&g_lock, previous_ipl);
}
