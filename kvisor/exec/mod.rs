// SPDX-License-Identifier: GPL-2.0

//! Process execution for kVisor
//!
//! This module handles spawning new processes under kVisor supervision.
//! It implements ELF loading, memory mapping, and process initialization.

use kernel::prelude::*;
use kernel::uaccess::{UserPtr, UserSlice};

mod elf;

pub use elf::{ElfLoader, PhdrType, phdr_flags};

/// FFI declarations for C helpers
mod ffi {
    use core::ffi::{c_char, c_int, c_ulong, c_void};

    unsafe extern "C" {
        // File helpers
        pub fn kvisor_file_read_alloc(
            path: *const c_char,
            buf_out: *mut *mut c_void,
            size_out: *mut usize,
        ) -> c_int;

        pub fn kvisor_file_free(buf: *mut c_void);

        // Process helpers
        pub fn kvisor_setup_entry(entry: c_ulong, sp: c_ulong) -> c_int;
        pub fn kvisor_mark_sandboxed();
        pub fn kvisor_get_current_pid() -> c_int;

        // Memory mapping helpers
        pub fn kvisor_mmap(
            addr: c_ulong,
            len: c_ulong,
            prot: c_ulong,
            flags: c_ulong,
        ) -> c_ulong;

        pub fn kvisor_munmap(addr: c_ulong, len: c_ulong) -> c_int;

        pub fn kvisor_copy_to_user_addr(
            to: c_ulong,
            from: *const c_void,
            len: c_ulong,
        ) -> c_int;

        pub fn kvisor_clear_user_addr(addr: c_ulong, len: c_ulong) -> c_int;

        pub fn kvisor_mprotect(addr: c_ulong, len: c_ulong, prot: c_ulong) -> c_int;

        pub fn kvisor_flush_old_exec() -> c_int;
    }
}

/// Maximum path length
const PATH_MAX: usize = 4096;

/// Default stack size (8 MB)
const STACK_SIZE: u64 = 8 * 1024 * 1024;

/// Stack top address
/// Using a lower address (127TB) to avoid conflicts with the parent's stack
/// which is typically near 0x7fff_xxxx_xxxx.
const STACK_TOP: u64 = 0x7f00_0000_0000;

/// Page size
const PAGE_SIZE: u64 = 4096;

/// Protection flags
mod prot {
    pub const READ: u64 = 0x1;
    pub const WRITE: u64 = 0x2;
    pub const EXEC: u64 = 0x4;
}

/// Map flags
mod map_flags {
    pub const PRIVATE: u64 = 0x02;
    pub const FIXED: u64 = 0x10;
    pub const ANONYMOUS: u64 = 0x20;
}

/// Result of kvisor_exec
pub type ExecResult = core::result::Result<i32, ExecError>;

/// Errors that can occur during exec
#[derive(Debug)]
pub enum ExecError {
    InvalidPath,
    NotFound,
    PermissionDenied,
    InvalidElf,
    OutOfMemory,
    IoError,
    NotImplemented,
    ForkFailed,
    MmapFailed,
}

impl From<ExecError> for i64 {
    fn from(e: ExecError) -> i64 {
        match e {
            ExecError::InvalidPath => -14,      // EFAULT
            ExecError::NotFound => -2,          // ENOENT
            ExecError::PermissionDenied => -13, // EACCES
            ExecError::InvalidElf => -8,        // ENOEXEC
            ExecError::OutOfMemory => -12,      // ENOMEM
            ExecError::IoError => -5,           // EIO
            ExecError::NotImplemented => -38,   // ENOSYS
            ExecError::ForkFailed => -11,       // EAGAIN
            ExecError::MmapFailed => -12,       // ENOMEM
        }
    }
}

/// Copy a NUL-terminated path string from userspace.
fn copy_path_from_user(path_ptr: UserPtr) -> Result<KVec<u8>, ExecError> {
    let mut buf: KVec<u8> = KVec::new();
    buf.resize(PATH_MAX, 0, GFP_KERNEL)
        .map_err(|_| ExecError::OutOfMemory)?;

    let reader = UserSlice::new(path_ptr, PATH_MAX).reader();
    let cstr = reader
        .strcpy_into_buf(&mut buf)
        .map_err(|_| ExecError::InvalidPath)?;

    let len = cstr.len_with_nul();
    buf.truncate(len);
    Ok(buf)
}

