// SPDX-License-Identifier: GPL-2.0

//! File descriptor table for kVisor processes
//!
//! Each sandboxed process maintains its own file descriptor table,
//! separate from the host kernel's fd table.

use kernel::prelude::*;

use super::FileHandle;

/// Maximum number of file descriptors per process
const MAX_FDS: usize = 1024;

/// Error type for fd table operations
#[derive(Debug)]
pub enum FdError {
    /// File descriptor is invalid
    BadFd,
    /// Too many open files
    TooManyFiles,
    /// Invalid argument
    InvalidArg,
}

/// Per-process file descriptor table
pub struct FdTable {
    /// Array of file descriptors (None = not in use)
    fds: [Option<FileHandle>; MAX_FDS],
    /// Next fd to try for allocation
    next_fd: usize,
}

impl FdTable {
    /// Create a new empty file descriptor table
    pub fn new() -> Self {
        // Initialize with no open files
        // We use a const array initializer since FileHandle doesn't implement Copy
        const NONE: Option<FileHandle> = None;
        FdTable {
            fds: [NONE; MAX_FDS],
            next_fd: 0,
        }
    }

    /// Create a file descriptor table with standard streams pre-opened
    pub fn with_stdio() -> Self {
        let mut table = Self::new();

        // TODO: Set up stdin, stdout, stderr
        // For now, leave them as None - they'll need to be connected
        // to the appropriate I/O channels

        let _ = &mut table;
        table
    }

    /// Allocate a new file descriptor
    pub fn alloc(&mut self, handle: FileHandle) -> Result<i32, FdError> {
        // Find first available fd starting from next_fd
        for i in 0..MAX_FDS {
            let fd = (self.next_fd + i) % MAX_FDS;
            if self.fds[fd].is_none() {
                self.fds[fd] = Some(handle);
                self.next_fd = (fd + 1) % MAX_FDS;
                return Ok(fd as i32);
            }
        }
        Err(FdError::TooManyFiles)
    }

    /// Allocate a specific file descriptor
    pub fn alloc_at(&mut self, fd: i32, handle: FileHandle) -> Result<i32, FdError> {
        if fd < 0 || fd >= MAX_FDS as i32 {
            return Err(FdError::InvalidArg);
        }

        let fd = fd as usize;

        // Close existing fd if any
        self.fds[fd] = Some(handle);
        Ok(fd as i32)
    }

    /// Get a reference to a file handle
    pub fn get(&self, fd: i32) -> Option<&FileHandle> {
        if fd < 0 || fd >= MAX_FDS as i32 {
            return None;
        }
        self.fds[fd as usize].as_ref()
    }

    /// Get a mutable reference to a file handle
    pub fn get_mut(&mut self, fd: i32) -> Option<&mut FileHandle> {
        if fd < 0 || fd >= MAX_FDS as i32 {
            return None;
        }
        self.fds[fd as usize].as_mut()
    }

    /// Close a file descriptor
    pub fn close(&mut self, fd: i32) -> Result<(), FdError> {
        if fd < 0 || fd >= MAX_FDS as i32 {
            return Err(FdError::BadFd);
        }

        let fd = fd as usize;
        if self.fds[fd].is_none() {
            return Err(FdError::BadFd);
        }

        self.fds[fd] = None;
        Ok(())
    }

    /// Duplicate a file descriptor (dup)
    pub fn dup(&mut self, oldfd: i32) -> Result<i32, FdError> {
        if oldfd < 0 || oldfd >= MAX_FDS as i32 {
            return Err(FdError::BadFd);
        }

        let old = oldfd as usize;
        let handle = match &self.fds[old] {
            Some(h) => FileHandle {
                file_type: h.file_type,
                offset: h.offset,
                flags: h.flags,
                inode: h.inode,
            },
            None => return Err(FdError::BadFd),
        };

        self.alloc(handle)
    }

    /// Duplicate a file descriptor to a specific fd (dup2)
    pub fn dup2(&mut self, oldfd: i32, newfd: i32) -> Result<i32, FdError> {
        if oldfd < 0 || oldfd >= MAX_FDS as i32 {
            return Err(FdError::BadFd);
        }
        if newfd < 0 || newfd >= MAX_FDS as i32 {
            return Err(FdError::InvalidArg);
        }

        // If oldfd == newfd, just return newfd
        if oldfd == newfd {
            if self.fds[oldfd as usize].is_none() {
                return Err(FdError::BadFd);
            }
            return Ok(newfd);
        }

        let old = oldfd as usize;
        let handle = match &self.fds[old] {
            Some(h) => FileHandle {
                file_type: h.file_type,
                offset: h.offset,
                flags: h.flags,
                inode: h.inode,
            },
            None => return Err(FdError::BadFd),
        };

        self.alloc_at(newfd, handle)
    }

    /// Close all file descriptors with CLOEXEC flag
    pub fn close_cloexec(&mut self) {
        for fd in &mut self.fds {
            if let Some(handle) = fd {
                if handle.flags.cloexec {
                    *fd = None;
                }
            }
        }
    }
}

impl Default for FdTable {
    fn default() -> Self {
        Self::new()
    }
}
