/*
    Credit: monkuous (https://gist.github.com/monkuous/5752282d03995080e99671ecb9969b3f)

    This is free and unencumbered software released into the public domain.

    Anyone is free to copy, modify, publish, use, compile, sell, or
    distribute this software, either in source code form or as a compiled
    binary, for any purpose, commercial or non-commercial, and by any
    means.

    In jurisdictions that recognize copyright laws, the author or authors
    of this software dedicate any and all copyright interest in the
    software to the public domain. We make this dedication for the benefit
    of the public at large and to the detriment of our heirs and
    successors. We intend this dedication to be an overt act of
    relinquishment in perpetuity of all present and future rights to this
    software under copyright law.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
    OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
    ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
    OTHER DEALINGS IN THE SOFTWARE.

    For more information, please refer to https://unlicense.org
*/

#ifdef __ENV_DEVELOPMENT

#include "common/log.h"

#include "arch/x86_64/cpu/cpu.h"
#include "arch/x86_64/debug.h"

#include <stddef.h>
#include <stdint.h>

#define MAX_RECORDS 0x1000

typedef struct {
    void *function;
    uint64_t total;
    size_t calls;
} record_t;

static record_t g_records[MAX_RECORDS];
static size_t g_record_count = 0;

static int g_prof_lock = 0;
static int g_prof_active = 0;

[[gnu::no_instrument_function]] static unsigned long sdi(void) {
    unsigned long value;
    asm volatile("pushfq\npopq %0\ncli" : "=rm"(value));
    while(__atomic_exchange_n(&g_prof_lock, 1, __ATOMIC_ACQUIRE)) __builtin_ia32_pause();
    return value;
}

[[gnu::no_instrument_function]] static void ri(unsigned long value) {
    __atomic_store_n(&g_prof_lock, 0, __ATOMIC_RELEASE);
    if((value & 0x200) != 0) asm("sti");
}

[[gnu::no_instrument_function]] static void print_string(const char *str) {
    for(char c = *str; c != 0; c = *++str) asm volatile("outb %0, %1" : : "a"(c), "Nd"(0xE9));
}

[[gnu::no_instrument_function]] static void print_hex(uintptr_t value) {
    char buffer[sizeof(value) * 2 + 1];
    size_t index = sizeof(buffer);
    buffer[--index] = 0;

    do {
        buffer[--index] = "0123456789abcdef"[value & 15];
        value >>= 4;
    } while(value > 0);

    print_string(&buffer[index]);
}

[[gnu::no_instrument_function]] [[noreturn]] static void profiler_panic(const char *str, void *function, void *call_site) {
    print_string("profiler panic: ");
    print_string(str);
    print_string(" (function: ");
    print_hex((uintptr_t) function);
    print_string(", call_site: ");
    print_hex((uintptr_t) call_site);
    print_string(")\n");
    while(true) {
        __builtin_ia32_pause();
        asm volatile("hlt");
    }
    __builtin_unreachable();
}

[[gnu::no_instrument_function]] static bool pred(record_t *a, record_t *b) {
    return a->total < b->total;
}

[[gnu::no_instrument_function]] void x86_64_debug_prof_start(void) {
    __atomic_store_n(&g_prof_active, 1, __ATOMIC_RELEASE);
}

[[gnu::no_instrument_function]] void x86_64_debug_prof_stop(void) {
    __atomic_store_n(&g_prof_active, 0, __ATOMIC_RELEASE);
}

[[gnu::no_instrument_function]] void x86_64_debug_prof_reset(void) {
    unsigned long state = sdi();
    g_record_count = 0;
    ri(state);
}

