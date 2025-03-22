include conf.mk

DESTDIR ?= /

# Kernel Flags
CFLAGS := -std=gnu23 -ffreestanding
ifeq ($(ARCH), x86_64)
ASMFLAGS := -f elf64 -Werror
CFLAGS += --target=x86_64-none-elf -mcmodel=kernel -mno-red-zone -mgeneral-regs-only -mabi=sysv
else
$(error unknown architecture)
endif

CFLAGS += -Wall -Wextra -Wvla -Wshadow
CFLAGS += -fno-stack-protector -fno-stack-check -fno-strict-aliasing

CFLAGS += -I$(SYSROOT)$(PREFIX)/include -I$(SRC_DIRECTORY)

CFLAGS += -D__ARCH_$(shell echo $(ARCH) | tr a-z A-Z) -D__ENV_$(shell echo $(ENVIRONMENT) | tr a-z A-Z)
CFLAGS += -DUACPI_NATIVE_ALLOC_ZEROED -DUACPI_FORMATTED_LOGGING -DUACPI_SIZED_FREES

ifeq ($(ENVIRONMENT), development)
CFLAGS += -g -fsanitize=undefined -finstrument-functions -fno-lto -fno-omit-frame-pointer -O0
ifeq ($(ARCH), x86_64)
CFLAGS += -fno-sanitize=alignment
endif
else ifeq ($(ENVIRONMENT), production)
CFLAGS += -O3
else
$(error invalid environment=`$(ENVIRONMENT)`)
endif

# Kernel Sources
C_SOURCES := $(shell find $(SRC_DIRECTORY) -type f -name "*.c" -not -path "$(SRC_DIRECTORY)/arch/*")
C_SOURCES += $(shell find $(SRC_DIRECTORY)/arch/$(ARCH) -type f -name "*.c")
ifeq ($(ARCH), x86_64)
ASM_SOURCES := $(shell find $(SRC_DIRECTORY) -type f -name "*.asm" -not -path "$(SRC_DIRECTORY)/arch/*")
ASM_SOURCES += $(shell find $(SRC_DIRECTORY)/arch/$(ARCH) -type f -name "*.asm")
endif

# Objects
OBJS := $(patsubst %.c,$(BUILD_DIRECTORY)/%_c.o,$(C_SOURCES))
ifeq ($(ARCH), x86_64)
OBJS += $(patsubst %.asm,$(BUILD_DIRECTORY)/%_asm.o,$(ASM_SOURCES))
endif

# Targets
.PHONY: all install clean

all: $(BUILD_DIRECTORY)/kernel.elf

ifeq ($(ARCH), x86_64)
$(BUILD_DIRECTORY)/kernel.elf: $(OBJS) $(SYSROOT)$(PREFIX)/share/uacpi.a
	mkdir -p $(@D)
	$(LD) -o $@ -T"$(SUPPORT_DIRECTORY)/link.x86_64.ld" $^

$(BUILD_DIRECTORY)/%_asm.o: %.asm
	mkdir -p $(@D)
	$(NASM) $< $(ASMFLAGS) -o $@
endif

$(BUILD_DIRECTORY)/%_c.o: %.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

install:
	install -d $(DESTDIR)$(PREFIX)/share
	install $(BUILD_DIRECTORY)/kernel.elf $(DESTDIR)$(PREFIX)/share

clean:
	rm -f $(shell find $(BUILD_DIRECTORY) -type f -name "*.o")
