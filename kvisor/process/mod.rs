// SPDX-License-Identifier: GPL-2.0

//! Process management for kVisor
//!
//! This module manages the per-process state for sandboxed processes.

use kernel::prelude::*;

mod context;

pub use context::KvisorContext;

/// Initialize the process subsystem
pub fn init() -> Result {
    pr_debug!("kvisor: process subsystem initialized\n");
    Ok(())
}

/// Cleanup the process subsystem
pub fn cleanup() {
    pr_debug!("kvisor: process subsystem cleanup\n");
}
