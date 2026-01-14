// SPDX-License-Identifier: GPL-2.0

//! Process execution for kVisor
//!
//! This module handles spawning new processes under kVisor supervision.
//! It implements ELF loading, memory mapping, and process initialization.

use kernel::prelude::*;
use kernel::uaccess::UserPtr;

mod elf;

#[allow(unused_imports)]
pub use elf::ElfLoader;

/// Result of kvisor_exec
pub type ExecResult = core::result::Result<i32, ExecError>;

/// Errors that can occur during exec
#[derive(Debug)]
pub enum ExecError {
    /// Invalid path
    InvalidPath,
    /// File not found
    NotFound,
    /// Permission denied
    PermissionDenied,
    /// Invalid ELF format
    InvalidElf,
    /// Out of memory
    OutOfMemory,
    /// I/O error
    IoError,
    /// Not implemented
    NotImplemented,
}

impl From<ExecError> for i64 {
    fn from(e: ExecError) -> i64 {
        match e {
            ExecError::InvalidPath => -14,  // EFAULT
            ExecError::NotFound => -2,      // ENOENT
            ExecError::PermissionDenied => -13, // EACCES
            ExecError::InvalidElf => -8,    // ENOEXEC
            ExecError::OutOfMemory => -12,  // ENOMEM
            ExecError::IoError => -5,       // EIO
            ExecError::NotImplemented => -38, // ENOSYS
        }
    }
}

/// Execute a program in the kVisor sandbox
///
/// This is the main entry point for spawning sandboxed processes.
/// The function:
/// 1. Reads and validates the ELF binary
/// 2. Creates a new kVisor context
/// 3. Sets up the virtual address space
/// 4. Loads the ELF segments
/// 5. Sets up the stack with argv/envp
/// 6. Returns to userspace at the ELF entry point
///
/// # Arguments
/// * `path` - User pointer to the path of the executable
/// * `argv` - User pointer to the argument vector (NULL-terminated)
/// * `envp` - User pointer to the environment vector (NULL-terminated)
///
/// # Returns
/// * On success: PID of the new sandboxed process
/// * On error: Negative errno value
pub fn kvisor_exec(
    path: UserPtr,
    argv: UserPtr,
    envp: UserPtr,
) -> ExecResult {
    pr_info!("kvisor: exec called\n");

    // TODO: Implementation steps:
    //
    // 1. Copy path from userspace
    //    - Use copy_from_user or similar to safely read the path
    //
    // 2. Open and read the ELF file
    //    - For now, we'd need to use Linux VFS to read the file
    //    - Later, this could use kVisor's own VFS
    //
    // 3. Parse ELF headers
    //    - Validate ELF magic, architecture, etc.
    //    - Extract program headers for loading
    //
    // 4. Fork a new process (or create from scratch)
    //    - Option A: Use Linux clone() then mark as sandboxed
    //    - Option B: Create process entirely in kVisor (more complex)
    //
    // 5. Set up address space
    //    - Map ELF segments with correct permissions
    //    - Set up stack
    //    - Set up heap (brk)
    //
    // 6. Copy argv/envp to new process stack
    //    - Follow Linux ABI for stack layout
    //
    // 7. Mark process as sandboxed
    //    - Call kvisor_set_sandboxed() equivalent
    //
    // 8. Set entry point and return
    //    - Set instruction pointer to ELF entry
    //    - Return PID to parent

    let _ = (path, argv, envp);

    Err(ExecError::NotImplemented)
}
