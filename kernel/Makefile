include conf.mk

DESTDIR ?= /

# Kernel Flags
CFLAGS := -std=gnu2x -ffreestanding -mcmodel=kernel -mno-red-zone -mgeneral-regs-only -mabi=sysv
CFLAGS += -Wall -Wextra -Wvla -Wshadow
CFLAGS += -fno-stack-protector -fno-stack-check -fno-strict-aliasing

CFLAGS += -I$(SYSROOT)$(PREFIX)/include -I$(SRC_DIRECTORY)

CFLAGS += -D__ARCH_$(shell echo $(ARCH) | tr a-z A-Z) -D__ENV_$(shell echo $(ENVIRONMENT) | tr a-z A-Z)
CFLAGS += -DUACPI_NATIVE_ALLOC_ZEROED -DUACPI_FORMATTED_LOGGING -DUACPI_SIZED_FREES

ifeq ($(ENVIRONMENT), development)
CFLAGS += -g -fsanitize=undefined -fno-lto -fno-omit-frame-pointer
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
S_SOURCES := $(shell find $(SRC_DIRECTORY) -type f -name "*.S" -not -path "$(SRC_DIRECTORY)/arch/*")
S_SOURCES += $(shell find $(SRC_DIRECTORY)/arch/$(ARCH) -type f -name "*.S")

# Objects
OBJS := $(patsubst %.c,$(BUILD_DIRECTORY)/%_c.o,$(C_SOURCES))
OBJS += $(patsubst %.S,$(BUILD_DIRECTORY)/%_S.o,$(S_SOURCES))

# Targets
.PHONY: all install clean

all: $(BUILD_DIRECTORY)/kernel.elf $(BUILD_DIRECTORY)/kernel_symbols.txt clean

ifeq ($(ARCH), x86_64)
$(BUILD_DIRECTORY)/kernel.elf: $(OBJS) $(SYSROOT)$(PREFIX)/share/uacpi.a /usr/local/lib/gcc/x86_64-elf/13.2.0/no-red-zone/libgcc.a
	mkdir -p $(@D)
	$(LD) -o $@ -T"$(SUPPORT_DIRECTORY)/link.x86_64.ld" $^

$(BUILD_DIRECTORY)/kernel_symbols.txt: $(BUILD_DIRECTORY)/kernel.elf
	x86_64-elf-nm $< -n > $@
endif

$(BUILD_DIRECTORY)/%_S.o: %.S
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIRECTORY)/%_c.o: %.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

install:
	install -d $(DESTDIR)$(PREFIX)/share
	install $(BUILD_DIRECTORY)/kernel.elf $(DESTDIR)$(PREFIX)/share
	install $(BUILD_DIRECTORY)/kernel_symbols.txt $(DESTDIR)$(PREFIX)/share

clean:
	rm -f $(shell find $(BUILD_DIRECTORY) -type f -name "*.o")