#include "cronlink.h"

#include "arch/cronlink.h"
#include "common/lock/spinlock.h"
#include "common/log.h"
#include "lib/format.h"
#include "sys/init.h"

static spinlock_t g_lock = SPINLOCK_INIT;
static interrupt_state_t g_prev_state;

static void escaped_putch(char ch) {
    switch(ch) {
        case '\n':
            arch_cronlink_write('\\');
            arch_cronlink_write('n');
            break;
        case '\r':
            arch_cronlink_write('\\');
            arch_cronlink_write('r');
            break;
        case '\0':
            arch_cronlink_write('\\');
            arch_cronlink_write('0');
            break;
        default: arch_cronlink_write(ch);
    }
}

static void escaped_str(const char *str) {
    for(; *str != '\0'; str++) escaped_putch(*str);
}

void cronlink_packet_start(uint8_t type) {
    g_prev_state = spinlock_acquire_noint(&g_lock);
    arch_cronlink_write('$'); // "Signature"
    arch_cronlink_write(type); // Type
}

void cronlink_packet_write_string(const char *str) {
    escaped_str(str);
    arch_cronlink_write('\0');
}

void cronlink_packet_write_uint(uint64_t n) {
    arch_cronlink_write('\r');
    for(int i = 0; i < 8; i++) arch_cronlink_write((n >> i * 8) & 0xFF);
}

void cronlink_packet_end() {
    arch_cronlink_write('\n');
    spinlock_release_noint(&g_lock, g_prev_state);
}

#ifdef __ENV_DEVELOPMENT

static void log_cronlink(log_level_t level, const char *tag, const char *fmt, va_list args) {
    cronlink_packet_start('L');
    cronlink_packet_write_string(log_level_stringify(level));
    cronlink_packet_write_string(tag);

    format(escaped_putch, fmt, args);
    arch_cronlink_write('\0');

    cronlink_packet_end();
}

static log_sink_t g_cronlink_sink = {
    .name = "CRONLINK",
    .filter = { .level = LOG_LEVEL_TRACE, .tags_as_include = false, .tags = nullptr, .tag_count = 0 },
    .log = log_cronlink
};

static void cronlink_init() {
    log_sink_add(&g_cronlink_sink);
}

INIT_TARGET(cronlink, INIT_STAGE_BEFORE_EARLY, cronlink_init);

#endif