/// RAII wrapper for file data allocated by C helper.
struct FileData {
    ptr: *mut core::ffi::c_void,
    len: usize,
}

impl FileData {
    fn as_slice(&self) -> &[u8] {
        if self.ptr.is_null() || self.len == 0 {
            &[]
        } else {
            unsafe { core::slice::from_raw_parts(self.ptr as *const u8, self.len) }
        }
    }
}

impl Drop for FileData {
    fn drop(&mut self) {
        if !self.ptr.is_null() {
            unsafe { ffi::kvisor_file_free(self.ptr) };
        }
    }
}

/// Read an ELF file from the filesystem.
fn read_elf_file(path: &[u8]) -> Result<FileData, ExecError> {
    let mut buf_ptr: *mut core::ffi::c_void = core::ptr::null_mut();
    let mut size: usize = 0;

    if path.is_empty() || path[path.len() - 1] != 0 {
        return Err(ExecError::InvalidPath);
    }

    let ret = unsafe {
        ffi::kvisor_file_read_alloc(
            path.as_ptr() as *const core::ffi::c_char,
            &mut buf_ptr,
            &mut size,
        )
    };

    if ret < 0 {
        return match ret {
            -2 => Err(ExecError::NotFound),
            -13 => Err(ExecError::PermissionDenied),
            -12 => Err(ExecError::OutOfMemory),
            _ => Err(ExecError::IoError),
        };
    }

    Ok(FileData { ptr: buf_ptr, len: size })
}

/// Align address down to page boundary.
fn page_align_down(addr: u64) -> u64 {
    addr & !(PAGE_SIZE - 1)
}

/// Align address up to page boundary.
fn page_align_up(addr: u64) -> u64 {
    (addr + PAGE_SIZE - 1) & !(PAGE_SIZE - 1)
}

/// Convert ELF protection flags to mmap protection flags.
fn elf_prot_to_mmap(elf_flags: u32) -> u64 {
    let mut prot_flags = 0u64;
    if elf_flags & phdr_flags::PF_R != 0 {
        prot_flags |= prot::READ;
    }
    if elf_flags & phdr_flags::PF_W != 0 {
        prot_flags |= prot::WRITE;
    }
    if elf_flags & phdr_flags::PF_X != 0 {
        prot_flags |= prot::EXEC;
    }
    prot_flags
}

