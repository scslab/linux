// SPDX-License-Identifier: GPL-2.0

//! Memory-related syscall implementations for kVisor

use kernel::prelude::*;

use crate::process::KvisorContext;
use super::errno;

/// mmap(2) - map files or devices into memory
pub fn sys_mmap(
    ctx: &mut KvisorContext,
    addr: usize,
    length: usize,
    prot: i32,
    flags: i32,
    fd: i32,
    offset: i64,
) -> i64 {
    pr_debug!(
        "kvisor: mmap(addr={:#x}, length={}, prot={:#x}, flags={:#x}, fd={}, offset={})\n",
        addr, length, prot, flags, fd, offset
    );

    // Validate parameters
    if length == 0 {
        return -errno::EINVAL;
    }

    // TODO: Implement memory mapping through platform abstraction
    // For anonymous mappings, allocate memory and track in context
    // For file mappings, coordinate with VFS

    let _ = (ctx, addr, length, prot, flags, fd, offset);
    -errno::ENOSYS
}

/// mprotect(2) - set protection on a region of memory
pub fn sys_mprotect(ctx: &mut KvisorContext, addr: usize, len: usize, prot: i32) -> i64 {
    pr_debug!("kvisor: mprotect(addr={:#x}, len={}, prot={:#x})\n", addr, len, prot);

    // Validate alignment
    if addr & 0xfff != 0 {
        return -errno::EINVAL;
    }

    // TODO: Implement through platform abstraction
    let _ = (ctx, addr, len, prot);
    -errno::ENOSYS
}

/// munmap(2) - unmap files or devices from memory
pub fn sys_munmap(ctx: &mut KvisorContext, addr: usize, length: usize) -> i64 {
    pr_debug!("kvisor: munmap(addr={:#x}, length={})\n", addr, length);

    // Validate alignment
    if addr & 0xfff != 0 {
        return -errno::EINVAL;
    }

    // TODO: Implement through platform abstraction
    let _ = (ctx, addr, length);
    -errno::ENOSYS
}

/// brk(2) - change data segment size
pub fn sys_brk(ctx: &mut KvisorContext, addr: usize) -> i64 {
    pr_debug!("kvisor: brk(addr={:#x})\n", addr);

    // TODO: Implement heap management
    // If addr is 0, return current break
    // Otherwise, try to set new break and return result

    let _ = ctx;

    if addr == 0 {
        // Return current break
        ctx.memory.get_brk() as i64
    } else {
        // Try to set new break
        match ctx.memory.set_brk(addr) {
            Ok(new_brk) => new_brk as i64,
            Err(_) => -errno::ENOMEM,
        }
    }
}
