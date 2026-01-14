// SPDX-License-Identifier: GPL-2.0

//! Per-process kVisor context
//!
//! Each sandboxed process has a KvisorContext that contains all the
//! process-specific state managed by kVisor.

use kernel::prelude::*;

use crate::vfs::{FdTable, MemFs};

/// Memory context for a sandboxed process
pub struct MemoryContext {
    /// Current program break (heap end)
    brk: usize,
    /// Initial program break
    initial_brk: usize,
    /// Maximum program break
    max_brk: usize,
}

impl MemoryContext {
    /// Create a new memory context
    pub fn new() -> Self {
        // Default initial break - will be set properly during exec
        const DEFAULT_BRK: usize = 0x1000000; // 16 MB

        MemoryContext {
            brk: DEFAULT_BRK,
            initial_brk: DEFAULT_BRK,
            max_brk: DEFAULT_BRK + 0x10000000, // Allow 256 MB heap
        }
    }

    /// Get the current program break
    pub fn get_brk(&self) -> usize {
        self.brk
    }

    /// Set a new program break
    pub fn set_brk(&mut self, addr: usize) -> Result<usize> {
        // Cannot decrease below initial break
        if addr < self.initial_brk {
            return Ok(self.brk);
        }

        // Cannot increase above max break
        if addr > self.max_brk {
            return Err(ENOMEM);
        }

        self.brk = addr;
        Ok(self.brk)
    }
}

impl Default for MemoryContext {
    fn default() -> Self {
        Self::new()
    }
}

/// Per-process kVisor context
///
/// This structure contains all the state for a sandboxed process,
/// including its virtual filesystem view, file descriptor table,
/// memory map, and identity information.
pub struct KvisorContext {
    /// Process ID (as seen by the sandboxed process)
    pub pid: u32,
    /// Parent process ID
    pub ppid: u32,
    /// Thread ID
    pub tid: u32,
    /// User ID
    pub uid: u32,
    /// Group ID
    pub gid: u32,
    /// Effective user ID
    pub euid: u32,
    /// Effective group ID
    pub egid: u32,

    /// File descriptor table
    pub fd_table: FdTable,

    /// In-memory filesystem
    pub memfs: MemFs,

    /// Current working directory (inode number in memfs)
    pub cwd: u64,

    /// Memory context
    pub memory: MemoryContext,

    /// FS base register (for TLS)
    pub fs_base: u64,
    /// GS base register
    pub gs_base: u64,

    /// Pointer for clear_child_tid (set_tid_address)
    pub clear_child_tid: u64,

    /// Exit code (set when process exits)
    pub exit_code: Option<i32>,
}

impl KvisorContext {
    /// Create a new kVisor context for a process
    pub fn new() -> Result<Self> {
        let memfs = MemFs::new()?;
        let cwd = memfs.root();

        Ok(KvisorContext {
            pid: 1,
            ppid: 0,
            tid: 1,
            uid: 1000,
            gid: 1000,
            euid: 1000,
            egid: 1000,
            fd_table: FdTable::with_stdio(),
            memfs,
            cwd,
            memory: MemoryContext::new(),
            fs_base: 0,
            gs_base: 0,
            clear_child_tid: 0,
            exit_code: None,
        })
    }

    /// Fork the context for a child process
    pub fn fork(&self, child_pid: u32) -> Result<Self> {
        // Create new context with copied state
        let memfs = MemFs::new()?; // TODO: Implement copy-on-write
        let cwd = memfs.root();

        Ok(KvisorContext {
            pid: child_pid,
            ppid: self.pid,
            tid: child_pid,
            uid: self.uid,
            gid: self.gid,
            euid: self.euid,
            egid: self.egid,
            fd_table: FdTable::with_stdio(), // TODO: Copy fd table
            memfs,
            cwd,
            memory: MemoryContext::new(),
            fs_base: self.fs_base,
            gs_base: self.gs_base,
            clear_child_tid: 0,
            exit_code: None,
        })
    }

    /// Check if the process has exited
    pub fn has_exited(&self) -> bool {
        self.exit_code.is_some()
    }
}
