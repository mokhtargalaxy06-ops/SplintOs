.section .rodata
.global _binary_build_user_hello_elf_start
.global _binary_build_user_hello_elf_end
_binary_build_user_hello_elf_start:
    .incbin "build/user/hello.elf"
_binary_build_user_hello_elf_end:

.global _binary_build_user_cat_elf_start
.global _binary_build_user_cat_elf_end
_binary_build_user_cat_elf_start:
    .incbin "build/user/cat.elf"
_binary_build_user_cat_elf_end:

.global _binary_build_user_runner_elf_start
.global _binary_build_user_runner_elf_end
_binary_build_user_runner_elf_start:
    .incbin "build/user/runner.elf"
_binary_build_user_runner_elf_end:

.global _binary_build_user_sh_elf_start
.global _binary_build_user_sh_elf_end
_binary_build_user_sh_elf_start:
    .incbin "build/user/sh.elf"
_binary_build_user_sh_elf_end:

.global _binary_build_user_fdtest_elf_start
.global _binary_build_user_fdtest_elf_end
_binary_build_user_fdtest_elf_start:
    .incbin "build/user/fdtest.elf"
_binary_build_user_fdtest_elf_end:

.global _binary_build_user_echo_elf_start
.global _binary_build_user_echo_elf_end
_binary_build_user_echo_elf_start:
    .incbin "build/user/echo.elf"
_binary_build_user_echo_elf_end:

.global _binary_build_user_pipetest_elf_start
.global _binary_build_user_pipetest_elf_end
_binary_build_user_pipetest_elf_start:
    .incbin "build/user/pipetest.elf"
_binary_build_user_pipetest_elf_end:

.global _binary_build_user_wc_elf_start
.global _binary_build_user_wc_elf_end
_binary_build_user_wc_elf_start:
    .incbin "build/user/wc.elf"
_binary_build_user_wc_elf_end:

.global _binary_build_user_ls_elf_start
.global _binary_build_user_ls_elf_end
_binary_build_user_ls_elf_start: .incbin "build/user/ls.elf"
_binary_build_user_ls_elf_end:
.global _binary_build_user_mem_elf_start
.global _binary_build_user_mem_elf_end
_binary_build_user_mem_elf_start: .incbin "build/user/mem.elf"
_binary_build_user_mem_elf_end:
.global _binary_build_user_uptime_elf_start
.global _binary_build_user_uptime_elf_end
_binary_build_user_uptime_elf_start: .incbin "build/user/uptime.elf"
_binary_build_user_uptime_elf_end:
.global _binary_build_user_ps_elf_start
.global _binary_build_user_ps_elf_end
_binary_build_user_ps_elf_start: .incbin "build/user/ps.elf"
_binary_build_user_ps_elf_end:
.global _binary_build_user_heaptest_elf_start
.global _binary_build_user_heaptest_elf_end
_binary_build_user_heaptest_elf_start: .incbin "build/user/heaptest.elf"
_binary_build_user_heaptest_elf_end:
.global _binary_build_user_disk_elf_start
.global _binary_build_user_disk_elf_end
_binary_build_user_disk_elf_start: .incbin "build/user/disk.elf"
_binary_build_user_disk_elf_end:

.section .note.GNU-stack,"",@progbits
