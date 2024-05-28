/*
 * This file is part of the QDMA userspace application
 * to enable the user to execute the QDMA functionality
 *
 * Copyright (c) 2018-2020,  Xilinx, Inc.
 * All rights reserved.
 *
 * This source code is licensed under BSD-style license (found in the
 * LICENSE file in the root directory of this source tree)
 */

#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 500
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

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "dma_xfer_utils.c"

#include <immintrin.h>

#define DEVICE_NAME_DEFAULT "/dev/qdma01000-MM-0"
#define DEVICE_DRAM "/sys/bus/pci/devices/0000:01:00.0/resource4"
#define DRAM_ACCESS_OFFSET 262144
#define GPR_ACCESS_OFFSET 32768
#define CFR_ACCESS_OFFSET 16384
#define SIZE_DEFAULT (32)
#define COUNT_DEFAULT (1)

#define MODE 1

enum OPCODE {WR_SBK=0,WR_GPR,WR_GB,WR_BIAS,WR_AFLUT,RD_MAC,RD_AF,COPY_BKGB,COPY_GBBK,MAC_SBK,MAC_ABK,AF,EWMUL,NONE};
//const int DRAM_ACCESS_OFFSET = 98304;
//
static  __m256i isr_data_256_1;
static  __m256i isr_data_256_2;
static uint64_t isr_opcode,isr_c,isr_p,isr_io,isr_opsize,isr_g,isr_addr,isr_th;
static uint64_t isr_gpr_addr,isr_ch_mask,isr_bk_af_idx,isr_row,isr_col,isr_slope;


static struct option const long_opts[] = {
	{"device", required_argument, NULL, 'd'},
	{"address", required_argument, NULL, 'a'},
	{"size", required_argument, NULL, 's'},
	{"offset", required_argument, NULL, 'o'},
	{"count", required_argument, NULL, 'c'},
	{"data infile", required_argument, NULL, 'f'},
	{"data outfile", required_argument, NULL, 'w'},
	{"help", no_argument, NULL, 'h'},
	{"verbose", no_argument, NULL, 'v'},
	{0, 0, 0, 0}
};

static int test_dma(char *devname, uint64_t addr, uint64_t size,
		    uint64_t offset, uint64_t count, char *infname, char *);

static void issue_host_aim_cmd(__m256i * map_base, uint64_t, enum OPCODE Sel_opcode,
uint64_t isr_c       ,uint64_t isr_p        ,uint64_t isr_io         ,uint64_t isr_opsize   ,uint64_t isr_g       ,uint64_t isr_th,
uint64_t isr_gpr_addr,uint64_t isr_ch_mask  ,uint64_t isr_bk_af_idx  ,uint64_t isr_row      ,uint64_t isr_col     ,uint64_t isr_slope);
static uint64_t isr_data[4] = {5,6,7,0};

static void usage(const char *name)
{
	int i = 0;

	fprintf(stdout, "%s\n\n", name);
	fprintf(stdout, "usage: %s [OPTIONS]\n\n", name);
	fprintf(stdout,
		"Write via SGDMA, optionally read input from a file.\n\n");

	fprintf(stdout, "  -%c (--%s) device (defaults to %s)\n",
		long_opts[i].val, long_opts[i].name, DEVICE_NAME_DEFAULT);
	i++;
	fprintf(stdout, "  -%c (--%s) the start address on the AXI bus\n",
		long_opts[i].val, long_opts[i].name);
	i++;
	fprintf(stdout,
		"  -%c (--%s) size of a single transfer in bytes, default %d,\n",
		long_opts[i].val, long_opts[i].name, SIZE_DEFAULT);
	i++;
	fprintf(stdout, "  -%c (--%s) page offset of transfer\n",
		long_opts[i].val, long_opts[i].name);
	i++;
	fprintf(stdout, "  -%c (--%s) number of transfers, default %d\n",
		long_opts[i].val, long_opts[i].name, COUNT_DEFAULT);
	i++;
	fprintf(stdout, "  -%c (--%s) filename to read the data from.\n",
		long_opts[i].val, long_opts[i].name);
	i++;
	fprintf(stdout,
		"  -%c (--%s) filename to write the data of the transfers\n",
		long_opts[i].val, long_opts[i].name);
	i++;
	fprintf(stdout, "  -%c (--%s) print usage help and exit\n",
		long_opts[i].val, long_opts[i].name);
	i++;
	fprintf(stdout, "  -%c (--%s) verbose output\n",
		long_opts[i].val, long_opts[i].name);
}

