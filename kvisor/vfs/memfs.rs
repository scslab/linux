// SPDX-License-Identifier: GPL-2.0

#![allow(unused_must_use)]

//! In-memory filesystem for kVisor
//!
//! Provides a tmpfs-like filesystem for sandboxed processes,
//! completely isolated from the host filesystem.

use kernel::prelude::*;

use super::{FileStat, FileType, FileMode};

/// Maximum path length
const PATH_MAX: usize = 4096;

/// Maximum number of inodes
const MAX_INODES: usize = 65536;

/// Inode number type
type InodeNum = u64;

/// An inode in the memory filesystem
pub struct Inode {
    /// Inode number
    pub ino: InodeNum,
    /// File type
    pub file_type: FileType,
    /// File mode (permissions)
    pub mode: FileMode,
    /// Owner UID
    pub uid: u32,
    /// Owner GID
    pub gid: u32,
    /// File size
    pub size: u64,
    /// Link count
    pub nlink: u64,
    /// File data (for regular files)
    pub data: Option<KVec<u8>>,
    /// Directory entries (for directories)
    pub entries: Option<KVec<DirEntry>>,
    /// Symlink target (for symlinks)
    pub symlink_target: Option<KVec<u8>>,
}

/// A directory entry
pub struct DirEntry {
    /// Entry name
    pub name: KVec<u8>,
    /// Inode number
    pub ino: InodeNum,
}

/// The in-memory filesystem
pub struct MemFs {
    /// All inodes
    inodes: KVec<Option<Inode>>,
    /// Next inode number to allocate
    next_ino: InodeNum,
    /// Root directory inode number
    root_ino: InodeNum,
}

impl MemFs {
    /// Create a new empty filesystem
    pub fn new() -> Result<Self> {
        let mut inodes = KVec::new();

        // Pre-allocate space for inodes
        for _ in 0..MAX_INODES {
            inodes.push(None, GFP_KERNEL)?;
        }

        let mut fs = MemFs {
            inodes,
            next_ino: 1,
            root_ino: 0,
        };

        // Create root directory
        fs.root_ino = fs.create_directory(FileMode::new(0o755), 0, 0)?;

        Ok(fs)
    }

    /// Allocate a new inode number
    fn alloc_ino(&mut self) -> Result<InodeNum> {
        let ino = self.next_ino;
        if ino as usize >= MAX_INODES {
            return Err(ENOSPC);
        }
        self.next_ino += 1;
        Ok(ino)
    }

    /// Get an inode by number
    pub fn get_inode(&self, ino: InodeNum) -> Option<&Inode> {
        if ino as usize >= MAX_INODES {
            return None;
        }
        self.inodes[ino as usize].as_ref()
    }

    /// Get a mutable inode by number
    pub fn get_inode_mut(&mut self, ino: InodeNum) -> Option<&mut Inode> {
        if ino as usize >= MAX_INODES {
            return None;
        }
        self.inodes[ino as usize].as_mut()
    }

    /// Create a new directory
    pub fn create_directory(&mut self, mode: FileMode, uid: u32, gid: u32) -> Result<InodeNum> {
        let ino = self.alloc_ino()?;

        let mut entries = KVec::new();

        // Add . and .. entries
        let mut dot_name = KVec::new();
        dot_name.push(b'.', GFP_KERNEL)?;
        entries.push(DirEntry { name: dot_name, ino }, GFP_KERNEL)?;

        let mut dotdot_name = KVec::new();
        dotdot_name.push(b'.', GFP_KERNEL)?;
        dotdot_name.push(b'.', GFP_KERNEL)?;
        entries.push(DirEntry { name: dotdot_name, ino }, GFP_KERNEL)?;

        let inode = Inode {
            ino,
            file_type: FileType::Directory,
            mode,
            uid,
            gid,
            size: 0,
            nlink: 2,
            data: None,
            entries: Some(entries),
            symlink_target: None,
        };

        self.inodes[ino as usize] = Some(inode);
        Ok(ino)
    }

    /// Create a new regular file
    pub fn create_file(&mut self, mode: FileMode, uid: u32, gid: u32) -> Result<InodeNum> {
        let ino = self.alloc_ino()?;

        let inode = Inode {
            ino,
            file_type: FileType::Regular,
            mode,
            uid,
            gid,
            size: 0,
            nlink: 1,
            data: Some(KVec::new()),
            entries: None,
            symlink_target: None,
        };

        self.inodes[ino as usize] = Some(inode);
        Ok(ino)
    }

