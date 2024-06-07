// #define MAX_PIM 8
// #include "../include/pim.h"

// /* pim_src: Broadcast의 destination PIM 주소들로 구성된 array */
// void pimBcast(short** pim_dst, short* cpu_src, int size) {
//     for(int i=0; i<MAX_PIM; i++) {  // i: PIM ID
//         /* PIM ID에 따라 각 PIM memory에 host memory의 데이터를 전송 */
//         /* PCCL의 모든 데이터 전송 API는 내부적으로 XDMA를 활용함 */
//         datacopy_cpu2pim(pim_dst[i], cpu_src, size, i);
//     }
// }

// void pimScatter(short** pim_dst, short* cpu_src, int size) {
//     /* Scatter된 vector의 크기 */
//     int each_size = size/MAX_PIM;
//     for(int i=0; i<MAX_PIM; i++) {  // i: PIM ID   
//         /* PIM ID에 따라 각 PIM memory에 host memory의 vector를 truncate하여 전송 */
//         /* PCCL의 모든 데이터 전송 API는 내부적으로 XDMA를 활용함 */
//         datacopy_cpu2pim(pim_dst[i], (void*)cpu_src+i*each_size, each_size, i);
//     }
// }

// void pimGather(short* cpu_dst, short** pim_src, int size) {
//     /* Gather할 각 PIM memory 데이터의 크기 */
//     int each_size = size/MAX_PIM;
//     for(int i=0; i<MAX_PIM; i++) {  // i: PIM ID   
//         /* PIM ID에 따라 각 PIM memory의 데이터를 PIM ID에 따른 host memory의 영역에 전송 */
//         /* PCCL의 모든 데이터 전송 API는 내부적으로 XDMA를 활용함 */
//         datacopy_pim2cpu((void*)cpu_dst+i*each_size, pim_src[i], each_size, i);
//     }
// }

// // 이미 host memory에 할당된 영역의 주소를 입력하는 것으로 가정

// void pimAllReduce(short** pim_dst, short** pim_src, int size) {
//     /* 각 PIM memory 데이터의 element 수 */
//     int vector_size = size/sizeof(short); 
//     /* Host memory에서 gather를 위해 buffer 할당 */
//     short* gat_buff = (short*)calloc(size*MAX_PIM);
//     /* Reduction을 위해 우선 gather 수행 */
//     pimGather(gat_buff, pim_src, size);
//     /* Reduction을 위한 buffer 할당 */
//     short* red_buff = (short*)calloc(vector_size);
    
//     /* naive CPU Reduction 수행 */
//     for(int i=0; i<MAX_PIM; i++) {  // i: PIM ID   
//         for(int j=0; j<vector_size; j++) {
//             red_buff[j] += gat_buff[j];
//         }
//         /* Gather buffer offset 변경 */
//         gat_buff += size;
//     }

//     /* CPU buffers free */
//     free(red_buff);
//     free(gat_buff);
//     /* Reduction된 데이터를 모든 PIM memory에 broadcast */
//     pimBcast(pim_dst, red_buff, size);
// }

// void pimAllGather(short** pim_dst, short** pim_src, int size) {
//     /* 각 PIM memory 데이터의 element 수 */
//     int vector_size = size/sizeof(short);
//     /* Host memory에서 gather를 위해 buffer 할당 */
//     short* gat_buff = (short*)calloc(size*MAX_PIM);
//     /* Gather 수행 */
//     pimGather(gat_buff, pim_src, size);
//     /* CPU buffer free */
//     free(gat_buff);
//     /* Gather 결과vector를 모든 PIM memory에 broadcast*/
//     pimBcast(pim_dst, gat_buff, size);
// }