int main(int argc, char *argv[])
{
	int cmd_opt;
	char *device = DEVICE_NAME_DEFAULT;
	uint64_t address = 0;
	uint64_t size = SIZE_DEFAULT;
	uint64_t offset = 0;
	uint64_t count = COUNT_DEFAULT;
	char *infname = NULL;
	char *ofname = NULL;

#if 1
    struct timespec ts_start, ts_end;
    double total_time = 0;
    double result;
    double avg_time = 0;
    ssize_t rc;

    //unsigned int *map_base = NULL;
    //uint64_t *map_base = NULL;

    //const uint64_t access_offset = GPR_ACCESS_OFFSET/sizeof(uint64_t);//DRAM_ACCESS_OFFSET;
    //const uint64_t access_offset = DRAM_ACCESS_OFFSET/sizeof(uint64_t);
    const uint64_t access_offset_cfr  = CFR_ACCESS_OFFSET/sizeof(uint64_t);
    //const uint64_t access_offset = DRAM_ACCESS_OFFSET;
    //const uint64_t access_offset = 524288/sizeof(uint64_t);
    //const uint64_t access_offset = 0;
    uint64_t access_offset2 = 0;
    uint64_t access_offset3[4] = {0,0,0,0};
    //uint64_t isr_data[4] = {5,6,7,0};
    //double isr_data[4] = {0,0,0,0};
    //__m256i isr_data_256_1;
    //__m256i isr_data_256_2;
    //uint64_t isr_opcode,isr_c,isr_p,isr_io,isr_opsize,isr_g,isr_addr,isr_th;
    //uint64_t isr_gpr_addr,isr_ch_mask,isr_bk_af_idx,isr_row,isr_col,isr_slope;
    enum OPCODE Sel_opcode;
    int i,j;
    int num_size = 1024;
    long int rd_address;
    int map_fd = open(DEVICE_DRAM, O_RDWR | O_SYNC);
    //int map_fd = open(DEVICE_DRAM, O_RDWR);
    int mapped_size = 64*1024*1024;
    if (map_fd < 0) {
    	fprintf(stderr, "unable to open output file %d.\n",map_fd);
	exit(1);
    }



    #if MODE == 1
    uint64_t *map_base = NULL;
    const uint64_t access_offset = DRAM_ACCESS_OFFSET/sizeof(uint64_t);

    //map_base = mmap((void *)0x0, mapped_size, PROT_WRITE, MAP_SHARED, map_fd, 0);
    map_base = mmap((void *)0x0, mapped_size, PROT_READ | PROT_WRITE, MAP_SHARED, map_fd, 0);
    if (map_base == MAP_FAILED) {
    	fprintf(stderr, "mmap fail\n");
	exit(1);
    }

    *(uint64_t *)(map_base + 0x8000 /*access_offset*/)     = 0x12345678aaabbbb;
    *(uint64_t *)(map_base + 0x8000 /*acaccess_offset*/ + 1) = 0x23456789ccccdddd;
    *(uint64_t *)(map_base + 0x8000 /*acaccess_offset*/ + 2) = 0x34567890eeeeffff;
    *(uint64_t *)(map_base + 0x8000 /*acaccess_offset*/ + 3) = 0x4567890122223333;

    printf("==> [%lX] \n", *(uint64_t *)(map_base + 0x8000/* access_offset*/ + 0));
    printf("==> [%lX] \n", *(uint64_t *)(map_base + 0x8000/* access_offset*/ + 1));
    printf("==> [%lX] \n", *(uint64_t *)(map_base + 0x8000/* access_offset*/ + 2));
    printf("==> [%lX] \n", *(uint64_t *)(map_base + 0x8000/* access_offset*/ + 3));

   if (munmap(map_base, mapped_size) == -1) {
    	fprintf(stderr, "mummap fail\n");
	exit(1);
   }
  close(map_fd);
  return 0;
  #elif MODE == 2
    __m256i *map_base = NULL;

    map_base = mmap((void *)0x0, mapped_size, PROT_READ | PROT_WRITE, MAP_SHARED, map_fd, 0);

    if (map_base == MAP_FAILED) {
    	fprintf(stderr, "mmap fail\n");
	exit(1);
    }


  uint64_t offset_address = 0x2000;


  printf(" MMAP Read/Write Test (AVX2) \n\n");
  printf(" ================= MMAP WRITE ============== \n");
  printf(" ================= DRAM Region ============ \n");
  for(i = 0; i < 64; i++)
  {
    __m256i _4data_256;
    uint64_t _4data[4] = {0x2222<<32+0x1+i ,0x222<<32+0x11+i,0x22<<32+ 0x111+i ,0x2<<32+0x1111+i};

    _4data_256 = _mm256_set_epi64x(_4data[3],_4data[2],_4data[1],_4data[0]);
    _mm256_store_si256((__m256i *)(map_base + offset_address + i),_4data_256);
    printf(" -- [%d] Address[%p] Write Data [%lX] / [%lX] / [%lX] / [%lX] \n",i,(void*)((offset_address + 1*i)*32), _4data[0], _4data[1], _4data[2], _4data[3]);
  }

  offset_address = 0x400;
  printf(" ================= GPR Region ============ \n");
  for(i = 0; i < 64; i++)
  {
    __m256i _4data_256;
    uint64_t _4data[4] = {0x3<<32+0x4+i ,0x33<<32+0x44+i,0x333<<32+ 0x444+i ,0x3333<<32+0x4444+i};

    _4data_256 = _mm256_set_epi64x(_4data[3],_4data[2],_4data[1],_4data[0]);
    _mm256_store_si256((__m256i *)(map_base + offset_address + i),_4data_256);
    printf(" -- [%d] Address[%p] Write Data [%lX] / [%lX] / [%lX] / [%lX] \n",i,(void*)((offset_address + 1*i)*32), _4data[0], _4data[1], _4data[2], _4data[3]);
  }



   printf(" ================= MMAP READ  ============== \n");
  for(j = 0; j < 2;j++) {
  printf("============= Iternation: %d ================= \n",j);
  printf(" ================= DRAM Region  ============== \n");
  offset_address = 0x2000;
    for(i = 0; i < 64; i++)
    {
      __m256i _4data_2561;
      _4data_2561 = _mm256_load_si256(  (__m256i *)(map_base + offset_address+ i)  );
      uint64_t* res = (uint64_t*)&_4data_2561;
      printf(" -- [%2d] Address[%p] Read Data [%8lX] / [%8lX] / [%8lX] / [%8lX] \n",i,(void*)((offset_address + 1*i)*32), res[0],res[1],res[2],res[3]);

    }
  printf(" ================= GPR Region  ============== \n");
  offset_address = 0x400;
    for(i = 0; i < 64; i++)
    {
      __m256i _4data_2561;
      _4data_2561 = _mm256_load_si256(  (__m256i *)(map_base + offset_address+ i)  );
      uint64_t* res = (uint64_t*)&_4data_2561;
      printf(" -- [%2d] Address[%p] Read Data [%8lX] / [%8lX] / [%8lX] / [%8lX] \n",i,(void*)((offset_address + 1*i)*32), res[0],res[1],res[2],res[3]);

    }

 }

   if (munmap(map_base, mapped_size) == -1) {
    	fprintf(stderr, "mummap fail\n");
	exit(1);
   }
  close(map_fd);


  #elif MODE == 3

    __m256i *map_base = NULL;

    map_base = mmap((void *)0x0, mapped_size, PROT_READ | PROT_WRITE, MAP_SHARED, map_fd, 0);

    if (map_base == MAP_FAILED) {
    	fprintf(stderr, "mmap fail\n");
	exit(1);
    }

  uint64_t offset_address = 0x2000;

//  printf(" MMAP Read/Write Test (AVX2) \n\n");
//  printf(" ================= MMAP WRITE ============== \n");
//  printf(" ================= DRAM Region ============ \n");
//  for(i = 0; i < 64; i++)
//  {
//    __m256i _4data_256;
//    uint64_t _4data[4] = {0x1+i ,0x11+i, 0x111+i ,0x1111+i};
//
//    _4data_256 = _mm256_set_epi64x(_4data[3],_4data[2],_4data[1],_4data[0]);
//    _mm256_store_si256((__m256i *)(map_base + offset_address + i),_4data_256);
//    printf(" -- [%d] Address[%p] Write Data [%lX] / [%lX] / [%lX] / [%lX] \n",i,(void*)((offset_address + 1*i)*32), _4data[0], _4data[1], _4data[2], _4data[3]);
//  }
//
//  offset_address = 0x400;
//  printf(" ================= GPR Region ============ \n");
//  for(i = 0; i < 64; i++)
//  {
//    __m256i _4data_256;
//    uint64_t _4data[4] = {0x2+i ,0x22+i, 0x222+i ,0x2222+i};
//
//    _4data_256 = _mm256_set_epi64x(_4data[3],_4data[2],_4data[1],_4data[0]);
//    _mm256_store_si256((__m256i *)(map_base + offset_address + i),_4data_256);
//    printf(" -- [%d] Address[%p] Write Data [%lX] / [%lX] / [%lX] / [%lX] \n",i,(void*)((offset_address + 1*i)*32), _4data[0], _4data[1], _4data[2], _4data[3]);
//  }

//  for(j=0;j<32;j++){
//  issue_host_aim_cmd(map_base,0x10,WR_GPR,j%2/*C*/,0/*P*/,0/*IO*/,63/*OP_SIZE*/,0/*G*/,0/*TH*/,0/*GPR_ADR*/,0/*CH_MASK*/,0/*BK_ADDR*/,0/*ROW*/,0/*COL*/,0/*SLOP*/);
//  offset_address = 0x400;
//  printf(" ================= GPR Region  ============== \n");
//  offset_address = 0x400;
//    for(i = 16; i < 64; i++)
//    {
//      __m256i _4data_2561;
//      _4data_2561 = _mm256_load_si256(  (__m256i *)(map_base + offset_address+ i)  );
//      uint64_t* res = (uint64_t*)&_4data_2561;
//      printf(" -- [%2d] Address[%p] Read Data [%8lX] / [%8lX] / [%8lX] / [%8lX] \n",i,(void*)((offset_address + 1*i)*32), res[0],res[1],res[2],res[3]);
//
//    }
//
//
//   }
//

 for(i = 0;i<256;i++) {
        printf("============= Iternation: %d ================= \n",i);

      issue_host_aim_cmd(map_base,0x0,WR_SBK,0/*C*/,0/*P*/,0/*IO*/,63/*OP_SIZE*/,0/*G*/,0/*TH*/,0/*GPR_ADR*/,1/*CH_MASK*/,4/*BK_ADDR*/,3/*ROW*/,6/*COL*/,0/*SLOP*/);
      issue_host_aim_cmd(map_base,0x10,WR_GPR,0/*C*/,0/*P*/,0/*IO*/,0x17F/*OP_SIZE*/,0/*G*/,0/*TH*/,100/*GPR_ADR*/,0/*CH_MASK*/,0/*BK_ADDR*/,0/*ROW*/,0/*COL*/,0/*SLOP*/);
      issue_host_aim_cmd(map_base,0x20,WR_GB,1,0,0,63,0,0,0,0,0,0,0,0);
      issue_host_aim_cmd(map_base,0x30,WR_BIAS,0/*C*/,0/*P*/,0/*IO*/,63/*OP_SIZE*/,0/*G*/,0/*TH*/,0/*GPR_ADR*/,1/*CH_MASK*/,0/*BK_ADDR*/,6/*ROW*/,0/*COL*/,0/*SLOP*/);
      issue_host_aim_cmd(map_base,0x40,WR_AFLUT,0/*C*/,0/*P*/,0/*IO*/,63/*OP_SIZE*/,0/*G*/,0/*TH*/,0/*GPR_ADR*/,1/*CH_MASK*/,1/*BK_ADDR*/,0/*ROW*/,0/*COL*/,0/*SLOP*/);
      issue_host_aim_cmd(map_base,0x50,RD_AF,0/*C*/,0/*P*/,0/*IO*/,0/*OP_SIZE*/,0/*G*/,0/*TH*/,0/*GPR_ADR*/,1/*CH_MASK*/,1/*BK_ADDR*/,32/*ROW*/,0/*COL*/,0/*SLOP*/);
      issue_host_aim_cmd(map_base,0x60,COPY_BKGB,0/*C*/,0/*P*/,0/*IO*/,63/*OP_SIZE*/,0/*G*/,0/*TH*/,0/*GPR_ADR*/,1/*CH_MASK*/,1/*BK_ADDR*/,15/*ROW*/,0/*COL*/,0/*SLOP*/);
      issue_host_aim_cmd(map_base,0x60,COPY_GBBK,0/*C*/,0/*P*/,0/*IO*/,63/*OP_SIZE*/,0/*G*/,0/*TH*/,0/*GPR_ADR*/,1/*CH_MASK*/,13/*BK_ADDR*/,15/*ROW*/,0/*COL*/,0/*SLOP*/);

      issue_host_aim_cmd(map_base,0x70,MAC_SBK,0/*C*/,0/*P*/,0/*IO*/,63/*OP_SIZE*/,0/*G*/,0/*TH*/,0/*GPR_ADR*/,1/*CH_MASK*/,0/*BK_ADDR*/,1/*ROW*/,0/*COL*/,0/*SLOP*/);
      issue_host_aim_cmd(map_base,0x80,MAC_ABK,0/*C*/,0/*P*/,0/*IO*/,63/*OP_SIZE*/,0/*G*/,1/*TH*/,0/*GPR_ADR*/,1/*CH_MASK*/,0/*BK_ADDR*/,2/*ROW*/,0/*COL*/,0/*SLOP*/);
      issue_host_aim_cmd(map_base,0x90,AF,0/*C*/,1/*P*/,0/*IO*/,0/*OP_SIZE*/,0/*G*/,1/*TH*/,0/*GPR_ADR*/,1/*CH_MASK*/,0/*BK_ADDR*/,2/*ROW*/,0/*COL*/,1/*SLOP*/);
      issue_host_aim_cmd(map_base,0xA0,EWMUL,0/*C*/,0/*P*/,0/*IO*/,63/*OP_SIZE*/,0/*G*/,1/*TH*/,0/*GPR_ADR*/,1/*CH_MASK*/,0/*BK_ADDR*/,2/*ROW*/,0/*COL*/,0/*SLOP*/);
      issue_host_aim_cmd(map_base,0xB0,RD_MAC,0,0,0,0,0,0,0,1,0,i,0, 0);

  }
//    for(i = 0;i<256;i++)
//    issue_host_aim_cmd(map_base,0xB0,RD_MAC,0,0,0,0,0,0,0,1,0,i,0, 0);
//
//
 //    __m256i _4data_256;
 //    _4data_256 = _mm256_load_si256(  (__m256i *)(map_base + 0x400 + 3)  );
 //    uint64_t* res = (uint64_t*)&_4data_256;
 //    printf(" -- [%d] Address[%p] Read Data [%lX] / [%lX] / [%lX] / [%lX] \n ",i,(void*)((0x400 + 2)*32), res[0],res[1],res[2],res[3]);


//    num_size=64;
//    for(j=1;j<17;j++) {
//    clock_gettime(CLOCK_MONOTONIC, &ts_start);
//
//    for(int i=0;i<num_size/sizeof(uint64_t);i++) {
//  //   *(uint64_t  *)(map_base + access_offset + i)= i;
//        volatile unsigned int readout =  *(volatile unsigned int *)(map_base + access_offset + i);
//        printf("[%d] Address[%p] %d \n", i,map_base + access_offset + i, readout);
//
//    }
//
//    rc = clock_gettime(CLOCK_MONOTONIC, &ts_end);
//    timespec_sub(&ts_end, &ts_start);
//    total_time += (ts_end.tv_sec + ((double)ts_end.tv_nsec/NSEC_DIV));
//    fprintf(stdout,"#CLOCK_MONOTONIC %ld.%09ld sec. read %lu bytes\n",
//		    ts_end.tv_sec, ts_end.tv_nsec, num_size);
//    num_size*=2;
//    }


//  for(i = 0; i < 4; i++)
//  {
//      uint64_t access_offset_lv2 = (access_offset + 1024 + 8 * i)/sizeof(uint64_t);
//      *(uint64_t *)(map_base + access_offset_lv2)= i+4095;
//      printf("[%d] Address[%16p / %8p] Write: %lX \n", i,map_base + access_offset_lv2,(void*)access_offset_lv2,(long unsigned int)(i+4095));
//  }
//
//  for(i = 0; i < 4; i++)
//  {
//
//  //    access_offset2 = (access_offset + i*256) & 4294909951;
//      uint64_t access_offset_lv2 = (access_offset + 1024 + 8 * i)/sizeof(uint64_t);
//      volatile uint64_t readout =  *(volatile uint64_t *)(map_base + access_offset_lv2);
//      printf("[%d] Address[%16p / %8p] Read: %lX \n", i,map_base + access_offset_lv2,(void*)access_offset_lv2,readout);
//  }


  printf(" MMAP Read/Write Test (AVX2) \n\n");
//  uint64_t offset_address = 0x2000;

 /*
  printf(" ================= MMAP WRITE ============== \n");
  for(i = 0; i < 64; i++)
  {
    __m256i _4data_256;
    uint64_t _4data[4] = {0x1+i ,0x11+i, 0x111+i ,0x1111+i};

    _4data_256 = _mm256_set_epi64x(_4data[3],_4data[2],_4data[1],_4data[0]);
    _mm256_store_si256((__m256i *)(map_base + offset_address + i),_4data_256);
    printf(" -- [%d] Address[%p] Write Data [%lX] / [%lX] / [%lX] / [%lX] \n",i,(void*)((offset_address + 1*i)*32), _4data[0], _4data[1], _4data[2], _4data[3]);
  }
  */
  /*
  printf(" ================= MMAP READ  ============== \n");
  for(j = 0; j < 2;j++) {
  printf("============= Iternation: %d ================= \n",j);
  printf(" ================= DRAM Region  ============== \n");
  offset_address = 0x2000;
    for(i = 0; i < 64; i++)
    {
      __m256i _4data_2561;
      _4data_2561 = _mm256_load_si256(  (__m256i *)(map_base + offset_address+ i)  );
      uint64_t* res = (uint64_t*)&_4data_2561;
      printf(" -- [%2d] Address[%p] Read Data [%8lX] / [%8lX] / [%8lX] / [%8lX] \n",i,(void*)((offset_address + 1*i)*32), res[0],res[1],res[2],res[3]);

    }
  printf(" ================= GPR Region  ============== \n");
  offset_address = 0x400;
    for(i = 0; i < 64; i++)
    {
      __m256i _4data_2561;
      _4data_2561 = _mm256_load_si256(  (__m256i *)(map_base + offset_address+ i)  );
      uint64_t* res = (uint64_t*)&_4data_2561;
      printf(" -- [%2d] Address[%p] Read Data [%8lX] / [%8lX] / [%8lX] / [%8lX] \n",i,(void*)((offset_address + 1*i)*32), res[0],res[1],res[2],res[3]);

    }

 }
 */
   if (munmap(map_base, mapped_size) == -1) {
    	fprintf(stderr, "mummap fail\n");
	exit(1);
   }
  close(map_fd);
  #endif

#else

#endif
}


