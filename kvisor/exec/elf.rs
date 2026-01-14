// SPDX-License-Identifier: GPL-2.0

//! ELF parsing and loading for kVisor
//!
//! This module handles parsing ELF binaries and loading them into
//! the sandboxed process's address space.

use kernel::prelude::*;

/// ELF magic number
pub const ELF_MAGIC: [u8; 4] = [0x7f, b'E', b'L', b'F'];

/// ELF class (32-bit vs 64-bit)
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum ElfClass {
    None = 0,
    Elf32 = 1,
    Elf64 = 2,
}

/// ELF data encoding (endianness)
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum ElfData {
    None = 0,
    Lsb = 1,  // Little endian
    Msb = 2,  // Big endian
}

/// ELF file type
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u16)]
pub enum ElfType {
    None = 0,
    Rel = 1,      // Relocatable
    Exec = 2,     // Executable
    Dyn = 3,      // Shared object
    Core = 4,     // Core dump
}

/// ELF machine type
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u16)]
pub enum ElfMachine {
    None = 0,
    X86_64 = 62,
    Aarch64 = 183,
}

/// Program header type
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u32)]
pub enum PhdrType {
    Null = 0,
    Load = 1,
    Dynamic = 2,
    Interp = 3,
    Note = 4,
    Shlib = 5,
    Phdr = 6,
    Tls = 7,
    GnuEhFrame = 0x6474e550,
    GnuStack = 0x6474e551,
    GnuRelro = 0x6474e552,
}

/// Program header flags
pub mod phdr_flags {
    pub const PF_X: u32 = 1;  // Execute
    pub const PF_W: u32 = 2;  // Write
    pub const PF_R: u32 = 4;  // Read
}

/// ELF64 header
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Elf64Header {
    pub e_ident: [u8; 16],
    pub e_type: u16,
    pub e_machine: u16,
    pub e_version: u32,
    pub e_entry: u64,
    pub e_phoff: u64,
    pub e_shoff: u64,
    pub e_flags: u32,
    pub e_ehsize: u16,
    pub e_phentsize: u16,
    pub e_phnum: u16,
    pub e_shentsize: u16,
    pub e_shnum: u16,
    pub e_shstrndx: u16,
}

impl Elf64Header {
    /// Check if the ELF magic is valid
    pub fn is_valid_magic(&self) -> bool {
        self.e_ident[0..4] == ELF_MAGIC
    }

    /// Check if this is a 64-bit ELF
    pub fn is_64bit(&self) -> bool {
        self.e_ident[4] == ElfClass::Elf64 as u8
    }

    /// Check if this is little-endian
    pub fn is_little_endian(&self) -> bool {
        self.e_ident[5] == ElfData::Lsb as u8
    }

    /// Check if this is an executable
    pub fn is_executable(&self) -> bool {
        self.e_type == ElfType::Exec as u16 || self.e_type == ElfType::Dyn as u16
    }

    /// Check if this is for x86_64
    pub fn is_x86_64(&self) -> bool {
        self.e_machine == ElfMachine::X86_64 as u16
    }

    /// Validate the header for kVisor
    pub fn validate(&self) -> Result<(), &'static str> {
        if !self.is_valid_magic() {
            return Err("Invalid ELF magic");
        }
        if !self.is_64bit() {
            return Err("Not a 64-bit ELF");
        }
        if !self.is_little_endian() {
            return Err("Not little-endian");
        }
        if !self.is_executable() {
            return Err("Not an executable");
        }
        if !self.is_x86_64() {
            return Err("Not x86_64");
        }
        Ok(())
    }
}

/// ELF64 program header
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct Elf64Phdr {
    pub p_type: u32,
    pub p_flags: u32,
    pub p_offset: u64,
    pub p_vaddr: u64,
    pub p_paddr: u64,
    pub p_filesz: u64,
    pub p_memsz: u64,
    pub p_align: u64,
}

impl Elf64Phdr {
    /// Check if this is a loadable segment
    pub fn is_load(&self) -> bool {
        self.p_type == PhdrType::Load as u32
    }

    /// Check if this segment is readable
    pub fn is_readable(&self) -> bool {
        self.p_flags & phdr_flags::PF_R != 0
    }

    /// Check if this segment is writable
    pub fn is_writable(&self) -> bool {
        self.p_flags & phdr_flags::PF_W != 0
    }

    /// Check if this segment is executable
    pub fn is_executable(&self) -> bool {
        self.p_flags & phdr_flags::PF_X != 0
    }
}

/// ELF loader for kVisor
pub struct ElfLoader {
    /// The ELF header
    pub header: Elf64Header,
    /// Program headers
    pub phdrs: KVec<Elf64Phdr>,
    /// Entry point address
    pub entry: u64,
}

impl ElfLoader {
    /// Parse an ELF file from a byte buffer
    pub fn parse(data: &[u8]) -> Result<Self> {
        if data.len() < core::mem::size_of::<Elf64Header>() {
            pr_err!("kvisor: ELF file too small\n");
            return Err(EINVAL);
        }

        // Safety: We've checked the buffer is large enough
        let header: Elf64Header = unsafe {
            core::ptr::read_unaligned(data.as_ptr() as *const Elf64Header)
        };

        if let Err(msg) = header.validate() {
            pr_err!("kvisor: ELF validation failed: {}\n", msg);
            return Err(ENOEXEC);
        }

        // Parse program headers
        let phdr_start = header.e_phoff as usize;
        let phdr_size = header.e_phentsize as usize;
        let phdr_count = header.e_phnum as usize;

        let mut phdrs = KVec::new();

        for i in 0..phdr_count {
            let offset = phdr_start + i * phdr_size;
            if offset + phdr_size > data.len() {
                pr_err!("kvisor: Program header {} out of bounds\n", i);
                return Err(EINVAL);
            }

            let phdr: Elf64Phdr = unsafe {
                core::ptr::read_unaligned(data.as_ptr().add(offset) as *const Elf64Phdr)
            };

            phdrs.push(phdr, GFP_KERNEL)?;
        }

        Ok(ElfLoader {
            header,
            phdrs,
            entry: header.e_entry,
        })
    }

    /// Get all loadable segments
    pub fn load_segments(&self) -> impl Iterator<Item = &Elf64Phdr> {
        self.phdrs.iter().filter(|p| p.is_load())
    }

    /// Calculate the total memory size needed
    pub fn total_memory_size(&self) -> u64 {
        let mut min_addr = u64::MAX;
        let mut max_addr = 0u64;

        for phdr in self.load_segments() {
            let start = phdr.p_vaddr;
            let end = phdr.p_vaddr + phdr.p_memsz;

            if start < min_addr {
                min_addr = start;
            }
            if end > max_addr {
                max_addr = end;
            }
        }

        if max_addr > min_addr {
            max_addr - min_addr
        } else {
            0
        }
    }

    /// Get the base load address (lowest vaddr)
    pub fn base_address(&self) -> u64 {
        self.load_segments()
            .map(|p| p.p_vaddr)
            .min()
            .unwrap_or(0)
    }
}
