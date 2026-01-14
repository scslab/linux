// SPDX-License-Identifier: GPL-2.0

//! Virtual Filesystem for kVisor
//!
//! This module provides a virtual filesystem layer that isolates sandboxed
//! processes from the host filesystem.

use kernel::prelude::*;

pub mod fd_table;
pub mod memfs;

pub use fd_table::FdTable;
pub use memfs::MemFs;

/// File types in the virtual filesystem
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FileType {
    /// Regular file
    Regular,
    /// Directory
    Directory,
    /// Symbolic link
    Symlink,
    /// Character device
    CharDevice,
    /// Block device
    BlockDevice,
    /// FIFO (named pipe)
    Fifo,
    /// Socket
    Socket,
}

/// File open flags
#[derive(Debug, Clone, Copy)]
pub struct OpenFlags {
    pub read: bool,
    pub write: bool,
    pub append: bool,
    pub create: bool,
    pub truncate: bool,
    pub exclusive: bool,
    pub nonblock: bool,
    pub cloexec: bool,
}

impl OpenFlags {
    /// Parse flags from the Linux O_* constants
    pub fn from_raw(flags: i32) -> Self {
        const O_RDONLY: i32 = 0;
        const O_WRONLY: i32 = 1;
        const O_RDWR: i32 = 2;
        const O_CREAT: i32 = 0o100;
        const O_EXCL: i32 = 0o200;
        const O_TRUNC: i32 = 0o1000;
        const O_APPEND: i32 = 0o2000;
        const O_NONBLOCK: i32 = 0o4000;
        const O_CLOEXEC: i32 = 0o2000000;

        let access_mode = flags & 3;

        OpenFlags {
            read: access_mode == O_RDONLY || access_mode == O_RDWR,
            write: access_mode == O_WRONLY || access_mode == O_RDWR,
            append: flags & O_APPEND != 0,
            create: flags & O_CREAT != 0,
            truncate: flags & O_TRUNC != 0,
            exclusive: flags & O_EXCL != 0,
            nonblock: flags & O_NONBLOCK != 0,
            cloexec: flags & O_CLOEXEC != 0,
        }
    }
}

/// File permissions
#[derive(Debug, Clone, Copy)]
pub struct FileMode(pub u32);

impl FileMode {
    pub fn new(mode: u32) -> Self {
        FileMode(mode & 0o7777)
    }

    pub fn user_read(&self) -> bool {
        self.0 & 0o400 != 0
    }

    pub fn user_write(&self) -> bool {
        self.0 & 0o200 != 0
    }

    pub fn user_exec(&self) -> bool {
        self.0 & 0o100 != 0
    }
}

/// File metadata (stat structure)
#[derive(Debug, Clone)]
pub struct FileStat {
    pub dev: u64,
    pub ino: u64,
    pub mode: u32,
    pub nlink: u64,
    pub uid: u32,
    pub gid: u32,
    pub rdev: u64,
    pub size: i64,
    pub blksize: i64,
    pub blocks: i64,
    pub atime_sec: i64,
    pub atime_nsec: i64,
    pub mtime_sec: i64,
    pub mtime_nsec: i64,
    pub ctime_sec: i64,
    pub ctime_nsec: i64,
}

impl Default for FileStat {
    fn default() -> Self {
        FileStat {
            dev: 0,
            ino: 0,
            mode: 0,
            nlink: 1,
            uid: 0,
            gid: 0,
            rdev: 0,
            size: 0,
            blksize: 4096,
            blocks: 0,
            atime_sec: 0,
            atime_nsec: 0,
            mtime_sec: 0,
            mtime_nsec: 0,
            ctime_sec: 0,
            ctime_nsec: 0,
        }
    }
}

/// A handle to an open file
pub struct FileHandle {
    /// The file type
    pub file_type: FileType,
    /// Current file offset
    pub offset: u64,
    /// Open flags
    pub flags: OpenFlags,
    /// Reference to the underlying file data (for memfs)
    pub inode: u64,
}

impl FileHandle {
    pub fn new(file_type: FileType, flags: OpenFlags, inode: u64) -> Self {
        FileHandle {
            file_type,
            offset: 0,
            flags,
            inode,
        }
    }
}

/// Initialize the VFS subsystem
pub fn init() -> Result {
    pr_debug!("kvisor: VFS subsystem initialized\n");
    Ok(())
}

/// Cleanup the VFS subsystem
pub fn cleanup() {
    pr_debug!("kvisor: VFS subsystem cleanup\n");
}