static void issue_host_aim_cmd(__m256i * map_base, uint64_t access_offset, enum OPCODE Sel_opcode,
uint64_t isr_c        ,uint64_t isr_p        ,uint64_t isr_io         ,uint64_t isr_opsize   ,uint64_t isr_g       ,uint64_t isr_th,
uint64_t isr_gpr_addr ,uint64_t isr_ch_mask  ,uint64_t isr_bk_af_idx  ,uint64_t isr_row      ,uint64_t isr_col     ,uint64_t isr_slope) {
	int j=0;

	switch(Sel_opcode) {
		case WR_SBK:
                	isr_opcode    = 0x00;
			//isr_c         = 0x0;
			//isr_io        = 0; // 00:{ROW,COL}, 01: {BK,COL}, 10: {COL,CH}, 11: RESERVED
			//isr_opsize    = 4;
			//isr_ch_mask   = 1;
			//isr_bk_af_idx = 3;
			//isr_row       = 55;
			//isr_col       = 50;
			isr_data[3]   = isr_opcode<<59|isr_c<<58|isr_io<<56|isr_opsize<<46|isr_ch_mask<<24|isr_bk_af_idx<<20|isr_row<<6|isr_col;
			printf("issue INST WR SBK - x%016lX\n",isr_data[3]);
                        printf("  -- isr_c       : %ld \n",isr_c);
                        printf("  -- isr_io      : %ld \n",isr_io);
                        printf("  -- isr_opsize  : %ld \n",isr_opsize);
                        printf("  -- isr_ch_mask : %ld \n",isr_ch_mask);
                        printf("  -- isr_row     : %ld \n",isr_row);
                        printf("  -- isr_col     : %ld \n",isr_col);

			isr_data_256_1 = _mm256_set_epi64x(isr_data[0],isr_data[1],isr_data[2],isr_data[3]);
			_mm256_store_si256((__m256i *)(map_base+access_offset),isr_data_256_1);
			for(j=0;j<isr_opsize+1;j++) {
			isr_data_256_2 = _mm256_set_epi64x(0+4*j,1+4*j,2+4*j,3+4*j);
			_mm256_store_si256((__m256i *)(map_base+access_offset+(j+1)),isr_data_256_2);
		}
		break;
		case WR_GPR:
                    isr_opcode    = 0x02;
		    //isr_c         = 0x0;
		    //isr_opsize    = 4;
		    //isr_row       = 1;
		    isr_data[3]   = isr_opcode<<59|isr_c<<58|isr_opsize<<46|isr_row<<6;
		    printf("issue INST WR GPR - x%016lX\n",isr_data[3]);
                    printf("  -- isr_c       : %ld \n",isr_c);
                    printf("  -- isr_opsize  : %ld \n",isr_opsize);
                    printf("  -- isr_row     : %ld \n",isr_row);

		    isr_data_256_1 = _mm256_set_epi64x(isr_data[0],isr_data[1],isr_data[2],isr_data[3]);
		    _mm256_store_si256((__m256i *)(map_base+access_offset),isr_data_256_1);
		    for(j=0;j<isr_opsize+1;j++) {
		    isr_data_256_2 = _mm256_set_epi64x(0+4*j<<16,1+4*j<<16,2+4*j<<16,3+4*j<<16);
		    //_mm256_store_si256((__m256i *)(map_base+access_offset+8+16*(j+1)),isr_data_256_2);
		    _mm256_store_si256((__m256i *)(map_base+access_offset+(j+1)),isr_data_256_2);
		    }
		break;
		case WR_GB:
	            isr_opcode    = 0x03;
		    //isr_c         = 0x0;
		    //isr_opsize    = 4;
		    //isr_g         = 1;
		    //isr_ch_mask   = 1;
		    //isr_row       = 1;
		    //isr_col       = 3;
		    isr_data[3]   = isr_opcode<<59|isr_c<<58|isr_opsize<<46|isr_g<<45|isr_ch_mask<<24|isr_row<<6|isr_col;
		    printf("issue INST WR GB - x%016lX\n",isr_data[3]);
                    printf("  -- isr_c       : %ld \n",isr_c);
                    printf("  -- isr_opsize  : %ld \n",isr_opsize);
                    printf("  -- isr_g       : %ld \n",isr_g);
                    printf("  -- isr_ch_mask : %ld \n",isr_ch_mask);
                    printf("  -- isr_row     : %ld \n",isr_row);
                    printf("  -- isr_col     : %ld \n",isr_col);
		    isr_data_256_1 = _mm256_set_epi64x(isr_data[0],isr_data[1],isr_data[2],isr_data[3]);
		    _mm256_store_si256((__m256i *)(map_base+access_offset),isr_data_256_1);
		    if(isr_g == 0) {
		    for(j=0;j<isr_opsize+1;j++) {
		    isr_data_256_2 = _mm256_set_epi64x(0+4*j,1+4*j,2+4*j,3+4*j);
		    _mm256_store_si256((__m256i *)(map_base+access_offset+(j+1)),isr_data_256_2);
		    }
		    }
		break;
 		case WR_BIAS:
	            isr_opcode    = 0x04;
		    //isr_opsize    = 1;
		    //isr_th        = 1;
		    //isr_ch_mask   = 1;
		    //isr_row       = 0;
		    isr_data[3]   = isr_opcode<<59|isr_opsize<<46|isr_th<<32|isr_ch_mask<<24|isr_row<<6;
		    printf("issue INST WR BIAS - x%016lX\n",isr_data[3]);
                    printf("  -- isr_opsize  : %ld \n",isr_opsize);
                    printf("  -- isr_th      : %ld \n",isr_th);
                    printf("  -- isr_ch_mask : %ld \n",isr_ch_mask);
                    printf("  -- isr_row     : %ld \n",isr_row);

		    isr_data_256_1 = _mm256_set_epi64x(isr_data[0],isr_data[1],isr_data[2],isr_data[3]);
		    _mm256_store_si256((__m256i *)(map_base+access_offset),isr_data_256_1);
		break;
 		case WR_AFLUT:
	            isr_opcode    = 0x05;
		    //isr_opsize    = 4;
		    //isr_ch_mask   = 1;
		    //isr_bk_af_idx = 0;
		    //isr_col       = 0;
		    isr_data[3]   = isr_opcode<<59|isr_opsize<<46|isr_ch_mask<<24|isr_bk_af_idx<<20|isr_col;
		    printf("issue INST WR AFLUT - x%016lX\n",isr_data[3]);
                    printf("  -- isr_opsize    : %ld \n",isr_opsize);
                    printf("  -- isr_ch_mask   : %ld \n",isr_ch_mask);
                    printf("  -- isr_bk_af_idx : %ld \n",isr_bk_af_idx);
                    printf("  -- isr_col       : %ld \n",isr_col);

		    isr_data_256_1 = _mm256_set_epi64x(isr_data[0],isr_data[1],isr_data[2],isr_data[3]);
		    _mm256_store_si256((__m256i *)(map_base+access_offset),isr_data_256_1);
		    for(j=0;j<isr_opsize+1;j++) {
		    isr_data_256_2 = _mm256_set_epi64x(0+4*j,1+4*j,2+4*j,3+4*j);
		    _mm256_store_si256((__m256i *)(map_base+access_offset+(j+1)),isr_data_256_2);
		    }

                break;
 		case RD_MAC:
	            isr_opcode    = 0x06;
		    //isr_opsize    = 4;
                    //isr_th        = 0;
		    //isr_ch_mask   = 1;
                    //isr_row       = 1;
		    isr_data[3]   = isr_opcode<<59|isr_opsize<<46|isr_th<<32|isr_ch_mask<<24|isr_row<<6;
		    printf("issue INST RD MAC - x%016lX\n",isr_data[3]);
                    printf("  -- isr_c       : %ld \n",isr_c);
                    printf("  -- isr_opsize  : %ld \n",isr_opsize);
                    printf("  -- isr_g       : %ld \n",isr_g);
                    printf("  -- isr_ch_mask : %ld \n",isr_ch_mask);
                    printf("  -- isr_row     : %ld \n",isr_row);
                    printf("  -- isr_col     : %ld \n",isr_col);

		    isr_data_256_1 = _mm256_set_epi64x(isr_data[0],isr_data[1],isr_data[2],isr_data[3]);
		    _mm256_store_si256((__m256i *)(map_base+access_offset),isr_data_256_1);
        break;
  		case RD_AF:
	            isr_opcode    = 0x07;
		    //isr_opsize    = 4;
		    //isr_ch_mask   = 1;
                    //isr_row       = 1;
		    isr_data[3]   = isr_opcode<<59|isr_opsize<<46|isr_ch_mask<<24|isr_row<<6;
		    printf("issue INST RD AF - x%016lX\n",isr_data[3]);
                    printf("  -- isr_opsize    : %ld \n",isr_opsize);
                    printf("  -- isr_ch_mask   : %ld \n",isr_ch_mask);
                    printf("  -- isr_row       : %ld \n",isr_row);

		    isr_data_256_1 = _mm256_set_epi64x(isr_data[0],isr_data[1],isr_data[2],isr_data[3]);
		    _mm256_store_si256((__m256i *)(map_base+access_offset),isr_data_256_1);
        break;
   		case COPY_BKGB:
	            isr_opcode    = 0x08;
		    //isr_opsize    = 4;
		    //isr_ch_mask   = 1;
		    //isr_bk_af_idx = 2;
                    //isr_row       = 1;
                    //isr_col       = 3;
		    isr_data[3]   = isr_opcode<<59|isr_opsize<<46|isr_ch_mask<<24|isr_bk_af_idx<<20|isr_row<<6|isr_col;
		    printf("issue INST COPY BKGB - x%016lX\n",isr_data[3]);
                    printf("  -- isr_opsize    : %ld \n",isr_opsize);
                    printf("  -- isr_ch_mask   : %ld \n",isr_ch_mask);
                    printf("  -- isr_bk_af_idx : %ld \n",isr_bk_af_idx);
                    printf("  -- isr_row       : %ld \n",isr_row);
                    printf("  -- isr_col       : %ld \n",isr_col);

		    isr_data_256_1 = _mm256_set_epi64x(isr_data[0],isr_data[1],isr_data[2],isr_data[3]);
		    _mm256_store_si256((__m256i *)(map_base+access_offset),isr_data_256_1);
        break;
    		case COPY_GBBK:
	            isr_opcode    = 0x09;
		    //isr_opsize    = 4;
		    //isr_ch_mask   = 1;
		    //isr_bk_af_idx = 2;
                    //isr_row       = 1;
                    //isr_col       = 3;
		    isr_data[3]   = isr_opcode<<59|isr_opsize<<46|isr_ch_mask<<24|isr_bk_af_idx<<20|isr_row<<6|isr_col;
		    printf("issue INST COPY GBBK - x%016lX\n",isr_data[3]);
                    printf("  -- isr_opsize    : %ld \n",isr_opsize);
                    printf("  -- isr_ch_mask   : %ld \n",isr_ch_mask);
                    printf("  -- isr_bk_af_idx : %ld \n",isr_bk_af_idx);
                    printf("  -- isr_row       : %ld \n",isr_row);
                    printf("  -- isr_col       : %ld \n",isr_col);

		    isr_data_256_1 = _mm256_set_epi64x(isr_data[0],isr_data[1],isr_data[2],isr_data[3]);
		    _mm256_store_si256((__m256i *)(map_base+access_offset),isr_data_256_1);
        break;
     		case MAC_SBK:
	            isr_opcode    = 0x0A;
		    //isr_io        = 0; // 00:{ROW,COL}, 01: {BK,COL}, 10: {COL,CH}, 11: RESERVED
		    //isr_opsize    = 4;
                    //isr_th        = 0;
		    //isr_ch_mask   = 1;
		    //isr_bk_af_idx = 2;
                    //isr_row       = 1;
                    //isr_col       = 3;
		    isr_data[3]   = isr_opcode<<59|isr_io<<56|isr_opsize<<46|isr_th<<32|isr_ch_mask<<24|isr_bk_af_idx<<20|isr_row<<6|isr_col;
		    printf("issue INST MAC SBK - x%016lX\n",isr_data[3]);
                    printf("  -- isr_io      : %ld \n",isr_io);
                    printf("  -- isr_opsize  : %ld \n",isr_opsize);
                    printf("  -- isr_th      : %ld \n",isr_th);
                    printf("  -- isr_ch_mask : %ld \n",isr_ch_mask);
                    printf("  -- isr_bk      : %ld \n",isr_bk_af_idx);
                    printf("  -- isr_row     : %ld \n",isr_row);
                    printf("  -- isr_col     : %ld \n",isr_col);

		    isr_data_256_1 = _mm256_set_epi64x(isr_data[0],isr_data[1],isr_data[2],isr_data[3]);
		    _mm256_store_si256((__m256i *)(map_base+access_offset),isr_data_256_1);
        break;
      		case MAC_ABK:
	            isr_opcode    = 0x0C;
		    //isr_opsize    = 4;
                    //isr_th        = 0;
		    //isr_ch_mask   = 1;
                    //isr_row       = 1;
                    //isr_col       = 3;
		    isr_data[3]   = isr_opcode<<59|isr_opsize<<46|isr_th<<32|isr_ch_mask<<24|isr_row<<6|isr_col;
		    printf("issue INST MAC ABK - x%016lX\n",isr_data[3]);
                    printf("  -- isr_opsize  : %ld \n",isr_opsize);
                    printf("  -- isr_th      : %ld \n",isr_th);
                    printf("  -- isr_ch_mask : %ld \n",isr_ch_mask);
                    printf("  -- isr_row     : %ld \n",isr_row);
                    printf("  -- isr_col     : %ld \n",isr_col);

		    isr_data_256_1 = _mm256_set_epi64x(isr_data[0],isr_data[1],isr_data[2],isr_data[3]);
		    _mm256_store_si256((__m256i *)(map_base+access_offset),isr_data_256_1);
        break;
       		case AF:
	            isr_opcode    = 0x0D;
                    //isr_p         = 0;
                    //isr_slope     = 0;
                    //isr_th        = 0;
		    //isr_ch_mask   = 1;
                    //isr_bk_af_idx = 2;
		    isr_data[3]   = isr_opcode<<59|isr_p<<58|isr_slope<<43|isr_th<<32|isr_ch_mask<<24|isr_bk_af_idx<<20; // @TODO: check isr_slope shift
		    printf("issue INST AF - x%016lX\n",isr_data[3]);
                    printf("  -- isr_p         : %ld \n",isr_p);
                    printf("  -- isr_slope     : %ld \n",isr_slope);
                    printf("  -- isr_th        : %ld \n",isr_th);
                    printf("  -- isr_ch_mask   : %ld \n",isr_ch_mask);
                    printf("  -- isr_bk_af_idx : %ld \n",isr_bk_af_idx);

		    isr_data_256_1 = _mm256_set_epi64x(isr_data[0],isr_data[1],isr_data[2],isr_data[3]);
		    _mm256_store_si256((__m256i *)(map_base+access_offset),isr_data_256_1);
        break;
       		case EWMUL:
	        isr_opcode    = 0x0E;
                    //isr_opsize    = 4;
		    //isr_ch_mask   = 1;
                    //isr_bk_af_idx = 2;
                    //isr_row       = 2;
                    //isr_col       = 4;
		    isr_data[3]   = isr_opcode<<59|isr_opsize<<46|isr_ch_mask<<24|isr_bk_af_idx<<20|isr_row<<6|isr_col;
		    printf("issue INST EWMUL - x%016lX\n",isr_data[3]);
                    printf("  -- isr_opsize    : %ld \n",isr_opsize);
                    printf("  -- isr_ch_mask   : %ld \n",isr_ch_mask);
                    printf("  -- isr_bk_af_idx : %ld \n",isr_bk_af_idx);
                    printf("  -- isr_row       : %ld \n",isr_row);
                    printf("  -- isr_col       : %ld \n",isr_col);

		    isr_data_256_1 = _mm256_set_epi64x(isr_data[0],isr_data[1],isr_data[2],isr_data[3]);
		    _mm256_store_si256((__m256i *)(map_base+access_offset),isr_data_256_1);
        break;

		default:
    	printf("Wrong Command Type");
	}



  uint64_t offset_address = 0x2000;
  int i;
//  printf(" ================= MMAP READ  ============== \n");
//  for(j = 0; j < 1;j++) {
//  printf("============= Iternation: %d ================= \n",j);
//  printf(" ================= DRAM Region  ============== \n");
//  for(i = 16; i < 32; i++)
//  {
//   __m256i _4data_2561;
//   _4data_2561 = _mm256_load_si256(  (__m256i *)(map_base + offset_address+ i)  );
//  uint64_t* res = (uint64_t*)&_4data_2561;
//  printf(" -- [%2d] Address[%p] Read Data [%8lX] / [%8lX] / [%8lX] / [%8lX] \n",i,(void*)((offset_address + 1*i)*32), res[0],res[1],res[2],res[3]);
//
//  }
//
  printf(" ================= GPR Region  ============== \n");
  offset_address = 0x400;
    for(i = 0; i < isr_opsize; i++)
    {
      __m256i _4data_2561;
      _4data_2561 = _mm256_load_si256(  (__m256i *)(map_base + offset_address+ i)  );
      uint64_t* res = (uint64_t*)&_4data_2561;
      printf(" -- [%2d] Address[%p] Read Data [%8lX] / [%8lX] / [%8lX] / [%8lX] \n",i,(void*)((offset_address + 1*i)*32), res[0],res[1],res[2],res[3]);

    }

// }
     /*
     int go_and_stop = 0;
      while(go_and_stop == 0) {
        printf("GO(1) and Stop(0): ");
        scanf("%d",&go_and_stop);
        if(!(go_and_stop == 0 || go_and_stop == 1)) go_and_stop = 0;
      }
     */

};




