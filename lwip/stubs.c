#include "lwip/opt.h"
#include "lwip/sys.h"
#include <sys/time.h>
#include <stdlib.h>

sys_prot_t sys_arch_protect(void) { return 0; }
void sys_arch_unprotect(sys_prot_t p) { (void)p; }

u32_t sys_now(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

u32_t lwip_port_rand(void) {
    static unsigned long next = 1;
    next = next * 1103515245 + 12345;
    return (u32_t)(next / 65536) % 32768;
}
