#include <tartarus.h>
#include <stddef.h>

uintptr_t g_hhdm_offset;
size_t g_hhdm_size;


[[noreturn]] void init([[maybe_unused]] tartarus_boot_info_t *boot_info) {
    g_hhdm_offset = boot_info->hhdm.offset;
    g_hhdm_size = boot_info->hhdm.size;

}