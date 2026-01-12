#include "x86_64/profiler.h"

#include "arch/cpu.h"
#include "common/assert.h"
#include "common/log.h"
#include "common/panic.h"
#include "memory/heap.h"
#include "sys/kernel_symbol.h"

#ifdef __ENV_DEBUG

static bool g_profiler_enabled = false; // TODO: remove when is_threaded exists

static rb_value_t record_value(rb_node_t *node) {
    return (rb_value_t) CONTAINER_OF(node, x86_64_profiler_record_t, rb_node)->function;
}

[[gnu::no_instrument_function]] static bool pred(x86_64_profiler_record_t *a, x86_64_profiler_record_t *b) {
    return a->total_time < b->total_time;
}

[[gnu::no_instrument_function]] void x86_64_profiler_start() {
    x86_64_thread_t *thread = ARCH_CPU_CURRENT_THREAD();
    thread->profiler.active = true;
    thread->profiler.in_profiler = false;
    thread->profiler.current_frame = 0;
    __atomic_store_n(&g_profiler_enabled, 1, __ATOMIC_RELEASE);
}

[[gnu::no_instrument_function]] void x86_64_profiler_stop() {
    ARCH_CPU_CURRENT_THREAD()->profiler.active = false;
}

[[gnu::no_instrument_function]] void x86_64_profiler_reset() {
    x86_64_thread_t *thread = ARCH_CPU_CURRENT_THREAD();
    while(true) {
        rb_node_t *node = rb_search(&thread->profiler.records, 0, RB_SEARCH_TYPE_NEAREST_GTE);
        if(node == nullptr) break;
        rb_remove(&thread->profiler.records, node);
        heap_free(CONTAINER_OF(node, x86_64_profiler_record_t, rb_node), sizeof(x86_64_profiler_record_t));
    }
    thread->profiler.records = x86_64_profiler_records();
}

[[gnu::no_instrument_function]] void x86_64_profiler_print(const char *name) {
    x86_64_thread_t *thread = ARCH_CPU_CURRENT_THREAD();

    size_t record_count = thread->profiler.records.count;
    x86_64_profiler_record_t **records = heap_alloc(sizeof(x86_64_profiler_record_t *) * record_count);

    rb_node_t *node = rb_search(&thread->profiler.records, 0, RB_SEARCH_TYPE_NEAREST_GTE);
    size_t counter = 0;
    if(node != nullptr) {
        do {
            x86_64_profiler_record_t *record = CONTAINER_OF(node, x86_64_profiler_record_t, rb_node);
            ASSERT(counter <= record_count);
            records[counter++] = record;
            node = rb_search(&thread->profiler.records, (rb_value_t) record->function, RB_SEARCH_TYPE_NEAREST_GT);
        } while(node != nullptr);
    }
    ASSERT(counter == record_count);

    for(size_t i = 1; i < record_count; i++) {
        for(size_t j = i; j > 0 && pred(records[j - 1], records[j]); j--) {
            x86_64_profiler_record_t *temp = records[j - 1];
            records[j - 1] = records[j];
            records[j] = temp;
        }
    }

    log(LOG_LEVEL_DEBUG, "PROFILER", "Profiler results for `%s`, thread id %lu (%lu records):", name, thread->common.id, record_count);
    log(LOG_LEVEL_DEBUG, "PROFILER", "#   | Total Time   | Average Time | Calls        | Function Name");
    log(LOG_LEVEL_DEBUG, "PROFILER", "----|--------------|--------------|--------------|--------------");
    for(size_t i = 0; i < record_count; i++) {
        kernel_symbol_t symbol;

        const char *fn_name;
        if(kernel_symbol_lookup_by_address((uintptr_t) records[i]->function, &symbol) && symbol.address == (uintptr_t) records[i]->function) {
            fn_name = symbol.name;
        } else {
            fn_name = "unknown";
        }
        log(LOG_LEVEL_DEBUG, "PROFILER", "%-3lu | %-12lu | %-12lu | %-12lu | %s", i, records[i]->total_time, records[i]->total_time / records[i]->calls, records[i]->calls, fn_name);
    }
    heap_free(records, sizeof(x86_64_profiler_record_t *) * record_count);
}

[[gnu::no_instrument_function]] [[clang::no_sanitize("undefined")]] void __cyg_profile_func_enter(void *function, void *call_site) {
    uint64_t start = __builtin_ia32_rdtsc();
    if(!g_profiler_enabled) return;

    x86_64_thread_t *thread = ARCH_CPU_CURRENT_THREAD();
    if(!thread->profiler.active) return;
    if(thread->in_interrupt_handler || thread->profiler.in_profiler) return;

    thread->profiler.in_profiler = true;

    size_t index = thread->profiler.current_frame++;
    if(index >= X86_64_PROFILER_FRAMES) panic("PROFILER", "Too many frames (function: %#lx, call_site: %#lx)", function, call_site);

    x86_64_profiler_frame_t *frame = &thread->profiler.frames[index];
    frame->function = function;
    frame->call_site = call_site;
    frame->profiler_time = 0;

    uint64_t profiler_time = __builtin_ia32_rdtsc() - start;
    for(size_t i = 0; i < index; i++) thread->profiler.frames[i].profiler_time += profiler_time;

    frame->start = __builtin_ia32_rdtsc();
    thread->profiler.in_profiler = false;
}

[[gnu::no_instrument_function]] [[clang::no_sanitize("undefined")]] void __cyg_profile_func_exit(void *function, void *call_site) {
    uint64_t start = __builtin_ia32_rdtsc();
    if(!g_profiler_enabled) return;

    x86_64_thread_t *thread = ARCH_CPU_CURRENT_THREAD();
    if(!thread->profiler.active) return;
    if(thread->in_interrupt_handler || thread->profiler.in_profiler) return;

    thread->profiler.in_profiler = true;

    size_t index = --thread->profiler.current_frame;
    x86_64_profiler_frame_t *frame = &thread->profiler.frames[index];
    uint64_t time = start - frame->start - frame->profiler_time;

    ASSERT_COMMENT(frame->function == function, "frame mismatch");
    ASSERT_COMMENT(frame->call_site == call_site, "frame mismatch");

    x86_64_profiler_record_t *record;
    rb_node_t *node = rb_search(&thread->profiler.records, (rb_value_t) function, RB_SEARCH_TYPE_EXACT);
    if(node != nullptr) {
        record = CONTAINER_OF(node, x86_64_profiler_record_t, rb_node);
    } else {
        record = heap_alloc(sizeof(x86_64_profiler_record_t));
        record->function = function;
        record->calls = 0;
        record->total_time = 0;
        rb_insert(&thread->profiler.records, &record->rb_node);
    }

    record->total_time += time;
    record->calls++;

    uint64_t profiler_time = __builtin_ia32_rdtsc() - start;
    for(size_t i = 0; i < index; i++) thread->profiler.frames[i].profiler_time += profiler_time;

    thread->profiler.in_profiler = false;
}

rb_tree_t x86_64_profiler_records() {
    return RB_TREE_INIT(record_value);
}

#endif
