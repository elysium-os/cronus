#include "x86_64/cpu/cpu.h"
#include "x86_64/thread.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if(argc < 2) {
        fprintf(stderr, "Missing output argument\n");
        return 1;
    }

    const char *output_file = argv[1];

    FILE *f = fopen(output_file, "w+");
    if(f == nullptr) {
        fprintf(stderr, "failed to open `%s`\n", output_file);
        return 1;
    }

    struct {
        const char *key;
        size_t value;
    } defines[] = {
        { .key = "ASMGEN_CURRENT_THREAD_OFFSET",    .value = offsetof(x86_64_cpu_t,    current_thread) },
        { .key = "ASMGEN_THREAD_RSP_OFFSET",        .value = offsetof(x86_64_thread_t, rsp)            },
        { .key = "ASMGEN_SYSCALL_RSP_OFFSET",       .value = offsetof(x86_64_thread_t, syscall_rsp)    },
        { .key = "ASMGEN_KERNEL_STACK_BASE_OFFSET", .value = offsetof(x86_64_thread_t, kernel_stack)   },
    };

    for(size_t i = 0; i < sizeof(defines) / sizeof(*defines); i++) {
        if(fprintf(f, "-d%s=%lu\n", defines[i].key, defines[i].value) < 0) {
            fprintf(stderr, "failed to write file `%s`: %s\n", output_file, strerror(errno));
            return 1;
        }
    }

    fclose(f);

    return 0;
}
