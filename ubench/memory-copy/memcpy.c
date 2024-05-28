
#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>


/*
#include "dma_xfer_utils.c"
#define DEVICE_NAME_DEFAULT "/dev/qdma01000-MM-0"
#define SIZE_DEFAULT (32)
#define COUNT_DEFAULT (1)
*/

#define USE_MMAP 0
#define USE_QDMA 1
#define AIM_RESERVED_OFFSET 0x00040000 //x MB


void ParseArguments(const std::string& input, int& method, long long& size, bool& dump_result);
float* AllocMat(long long size);
void InitMat(float* mat, long long size);
void ZeroMat(float* mat, long long size);
void WriteToMmap(void* mmap_ptr, float* matrix_ptr, long long size);
void ReadFromMmap(float* matrix_ptr, void* mmap_ptr, long long size);
void CompareMatrices(float* matrix1, float* matrix2, long long size);




float* AllocMat(long long size) {
    return static_cast<float*>(malloc(size * sizeof(float)));
}
void InitMat(float* mat, long long size) {
    srand(time(nullptr));
    for (long long i = 0; i < size; ++i) {
        mat[i] = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    }
}
void ZeroMat(float* mat, long long size) {
    srand(time(nullptr));
    for (int i = 0; i < size; ++i) {
        mat[i] = 0;
    }
}
void ParseArguments(const std::string& input, int& method, long long& size, bool& dump_result) {
    size_t method_pos = input.find("-method=");
    size_t size_pos = input.find("-size=");
    size_t dump_pos = input.find("-dump=");

    if (method_pos == std::string::npos || size_pos == std::string::npos || dump_pos == std::string::npos) {
        std::cerr << "Invalid input format. Please use '-method=', '-size=', '-dump=' format." << std::endl;
        return;
    }

    
    size_t method_end = input.find(" ", method_pos);
    std::string method_str = input.substr(method_pos + 8, method_end - method_pos - 8);
    use_mmap = (method_str == "mmap");

    size_t size_end = input.find(" ", size_pos);
    std::string size_str = input.substr(size_pos + 6, size_end - size_pos - 6);
    std::string size_value_str = size_str.substr(0, size_str.size() - 1);
    char unit = size_str.back();
    size = std::stoll(size_value_str);
    switch (unit) {
        case 'K':
            size *= 1024;
            break;
        case 'M':
            size *= 1024 * 1024;
            break;
        case 'G':
            size *= 1024 * 1024 * 1024;
            break;
        default:
            std::cerr << "Invalid size unit. Please use K, M, or G." << std::endl;
            return;
    }
    size_t dump_end = input.find(" ", dump_pos);
    std::string dump_str = input.substr(dump_pos + 6, dump_end - dump_pos - 6);
    dump_result = (dump_str == "true");
}
void WriteToMmap(void* mmap_ptr, float* matrix_ptr, long long size){
    for(int i=0; i<size/sizeof(float); i++){
        mmap_ptr[i]=matrix_ptr[i];
    }
}
void ReadFromMmap(float* matrix_ptr, void* mmap_ptr, long long size){
    for(int i=0; i<size/sizeof(float); i++){
        matrix_ptr[i]=mmap_ptr[i];
    }
}
void CompareMatrices( float* matrix1,  float* matrix2, long long size) {
    bool same = true;
    for (int i = 0; i < size; ++i) {
        if (matrix1[i] != matrix2[i]) {
            same = false;
            std::cout << "Different value at index " << i << ": matrix1 = " << matrix1[i] << ", matrix2 = " << matrix2[i] << std::endl;
        }
    }
    if (same) {
        std::cout << "Matrices are identical." << std::endl;
    }
}


int main(int argc, char *argv[]){

   if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input_string>" << std::endl;
        return 1;
    }

    std::string input = argv[1];
    int method = -1;
    long long size;
    bool dump_result;
    ParseArguments(input, method, size, dump_result);

    float *SrcIn = AllocMat(size);
    float *DstOut = AllocMat(size);
    float *Ans = AllocMat(size);

    InitMat(SrcIn, size);
    ZeroMat(DstOut, size);
    ZeroMat(Ans, size);
    memcpy(Ans, SrcIn, size);

    //Case 0: USE_MMAP
    if(method==USE_MMAP){
      int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
      if (fd == -1) {
          std::cerr << "Failed to open /dev/mem" << std::endl;
          return;
      }

      off_t offset = AIM_RESERVED_OFFSET;
      void *map_base = mmap(NULL,
    			            size,
    			            PROT_READ | PROT_WRITE,
    			            MAP_SHARED,
    			            mem_fd,
    			            offset);
      if (map_base == MAP_FAILED) {
        std::cerr << "Failed to mmap /dev/mem" << std::endl;
        close(fd);
        return;
      }
      memset(map_base, 0, size);

      //Host -> Card 
      WriteToMmap(map_base, SrcIn, size);
      //Card -> Host
      ReadFromMmap(DstOut, map_base, size);
    
      if(dump_result==TRUE) CompareMatrices(Ans, DstOut, size);

      if (munmap(map_base, size) == -1) {
        std::cerr << "Failed to munmap /dev/mem" << std::endl;
      }
      close(mem_fd);
    }
}

