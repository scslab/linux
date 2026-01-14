// SPDX-License-Identifier: GPL-2.0

//! Platform abstraction layer for kVisor
//!
//! This module provides traits that abstract over the underlying platform
//! (Linux kernel). This allows for:
//! - Testing with mock implementations
//! - Incremental replacement of Linux dependencies with pure Rust
//! - Clear documentation of Linux API surface used by kVisor

pub mod linux;

use kernel::prelude::*;

/// Error type for allocation operations
#[derive(Debug)]
pub enum AllocError {
    /// Out of memory
    OutOfMemory,
    /// Invalid alignment
    InvalidAlignment,
}

/// Trait for memory allocation
pub trait Allocator {
    /// Allocate memory with the given size and alignment
    fn alloc(&self, size: usize, align: usize) -> Result<*mut u8, AllocError>;

    /// Free memory previously allocated with alloc
    ///
    /// # Safety
    /// The pointer must have been returned by a previous call to alloc
    /// with the same size and alignment.
    unsafe fn free(&self, ptr: *mut u8, size: usize, align: usize);

    /// Reallocate memory to a new size
    ///
    /// # Safety
    /// The pointer must have been returned by a previous call to alloc.
    unsafe fn realloc(
        &self,
        ptr: *mut u8,
        old_size: usize,
        new_size: usize,
        align: usize,
    ) -> Result<*mut u8, AllocError>;
}

/// Memory protection flags
#[derive(Debug, Clone, Copy)]
pub struct Prot(pub u32);

impl Prot {
    pub const NONE: Prot = Prot(0);
    pub const READ: Prot = Prot(1);
    pub const WRITE: Prot = Prot(2);
    pub const EXEC: Prot = Prot(4);

    pub fn readable(&self) -> bool {
        self.0 & 1 != 0
    }

    pub fn writable(&self) -> bool {
        self.0 & 2 != 0
    }

    pub fn executable(&self) -> bool {
        self.0 & 4 != 0
    }
}

/// Memory mapping flags
#[derive(Debug, Clone, Copy)]
pub struct MapFlags(pub u32);

impl MapFlags {
    pub const SHARED: MapFlags = MapFlags(1);
    pub const PRIVATE: MapFlags = MapFlags(2);
    pub const ANONYMOUS: MapFlags = MapFlags(0x20);
    pub const FIXED: MapFlags = MapFlags(0x10);

    pub fn is_shared(&self) -> bool {
        self.0 & 1 != 0
    }

    pub fn is_private(&self) -> bool {
        self.0 & 2 != 0
    }

    pub fn is_anonymous(&self) -> bool {
        self.0 & 0x20 != 0
    }

    pub fn is_fixed(&self) -> bool {
        self.0 & 0x10 != 0
    }
}

/// Trait for memory operations
pub trait MemoryOps {
    /// Map memory
    fn mmap(
        &self,
        addr: Option<usize>,
        len: usize,
        prot: Prot,
        flags: MapFlags,
    ) -> Result<usize>;

    /// Unmap memory
    fn munmap(&self, addr: usize, len: usize) -> Result;

    /// Change memory protection
    fn mprotect(&self, addr: usize, len: usize, prot: Prot) -> Result;
}

/// Process ID type
pub type Pid = i32;

/// Clone flags for process creation
#[derive(Debug, Clone, Copy)]
pub struct CloneFlags(pub u64);

/// Trait for process operations
pub trait ProcessOps {
    /// Exit the current process
    fn exit(&self, code: i32) -> !;

    /// Get the current process ID
    fn get_pid(&self) -> Pid;

    /// Get the current thread ID
    fn get_tid(&self) -> Pid;
}

/// The platform interface bundles all traits
pub trait Platform {
    type Alloc: Allocator;
    type Memory: MemoryOps;
    type Process: ProcessOps;

    fn allocator(&self) -> &Self::Alloc;
    fn memory(&self) -> &Self::Memory;
    fn process(&self) -> &Self::Process;
}

// Re-export the Linux platform as the default
#[allow(unused_imports)]
pub use linux::LinuxPlatform;
