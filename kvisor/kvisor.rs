// SPDX-License-Identifier: GPL-2.0

//! kVisor - Kernel-based sandbox for running untrusted code

#![allow(dead_code)]
#![allow(unreachable_pub)]
//!
//! kVisor provides a Linux-compatible syscall interface implemented in Rust,
//! running directly in the kernel for maximum performance. Untrusted applications
//! see a standard Linux environment, but all syscalls are mediated by kVisor.

use kernel::prelude::*;

mod exec;
mod platform;
mod process;
mod syscall;
mod vfs;

use process::KvisorContext;

/// The kVisor module instance
pub struct Kvisor;

module! {
    type: Kvisor,
    name: "kvisor",
    authors: ["kVisor Authors"],
    description: "Kernel-based sandbox for running untrusted code",
    license: "GPL",
}

impl kernel::Module for Kvisor {
    fn init(_module: &'static ThisModule) -> Result<Self> {
        pr_info!("kVisor initializing\n");

        // Initialize subsystems
        vfs::init()?;
        syscall::init()?;
        process::init()?;

        pr_info!("kVisor initialized successfully\n");
        Ok(Kvisor)
    }
}

impl Drop for Kvisor {
    fn drop(&mut self) {
        pr_info!("kVisor shutting down\n");

        process::cleanup();
        syscall::cleanup();
        vfs::cleanup();

        pr_info!("kVisor shutdown complete\n");
    }
}

/// FFI exports for the C interposition layer
mod ffi {
    use super::*;
    use kernel::bindings;

    /// Handle a syscall from a sandboxed task
    ///
    /// # Safety
    /// Called from C with valid pointers
    #[no_mangle]
    pub unsafe extern "C" fn rust_kvisor_handle_syscall(
        regs: *mut bindings::pt_regs,
        nr: core::ffi::c_int,
        ctx: *mut KvisorContext,
    ) -> core::ffi::c_long {
        if regs.is_null() || ctx.is_null() {
            return -(bindings::EINVAL as core::ffi::c_long);
        }

        let ctx = unsafe { &mut *ctx };
        syscall::dispatch(regs, nr, ctx)
    }

    /// Allocate a new kVisor context for a task
    ///
    /// # Safety
    /// Called from C, returns a pointer that must be freed with rust_kvisor_free_ctx
    #[no_mangle]
    pub extern "C" fn rust_kvisor_alloc_ctx() -> *mut KvisorContext {
        match KvisorContext::new() {
            Ok(ctx) => match KBox::new(ctx, GFP_KERNEL) {
                Ok(boxed) => KBox::into_raw(boxed),
                Err(_) => core::ptr::null_mut(),
            },
            Err(_) => core::ptr::null_mut(),
        }
    }

    /// Free a kVisor context
    ///
    /// # Safety
    /// Called from C with a pointer previously returned by rust_kvisor_alloc_ctx
    #[no_mangle]
    pub unsafe extern "C" fn rust_kvisor_free_ctx(ctx: *mut KvisorContext) {
        if !ctx.is_null() {
            let _: KBox<KvisorContext> = unsafe { KBox::from_raw(ctx) };
        }
    }

    /// Initialize the Rust kVisor subsystem
    ///
    /// # Safety
    /// Called once during module initialization
    #[no_mangle]
    pub extern "C" fn rust_kvisor_init() -> core::ffi::c_int {
        // Initialization is handled by the module! macro
        0
    }

    /// Cleanup the Rust kVisor subsystem
    ///
    /// # Safety
    /// Called once during module cleanup
    #[no_mangle]
    pub extern "C" fn rust_kvisor_exit() {
        // Cleanup is handled by Drop
    }

    /// Execute a program in the kVisor sandbox
    ///
    /// # Safety
    /// Called from C with user pointers that must be validated
    #[no_mangle]
    pub extern "C" fn rust_kvisor_exec(
        path: *const core::ffi::c_char,
        argv: *const *const core::ffi::c_char,
        envp: *const *const core::ffi::c_char,
    ) -> core::ffi::c_long {
        use kernel::uaccess::UserPtr;

        // Convert raw pointers to UserPtr for safe handling
        let path_ptr = UserPtr::from_addr(path as usize);
        let argv_ptr = UserPtr::from_addr(argv as usize);
        let envp_ptr = UserPtr::from_addr(envp as usize);

        match exec::kvisor_exec(path_ptr, argv_ptr, envp_ptr) {
            Ok(pid) => pid as core::ffi::c_long,
            Err(e) => i64::from(e) as core::ffi::c_long,
        }
    }
}
