// SPDX-License-Identifier: GPL-2.0

//! Filesystem-related syscall implementations for kVisor
//!
//! Phase 1: All stubs returning ENOSYS - syscalls are intercepted and killed.

use kernel::prelude::*;

use crate::process::KvisorContext;
use super::errno;

/// read(2) - read from a file descriptor
pub fn sys_read(_ctx: &mut KvisorContext, fd: i32, buf: *mut u8, count: usize) -> i64 {
    pr_debug!("kvisor: read(fd={}, buf={:p}, count={})\n", fd, buf, count);
    -errno::ENOSYS
}

/// write(2) - write to a file descriptor
pub fn sys_write(_ctx: &mut KvisorContext, fd: i32, buf: *const u8, count: usize) -> i64 {
    pr_debug!("kvisor: write(fd={}, buf={:p}, count={})\n", fd, buf, count);
    -errno::ENOSYS
}

/// open(2) - open a file
pub fn sys_open(_ctx: &mut KvisorContext, pathname: *const u8, flags: i32, mode: u32) -> i64 {
    pr_debug!("kvisor: open(pathname={:p}, flags={:#x}, mode={:#o})\n", pathname, flags, mode);
    -errno::ENOSYS
}

/// close(2) - close a file descriptor
pub fn sys_close(_ctx: &mut KvisorContext, fd: i32) -> i64 {
    pr_debug!("kvisor: close(fd={})\n", fd);
    -errno::ENOSYS
}

/// fstat(2) - get file status
pub fn sys_fstat(_ctx: &mut KvisorContext, fd: i32, statbuf: *mut u8) -> i64 {
    pr_debug!("kvisor: fstat(fd={}, statbuf={:p})\n", fd, statbuf);
    -errno::ENOSYS
}

/// lseek(2) - reposition file offset
pub fn sys_lseek(_ctx: &mut KvisorContext, fd: i32, offset: i64, whence: i32) -> i64 {
    pr_debug!("kvisor: lseek(fd={}, offset={}, whence={})\n", fd, offset, whence);
    -errno::ENOSYS
}

/// openat(2) - open a file relative to a directory fd
pub fn sys_openat(_ctx: &mut KvisorContext, dirfd: i32, pathname: *const u8, flags: i32, mode: u32) -> i64 {
    pr_debug!("kvisor: openat(dirfd={}, pathname={:p}, flags={:#x}, mode={:#o})\n",
              dirfd, pathname, flags, mode);
    -errno::ENOSYS
}

/// newfstatat(2) - get file status relative to a directory fd
pub fn sys_newfstatat(_ctx: &mut KvisorContext, dirfd: i32, pathname: *const u8, statbuf: *mut u8, flags: i32) -> i64 {
    pr_debug!("kvisor: newfstatat(dirfd={}, pathname={:p}, statbuf={:p}, flags={:#x})\n",
              dirfd, pathname, statbuf, flags);
    -errno::ENOSYS
}

/// getcwd(2) - get current working directory
pub fn sys_getcwd(_ctx: &mut KvisorContext, buf: *mut u8, size: usize) -> i64 {
    pr_debug!("kvisor: getcwd(buf={:p}, size={})\n", buf, size);
    -errno::ENOSYS
}

/// chdir(2) - change working directory
pub fn sys_chdir(_ctx: &mut KvisorContext, path: *const u8) -> i64 {
    pr_debug!("kvisor: chdir(path={:p})\n", path);
    -errno::ENOSYS
}

/// dup(2) - duplicate a file descriptor
pub fn sys_dup(_ctx: &mut KvisorContext, oldfd: i32) -> i64 {
    pr_debug!("kvisor: dup(oldfd={})\n", oldfd);
    -errno::ENOSYS
}

/// dup2(2) - duplicate a file descriptor to a specific fd
pub fn sys_dup2(_ctx: &mut KvisorContext, oldfd: i32, newfd: i32) -> i64 {
    pr_debug!("kvisor: dup2(oldfd={}, newfd={})\n", oldfd, newfd);
    -errno::ENOSYS
}
