// SPDX-License-Identifier: GPL-2.0

//! Syscall dispatcher for kVisor
//!
//! This module receives syscalls from sandboxed processes and routes them
//! to the appropriate handler.

use kernel::bindings;
use kernel::prelude::*;

use crate::process::KvisorContext;

mod fs;
mod memory;
mod process;

/// Syscall numbers (x86_64)
mod nr {
    pub const READ: i32 = 0;
    pub const WRITE: i32 = 1;
    pub const OPEN: i32 = 2;
    pub const CLOSE: i32 = 3;
    pub const STAT: i32 = 4;
    pub const FSTAT: i32 = 5;
    pub const LSTAT: i32 = 6;
    pub const POLL: i32 = 7;
    pub const LSEEK: i32 = 8;
    pub const MMAP: i32 = 9;
    pub const MPROTECT: i32 = 10;
    pub const MUNMAP: i32 = 11;
    pub const BRK: i32 = 12;
    pub const IOCTL: i32 = 16;
    pub const ACCESS: i32 = 21;
    pub const DUP: i32 = 32;
    pub const DUP2: i32 = 33;
    pub const GETPID: i32 = 39;
    pub const FORK: i32 = 57;
    pub const EXECVE: i32 = 59;
    pub const EXIT: i32 = 60;
    pub const UNAME: i32 = 63;
    pub const FCNTL: i32 = 72;
    pub const GETCWD: i32 = 79;
    pub const CHDIR: i32 = 80;
    pub const GETUID: i32 = 102;
    pub const GETGID: i32 = 104;
    pub const GETEUID: i32 = 107;
    pub const GETEGID: i32 = 108;
    pub const GETPPID: i32 = 110;
    pub const ARCH_PRCTL: i32 = 158;
    pub const SET_TID_ADDRESS: i32 = 218;
    pub const EXIT_GROUP: i32 = 231;
    pub const OPENAT: i32 = 257;
    pub const NEWFSTATAT: i32 = 262;
    pub const READLINKAT: i32 = 267;
    pub const GETRANDOM: i32 = 318;
}

/// Error codes
#[allow(dead_code)]
mod errno {
    pub const EPERM: i64 = 1;
    pub const ENOENT: i64 = 2;
    pub const ESRCH: i64 = 3;
    pub const EINTR: i64 = 4;
    pub const EIO: i64 = 5;
    pub const ENXIO: i64 = 6;
    pub const EBADF: i64 = 9;
    pub const EAGAIN: i64 = 11;
    pub const ENOMEM: i64 = 12;
    pub const EACCES: i64 = 13;
    pub const EFAULT: i64 = 14;
    pub const EBUSY: i64 = 16;
    pub const EEXIST: i64 = 17;
    pub const ENODEV: i64 = 19;
    pub const ENOTDIR: i64 = 20;
    pub const EISDIR: i64 = 21;
    pub const EINVAL: i64 = 22;
    pub const ENFILE: i64 = 23;
    pub const EMFILE: i64 = 24;
    pub const ENOSPC: i64 = 28;
    pub const ESPIPE: i64 = 29;
    pub const EROFS: i64 = 30;
    pub const EPIPE: i64 = 32;
    pub const ENOSYS: i64 = 38;
    pub const ENOTEMPTY: i64 = 39;
}

/// Initialize the syscall subsystem
pub fn init() -> Result {
    pr_debug!("kvisor: syscall subsystem initialized\n");
    Ok(())
}

/// Cleanup the syscall subsystem
pub fn cleanup() {
    pr_debug!("kvisor: syscall subsystem cleanup\n");
}

/// Dispatch a syscall to the appropriate handler
///
/// # Safety
/// The regs pointer must be valid and point to the current task's registers.
pub fn dispatch(
    regs: *mut bindings::pt_regs,
    nr: core::ffi::c_int,
    ctx: &mut KvisorContext,
) -> core::ffi::c_long {
    // Extract syscall arguments from registers
    // x86_64 ABI: rdi, rsi, rdx, r10, r8, r9
    let (arg0, arg1, arg2, arg3, arg4, arg5) = unsafe {
        let r = &*regs;
        (r.di, r.si, r.dx, r.r10, r.r8, r.r9)
    };

    let result: i64 = match nr {
        // File operations
        nr::READ => fs::sys_read(ctx, arg0 as i32, arg1 as *mut u8, arg2 as usize),
        nr::WRITE => fs::sys_write(ctx, arg0 as i32, arg1 as *const u8, arg2 as usize),
        nr::OPEN => fs::sys_open(ctx, arg0 as *const u8, arg1 as i32, arg2 as u32),
        nr::CLOSE => fs::sys_close(ctx, arg0 as i32),
        nr::FSTAT => fs::sys_fstat(ctx, arg0 as i32, arg1 as *mut u8),
        nr::LSEEK => fs::sys_lseek(ctx, arg0 as i32, arg1 as i64, arg2 as i32),
        nr::OPENAT => fs::sys_openat(ctx, arg0 as i32, arg1 as *const u8, arg2 as i32, arg3 as u32),
        nr::NEWFSTATAT => fs::sys_newfstatat(ctx, arg0 as i32, arg1 as *const u8, arg2 as *mut u8, arg3 as i32),
        nr::GETCWD => fs::sys_getcwd(ctx, arg0 as *mut u8, arg1 as usize),
        nr::CHDIR => fs::sys_chdir(ctx, arg0 as *const u8),
        nr::DUP => fs::sys_dup(ctx, arg0 as i32),
        nr::DUP2 => fs::sys_dup2(ctx, arg0 as i32, arg1 as i32),

        // Memory operations
        nr::MMAP => memory::sys_mmap(ctx, arg0 as usize, arg1 as usize, arg2 as i32, arg3 as i32, arg4 as i32, arg5 as i64),
        nr::MPROTECT => memory::sys_mprotect(ctx, arg0 as usize, arg1 as usize, arg2 as i32),
        nr::MUNMAP => memory::sys_munmap(ctx, arg0 as usize, arg1 as usize),
        nr::BRK => memory::sys_brk(ctx, arg0 as usize),

        // Process operations
        nr::EXIT => process::sys_exit(ctx, arg0 as i32),
        nr::EXIT_GROUP => process::sys_exit_group(ctx, arg0 as i32),
        nr::GETPID => process::sys_getpid(ctx),
        nr::GETPPID => process::sys_getppid(ctx),
        nr::GETUID => process::sys_getuid(ctx),
        nr::GETGID => process::sys_getgid(ctx),
        nr::GETEUID => process::sys_geteuid(ctx),
        nr::GETEGID => process::sys_getegid(ctx),
        nr::ARCH_PRCTL => process::sys_arch_prctl(ctx, arg0 as i32, arg1 as u64),
        nr::SET_TID_ADDRESS => process::sys_set_tid_address(ctx, arg0 as *mut i32),
        nr::UNAME => process::sys_uname(ctx, arg0 as *mut u8),
        nr::GETRANDOM => process::sys_getrandom(ctx, arg0 as *mut u8, arg1 as usize, arg2 as u32),

        // Unimplemented syscalls
        _ => {
            pr_debug!("kvisor: unimplemented syscall {}\n", nr);
            -errno::ENOSYS
        }
    };

    result as core::ffi::c_long
}