[[gnu::no_instrument_function]] void x86_64_debug_prof_show(const char *name) {
    unsigned long state = sdi();

    for(size_t i = 1; i < g_record_count; i++) {
        for(size_t j = i; j > 0 && pred(&g_records[j - 1], &g_records[j]); j--) {
            record_t *a = &g_records[j - 1];
            record_t *b = &g_records[j];

            record_t temp = *a;
            *a = *b;
            *b = temp;
        }
    }

    log(LOG_LEVEL_DEBUG, "PROFILE", "Profiler results for `%s` (%lu):", name, g_record_count);
    for(size_t i = 0; i < g_record_count; i++) {
        x86_64_debug_symbol_t debug_symbol = x86_64_debug_symbol((uintptr_t) g_records[i].function);

        if(debug_symbol.found && debug_symbol.address == (uintptr_t) g_records[i].function) {
            log(LOG_LEVEL_DEBUG,
                "PROFILE",
                "%lu. %.*s <%#lx>: %lu (calls: %lu, average: %lu)",
                i + 1,
                (int) debug_symbol.length,
                debug_symbol.name,
                (uintptr_t) g_records[i].function,
                g_records[i].total,
                g_records[i].calls,
                (g_records[i].total + (g_records[i].calls / 2)) / g_records[i].calls);
        } else {
            log(LOG_LEVEL_DEBUG,
                "PROFILE",
                "%lu. %#lx: %lu (calls: %lu, average: %lu)",
                i + 1,
                (uintptr_t) g_records[i].function,
                g_records[i].total,
                g_records[i].calls,
                (g_records[i].total + (g_records[i].calls / 2)) / g_records[i].calls);
        }
    }

    ri(state);
}

[[gnu::no_instrument_function]] [[clang::no_sanitize("undefined")]] void __cyg_profile_func_enter(void *function, void *call_site) {
    uint64_t start = __builtin_ia32_rdtsc();
    if(!__atomic_load_n(&g_prof_active, __ATOMIC_ACQUIRE)) return;

    x86_64_thread_t *thread = X86_64_CPU_LOCAL_MEMBER(current_thread);

    size_t idx = thread->prof_current_call_frame++;
    if(idx >= X86_64_DEBUG_PROF_MAX_FRAMES) profiler_panic("too many frames", function, call_site);

    x86_64_debug_prof_call_frame_t *frame = &thread->prof_call_frames[idx];
    frame->function = function;
    frame->call_site = call_site;
    frame->ptime = 0;

    uint64_t end = __builtin_ia32_rdtsc();
    uint64_t time = end - start;

    for(size_t i = 0; i < idx; i++) {
        thread->prof_call_frames[i].ptime += time;
    }

    frame->start = __builtin_ia32_rdtsc();
}

[[gnu::no_instrument_function]] [[clang::no_sanitize("undefined")]] void __cyg_profile_func_exit(void *function, void *call_site) {
    uint64_t start = __builtin_ia32_rdtsc();
    if(!__atomic_load_n(&g_prof_active, __ATOMIC_ACQUIRE)) return;

    unsigned state = sdi();

    x86_64_thread_t *thread = X86_64_CPU_LOCAL_MEMBER(current_thread);

    size_t idx = --thread->prof_current_call_frame;
    x86_64_debug_prof_call_frame_t *frame = &thread->prof_call_frames[idx];
    uint64_t time = start - frame->start - frame->ptime;

    if(frame->function != function && frame->call_site != call_site) profiler_panic("frame mismatch", function, call_site);

    for(size_t i = 0; i < g_record_count; i++) {
        record_t *record = &g_records[i];

        if(record->function == function) {
            record->total += time;
            record->calls += 1;
            ri(state);
            return;
        }
    }

    if(g_record_count == MAX_RECORDS) profiler_panic("max records", function, call_site);

    record_t *record = &g_records[g_record_count++];
    record->function = function;
    record->total = time;
    record->calls = 1;

    ri(state);

    uint64_t end = __builtin_ia32_rdtsc();
    time = end - start;

    for(size_t i = 0; i < idx; i++) {
        thread->prof_call_frames[i].ptime += time;
    }
}

#endif
