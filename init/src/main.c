#include <elysium/syscall.h>

int main(int argc, char **vargs) {
    syscall2(SYSCALL_DEBUG, 5, (syscall_int_t) "Hello");

    return 123;
}