/// Load ELF segments into the address space.
fn load_elf_segments(loader: &ElfLoader, elf_data: &[u8], load_bias: u64) -> Result<(), ExecError> {
    for phdr in loader.load_segments() {
        // Calculate page-aligned boundaries
        let seg_start = phdr.p_vaddr + load_bias;
        let seg_end = seg_start + phdr.p_memsz;
        let map_start = page_align_down(seg_start);
        let map_end = page_align_up(seg_end);
        let map_size = map_end - map_start;

        if map_size == 0 {
            continue;
        }

        pr_info!(
            "kvisor: Loading segment: vaddr={:#x}, memsz={:#x}, filesz={:#x}\n",
            phdr.p_vaddr,
            phdr.p_memsz,
            phdr.p_filesz
        );

        // Map anonymous memory for the segment with RW permissions for loading
        // (we need to be able to write the data)
        let flags = map_flags::PRIVATE | map_flags::ANONYMOUS | map_flags::FIXED;
        let final_prot = elf_prot_to_mmap(phdr.p_flags);
        // Always include WRITE permission initially so we can copy data
        let initial_prot = final_prot | prot::WRITE;

        let mapped = unsafe {
            ffi::kvisor_mmap(map_start, map_size, initial_prot, flags)
        };

        // Check for mmap failure (returns address on success, negative on error)
        if (mapped as i64) < 0 || mapped != map_start {
            pr_err!("kvisor: Failed to map segment at {:#x}, got {:#x}\n", map_start, mapped);
            return Err(ExecError::MmapFailed);
        }

        // Copy file data to the segment
        if phdr.p_filesz > 0 {
            let file_offset = phdr.p_offset as usize;
            let file_size = phdr.p_filesz as usize;

            if file_offset + file_size > elf_data.len() {
                pr_err!("kvisor: Segment data exceeds file bounds\n");
                return Err(ExecError::InvalidElf);
            }

            let src = &elf_data[file_offset..file_offset + file_size];
            let ret = unsafe {
                ffi::kvisor_copy_to_user_addr(
                    seg_start,
                    src.as_ptr() as *const core::ffi::c_void,
                    file_size as u64,
                )
            };

            if ret != 0 {
                pr_err!("kvisor: Failed to copy segment data\n");
                return Err(ExecError::MmapFailed);
            }
        }

        // Zero BSS portion (memsz > filesz)
        if phdr.p_memsz > phdr.p_filesz {
            let bss_start = seg_start + phdr.p_filesz;
            let bss_size = phdr.p_memsz - phdr.p_filesz;
            let ret = unsafe { ffi::kvisor_clear_user_addr(bss_start, bss_size) };
            if ret != 0 {
                pr_err!("kvisor: Failed to clear BSS\n");
                return Err(ExecError::MmapFailed);
            }
        }

        // Set final protection if it's different from initial (remove WRITE if needed)
        // Note: mprotect is currently a no-op, so segments will remain writable
        // This is OK for Phase 1 testing
        if initial_prot != final_prot {
            let _ret = unsafe { ffi::kvisor_mprotect(map_start, map_size, final_prot) };
            // Ignore error for now since mprotect is a no-op
        }
    }

    Ok(())
}

/// Set up the process stack with argv, envp, and auxv.
///
/// Stack layout (growing downward):
/// ```text
/// High addresses:
///   - Argument strings
///   - Environment strings
///   - Padding for alignment
///   - Auxiliary vector (NULL terminated)
///   - envp[] (NULL terminated)
///   - argv[] (NULL terminated)
///   - argc
/// Low addresses (initial SP)
/// ```
fn setup_stack(
    _argv: UserPtr,
    _envp: UserPtr,
    entry: u64,
    _phdr_addr: u64,
    _phnum: u16,
) -> Result<u64, ExecError> {
    // Map stack
    let stack_bottom = STACK_TOP - STACK_SIZE;
    let flags = map_flags::PRIVATE | map_flags::ANONYMOUS | map_flags::FIXED;

    let mapped = unsafe {
        ffi::kvisor_mmap(stack_bottom, STACK_SIZE, prot::READ | prot::WRITE, flags)
    };

    if (mapped as i64) < 0 || mapped != stack_bottom {
        pr_err!("kvisor: Failed to map stack\n");
        return Err(ExecError::MmapFailed);
    }

    pr_info!("kvisor: Stack mapped at {:#x}-{:#x}\n", stack_bottom, STACK_TOP);

    // For Phase 1, we'll set up a minimal stack with just argc=0
    // TODO: Parse argv/envp from userspace and copy to stack

    // Build stack content
    let mut stack_data: [u64; 32] = [0; 32];
    let mut idx = 0;

    // argc
    stack_data[idx] = 0; // argc = 0 for now
    idx += 1;

    // argv[0] = NULL (end of argv)
    stack_data[idx] = 0;
    idx += 1;

    // envp[0] = NULL (end of envp)
    stack_data[idx] = 0;
    idx += 1;

    // Auxiliary vector
    // AT_NULL (end of auxv)
    stack_data[idx] = 0; // AT_NULL
    idx += 1;
    stack_data[idx] = 0;
    idx += 1;

    let _ = entry; // Will use for AT_ENTRY later

    // Calculate stack pointer (16-byte aligned)
    let stack_content_size = (idx * 8) as u64;
    let sp = (STACK_TOP - stack_content_size) & !15;

    // Copy stack data
    let ret = unsafe {
        ffi::kvisor_copy_to_user_addr(
            sp,
            stack_data.as_ptr() as *const core::ffi::c_void,
            stack_content_size,
        )
    };

    if ret != 0 {
        pr_err!("kvisor: Failed to set up stack\n");
        return Err(ExecError::MmapFailed);
    }

    pr_info!("kvisor: Stack pointer set to {:#x}\n", sp);
    Ok(sp)
}

