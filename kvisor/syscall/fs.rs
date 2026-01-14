// SPDX-License-Identifier: GPL-2.0

//! Filesystem-related syscall implementations for kVisor

use kernel::prelude::*;

use crate::process::KvisorContext;
use super::errno;

/// read(2) - read from a file descriptor
pub fn sys_read(ctx: &mut KvisorContext, fd: i32, buf: *mut u8, count: usize) -> i64 {
    pr_debug!("kvisor: read(fd={}, buf={:p}, count={})\n", fd, buf, count);

    // Validate fd
    let file = match ctx.fd_table.get(fd) {
        Some(f) => f,
        None => return -errno::EBADF,
    };

    // TODO: Implement actual read through VFS
    // For now, return EOF
    let _ = (file, buf, count);
    0
}

/// write(2) - write to a file descriptor
pub fn sys_write(ctx: &mut KvisorContext, fd: i32, buf: *const u8, count: usize) -> i64 {
    pr_debug!("kvisor: write(fd={}, buf={:p}, count={})\n", fd, buf, count);

    // Validate fd
    let file = match ctx.fd_table.get(fd) {
        Some(f) => f,
        None => return -errno::EBADF,
    };

    // TODO: Implement actual write through VFS
    // For stdout/stderr (fd 1, 2), we could forward to the real console
    let _ = (file, buf);
    count as i64
}

/// open(2) - open a file
pub fn sys_open(ctx: &mut KvisorContext, pathname: *const u8, flags: i32, mode: u32) -> i64 {
    pr_debug!("kvisor: open(pathname={:p}, flags={:#x}, mode={:#o})\n", pathname, flags, mode);

    // TODO: Implement through VFS
    let _ = (ctx, pathname, flags, mode);
    -errno::ENOSYS
}

/// close(2) - close a file descriptor
pub fn sys_close(ctx: &mut KvisorContext, fd: i32) -> i64 {
    pr_debug!("kvisor: close(fd={})\n", fd);

    match ctx.fd_table.close(fd) {
        Ok(()) => 0,
        Err(_) => -errno::EBADF,
    }
}

/// fstat(2) - get file status
pub fn sys_fstat(ctx: &mut KvisorContext, fd: i32, statbuf: *mut u8) -> i64 {
    pr_debug!("kvisor: fstat(fd={}, statbuf={:p})\n", fd, statbuf);

    // Validate fd
    let file = match ctx.fd_table.get(fd) {
        Some(f) => f,
        None => return -errno::EBADF,
    };

    // TODO: Implement actual stat through VFS
    let _ = (file, statbuf);
    -errno::ENOSYS
}

/// lseek(2) - reposition file offset
pub fn sys_lseek(ctx: &mut KvisorContext, fd: i32, offset: i64, whence: i32) -> i64 {
    pr_debug!("kvisor: lseek(fd={}, offset={}, whence={})\n", fd, offset, whence);

    // Validate fd
    let file = match ctx.fd_table.get(fd) {
        Some(f) => f,
        None => return -errno::EBADF,
    };

    // TODO: Implement actual seek
    let _ = (file, offset, whence);
    -errno::ENOSYS
}

/// openat(2) - open a file relative to a directory fd
pub fn sys_openat(ctx: &mut KvisorContext, dirfd: i32, pathname: *const u8, flags: i32, mode: u32) -> i64 {
    pr_debug!("kvisor: openat(dirfd={}, pathname={:p}, flags={:#x}, mode={:#o})\n",
              dirfd, pathname, flags, mode);

    // TODO: Implement through VFS
    let _ = (ctx, dirfd, pathname, flags, mode);
    -errno::ENOSYS
}

/// newfstatat(2) - get file status relative to a directory fd
pub fn sys_newfstatat(ctx: &mut KvisorContext, dirfd: i32, pathname: *const u8, statbuf: *mut u8, flags: i32) -> i64 {
    pr_debug!("kvisor: newfstatat(dirfd={}, pathname={:p}, statbuf={:p}, flags={:#x})\n",
              dirfd, pathname, statbuf, flags);

    // TODO: Implement through VFS
    let _ = (ctx, dirfd, pathname, statbuf, flags);
    -errno::ENOSYS
}

/// getcwd(2) - get current working directory
pub fn sys_getcwd(ctx: &mut KvisorContext, buf: *mut u8, size: usize) -> i64 {
    pr_debug!("kvisor: getcwd(buf={:p}, size={})\n", buf, size);

    // TODO: Return the process's current working directory from VFS
    let _ = (ctx, buf, size);
    -errno::ENOSYS
}

/// chdir(2) - change working directory
pub fn sys_chdir(ctx: &mut KvisorContext, path: *const u8) -> i64 {
    pr_debug!("kvisor: chdir(path={:p})\n", path);

    // TODO: Implement through VFS
    let _ = (ctx, path);
    -errno::ENOSYS
}

/// dup(2) - duplicate a file descriptor
pub fn sys_dup(ctx: &mut KvisorContext, oldfd: i32) -> i64 {
    pr_debug!("kvisor: dup(oldfd={})\n", oldfd);

    match ctx.fd_table.dup(oldfd) {
        Ok(newfd) => newfd as i64,
        Err(_) => -errno::EBADF,
    }
}

/// dup2(2) - duplicate a file descriptor to a specific fd
pub fn sys_dup2(ctx: &mut KvisorContext, oldfd: i32, newfd: i32) -> i64 {
    pr_debug!("kvisor: dup2(oldfd={}, newfd={})\n", oldfd, newfd);

    match ctx.fd_table.dup2(oldfd, newfd) {
        Ok(fd) => fd as i64,
        Err(_) => -errno::EBADF,
    }
}
