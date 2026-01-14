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

// FFI declaration for killing the current task
extern "C" {
    fn kvisor_kill_current() -> !;
}

/// Dispatch a syscall to the appropriate handler
///
/// For now, this just prints the syscall number and kills the task.
/// This is for testing that syscall interception is working.
///
/// # Safety
/// The regs pointer must be valid and point to the current task's registers.
pub fn dispatch(
    _regs: *mut bindings::pt_regs,
    nr: core::ffi::c_int,
    _ctx: &mut KvisorContext,
) -> core::ffi::c_long {
    pr_info!("kvisor: intercepted syscall {} - killing task\n", nr);

    // Kill the sandboxed process
    unsafe { kvisor_kill_current() }
}
