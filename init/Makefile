CFLAGS = -std=gnu2x -static -O0 -g
CFLAGS += -Wall -Wextra -Wvla -Wshadow -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter

C_SOURCES = $(shell find ./src -name "*.c")

init: $(C_SOURCES)
	x86_64-elysium-gcc $(CFLAGS) $^ -o $@
