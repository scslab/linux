// SPDX-License-Identifier: GPL-2.0

//! Process-related syscall implementations for kVisor

use kernel::prelude::*;

use crate::process::KvisorContext;
use super::errno;

/// exit(2) - terminate the calling process
pub fn sys_exit(ctx: &mut KvisorContext, status: i32) -> i64 {
    pr_debug!("kvisor: exit(status={})\n", status);

    ctx.exit_code = Some(status);

    // TODO: Actually terminate the process
    // For now, just record the exit code
    0
}

/// exit_group(2) - exit all threads in a process
pub fn sys_exit_group(ctx: &mut KvisorContext, status: i32) -> i64 {
    pr_debug!("kvisor: exit_group(status={})\n", status);

    // For single-threaded processes, same as exit
    sys_exit(ctx, status)
}

/// getpid(2) - get process ID
pub fn sys_getpid(ctx: &KvisorContext) -> i64 {
    pr_debug!("kvisor: getpid()\n");

    ctx.pid as i64
}

/// getppid(2) - get parent process ID
pub fn sys_getppid(ctx: &KvisorContext) -> i64 {
    pr_debug!("kvisor: getppid()\n");

    ctx.ppid as i64
}

/// getuid(2) - get user ID
pub fn sys_getuid(ctx: &KvisorContext) -> i64 {
    pr_debug!("kvisor: getuid()\n");

    ctx.uid as i64
}

/// getgid(2) - get group ID
pub fn sys_getgid(ctx: &KvisorContext) -> i64 {
    pr_debug!("kvisor: getgid()\n");

    ctx.gid as i64
}

/// geteuid(2) - get effective user ID
pub fn sys_geteuid(ctx: &KvisorContext) -> i64 {
    pr_debug!("kvisor: geteuid()\n");

    // For Phase 1, euid == uid
    ctx.uid as i64
}

/// getegid(2) - get effective group ID
pub fn sys_getegid(ctx: &KvisorContext) -> i64 {
    pr_debug!("kvisor: getegid()\n");

    // For Phase 1, egid == gid
    ctx.gid as i64
}

/// arch_prctl(2) - set architecture-specific thread state
pub fn sys_arch_prctl(ctx: &mut KvisorContext, code: i32, addr: u64) -> i64 {
    pr_debug!("kvisor: arch_prctl(code={:#x}, addr={:#x})\n", code, addr);

    // x86_64 arch_prctl codes
    const ARCH_SET_FS: i32 = 0x1002;

    match code {
        ARCH_SET_FS => {
            ctx.fs_base = addr;
            // TODO: Actually set FS base through platform
            0
        }
        _ => -errno::ENOSYS,
    }
}

/// set_tid_address(2) - set pointer to thread ID
pub fn sys_set_tid_address(ctx: &mut KvisorContext, tidptr: *mut i32) -> i64 {
    pr_debug!("kvisor: set_tid_address(tidptr={:p})\n", tidptr);

    // For Phase 1, just return the PID as TID
    let _ = tidptr;
    ctx.pid as i64
}

/// uname(2) - get name and information about current kernel
pub fn sys_uname(ctx: &KvisorContext, buf: *mut u8) -> i64 {
    pr_debug!("kvisor: uname(buf={:p})\n", buf);

    // TODO: Fill in utsname structure
    // For now, return success with placeholder data
    let _ = (ctx, buf);
    -errno::ENOSYS
}

/// getrandom(2) - obtain random bytes
pub fn sys_getrandom(ctx: &KvisorContext, buf: *mut u8, buflen: usize, flags: u32) -> i64 {
    pr_debug!("kvisor: getrandom(buf={:p}, buflen={}, flags={:#x})\n", buf, buflen, flags);

    // TODO: Implement through platform abstraction
    let _ = (ctx, buf, buflen, flags);
    -errno::ENOSYS
}
