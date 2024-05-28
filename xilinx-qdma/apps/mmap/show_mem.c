#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>
#include <dirent.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define DRIVER_NAME		"/sys/bus/pci/drivers/qdma-pf"
#define DEVICE_PREFIX	"/sys/bus/pci/devices/"
#define DEVICE_FILE		"/resource4"

/*
# AiM DMA configuration
# 0: Instruction region 16KB, CFR 16KB, GPR 64KB
# 1: Instruction region 2MB, CFR 16KB, GPR 512KB
*/
#define AIM_DMA_CONFIG 1

#define INST_REGION_BASE (0)
#if AIM_DMA_CONFIG == 0
#define INST_REGION_SIZE (16 * 1024)
#define CFR_SIZE (16 * 1024)
#define GPR_SIZE (64 * 1024)
#elif AIM_DMA_CONFIG == 1
#define INST_REGION_SIZE (2 * 1024 * 1024)
#define CFR_SIZE (16 * 1024)
#define GPR_SIZE (512 * 1024)
#elif AIM_DMA_CONFIG == 2
#define INST_REGION_SIZE (2 * 1024 * 1024)
#define CFR_SIZE (16 * 1024)
#define GPR_SIZE (8 *1024 * 1024)
#endif

#define CFR_BASE INST_REGION_SIZE
#define GPR_BASE (INST_REGION_SIZE + CFR_SIZE)
#define DRAM_BASE (INST_REGION_SIZE + CFR_SIZE + GPR_SIZE)

