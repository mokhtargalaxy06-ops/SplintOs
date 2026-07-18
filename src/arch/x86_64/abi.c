#include <stdint.h>
#include "arch/x86_64/abi.h"
#include "splint/abi64.h"

int x86_64_abi_conformance_test(void)
{
    const struct splint64_abi_info info = {
        SPLINT64_ABI_MAGIC, SPLINT64_ABI_VERSION, sizeof(info),
        SPLINT64_SYS_COUNT, 0
    };
    return info.magic == UINT32_C(0x364c5053) && info.version == 1 &&
           info.size == 16 && info.syscall_count == SPLINT64_SYS_ABI_QUERY &&
           SPLINT64_SYS_WRITE == 1 && SPLINT64_SYS_SETPGID == 40 &&
           SPLINT64_EINVAL == -1 && SPLINT64_EFAULT == -8 &&
           SPLINT64_MAX_IO <= SPLINT64_MAX_PATH;
}