//  for(i = 0; i < 4; i++)
//  {
//
//  //    access_offset2 = (access_offset + i*256) & 4294909951;
//      uint64_t access_offset_lv2 = (access_offset + 0 + 4 * i)/sizeof(uint64_t);
//      volatile uint64_t readout =  *(volatile uint64_t *)(map_base + access_offset_lv2);
//      printf("[%d] Address[%16p / %8p] Read: %lX \n", i,map_base + access_offset_lv2,(void*)access_offset_lv2,readout);
//  }

//    access_offset3[3] = (access_offset + 8192 + 3*256) & 4294909951;
//    clock_gettime(CLOCK_MONOTONIC, &ts_start);
//
//    readout0[0] =  *(volatile unsigned int *)(map_base + access_offset3[0]);
//    readout0[1] =  *(volatile unsigned int *)(map_base + access_offset3[1]);
//    readout0[2] =  *(volatile unsigned int *)(map_base + access_offset3[2]);
//    readout0[3] =  *(volatile unsigned int *)(map_base + access_offset3[3]);
//    rc = clock_gettime(CLOCK_MONOTONIC, &ts_end);
//    timespec_sub(&ts_end, &ts_start);
//    total_time += (ts_end.tv_sec + ((double)ts_end.tv_nsec/NSEC_DIV));
//    fprintf(stdout,"CLOCK_MONOTONIC %ld.%09ld sec. Read %lu bytes\n",
//		    ts_end.tv_sec, ts_end.tv_nsec, 4 * sizeof(unsigned int));
//
//  for(i = 0; i < num_size; i++)
//  {
//
//  //    access_offset2 = (access_offset + i*256) & 4294909951;
//  //    volatile unsigned int readout =  *(volatile unsigned int *)(map_base + access_offset2);
//      printf("[%d] Address[%p] %d \n", i,map_base + access_offset3[i], readout0[i]);
//  }