/// Execute a program in the kVisor sandbox
///
/// This is an execve-like syscall that replaces the current process image
/// with a new program and marks it as sandboxed. The calling process should
/// have already forked; this syscall does not return on success.
///
/// Usage from userspace:
/// ```c
/// pid_t pid = fork();
/// if (pid == 0) {
///     // Child
///     kvisor_exec(path, argv, envp);  // Does not return on success
///     _exit(1);  // Only reached on error
/// }
/// // Parent
/// waitpid(pid, &status, 0);
/// ```
///
/// On success, this syscall does not return - execution jumps to the
/// new program's entry point. On error, returns a negative errno.
pub fn kvisor_exec(
    path: UserPtr,
    argv: UserPtr,
    envp: UserPtr,
) -> ExecResult {
    pr_info!("kvisor: exec called for pid {}\n", unsafe { ffi::kvisor_get_current_pid() });

    // Step 1: Copy path from userspace
    let path_buf = copy_path_from_user(path)?;
    pr_info!("kvisor: path copied from userspace\n");

    // Step 2: Read the ELF file
    let elf_data = read_elf_file(&path_buf)?;
    pr_info!("kvisor: ELF file read, size = {} bytes\n", elf_data.len);

    // Step 3: Parse and validate ELF
    let elf_loader = ElfLoader::parse(elf_data.as_slice())
        .map_err(|_| ExecError::InvalidElf)?;

    pr_info!("kvisor: ELF parsed, entry = {:#x}\n", elf_loader.entry);
    pr_info!("kvisor: {} program headers\n", elf_loader.phdrs.len());

    // Check for PT_INTERP (dynamic linking) - not supported yet
    for phdr in elf_loader.phdrs.iter() {
        if phdr.p_type == PhdrType::Interp as u32 {
            pr_err!("kvisor: Dynamic executables not supported yet\n");
            return Err(ExecError::NotImplemented);
        }
    }

    // Step 4: Flush old address space
    // This creates a new mm_struct and discards all inherited mappings
    // from the parent process. Essential for:
    //   - Clearing stale TLB entries
    //   - Releasing parent's COW pages
    //   - Starting with a clean address space
    let ret = unsafe { ffi::kvisor_flush_old_exec() };
    if ret != 0 {
        pr_err!("kvisor: Failed to flush old address space: {}\n", ret);
        return Err(ExecError::OutOfMemory);
    }
    pr_info!("kvisor: Old address space flushed\n");

    // Step 5: Calculate load bias
    // For PIE executables with base address 0, load at a high address
    // that won't conflict with the parent's mappings
    let load_bias = if elf_loader.base_address() == 0 {
        0x10000000u64  // 256MB - should be clear of parent's code
    } else {
        0u64
    };

    // Step 6: Load ELF segments
    load_elf_segments(&elf_loader, elf_data.as_slice(), load_bias)?;
    pr_info!("kvisor: ELF segments loaded\n");

    // Step 7: Set up stack
    let entry = elf_loader.entry + load_bias;
    let sp = setup_stack(argv, envp, entry, 0, 0)?;
    pr_info!("kvisor: Stack set up, entry={:#x}, sp={:#x}\n", entry, sp);

    // Step 8: Mark process as sandboxed
    unsafe { ffi::kvisor_mark_sandboxed() };

    // Step 9: Set up entry point
    // This modifies pt_regs so when we return from this syscall,
    // we jump to the ELF entry point instead of returning to userspace
    let ret = unsafe { ffi::kvisor_setup_entry(entry, sp) };
    if ret != 0 {
        pr_err!("kvisor: Failed to set up entry point: {}\n", ret);
        return Err(ExecError::MmapFailed);
    }

    pr_info!("kvisor: Process ready, jumping to entry {:#x} with sp {:#x}\n", entry, sp);

    // This return value is never seen by the caller because pt_regs.ip
    // has been modified to jump to the new entry point
    Ok(0)
}
