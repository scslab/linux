# KSI (Kernel System Interface)

KSI is a Linux architecture port that replaces all hardware access with
hypercalls to a host environment. The host can be a microkernel, a hypervisor,
a user-mode process, or any other environment that implements the KSI interface.
The userspace ABI is aarch64.

## Design

KSI reuses arm64 code wholesale. The Kconfig selects `CONFIG_ARM64`, and the
build system injects `arch/arm64/include` into the compiler include path after
`arch/ksi/include`. KSI overrides only the headers that need to change. All
other arm64 headers are found via the fallback path. arm64's kernel and mm
source files are compiled directly through kbuild composite object rules that
reference `arch/arm64/kernel/*.c` and `arch/arm64/mm/*.c`.

A handful of source files are replaced entirely (setup, IRQ, timer, entry) where
arm64's version is too tied to hardware. Everything else compiles unmodified
with KSI's include path.

### System registers

MSR and MRS instructions are left unmodified in the binary. The host is
expected to handle them, either by trapping and emulating, by rewriting them
at load time, or by providing a real register file. This means `current`
(SP_EL0), per-cpu offsets (TPIDR_EL1), interrupt flags (DAIF), page table base
registers (TTBR0/TTBR1), and all ID registers use real arm64 instructions.

### Page tables

arm64's page table code runs unmodified. `set_pte()` and friends write PTEs to
memory. The host must observe these writes and update its address space
accordingly. The natural synchronization points are TLB flush operations:
`tlbflush.h` is overridden to remove TLBI instructions (which are privileged)
but preserves DSB/ISB barriers. These flush call sites are where KSI can notify
the host of mapping changes.

### TLB and cache maintenance

TLBI instructions are no-ops (the host manages the real TLB). Cache maintenance
instructions (DC, IC) currently pass through to arm64's implementations. In a
fully para-virtualized environment the host manages cache coherency and these
could become no-ops.

### Alternatives

arm64's code patching infrastructure (alternatives) is included but effectively
inert. No CPU errata apply in a para-virtualized environment, so the base
instruction sequences are used everywhere. The `.altinstructions` section is
present in the ELF but empty.

## Build

```
make ARCH=ksi CROSS_COMPILE=aarch64-linux-gnu- ksi_defconfig
make ARCH=ksi CROSS_COMPILE=aarch64-linux-gnu- -j$(nproc)
```

The output is a standard aarch64 ELF binary (`vmlinux`).

### Include path

```
arch/ksi/include          # KSI-specific (checked first)
arch/arm64/include         # arm64 fallback
arch/arm64/include/uapi    # arm64 userspace ABI
include/                   # generic kernel headers
```

### Directory layout

```
arch/ksi/
  Makefile                  Top-level build configuration
  Kbuild                    obj-y += kernel/ kernel/arch-arm64/ mm/arch-arm64/
  Kconfig                   Selects KSI_ARCH_ARM64, sources Kconfig.arm64
  Kconfig.arm64             Selects CONFIG_ARM64, ~80 feature flags
  Kconfig.debug             Empty
  configs/ksi_defconfig     Minimal config (4K pages, 48-bit VA, no hardware)

  include/asm/
    Kbuild                  Includes arm64's Kbuild for generic-y mappings
    pv_calls.h              KSI hypercall declarations
    tlbflush.h              TLB ops with TLBI removed, barriers preserved
    alternative.h           Pass-through to arm64 (patching is inert)
    cacheflush.h            Pass-through to arm64
    io.h                    Pass-through to arm64
    mmu.h                   Pass-through to arm64
    uaccess.h               Pass-through to arm64

  include/uapi/asm/
    Kbuild                  Includes arm64's uapi Kbuild

  kernel/
    Makefile                Builds: pv_calls, setup, irq, time, early_console
    vmlinux.lds.S           Dispatcher (includes arch-arm64/vmlinux.lds.S)
    asm-offsets.c           Dispatcher (includes arm64's asm-offsets.c)
    pv_calls.c              Hypercall stubs + ksi_start_kernel() entry
    setup.c                 setup_arch() without FDT
    irq.c                   init_IRQ() without GIC
    time.c                  time_init() with KSI clocksource
    early_console.c         Boot console via ksi_console_write()

    arch-arm64/
      Makefile              Compiles ~50 arm64 kernel sources, filters 8
      head.S                Minimal entry: set stack, clear BSS, call C
      stubs.c               Stub symbols (page tables, vdso, cpu_ops)
      vmlinux.lds.S         Standard aarch64 ELF layout at KIMAGE_VADDR
      probes/Makefile       kprobes/uprobes from arm64

  mm/arch-arm64/
    Makefile                Compiles ~20 arm64 mm sources (mmu, fault, etc.)
```

### What's replaced vs reused

Replaced (KSI provides its own implementation):

