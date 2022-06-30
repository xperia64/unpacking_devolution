#include "qemu/osdep.h"
#include "cpu.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "hw/hw.h"
#include "hw/loader.h"
#include "hw/boards.h"
#include "exec/address-spaces.h"
#include "hw/ppc/ppc.h"

#define MAX_CPUS 1

#define MEM1PH_ADDR   0				 // Physical
#define MEM1PH_SIZE   0x017FFFFF

#define MEM2PH_ADDR   0x10000000	 // Physical
#define MEM2PH_SIZE   0x03FFFFFF

#define MEM1C_ADDR    0x80000000 	 // Physical Address = 0x00000000
#define MEM1C_SIZE    0x017FFFFF     // (Cached)

#define MEM1U_ADDR    0xC0000000 	 // Physical Address = 0x00000000
#define MEM1U_SIZE    0x017FFFFF     // (Uncached)

#define MEM2C_ADDR    0x90000000 	 // Physical Address = 0x10000000
#define MEM2C_SIZE    0x03FFFFFF     // (Cached)

#define MEM2U_ADDR    0xD0000000 	 // Physical Address = 0x10000000
#define MEM2U_SIZE    0x03FFFFFF     // (Uncached)

#define REGS_ADDR     0xCD000000 	 // Physical Address = 0x0D000000
#define REGS_SIZE     0x00008000     // Hollywood Registers

#define LOCKED_CACHE_ADDR  0xF0000000
#define LOCKED_CACHE_SIZE  0x8000

typedef struct
{
    PowerPCCPU *cpu;
    MemoryRegion *system_mem;
	MemoryRegion *mem1ph;
    MemoryRegion *mem1c;
	MemoryRegion *mem1u;
	MemoryRegion *mem2ph;
    MemoryRegion *mem2c;
	MemoryRegion *mem2u;
	MemoryRegion *regs;
	MemoryRegion *locked_cache;
} BroadwayState;

static void broadway_load_image(BroadwayState *s, const char* file, uint32_t addr)
{
	unsigned int size = get_image_size(file);

	if (load_image_targphys(file, addr, size) != size)
	{
		fprintf(stderr, "%s: error loading '%s'\n", __FUNCTION__, file);
		abort();
	}
}

static void main_cpu_reset(void *opaque)
{
	PowerPCCPU *cpu = opaque;

	cpu_reset(CPU(cpu));

	cpu->env.nip = 0x80004000;
}

static void broadway_init_cpu(MachineState *machine)
{
	Error *err = NULL;
	BroadwayState *s = g_new(BroadwayState, 1);

	fprintf(stdout, "Get Memory\n");

    s->system_mem = get_system_memory();

	s->mem1ph = g_new(MemoryRegion, 1);
	s->mem1c = g_new(MemoryRegion, 1);
	s->mem1u = g_new(MemoryRegion, 1);
	s->mem2ph = g_new(MemoryRegion, 1);
	s->mem2c = g_new(MemoryRegion, 1);
	s->mem2u = g_new(MemoryRegion, 1);
	s->regs = g_new(MemoryRegion, 1);
	s->locked_cache = g_new(MemoryRegion, 1);

	memory_region_init_ram(s->mem1c, NULL, "broadway.mem1c", MEM1C_SIZE, &err);
	memory_region_init_alias(s->mem1u, NULL, "broadway.mem1u", s->mem1c, 0, MEM1U_SIZE);
	memory_region_init_alias(s->mem1ph, NULL, "broadway.mem1ph", s->mem1c, 0, MEM1PH_SIZE);
    memory_region_add_subregion(s->system_mem, MEM1C_ADDR, s->mem1c);
	memory_region_add_subregion(s->system_mem, MEM1U_ADDR, s->mem1u);
	memory_region_add_subregion(s->system_mem, MEM1PH_ADDR, s->mem1ph);

	memory_region_init_ram(s->mem2c, NULL, "broadway.mem2c", MEM2C_SIZE, &err);
	memory_region_init_alias(s->mem2u, NULL, "broadway.mem2u", s->mem2c, 0, MEM2U_SIZE);
	memory_region_init_alias(s->mem2ph, NULL, "broadway.mem2ph", s->mem2c, 0, MEM2PH_SIZE);
	memory_region_add_subregion(s->system_mem, MEM2C_ADDR, s->mem2c);
	memory_region_add_subregion(s->system_mem, MEM2U_ADDR, s->mem2u);
	memory_region_add_subregion(s->system_mem, MEM2PH_ADDR, s->mem2ph);

	memory_region_init_ram(s->locked_cache, NULL, "broadway.locked_cache", LOCKED_CACHE_SIZE, &err);
	memory_region_add_subregion(s->system_mem, LOCKED_CACHE_ADDR, s->locked_cache);

	fprintf(stdout, "Load Image\n");

	broadway_load_image(s, "payload.bin", 0x80004000);

	fprintf(stdout, "Create CPU\n");

	s->cpu = POWERPC_CPU(cpu_create(machine->cpu_type));

	cpu_ppc_tb_init(&s->cpu->env, 100UL * 1000UL * 1000UL);

	qemu_register_reset(main_cpu_reset, s->cpu);

	fprintf(stdout, "Done\n");
}

static void broadway_init(MachineClass *mc)
{
	mc->desc = "broadway";
	mc->init = broadway_init_cpu;
	mc->default_cpu_type = POWERPC_CPU_TYPE_NAME("750cl_v2.0");
}

DEFINE_MACHINE("broadway", broadway_init)