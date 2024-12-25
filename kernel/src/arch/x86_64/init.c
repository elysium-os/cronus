#include <tartarus.h>

[[noreturn]] void init([[maybe_unused]] tartarus_boot_info_t *boot_info) {
    for(;;) asm volatile ("hlt");
}