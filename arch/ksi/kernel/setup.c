// SPDX-License-Identifier: GPL-2.0-only
/*
 * KSI architecture setup
 *
 * Replaces arm64's setup.c with KSI-specific initialization.
 * Key difference: no FDT parsing — memory layout is provided
 * directly (hardcoded for skeleton, ksi_get_boot_info() later).
 */

#include <linux/acpi.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/memblock.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/cpu.h>
#include <linux/smp.h>

#include <asm/cputype.h>
#include <asm/daifflags.h>
#include <asm/fixmap.h>
#include <asm/memory.h>
#include <asm/mmu.h>
#include <asm/mmu_context.h>
#include <asm/sections.h>
#include <asm/setup.h>
#include <asm/smp.h>
#include <asm/smp_plat.h>
#include <asm/tlbflush.h>
#include <asm/pv_calls.h>

/*
 * Variables normally set by arm64's head.S.
 * KSI's head.S doesn't use the arm64 bootloader protocol,
 * so these remain at their default values (zero).
 */
phys_addr_t __fdt_pointer __initdata;
u64 mmu_enabled_at_boot __initdata;
u64 __cacheline_aligned boot_args[4];

/*
 * CPU logical map — maps Linux CPU numbers to hardware IDs.
 */
u64 __cpu_logical_map[NR_CPUS] = { [0 ... NR_CPUS-1] = INVALID_HWID };

u64 cpu_logical_map(unsigned int cpu)
{
	return __cpu_logical_map[cpu];
}

/*
 * MPIDR hash — needed by arm64 SMP infrastructure.
 */
struct mpidr_hash mpidr_hash;

/*
 * Called from start_kernel() before setup_arch().
 * Reads MPIDR via real MRS instruction (host handles it).
 */
void __init smp_setup_processor_id(void)
{
	u64 mpidr = read_cpuid_mpidr() & MPIDR_HWID_BITMASK;

	set_cpu_logical_map(0, mpidr);
	pr_info("Booting KSI on virtual CPU 0x%llx\n", mpidr);
}

bool arch_match_cpu_phys_id(int cpu, u64 phys_id)
{
	return phys_id == cpu_logical_map(cpu);
}

bool arch_cpu_is_hotpluggable(int num)
{
	return false;
}

/*
 * Standard memory resources — iomem entries for /proc/iomem.
 */
static int num_standard_resources;
static struct resource *standard_resources;

static struct resource mem_res[] = {
	{
		.name = "Kernel code",
		.start = 0,
		.end = 0,
		.flags = IORESOURCE_SYSTEM_RAM
	},
	{
		.name = "Kernel data",
		.start = 0,
		.end = 0,
		.flags = IORESOURCE_SYSTEM_RAM
	}
};

#define kernel_code mem_res[0]
#define kernel_data mem_res[1]

static void __init request_standard_resources(void)
{
	struct memblock_region *region;
	struct resource *res;
	unsigned long i = 0;
	size_t res_size;

	kernel_code.start   = __pa_symbol(_text);
	kernel_code.end     = __pa_symbol(__init_begin - 1);
	kernel_data.start   = __pa_symbol(_sdata);
	kernel_data.end     = __pa_symbol(_end - 1);
	insert_resource(&iomem_resource, &kernel_code);
	insert_resource(&iomem_resource, &kernel_data);

	num_standard_resources = memblock.memory.cnt;
	res_size = num_standard_resources * sizeof(*standard_resources);
	standard_resources = memblock_alloc_or_panic(res_size, SMP_CACHE_BYTES);

	for_each_mem_region(region) {
		res = &standard_resources[i++];
		if (memblock_is_nomap(region)) {
			res->name  = "reserved";
			res->flags = IORESOURCE_MEM;
			res->start = __pfn_to_phys(memblock_region_reserved_base_pfn(region));
			res->end = __pfn_to_phys(memblock_region_reserved_end_pfn(region)) - 1;
		} else {
			res->name  = "System RAM";
			res->flags = IORESOURCE_SYSTEM_RAM | IORESOURCE_BUSY;
			res->start = __pfn_to_phys(memblock_region_memory_base_pfn(region));
			res->end = __pfn_to_phys(memblock_region_memory_end_pfn(region)) - 1;
		}

		insert_resource(&iomem_resource, res);
	}
}

static int __init reserve_memblock_reserved_regions(void)
{
	u64 i, j;

	for (i = 0; i < num_standard_resources; ++i) {
		struct resource *mem = &standard_resources[i];
		phys_addr_t r_start, r_end, mem_size = resource_size(mem);

		if (!memblock_is_region_reserved(mem->start, mem_size))
			continue;

		for_each_reserved_mem_range(j, &r_start, &r_end) {
			resource_size_t start, end;

			start = max(PFN_PHYS(PFN_DOWN(r_start)), mem->start);
			end = min(PFN_PHYS(PFN_UP(r_end)) - 1, mem->end);

			if (start > mem->end || end < mem->start)
				continue;

			reserve_region_with_split(mem, start, end, "reserved");
		}
	}

	return 0;
}
arch_initcall(reserve_memblock_reserved_regions);

void __init __no_sanitize_address setup_arch(char **cmdline_p)
{
	setup_initial_init_mm(_text, _etext, _edata, _end);

	/*
	 * Set a minimal boot command line.
	 * TODO: get from ksi_get_boot_info() once host is implemented.
	 */
	strscpy(boot_command_line, "console=ksi", COMMAND_LINE_SIZE);
	*cmdline_p = boot_command_line;

	early_fixmap_init();
	early_ioremap_init();

	/*
	 * KSI: no FDT — populate memblock directly.
	 * The host provides memory at physical address 0.
	 * TODO: get memory layout from ksi_get_boot_info().
	 */
	memblock_add(0, SZ_256M);

	jump_label_init();
	parse_early_param();

	/*
	 * Unmask Debug and SError exceptions (KSI: software IRQ state).
	 */
	local_daif_restore(DAIF_PROCCTX_NOIRQ);

	/*
	 * Clear identity map — no-op in KSI but maintains API contract.
	 */
	cpu_uninstall_idmap();

	arm64_memblock_init();

	paging_init();

	bootmem_init();

	request_standard_resources();

	early_ioremap_reset();
}
