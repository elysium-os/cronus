#include "arch/cpu.h"
#include "arch/page.h"
#include "arch/sched.h"
#include "arch/time.h"
#include "common/assert.h"
#include "common/lock/mutex.h"
#include "common/log.h"
#include "common/panic.h"
#include "dev/acpi.h"
#include "dev/pci.h"
#include "lib/math.h"
#include "lib/mem.h"
#include "lib/string.h"
#include "memory/heap.h"
#include "sys/time.h"

#include <uacpi/kernel_api.h>
#include <uacpi/platform/arch_helpers.h>
#include <uacpi/types.h>

// FIX using x86_64 in generic code
#include "x86_64/cpu/lapic.h"
#include "x86_64/cpu/port.h"
#include "x86_64/dev/ioapic.h"
#include "x86_64/interrupt.h"

typedef struct {
    uacpi_io_addr base;
    uacpi_size length;
} uacpi_io_range_t;

typedef struct {
    int vector;
    uacpi_interrupt_handler fn;
    uacpi_handle ctx;
} uacpi_interrupt_handler_t;


/*Convenience initialization / deinitialization hooks that will be called by *uACPI automatically when appropriate if compiled -
    in.*/
#ifdef UACPI_KERNEL_INITIALIZATION
/*
 * This API is invoked for each initialization level so that appropriate parts
 * of the host kernel and/or glue code can be initialized at different stages.
 *
 * uACPI API that triggers calls to uacpi_kernel_initialize and the respective
 * 'current_init_lvl' passed to the hook at that stage:
 * 1. uacpi_initialize() -> UACPI_INIT_LEVEL_EARLY
 * 2. uacpi_namespace_load() -> UACPI_INIT_LEVEL_SUBSYSTEM_INITIALIZED
 * 3. (start of) uacpi_namespace_initialize() -> UACPI_INIT_LEVEL_NAMESPACE_LOADED
 * 4. (end of) uacpi_namespace_initialize() -> UACPI_INIT_LEVEL_NAMESPACE_INITIALIZED
 */
uacpi_status uacpi_kernel_initialize(uacpi_init_level current_init_lvl);
void uacpi_kernel_deinitialize(void);
#endif

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address) {
    if(g_acpi_rsdp == 0) return UACPI_STATUS_NOT_FOUND;
    *out_rsdp_address = g_acpi_rsdp;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address address, uacpi_handle *out_handle) {
    pci_device_t *device = heap_alloc(sizeof(pci_device_t));
    device->segment = address.segment;
    device->bus = address.bus;
    device->slot = address.device;
    device->func = address.function;

    // TODO: this should not just create a device and push it,
    // it REALLY needs to first check if we are aware of such a device
    interrupt_state_t previous_state = spinlock_acquire_noint(&g_pci_devices_lock);
    list_push(&g_pci_devices, &device->list_node);
    spinlock_release_noint(&g_pci_devices_lock, previous_state);

    *out_handle = device;

    return UACPI_STATUS_OK;
}

void uacpi_kernel_pci_device_close([[maybe_unused]] uacpi_handle handle) { /* no-op */ }