    /// Look up a name in a directory
    pub fn lookup(&self, dir_ino: InodeNum, name: &[u8]) -> Option<InodeNum> {
        let dir = self.get_inode(dir_ino)?;

        if dir.file_type != FileType::Directory {
            return None;
        }

        let entries = dir.entries.as_ref()?;
        for entry in entries.iter() {
            if entry.name.as_slice() == name {
                return Some(entry.ino);
            }
        }

        None
    }

    /// Add an entry to a directory
    pub fn add_entry(&mut self, dir_ino: InodeNum, name: &[u8], ino: InodeNum) -> Result {
        let dir = self.get_inode_mut(dir_ino).ok_or(ENOENT)?;

        if dir.file_type != FileType::Directory {
            return Err(ENOTDIR);
        }

        let entries = dir.entries.as_mut().ok_or(ENOTDIR)?;

        // Check if name already exists
        for entry in entries.iter() {
            if entry.name.as_slice() == name {
                return Err(EEXIST);
            }
        }

        // Add new entry
        let mut entry_name = KVec::new();
        for &b in name {
            entry_name.push(b, GFP_KERNEL)?;
        }
        entries.push(DirEntry { name: entry_name, ino }, GFP_KERNEL)?;

        Ok(())
    }

    /// Remove an entry from a directory
    pub fn remove_entry(&mut self, dir_ino: InodeNum, name: &[u8]) -> Result<InodeNum> {
        let dir = self.get_inode_mut(dir_ino).ok_or(ENOENT)?;

        if dir.file_type != FileType::Directory {
            return Err(ENOTDIR);
        }

        let entries = dir.entries.as_mut().ok_or(ENOTDIR)?;

        // Find and remove entry
        let mut found_idx = None;
        let mut found_ino = 0;
        for (idx, entry) in entries.iter().enumerate() {
            if entry.name.as_slice() == name {
                found_idx = Some(idx);
                found_ino = entry.ino;
                break;
            }
        }

        match found_idx {
            Some(idx) => {
                entries.remove(idx);
                Ok(found_ino)
            }
            None => Err(ENOENT),
        }
    }

    /// Get file statistics
    pub fn stat(&self, ino: InodeNum) -> Option<FileStat> {
        let inode = self.get_inode(ino)?;

        let mode = match inode.file_type {
            FileType::Regular => 0o100000,
            FileType::Directory => 0o040000,
            FileType::Symlink => 0o120000,
            FileType::CharDevice => 0o020000,
            FileType::BlockDevice => 0o060000,
            FileType::Fifo => 0o010000,
            FileType::Socket => 0o140000,
        } | inode.mode.0;

        Some(FileStat {
            dev: 0,
            ino: inode.ino,
            mode,
            nlink: inode.nlink,
            uid: inode.uid,
            gid: inode.gid,
            rdev: 0,
            size: inode.size as i64,
            blksize: 4096,
            blocks: ((inode.size + 511) / 512) as i64,
            ..Default::default()
        })
    }

    /// Read from a file
    pub fn read(&self, ino: InodeNum, offset: u64, buf: &mut [u8]) -> Result<usize> {
        let inode = self.get_inode(ino).ok_or(ENOENT)?;

        if inode.file_type != FileType::Regular {
            return Err(EISDIR);
        }

        let data = inode.data.as_ref().ok_or(EIO)?;

        if offset >= inode.size {
            return Ok(0);
        }

        let start = offset as usize;
        let end = core::cmp::min(start + buf.len(), inode.size as usize);
        let len = end - start;

        buf[..len].copy_from_slice(&data.as_slice()[start..end]);
        Ok(len)
    }

    /// Write to a file
    pub fn write(&mut self, ino: InodeNum, offset: u64, buf: &[u8]) -> Result<usize> {
        let inode = self.get_inode_mut(ino).ok_or(ENOENT)?;

        if inode.file_type != FileType::Regular {
            return Err(EISDIR);
        }

        let data = inode.data.as_mut().ok_or(EIO)?;

        let end = offset as usize + buf.len();

        // Extend file if necessary
        while data.len() < end {
            data.push(0, GFP_KERNEL)?;
        }

        // Write data
        let start = offset as usize;
        data.as_mut_slice()[start..end].copy_from_slice(buf);

        // Update size
        if end as u64 > inode.size {
            inode.size = end as u64;
        }

        Ok(buf.len())
    }

    /// Get root directory inode number
    pub fn root(&self) -> InodeNum {
        self.root_ino
    }
}

impl Default for MemFs {
    fn default() -> Self {
        Self::new().expect("Failed to create memfs")
    }
}
