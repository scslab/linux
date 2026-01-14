// SPDX-License-Identifier: GPL-2.0

//! Linux platform implementation for kVisor
//!
//! This module implements the platform traits by calling into the Linux kernel.
//! As kVisor matures, these implementations can be replaced with pure Rust
//! versions one by one.

use kernel::prelude::*;

use super::{AllocError, Allocator, MapFlags, MemoryOps, Pid, Platform, ProcessOps, Prot};

/// Linux kernel allocator
pub struct LinuxAllocator;

impl Allocator for LinuxAllocator {
    fn alloc(&self, size: usize, _align: usize) -> Result<*mut u8, AllocError> {
        // Use kernel's allocation
        // TODO: Actually call kmalloc via bindings
        let _ = size;
        Err(AllocError::OutOfMemory)
    }

    unsafe fn free(&self, ptr: *mut u8, _size: usize, _align: usize) {
        // TODO: Actually call kfree via bindings
        let _ = ptr;
    }

    unsafe fn realloc(
        &self,
        ptr: *mut u8,
        old_size: usize,
        new_size: usize,
        align: usize,
    ) -> Result<*mut u8, AllocError> {
        // Simple implementation: alloc new, copy, free old
        let new_ptr = self.alloc(new_size, align)?;
        if !ptr.is_null() {
            // SAFETY: Both pointers are valid and non-overlapping
            unsafe {
                core::ptr::copy_nonoverlapping(ptr, new_ptr, core::cmp::min(old_size, new_size));
            }
            // SAFETY: ptr was allocated by this allocator
            unsafe {
                self.free(ptr, old_size, align);
            }
        }
        Ok(new_ptr)
    }
}

/// Linux memory operations
pub struct LinuxMemory;

impl MemoryOps for LinuxMemory {
    fn mmap(&self, addr: Option<usize>, len: usize, prot: Prot, flags: MapFlags) -> Result<usize> {
        // TODO: Call vm_mmap via bindings
        let _ = (addr, len, prot, flags);
        Err(EINVAL)
    }

    fn munmap(&self, addr: usize, len: usize) -> Result {
        // TODO: Call vm_munmap via bindings
        let _ = (addr, len);
        Err(EINVAL)
    }

    fn mprotect(&self, addr: usize, len: usize, prot: Prot) -> Result {
        // TODO: Call do_mprotect via bindings
        let _ = (addr, len, prot);
        Err(EINVAL)
    }
}

/// Linux process operations
pub struct LinuxProcess;

impl ProcessOps for LinuxProcess {
    fn exit(&self, code: i32) -> ! {
        // TODO: Call do_exit via bindings
        let _ = code;
        loop {
            // Should never reach here
        }
    }

    fn get_pid(&self) -> Pid {
        // TODO: Get current->pid via bindings
        1
    }

    fn get_tid(&self) -> Pid {
        // TODO: Get current->pid via bindings
        1
    }
}

/// The Linux platform implementation
pub struct LinuxPlatform {
    allocator: LinuxAllocator,
    memory: LinuxMemory,
    process: LinuxProcess,
}

impl LinuxPlatform {
    pub fn new() -> Self {
        LinuxPlatform {
            allocator: LinuxAllocator,
            memory: LinuxMemory,
            process: LinuxProcess,
        }
    }
}

impl Default for LinuxPlatform {
    fn default() -> Self {
        Self::new()
    }
}

impl Platform for LinuxPlatform {
    type Alloc = LinuxAllocator;
    type Memory = LinuxMemory;
    type Process = LinuxProcess;

    fn allocator(&self) -> &Self::Alloc {
        &self.allocator
    }

    fn memory(&self) -> &Self::Memory {
        &self.memory
    }

    fn process(&self) -> &Self::Process {
        &self.process
    }
}

/*
 * Linux API Surface Documentation
 *
 * This module uses the following Linux kernel APIs:
 *
 * | Category   | API              | Purpose              | Status       |
 * |------------|------------------|----------------------|--------------|
 * | Allocation | kmalloc          | Dynamic memory       | Placeholder  |
 * | Allocation | kfree            | Free memory          | Placeholder  |
 * | Memory     | vm_mmap          | Memory mapping       | Placeholder  |
 * | Memory     | vm_munmap        | Unmap memory         | Placeholder  |
 * | Memory     | do_mprotect      | Memory protection    | Placeholder  |
 * | Process    | do_exit          | Process termination  | Placeholder  |
 * | Process    | current          | Current task         | Placeholder  |
 *
 * As kVisor matures, these will be replaced with pure Rust implementations.
 */
