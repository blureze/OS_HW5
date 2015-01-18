#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <file.h>

// page size is 32 bytes
#define PAGESIZE 32
// 32 KB in shared memory
#define PHYSICAL_MEM_SIZE 32768
// 128 KB in global memory
#define STORAGE_SIZE 131072
#define DATAFILE "./data.bin"
#define OUTFILE "./snapshot.bin"

typedef unsigned char uchar;
typedef uint32_t u32;

// page table entries
__device__ __managed__ int PAGE_ENTRIES = 0;
// count the pagefault times
__device__ __managed__ int PAGEFAULT = 0;

// secondary memory
__device__ __managed__ uchar storage[STORAGE_SIZE];

// data input and output
__device__ __managed__ uchar results[STORAGE_SIZE];
__device__ __managed__ uchar input[STORAGE_SIZE];

// page table
extern __shared__ u32 pt[];

__device__ uchar Greed(uchar *buffer, u32 addr)
{
	u32 frame_num = addr / PAGESIZE;
	u32 offset = addr % PAGESIZE;

	addr = paging(buffer, frame_num, offset);

	return buffer[addr];
}

__device__ void Gwrite(uchar *buffer, u32 addr, uchar value)
{
	u32 frame_num = addr / PAGESIZE;
	u32 offset = addr % PAGESIZE;

	addr = paging(buffer, frame_num, offset);
	buffer[addr] = value;
}

__device__ void snapshot(uchar *results, uchar *buffer, int offset, int input_size)
{
	int i;

	for(i = 0; i < input_size; i++)
		results[i] = Gread(buffer, i+offset);
}

__global__ void mykernal(int input_size)
{
	// take shared memory as physical memory
	__shared__ uchar data[PHYSICAL_MEM_SIZE];
	// get page table entries
	int pt_entries = PHYSICAL_MEM_SIZE / PAGESIZE;

	// before first Gwrite or Gread
	init_pageTable(pt_entries);

	//####Gwrite/Gread code section start####
	for(int i = 0; i < input_size; i++)
		Gwrite(data, i, input[i]);

	for(int i = input_size-1; i >= input_size-10; i--)
		int value = Gread(data, i);

	// the last line of Gwrite/Gread code section should be snapshot()
	snapshot(results, data, 0, input_size);
	//####Gwrite/Gread code section end####
}

int main()
{
	int input_size = load_binaryFile(DATAFILE, input, STORAGE_SIZE);

	cudaSetDevice(1);
	mykernel<<<1, 1, 16384>>>(input_size);
	cudaDeviceSynchronize();
	cudaDeviceReset();

	write_binaryFile(OUTFILE, results, input_size);

	printf("pagefault times = %d\n", PAGEFAULT);

	return 0;
}

