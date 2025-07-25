#pragma once

#include "common/lock/spinlock.h"
#include "lib/list.h"
#include "lib/rb.h"

#include <stddef.h>
#include <stdint.h>

#define VM_FLAG_NONE 0
#define VM_FLAG_FIXED (1 << 1)
#define VM_FLAG_DYNAMICALLY_BACKED (1 << 2)
#define VM_FLAG_ZERO (1 << 10) /* only applies to anonymous mappings */

#define VM_PROT_NONE ((vm_protection_t) {})
#define VM_PROT_RW ((vm_protection_t) { .read = true, .write = true })
#define VM_PROT_RX ((vm_protection_t) { .read = true, .exec = true })
#define VM_PROT_RWX ((vm_protection_t) { .read = true, .write = true, .exec = true })

typedef struct {
    bool read  : 1;
    bool write : 1;
    bool exec  : 1;
} vm_protection_t;

typedef enum {
    VM_PRIVILEGE_KERNEL,
    VM_PRIVILEGE_USER
} vm_privilege_t;

typedef enum {
    VM_CACHE_STANDARD,
    VM_CACHE_WRITE_COMBINE,
    VM_CACHE_NONE
} vm_cache_t;

typedef enum {
    VM_FAULT_UNKNOWN,
    VM_FAULT_NOT_PRESENT
} vm_fault_t;

typedef enum {
    VM_REGION_TYPE_ANON,
    VM_REGION_TYPE_DIRECT
} vm_region_type_t;

typedef uint64_t vm_flags_t;

typedef struct {
    spinlock_t lock;
    rb_tree_t regions;
    uintptr_t start, end;
} vm_address_space_t;

typedef struct {
    vm_address_space_t *address_space;

    uintptr_t base;
    size_t length;

    vm_region_type_t type;
    vm_protection_t protection;
    vm_cache_t cache_behavior;

    bool dynamically_backed : 1;

    rb_node_t rb_node; /* Used for regions list */
    list_node_t list_node; /* Used for region cache */

    union {
        struct {
            bool back_zeroed;
        } anon;
        struct {
            uintptr_t physical_address;
        } direct;
    } type_data;
} vm_region_t;

extern vm_address_space_t *g_vm_global_address_space;

/// Map a region of anonymous memory.
/// @param hint Page aligned address
/// @param length Page aligned length
void *vm_map_anon(vm_address_space_t *address_space, void *hint, size_t length, vm_protection_t prot, vm_cache_t cache, vm_flags_t flags);

/// Map a region of direct memory.
/// @param hint Page aligned address
/// @param length Page aligned length
void *vm_map_direct(vm_address_space_t *address_space, void *hint, size_t length, vm_protection_t prot, vm_cache_t cache, uintptr_t physical_address, vm_flags_t flags);

/// Unmap a region of memory.
/// @param address Page aligned address
/// @param length Page aligned length
void vm_unmap(vm_address_space_t *address_space, void *address, size_t length);

/// Rewrite protection of a region of memory.
void vm_rewrite_prot(vm_address_space_t *address_space, void *address, size_t length, vm_protection_t prot);

/// Rewrite cacheability of a region of memory.
void vm_rewrite_cache(vm_address_space_t *address_space, void *address, size_t length, vm_cache_t cache);

/// Handle a virtual memory fault
/// @param fault Cause of the fault
/// @returns Is fault handled
bool vm_fault(uintptr_t address, vm_fault_t fault);

/// Copy data to another address space.
size_t vm_copy_to(vm_address_space_t *dest_as, uintptr_t dest_addr, void *src, size_t count);

/// Copy data from another address space.
size_t vm_copy_from(void *dest, vm_address_space_t *src_as, uintptr_t src_addr, size_t count);

/// Create a regions rbtree.
rb_tree_t vm_create_regions();
