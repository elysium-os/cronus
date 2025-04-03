typedef enum {
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_DEVONLY
} log_level_t;

[[gnu::format(printf, 3, 4)]] void log(log_level_t level, const char *tag, const char *fmt, ...);

void module_init() {
    log(LOG_LEVEL_INFO, "TEST_MODULE", "Hello world");
}
