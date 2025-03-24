#include <elysium/syscall.h>
#include <mlibc/all-sysdeps.hpp>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

namespace mlibc {

int sys_uname(struct utsname *buf) {
    syscall_system_info_t sysinfo;
    syscall1(SYSCALL_SYSINFO, (syscall_int_t) &sysinfo);

    strcpy(buf->sysname, "Elysium");
    strcpy(buf->nodename, "elysium");
    strcpy(buf->release, sysinfo.release);
    snprintf(buf->version, sizeof(buf->version), "%s %s", sysinfo.release, sysinfo.version);
    buf->domainname[0] = '\0';
    buf->machine[0] = '\0';

    return 0;
}

} // namespace mlibc
