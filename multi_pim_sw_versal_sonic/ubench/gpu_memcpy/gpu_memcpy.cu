#include "gpu_memcpy.h"

__global__ void gpu_print(short *a, int size) {
  for(int i = 0; i < size/2; ++i) printf("d_a[%d]: %d\n", i, a[i]);
}
void CUDART_CB my_callback(cudaStream_t stream, cudaError_t status, void* data)
{
    printf("callback from stream %d\n", *((int*)data));
}
// int memcpy_host2gpu(short* gpu_dst, short* host_src, size_t size) {
short* memcpy_host2gpu(short* host_src, int size) {    

    int dev = 0;
    cudaDeviceProp deviceProp;
    CHECK_CUDA(cudaGetDeviceProperties(&deviceProp, dev));
    printf("> Using device %d: %s\n", dev, deviceProp.name);
    CHECK_CUDA(cudaSetDevice(dev));


  short *d_a;
  CHECK_CUDA(cudaMalloc(&d_a, size));
  int stream_ids[1];
  stream_ids[0] = 0;
  cudaStream_t *streams = (cudaStream_t*)malloc(sizeof(cudaStream_t));
  CHECK_CUDA(cudaStreamCreate(&streams[0]));
  CHECK_CUDA(cudaStreamAddCallback(streams[0], my_callback, (void*)stream_ids, 0));

  CHECK_CUDA(cudaDeviceSynchronize());
  // CHECK_CUDA(cudaHostRegister(host_src, size, cudaHostRegisterDefault));
  // CHECK_CUDA(cudaHostRegister(host_src, size, cudaHostRegisterMapped));
  CHECK_CUDA(cudaHostRegister(host_src, size, cudaHostRegisterPortable));
  // CHECK_CUDA(cudaHostRegister((void*)host_src, size, cudaHostRegisterIoMemory));
  getchar();
  
  short* host_src_d;
  CHECK_CUDA(cudaHostGetDevicePointer((void**)&host_src_d, host_src, 0));
  
  // for(int i = 0; i < 100; ++i) printf("test_register[%d]: %d\n", i, test_register[i]);

  {
    CHECK_CUDA(cudaMemcpy(d_a, host_src_d, size, cudaMemcpyHostToDevice));
    // CHECK_CUDA(cudaMemcpy(host_src, d_a, size, cudaMemcpyDeviceToHost));
    gpu_print<<<1, 1>>>(d_a, size);
    CHECK_CUDA(cudaDeviceSynchronize());
  }
  printf("gpu addr: %x\n", d_a);
  // getchar();
  // free host registered space?
  CHECK_CUDA(cudaHostUnregister(host_src));
  return d_a;
}

short* memcpy_gpu2host(short* Host_dst, short* GPU_dst, int size) {
  CHECK_CUDA(cudaHostRegister(Host_dst, size, cudaHostRegisterIoMemory));

  short* host_dst_d;
  CHECK_CUDA(cudaHostGetDevicePointer((void**)&host_dst_d, Host_dst, 0));


  {
    CHECK_CUDA(cudaMemcpy(host_dst_d, GPU_dst, size, cudaMemcpyDeviceToHost));
    // gpu_print<<<1, 1>>>(d_a, size);
    CHECK_CUDA(cudaDeviceSynchronize());
  }
  // getchar();
  return NULL;
}