// SPDX-License-Identifier: GPL-2.0

//! Syscall dispatcher for kVisor
//!
//! Phase 1: Intercepts all syscalls and kills the task.
//! This validates that syscall interposition is working correctly.

use kernel::bindings;
use kernel::prelude::*;

use crate::process::KvisorContext;

/// Initialize the syscall subsystem
pub fn init() -> Result {
    pr_debug!("kvisor: syscall subsystem initialized\n");
    Ok(())
}

/// Cleanup the syscall subsystem
pub fn cleanup() {
    pr_debug!("kvisor: syscall subsystem cleanup\n");
}

// FFI declaration for killing the current task
extern "C" {
    fn kvisor_kill_current() -> !;
}

/// Dispatch a syscall to the appropriate handler
///
/// Phase 1: Just prints the syscall number and kills the task.
/// This validates that syscall interception is working.
///
/// # Safety
/// The regs pointer must be valid and point to the current task's registers.
pub fn dispatch(
    _regs: *mut bindings::pt_regs,
    nr: core::ffi::c_int,
    _ctx: &mut KvisorContext,
) -> core::ffi::c_long {
    pr_info!("kvisor: intercepted syscall {} - killing task\n", nr);

    // Kill the sandboxed process
    unsafe { kvisor_kill_current() }
}
