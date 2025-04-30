#include "common/log.h"

void __module_initialize() {
    log(LOG_LEVEL_INFO, "TEST_MODULE", "Hello world");
}
