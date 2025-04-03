CFLAGS := -target x86_64-elf -ffreestanding -m64 -fno-pic -mcmodel=large
CFLAGS += -Wall -Wextra -fvisibility=hidden
CFLAGS += -mno-red-zone -mgeneral-regs-only

test.elmod:
	$(CC) $(CFLAGS) $(LDFLAGS) -c test.c -o $@
