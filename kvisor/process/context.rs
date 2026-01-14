// SPDX-License-Identifier: GPL-2.0

//! Per-process kVisor context
//!
//! Phase 1: Minimal context just to satisfy the type system.
//! The context is allocated when a process is marked as sandboxed.

use kernel::prelude::*;

/// Per-process kVisor context
///
/// Phase 1: Empty struct - just needs to exist for the C/Rust interface.
pub struct KvisorContext {
    _placeholder: u8,
}

impl KvisorContext {
    /// Create a new kVisor context for a process
    pub fn new() -> Result<Self> {
        Ok(KvisorContext { _placeholder: 0 })
    }
}