int main(int argc, char **argv) {
    uint64_t *map_base = NULL;
	DIR *dir;
	struct dirent *d_e = NULL;
	char target_name[256] = {0};
	int count;
    int map_fd;
    int i;
    uint32_t r_size, r_base;
    int offset, size;
    uint64_t read_addr;

    if (argc != 6) {
        printf("[Error] Invalid number of arguments [%d], Number of argument should be 4\n", argc - 1);
    }

    if (atoi(argv[1]) != 2 && atoi(argv[1]) != 512) {
        printf("[Error] Invalid FPGA Memory Size (%d), Only 2MB or 512MB are available. Please set 2 or 512 as a first argument\n", atoi(argv[1]));
        return -1;
    }

	dir = opendir(DRIVER_NAME);
	if(dir == NULL) {
		printf("[Error] Invalid driver information [%s]\n", DRIVER_NAME);
		return -1;
	}
	printf("Finding in %s\n", DRIVER_NAME);

	while ((d_e = readdir(dir)) != NULL)
	{
		if(strncmp(d_e->d_name, "0000", 4) == 0)
		{
			printf("Found : %s\n", d_e->d_name);
			break;
		}
	}
	closedir(dir);

	count = sprintf(target_name, "%s", DEVICE_PREFIX);
	count += sprintf(target_name+count, "%s", d_e->d_name);
	count += sprintf(target_name+count, "%s", DEVICE_FILE);
	printf("Target file: %s, length of filename: %d", target_name, count);

	map_fd = open(target_name, O_RDWR);
	map_base = mmap((void *)0x0, atoi(argv[1])*1024*1024, PROT_READ | PROT_WRITE, MAP_SHARED, map_fd, 0);

    printf("= Mmap base address = %p\n", map_base);
    if (map_base == (uint64_t *)0xffffffffffffffff) {
        printf("[Error] mmap failed, you may not run on sudo permission. Please use sudo permission to run this application\n");
    }

    offset = (strtoul(argv[4], NULL, 16) / 32) * 32;
    size = (strtoul(argv[5], NULL, 16) / 32) * 32;

    if (!strcmp(argv[2], "inst") || !strcmp(argv[2], "INST"))
    {
        if (atoi(argv[4]) != 0x0)
        {
            printf("[Error] Invalid offset [0x%x], offset should be 0 for INST\n", atoi(argv[4]));
            return -1;
        }
        r_base = INST_REGION_BASE;
        r_size = INST_REGION_SIZE;
    }
    else if (!strcmp(argv[2], "cfr") || !strcmp(argv[2], "CFR"))
    {
        if (atoi(argv[4]) != 0x0)
        {
            printf("[Error] Invalid offset [0x%x], offset should be 0 for CFR\n", atoi(argv[4]));
            return -1;
        }
        r_base = CFR_BASE;
        r_size = CFR_SIZE;
    }
    else if (!strcmp(argv[2], "gpr") || !strcmp(argv[2], "GPR"))
    {
        if (atoi(argv[4]) != 0x0)
        {
            printf("[Error] Invalid offset [0x%x], offset should be 0 for GPR\n", atoi(argv[4]));
            return -1;
        }
        r_base = GPR_BASE;
        r_size = GPR_SIZE;
    }
    else if (!strcmp(argv[2], "dram") || !strcmp(argv[2], "DRAM"))
    {
        if (atoi(argv[4]) >= (atoi(argv[1])*1024*1024))
        {
            printf("[Error] Invalid offset [0x%x], offset should be less than 0x%x for DRAM\n",
                    atoi(argv[4]), ((atoi(argv[1])*1024*1024)));
            return -1;
        }
        r_base = offset;
        r_size = size;
    }
    else
    {
        printf("[Error] Invalid data region [%s]\n", argv[2]);
        return -1;
    }

    printf("= region selected [%s]\n", argv[2]);

    if (!strcmp(argv[3], "clear") || !strcmp(argv[3], "CLEAR"))
    {
        printf("= Clear & Read [%s]\n", argv[2]);
        for(i = 0; i < (int)(r_size/8); i++)
        {
            *(uint64_t *)(map_base + ((r_base) / 8) + i) = 0;
        }
    }
    else if (!strcmp(argv[3], "fill") || !strcmp(argv[3], "FILL"))
    {
        printf("= Fill with 0xff & Read [%s]\n", argv[2]);
        for(i = 0; i < (int)(r_size/8); i++)
        {
            *(uint64_t *)(map_base + ((r_base) / 8) + i) = 0xffffffffffffffff;
        }
    }
    else if (!strcmp(argv[3], "read") || !strcmp(argv[3], "READ"))
    {
        printf("= Read [%s]\n", argv[2]);
    }
    else
    {
        printf("[Error] Invalid operation [%s]. Third argument should be either clear or read\n", argv[3]);
        return -1;
    }
#if 0
    if (r_size > GPR_SIZE)
        r_size = GPR_SIZE;
#endif
#if 0
    for(i = ((r_size)/8)/4 - 0x20; i >= 0; i-=4)
    {
        read_addr = i * 8;
        if (!strcmp(argv[2], "dram") || !strcmp(argv[2], "DRAM"))
        {
            read_addr += DRAM_BASE + offset;
        }
        printf("[0x%04lx] 0x%016lx 0x%016lx 0x%016lx 0x%016lx\n", read_addr,
                *(uint64_t *)(map_base + r_base/8 + i + 3),
                *(uint64_t *)(map_base + r_base/8 + i + 2),
                *(uint64_t *)(map_base + r_base/8 + i + 1),
                *(uint64_t *)(map_base + r_base/8 + i + 0));
    }
#else

#if 0
    for(i = r_base + r_size - 0x20; i >= (int)r_base; i -= 32)
    {
        *(uint64_t *)(map_base + i/8 + 3) = 0x7777888812341234;
        *(uint64_t *)(map_base + i/8 + 2) = 0x12123434aaaabbbb;
        *(uint64_t *)(map_base + i/8 + 1) = 0x55556666ccccdddd;
        *(uint64_t *)(map_base + i/8 + 0) = 0x87871234eeeeffff;
    }
#endif
    for(i = r_base + r_size - 0x20; i >= (int)r_base; i -= 0x20)
    {
        if (!strcmp(argv[2], "dram") || !strcmp(argv[2], "DRAM"))
            read_addr = i;
        else
            read_addr = i - r_base;

        printf("[0x%08lx] 0x%016lx 0x%016lx 0x%016lx 0x%016lx\n", read_addr,
                *(uint64_t *)(map_base + i/8 + 3),
                *(uint64_t *)(map_base + i/8 +  2),
                *(uint64_t *)(map_base + i/8 +  1),
                *(uint64_t *)(map_base + i/8 +  0));
    }
#endif
}