uacpi_status uacpi_kernel_pci_read8(uacpi_handle handle, uacpi_size offset, uacpi_u8 *out_value) {
    pci_device_t *device = (pci_device_t *) handle;
    *out_value = pci_config_read_byte(device, offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read16(uacpi_handle handle, uacpi_size offset, uacpi_u16 *out_value) {
    pci_device_t *device = (pci_device_t *) handle;
    *out_value = pci_config_read_word(device, offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read32(uacpi_handle handle, uacpi_size offset, uacpi_u32 *out_value) {
    pci_device_t *device = (pci_device_t *) handle;
    *out_value = pci_config_read_double(device, offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(uacpi_handle handle, uacpi_size offset, uacpi_u8 in_value) {
    pci_device_t *device = (pci_device_t *) handle;
    pci_config_write_byte(device, offset, in_value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write16(uacpi_handle handle, uacpi_size offset, uacpi_u16 in_value) {
    pci_device_t *device = (pci_device_t *) handle;
    pci_config_write_word(device, offset, in_value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write32(uacpi_handle handle, uacpi_size offset, uacpi_u32 in_value) {
    pci_device_t *device = (pci_device_t *) handle;
    pci_config_write_double(device, offset, in_value);
    return UACPI_STATUS_OK;
}


/* System IO */
uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle) {
#ifdef __ARCH_X86_64
    if(base + len > 0xFFFF) return UACPI_STATUS_INVALID_ARGUMENT;
#endif
    uacpi_io_range_t *range = heap_alloc(sizeof(uacpi_io_range_t));
    range->base = base;
    range->length = len;
    *out_handle = range;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle handle) {
    heap_free(handle, sizeof(uacpi_io_range_t));
}

uacpi_status uacpi_kernel_io_read8(uacpi_handle handle, uacpi_size offset, uacpi_u8 *out_value) {
    uacpi_io_range_t *range = (uacpi_io_range_t *) handle;
    if(offset >= range->length) return UACPI_STATUS_INVALID_ARGUMENT;
    *out_value = x86_64_port_inb(range->base + offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read16(uacpi_handle handle, uacpi_size offset, uacpi_u16 *out_value) {
    uacpi_io_range_t *range = (uacpi_io_range_t *) handle;
    if(offset >= range->length) return UACPI_STATUS_INVALID_ARGUMENT;
    *out_value = x86_64_port_inw(range->base + offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read32(uacpi_handle handle, uacpi_size offset, uacpi_u32 *out_value) {
    uacpi_io_range_t *range = (uacpi_io_range_t *) handle;
    if(offset >= range->length) return UACPI_STATUS_INVALID_ARGUMENT;
    *out_value = x86_64_port_ind(range->base + offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(uacpi_handle handle, uacpi_size offset, uacpi_u8 in_value) {
    uacpi_io_range_t *range = (uacpi_io_range_t *) handle;
    if(offset >= range->length) return UACPI_STATUS_INVALID_ARGUMENT;
    x86_64_port_outb(range->base + offset, in_value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write16(uacpi_handle handle, uacpi_size offset, uacpi_u16 in_value) {
    uacpi_io_range_t *range = (uacpi_io_range_t *) handle;
    if(offset >= range->length) return UACPI_STATUS_INVALID_ARGUMENT;
    x86_64_port_outw(range->base + offset, in_value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write32(uacpi_handle handle, uacpi_size offset, uacpi_u32 in_value) {
    uacpi_io_range_t *range = (uacpi_io_range_t *) handle;
    if(offset >= range->length) return UACPI_STATUS_INVALID_ARGUMENT;
    x86_64_port_outd(range->base + offset, in_value);
    return UACPI_STATUS_OK;
}

/* Virtual Memory */
void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
    size_t offset = addr % PAGE_GRANULARITY;
    uintptr_t ret = (uintptr_t) vm_map_direct(g_vm_global_address_space, nullptr, MATH_CEIL(len + offset, PAGE_GRANULARITY), VM_PROT_RW, VM_CACHE_NONE, MATH_FLOOR(addr, PAGE_GRANULARITY), VM_FLAG_NONE);
    LOG_TRACE("UACPI", "MAP (%#lx, %#lx) <%#lx, %#lx>", ret, MATH_CEIL(len + offset, PAGE_GRANULARITY), addr, len);
    return (void *) (ret + offset);
}

void uacpi_kernel_unmap(void *addr, uacpi_size len) {
    LOG_TRACE("UACPI", "UNMAP (%#lx, %#lx) <%#lx, %#lx>", MATH_FLOOR((uintptr_t) addr, PAGE_GRANULARITY), MATH_CEIL(len + ((uintptr_t) addr % PAGE_GRANULARITY), ARCH_PAGE_GRANULARITY), (uintptr_t) addr, len);
    vm_unmap(g_vm_global_address_space, (void *) MATH_FLOOR((uintptr_t) addr, PAGE_GRANULARITY), MATH_CEIL(len + (uintptr_t) addr % PAGE_GRANULARITY, PAGE_GRANULARITY));
}

/* Heap */
void *uacpi_kernel_alloc(uacpi_size size) {
    return heap_alloc(size);
}

#ifdef UACPI_NATIVE_ALLOC_ZEROED
void *uacpi_kernel_alloc_zeroed(uacpi_size size) {
    void *ptr = uacpi_kernel_alloc(size);
    memclear(ptr, size);
    return ptr;
}
#endif

#ifndef UACPI_SIZED_FREES
#error UACPI_SIZED_FREES expected
#else
void uacpi_kernel_free(void *mem, uacpi_size size_hint) {
    heap_free(mem, size_hint);
}
#endif

/* Logging */
#ifndef UACPI_FORMATTED_LOGGING
#error UACPI_FORMATTED_LOGGING expected
#else
UACPI_PRINTF_DECL(2, 3)
void uacpi_kernel_log(uacpi_log_level level, const uacpi_char *fmt, ...) {
    va_list list;
    va_start(list, fmt);
    uacpi_kernel_vlog(level, fmt, list);
    va_end(list);
}

void uacpi_kernel_vlog(uacpi_log_level level, const uacpi_char *fmt, uacpi_va_list list) {
    log_level_t elysium_level = LOG_LEVEL_INFO;
    switch(level) {
        case UACPI_LOG_DEBUG: elysium_level = LOG_LEVEL_DEBUG; break;
        case UACPI_LOG_TRACE: elysium_level = LOG_LEVEL_DEBUG; break;
        case UACPI_LOG_INFO:  elysium_level = LOG_LEVEL_INFO; break;
        case UACPI_LOG_WARN:  elysium_level = LOG_LEVEL_WARN; break;
        case UACPI_LOG_ERROR: elysium_level = LOG_LEVEL_ERROR; break;
    }

    size_t len = string_length(fmt) + 1;
    char *new_fmt = heap_alloc(len);
    string_copy(new_fmt, fmt);
    new_fmt[len - 2] = ' ';
    log_list(elysium_level, "UACPI", new_fmt, list);
    heap_free(new_fmt, len);
}
#endif

/* Time */
uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot() {
    return (uacpi_u64) time_monotonic();
}

/* Mutexes*/
uacpi_handle uacpi_kernel_create_mutex() {
    mutex_t *mutex = heap_alloc(sizeof(mutex_t));
    *mutex = MUTEX_INIT;
    return mutex;
}

void uacpi_kernel_free_mutex(uacpi_handle handle) {
    heap_free(handle, sizeof(mutex_t));
}

/*
 * OPTIMIZE: try to match this behavior
 *
 * Try to acquire the mutex with a millisecond timeout.
 *
 * The timeout value has the following meanings:
 * 0x0000 - Attempt to acquire the mutex once, in a non-blocking manner
 * 0x0001...0xFFFE - Attempt to acquire the mutex for at least 'timeout'
 *                   milliseconds
 * 0xFFFF - Infinite wait, block until the mutex is acquired
 *
 * The following are possible return values:
 * 1. UACPI_STATUS_OK - successful acquire operation
 * 2. UACPI_STATUS_TIMEOUT - timeout reached while attempting to acquire (or the
 *                           single attempt to acquire was not successful for
 *                           calls with timeout=0)
 * 3. Any other value - signifies a host internal error and is treated as such
 */
uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle handle, [[maybe_unused]] uacpi_u16 timeout) {
    mutex_acquire((mutex_t *) handle);
    return UACPI_STATUS_OK;
}

void uacpi_kernel_release_mutex(uacpi_handle handle) {
    mutex_release((mutex_t *) handle);
}

/* Scheduling */
uacpi_thread_id uacpi_kernel_get_thread_id() {
    // return (uacpi_thread_id *) arch_sched_thread_current()->id;
    return (uacpi_thread_id *) 0; // TODO: use thread ids, but this is called before scheduling is initialized
}

/* Firmware Request */
uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request *request) {
    switch(request->type) {
        case UACPI_FIRMWARE_REQUEST_TYPE_FATAL:
            log(LOG_LEVEL_ERROR, "UACPI", "Fatal firmware request error");
            panic("UACPI", "Firmware code: %#lx, argument: %#lx", request->fatal.code, request->fatal.arg);
            break;
        default: break;
    }
    return UACPI_STATUS_OK;
}

/* Spinlocks */
uacpi_handle uacpi_kernel_create_spinlock() {
    spinlock_t *lock = heap_alloc(sizeof(spinlock_t));
    *lock = SPINLOCK_INIT;
    return lock;
}

void uacpi_kernel_free_spinlock(uacpi_handle lock) {
    heap_free(lock, sizeof(spinlock_t));
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle lock) {
    spinlock_acquire_nodw((spinlock_t *) lock);
    return 0;
}

void uacpi_kernel_unlock_spinlock(uacpi_handle lock, uacpi_cpu_flags) {
    spinlock_release_nodw((spinlock_t *) lock);
}

/* Events */
uacpi_handle uacpi_kernel_create_event() {
    size_t *counter = heap_alloc(sizeof(size_t));
    *counter = 0;
    return counter;
}

void uacpi_kernel_free_event(uacpi_handle handle) {
    heap_free(handle, sizeof(size_t));
}

// TODO: not 100% this is correct
uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle handle, uacpi_u16 timeout) {
    size_t *counter = (size_t *) handle;
    if(timeout == 0xFFFF) {
        while(*counter != 0) cpu_relax();
        return UACPI_TRUE;
    }

    time_t deadline = time_monotonic() + (timeout * (TIME_NANOSECONDS_IN_SECOND / TIME_MILLISECONDS_IN_SECOND));
    while(*counter != 0 && time_monotonic() < deadline) cpu_relax();
    if(*counter == 0) return UACPI_TRUE;
    __atomic_fetch_sub((size_t *) handle, 1, __ATOMIC_SEQ_CST);
    return UACPI_FALSE;
}

void uacpi_kernel_signal_event(uacpi_handle handle) {
    __atomic_fetch_add((size_t *) handle, 1, __ATOMIC_SEQ_CST);
}

void uacpi_kernel_reset_event(uacpi_handle handle) {
    __atomic_store_n((size_t *) handle, 0, __ATOMIC_SEQ_CST);
}

/* Interrupt Handling */
// CRITICAL: this doesnt even get registered
static uacpi_interrupt_handler_t g_interrupt_handlers[256] = {};

static void kernelapi_interrupt_handler(x86_64_interrupt_frame_t *frame) {
    uacpi_interrupt_handler_t *handler = &g_interrupt_handlers[frame->int_no];
    ASSERT(handler != nullptr);
    handler->fn(handler->ctx);
}

uacpi_status uacpi_kernel_install_interrupt_handler(uacpi_u32 irq, uacpi_interrupt_handler fn, uacpi_handle ctx, uacpi_handle *out_irq_handle) {
    uacpi_interrupt_handler_t *handler = heap_alloc(sizeof(uacpi_interrupt_handler_t));
    handler->ctx = ctx;
    handler->fn = fn;

    // TODO: this routine is unstable at best (besides it references x86 code anyway)
    int interrupt = x86_64_interrupt_request(INTERRUPT_PRIORITY_NORMAL, kernelapi_interrupt_handler);
    ASSERT(interrupt >= 0);
    handler->vector = interrupt;

    x86_64_ioapic_map_legacy_irq(irq, x86_64_lapic_id(), false, true, interrupt);
    // //

    *out_irq_handle = handler;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler([[maybe_unused]] uacpi_interrupt_handler fn, uacpi_handle irq_handle) {
    uacpi_interrupt_handler_t *handler = (uacpi_interrupt_handler_t *) irq_handle;
    x86_64_interrupt_set(handler->vector, nullptr);
    heap_free(handler, sizeof(uacpi_interrupt_handler_t));
    return UACPI_STATUS_OK;
}

/* Unimplemented */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

/*
 * Schedules deferred work for execution.
 * Might be invoked from an interrupt context.
 */
uacpi_status uacpi_kernel_schedule_work(uacpi_work_type, uacpi_work_handler, uacpi_handle ctx) { // UNIMPLEMENTED
    log(LOG_LEVEL_WARN, "UACPI", "Call to unimplemented fn `uacpi_kernel_schedule_work`");
    return UACPI_STATUS_UNIMPLEMENTED;
}

/*
 * Waits for two types of work to finish:
 * 1. All in-flight interrupts installed via uacpi_kernel_install_interrupt_handler
 * 2. All work scheduled via uacpi_kernel_schedule_work
 *
 * Note that the waits must be done in this order specifically.
 */
uacpi_status uacpi_kernel_wait_for_work_completion() { // UNIMPLEMENTED
    log(LOG_LEVEL_WARN, "UACPI", "Call to unimplemented fn `uacpi_kernel_wait_for_work_completion`");
    return UACPI_STATUS_UNIMPLEMENTED;
}

/*
 * Spin for N microseconds.
 */
void uacpi_kernel_stall(uacpi_u8 usec) { // UNIMPLEMENTED
    log(LOG_LEVEL_WARN, "UACPI", "Call to unimplemented fn `uacpi_kernel_stall`");
}

/*
 * Sleep for N milliseconds.
 */
void uacpi_kernel_sleep(uacpi_u64 msec) { // UNIMPLEMENTED
    log(LOG_LEVEL_WARN, "UACPI", "Call to unimplemented fn `uacpi_kernel_sleep`");
}

#pragma GCC diagnostic pop