static int test_dma(char *devname, uint64_t addr, uint64_t size,
		    uint64_t offset, uint64_t count, char *infname,
		    char *ofname)
{
	uint64_t i;
	ssize_t rc;
	char *buffer = NULL;
	char *allocated = NULL;
	struct timespec ts_start, ts_end;
	int infile_fd = -1;
	int outfile_fd = -1;
	int fpga_fd = open(devname, O_RDWR);
	double total_time = 0;
	double result;
	double avg_time = 0;


	if (fpga_fd < 0) {
		fprintf(stderr, "unable to open device %s, %d.\n",
			devname, fpga_fd);
		perror("open device");
		return -EINVAL;
	}

	if (infname) {
		infile_fd = open(infname, O_RDONLY);
		if (infile_fd < 0) {
			fprintf(stderr, "unable to open input file %s, %d.\n",
				infname, infile_fd);
			perror("open input file");
			rc = -EINVAL;
			goto out;
		}
	}

	if (ofname) {
		outfile_fd =
		    open(ofname, O_RDWR | O_CREAT | O_TRUNC | O_SYNC,
			 0666);
		if (outfile_fd < 0) {
			fprintf(stderr, "unable to open output file %s, %d.\n",
				ofname, outfile_fd);
			perror("open output file");
			rc = -EINVAL;
			goto out;
		}
	}

	posix_memalign((void **)&allocated, 4096 /*alignment */ , size + 4096);
	if (!allocated) {
		fprintf(stderr, "OOM %lu.\n", size + 4096);
		rc = -ENOMEM;
		goto out;
	}
	buffer = allocated + offset;
	if (verbose)
		fprintf(stdout, "host buffer 0x%lx = %p\n",
			size + 4096, buffer);

	if (infile_fd >= 0) {
		rc = read_to_buffer(infname, infile_fd, buffer, size, 0);
		if (rc < 0)
			goto out;
	}

	for (i = 0; i < count; i++) {
		/* write buffer to AXI MM address using SGDMA */
		clock_gettime(CLOCK_MONOTONIC, &ts_start);

		rc = write_from_buffer(devname, fpga_fd, buffer, size, addr);
		if (rc < 0)
			goto out;

		rc = clock_gettime(CLOCK_MONOTONIC, &ts_end);
		/* subtract the start time from the end time */
		timespec_sub(&ts_end, &ts_start);
		total_time += (ts_end.tv_sec + ((double)ts_end.tv_nsec/NSEC_DIV));
		/* a bit less accurate but side-effects are accounted for */
		if (verbose)
		fprintf(stdout,
			"#%lu: CLOCK_MONOTONIC %ld.%09ld sec. write %lu bytes\n",
			i, ts_end.tv_sec, ts_end.tv_nsec, size);

		if (outfile_fd >= 0) {
			rc = write_from_buffer(ofname, outfile_fd, buffer,
						 size, i * size);
			if (rc < 0)
				goto out;
		}
	}
	avg_time = (double)total_time/(double)count;
	result = ((double)size)/avg_time;
	if (verbose)
	printf("** Avg time device %s, total time %f nsec, avg_time = %f, size = %lu, BW = %f bytes/sec\n",
		devname, total_time, avg_time, size, result);
	dump_throughput_result(size, result);

	rc = 0;

out:
	close(fpga_fd);
	if (infile_fd >= 0)
		close(infile_fd);
	if (outfile_fd >= 0)
		close(outfile_fd);
	free(allocated);

	return rc;
}


