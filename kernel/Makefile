include conf.mk

DESTDIR ?= /

# Flags
CFLAGS := -std=gnu2x -ffreestanding -mcmodel=kernel -mno-red-zone -mgeneral-regs-only -O3
CFLAGS += -Wall -Wextra -Wvla -Wshadow
CFLAGS += -fno-stack-protector -fno-stack-check -fno-omit-frame-pointer -fno-strict-aliasing
CFLAGS += -D__ARCH_$(shell echo $(ARCH) | tr a-z A-Z) -D__ENV_$(shell echo $(ENVIRONMENT) | tr a-z A-Z)
CFLAGS += -I$(SYSROOT)$(PREFIX)/include -I$(SRC)

ifeq ($(ENVIRONMENT), development)
CFLAGS += -g -fsanitize=undefined
ifeq ($(ARCH), x86_64)
CFLAGS += -fno-sanitize=alignment
endif
else ifeq ($(ENVIRONMENT), production)
else
$(error invalid environment=`$(ENVIRONMENT)`)
endif

# Sources
S_SOURCES := $(shell find $(SRC) -type f -name "*.S" -not -path "$(SRC)/arch/*")
S_SOURCES += $(shell find $(SRC)/arch/$(ARCH) -type f -name "*.S")
C_SOURCES := $(shell find $(SRC) -type f -name "*.c" -not -path "$(SRC)/arch/*")
C_SOURCES += $(shell find $(SRC)/arch/$(ARCH) -type f -name "*.c")

# Targets
.PHONY: all install clean

all: $(BUILD)/kernel.elf clean

ifeq ($(ARCH), x86_64)
$(BUILD)/kernel.elf: $(S_SOURCES:.S=_S.o) $(C_SOURCES:.c=.o)
	mkdir -p $(BUILD)
	$(LD) -o $@ -T"$(SUPPORT)/link.x86_64.ld" $^
endif

%_S.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install:
	install -d $(DESTDIR)$(PREFIX)/share
	install $(BUILD)/kernel.elf $(DESTDIR)$(PREFIX)/share

clean:
	rm -f $(shell find $(SRC) -type f -name "*.o")