| File | Why |
|------|-----|
| `kernel/setup.c` | No FDT; memblock populated directly |
| `kernel/irq.c` | No GIC; empty init_IRQ() |
| `kernel/time.c` | No arch timer; KSI clocksource at 1 MHz |
| `kernel/early_console.c` | Boot console via ksi_console_write() |
| `kernel/pv_calls.c` | KSI entry point and hypercall stubs |
| `kernel/arch-arm64/head.S` | Minimal entry (no MMU setup, no EL transition) |
| `kernel/arch-arm64/stubs.c` | Stub symbols for filtered-out objects |
| `kernel/arch-arm64/vmlinux.lds.S` | Simplified linker script (no EFI, no hyp) |

Filtered out from arm64 (not compiled):

| Object | Why |
|--------|-----|
| `setup.o` | Replaced by KSI setup.c |
| `irq.o` | Replaced by KSI irq.c |
| `time.o` | Replaced by KSI time.c |
| `hyp-stub.o` | No EL2 |
| `psci.o` | No PSCI firmware |
| `smp_spin_table.o` | No spin-table boot |
| `pi/` | No position-independent early boot |
| `vdso.o` | No vdso (stub provided) |

Everything else (~50 kernel sources, ~20 mm sources, arm64 lib) compiles
unmodified from `arch/arm64/` with KSI's include path.

## KSI hypercall API

Declared in `include/asm/pv_calls.h`. All stubs currently `panic("not
implemented")`.

```c
/* Boot */
void ksi_get_boot_info(struct ksi_boot_info *info);

/* Console */
void ksi_console_write(const char *buf, unsigned int len);
int  ksi_console_read(char *buf, unsigned int len);

/* Address space management */
ksi_asid_t ksi_as_create(void);
void ksi_as_destroy(ksi_asid_t asid);
int  ksi_as_map(ksi_asid_t asid, unsigned long vaddr, phys_addr_t paddr,
                unsigned long size, unsigned long prot);
int  ksi_as_unmap(ksi_asid_t asid, unsigned long vaddr, unsigned long size);
int  ksi_kern_map(unsigned long vaddr, phys_addr_t paddr,
                  unsigned long size, unsigned long prot);

/* Execution */
int ksi_user_resume(void);

/* Time */
u64  ksi_time_now(void);
void ksi_timer_arm(u64 deadline_ns);
void ksi_timer_disarm(void);

/* SMP */
int  ksi_cpu_start(unsigned int cpu, unsigned long entry);
void ksi_send_ipi(unsigned int cpu);
void ksi_wait_for_event(void);

/* System */
void ksi_halt(void);
void ksi_yield(void);

/* Early console */
void ksi_setup_early_console(void);
```

## Boot sequence

1. Host loads the kernel ELF, mapping it at `KIMAGE_VADDR` with physical
   address 0. Host must also map the stack (`init_thread_union`) and BSS.

2. Host jumps to `_start` (head.S):
   - Sets `sp` to `init_thread_union + THREAD_SIZE`
   - Clears BSS
   - Calls `ksi_start_kernel()`

3. `ksi_start_kernel()` (pv_calls.c):
   - Writes `&init_task` to SP_EL0 (real MSR instruction)
   - Sets `kimage_voffset = KIMAGE_VADDR` for VA/PA translation
   - Registers the early boot console
   - Calls `start_kernel()`

4. `start_kernel()` (init/main.c) proceeds with normal Linux initialization.
   `setup_arch()` populates memblock with 256 MB at physical address 0 (hardcoded;
   will use `ksi_get_boot_info()` when a host is implemented).

## Host requirements

The host must provide:

- **Memory mappings**: kernel image at `KIMAGE_VADDR`, linear map at
  `PAGE_OFFSET`, and any other regions the kernel expects.
- **System register handling**: trap/emulate or rewrite all MRS/MSR
  instructions. Key registers: SP_EL0 (current task), TPIDR_EL1 (per-cpu
  offset), DAIF (interrupt flags), TTBR0/TTBR1 (page table bases),
  TCR_EL1, MPIDR_EL1, VBAR_EL1, and all ID registers.
- **Console I/O**: implement `ksi_console_write()` for boot output.
- **Time**: implement `ksi_time_now()` returning nanoseconds.

## Current status

The skeleton compiles cleanly with zero errors and zero warnings. It is not
yet bootable. Remaining work:

- Implement `ksi_console_write()` so the first printk doesn't panic.
- Implement host-side system register handling.
- Implement page table synchronization: the kernel writes PTEs to memory and
  issues TLB flush calls; the host must observe the PTE writes and update its
  address space. The `tlbflush.h` flush functions are the natural hook points.
- Implement `ksi_time_now()` for timekeeping.
- Replace the hardcoded 256 MB memblock with `ksi_get_boot_info()`.
