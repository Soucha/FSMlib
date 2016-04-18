/* Copyright (c) Michal Soucha, 2016
*
* This file is part of FSMlib
*
* FSMlib is free software: you can redistribute it and/or modify it under
* the terms of the GNU General Public License as published by the Free Software
* Foundation, either version 3 of the License, or (at your option) any later
* version.
*
* FSMlib is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* FSMlib. If not, see <http://www.gnu.org/licenses/>.
*
*
*	Parallel sum
*	------------
*	The algorithm handles arbitrarily long data array.
*	It uses prescan function from http://http.developer.nvidia.com/GPUGems3/gpugems3_ch39.html
*	with arrays devIdxs and devSums for storing indexes and meta-sums of indexes respectively.
*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>

#include "cuda_runtime.h"
#include "device_launch_parameters.h"

#include "FSMlib.h"

#define THREADS_PER_BLOCK 1024 // need to be power of 2

#define NUM_BANKS 16  
#define LOG_NUM_BANKS 4  
#define CONFLICT_FREE_OFFSET(n) ((n) >> NUM_BANKS + (n) >> (2 * LOG_NUM_BANKS))


//#define DEBUG 0

const int blockElements = 2 * THREADS_PER_BLOCK;

static int blocks, levels, count, level, startIdx;
static unsigned int *blockCounts = NULL, *devIdxs = NULL, *devSums = NULL, devN;
#if DEBUG
static unsigned int  *idxs = NULL, *sums = NULL;
#endif
#define IS_ERROR(error) isError(error, __FILE__, __LINE__)
#define CHECK_ERROR(error) if (isError(error, __FILE__, __LINE__)) return false;

static void freeCuda(bool onError = true);
static bool isError(cudaError_t error, const char *file, int line) {
	if (error != cudaSuccess) {
		ERROR_MESSAGE("prescan: %s in %s at line %d\n", cudaGetErrorString(error), file, line);
		freeCuda();
		return true;
	}
	return false;
}

int getLengthAsPowersTwoSum(int n) {
	return (n / blockElements)*blockElements + (1 << (int)ceilf(logf(n % blockElements) / logf(2)));
}

__global__ void prescan(unsigned int *g_odata, unsigned int *g_idata, unsigned int *g_udata, int n) {
	// modified function from http://http.developer.nvidia.com/GPUGems3/gpugems3_ch39.html
	extern __shared__ int temp[];  // allocated on invocation  

	// set proper length of array in current block
	n = (n < (2 * blockDim.x * (blockIdx.x + 1))) ? (n - 2 * blockDim.x*blockIdx.x) : 2 * blockDim.x;

	int thid = threadIdx.x;
	if ((thid <= n / 2) && (n <= 1)) {
		g_udata[blockIdx.x] = g_idata[thid + 2 * blockDim.x*blockIdx.x];
		g_odata[thid + 2 * blockDim.x*blockIdx.x] = 0;
	}
	else if (thid < n / 2) {
		int offset = 1;
		int cai = thid;
		int cbi = thid + (n / 2);
		int bankOffsetA = CONFLICT_FREE_OFFSET(cai);
		int bankOffsetB = CONFLICT_FREE_OFFSET(cbi);
		temp[cai + bankOffsetA] = g_idata[cai + 2 * blockDim.x*blockIdx.x];// modification: data from the correct block
		temp[cbi + bankOffsetB] = g_idata[cbi + 2 * blockDim.x*blockIdx.x];
		for (int d = n >> 1; d > 0; d >>= 1) { // build sum in place up the tree  
			__syncthreads();
			if (thid < d) {
				int ai = offset*(2 * thid + 1) - 1;
				int bi = offset*(2 * thid + 2) - 1;
				ai += CONFLICT_FREE_OFFSET(ai);
				bi += CONFLICT_FREE_OFFSET(bi);
				temp[bi] += temp[ai];
			}
			offset *= 2;
		}
		if (thid == 0) {
			// store sum of block values
			g_udata[blockIdx.x] = temp[n - 1 + CONFLICT_FREE_OFFSET(n - 1)];
			temp[n - 1 + CONFLICT_FREE_OFFSET(n - 1)] = 0;
		}

		for (int d = 1; d < n; d *= 2) { // traverse down tree & build scan  
			offset >>= 1;
			__syncthreads();
			if (thid < d) {
				int ai = offset*(2 * thid + 1) - 1;
				int bi = offset*(2 * thid + 2) - 1;
				ai += CONFLICT_FREE_OFFSET(ai);
				bi += CONFLICT_FREE_OFFSET(bi);
				int t = temp[ai];
				temp[ai] = temp[bi];
				temp[bi] += t;
			}
		}
		__syncthreads();

		g_odata[cai + 2 * blockDim.x*blockIdx.x] = temp[cai + bankOffsetA];// modification: data to the correct block
		g_odata[cbi + 2 * blockDim.x*blockIdx.x] = temp[cbi + bankOffsetB];
	}
}

__global__ void add(unsigned int *out, unsigned int *in, int n) {
	int thid = threadIdx.x + blockDim.x* blockIdx.x;
	if (thid < n)
		out[thid] += in[blockIdx.x / 2];
}

static bool initCUDA() {
	CHECK_ERROR(cudaMalloc((void**)&(devIdxs), devN*sizeof(unsigned int)));
	CHECK_ERROR(cudaMemset(devIdxs, 0, devN*sizeof(unsigned int)));

	blockCounts = (unsigned int*)malloc(levels*sizeof(unsigned int));

	count = getLengthAsPowersTwoSum(blocks);
	blockCounts[0] = count;
	if ((count - 1) / blockElements >= 1) {// how many times must be prescan run
		int dx = count;
		int i = 1;
		while (dx > 1) {
			dx = dx / blockElements + (dx%blockElements>0);
			dx = getLengthAsPowersTwoSum(dx);
			blockCounts[i++] = dx;
			count += dx;
		}
	}
	else {
		count++;
		blockCounts[1] = 1;
	}
#if DEBUG
	for (int i = 0; i < levels; i++) {
		printf("%d ", blockCounts[i]);
	}printf("\n");
	getchar();

	idxs = (unsigned int*)malloc(devN*sizeof(unsigned int));
	sums = (unsigned int*)malloc(count*sizeof(unsigned int));
#endif
	CHECK_ERROR(cudaMalloc((void**)&(devSums), count*sizeof(unsigned int)));
	CHECK_ERROR(cudaMemset(devSums, 0, count*sizeof(unsigned int)));
	return true;
}

static bool callPrescan() {
	prescan<<<blocks, THREADS_PER_BLOCK, 2 * THREADS_PER_BLOCK*sizeof(unsigned int)>>>(devIdxs, devIdxs, devSums, devN);

#if DEBUG
	CHECK_ERROR(cudaDeviceSynchronize());
	cudaMemcpy(idxs, devIdxs, devN*sizeof(unsigned int), cudaMemcpyDeviceToHost);
	for (int i = 0; i < devN; i++)	{
		printf("%d ", idxs[i]);
	}printf("\n");
	getchar();
#endif

	int dx, size, actBlocks;
	level = 0;
	count = blockCounts[level];
	startIdx = 0;
	for (level = 1; level < levels; level++) {// prescan of sums at each level
		dx = blockCounts[level];
		size = count - startIdx;
		actBlocks = size / blockElements + (size % blockElements > 0);
#if DEBUG
		printf("%d %d %d %d %d\n", dx, count, startIdx, size, actBlocks);
#endif
		CHECK_ERROR(cudaDeviceSynchronize());
		prescan<<<actBlocks, THREADS_PER_BLOCK, 2 * THREADS_PER_BLOCK*sizeof(unsigned int)>>>(
			devSums + startIdx, devSums + startIdx, devSums + count, size);
		CHECK_ERROR(cudaGetLastError());
		startIdx = count;
		count += dx;
	}

#if DEBUG
	cudaMemcpy(sums, devSums, count*sizeof(unsigned int), cudaMemcpyDeviceToHost);
	for (int i = 0; i < count; i++)
	{
		printf("%d ", sums[i]);
	}printf("\n");
	getchar();
#endif
	return true;
}

static bool callAdd(int N) {
	int dx;
	if (level > 1) {
		count -= blockCounts[--level];
		startIdx -= blockCounts[level - 1];
	}
	while (level > 1) {
		dx = blockCounts[--level];
		count -= dx;
		startIdx -= blockCounts[level - 1];
#if DEBUG
		printf("%d %d %d\n", dx, count, startIdx);
#endif
		CHECK_ERROR(cudaDeviceSynchronize());
		add<<<2 * dx, THREADS_PER_BLOCK>>>(devSums + startIdx, devSums + count, count - startIdx);
		CHECK_ERROR(cudaGetLastError());
	}
	if (blocks > 1) {// if all data are in one block, indexes are correct. otherwise add prescan sums
		CHECK_ERROR(cudaDeviceSynchronize());
		add<<<2 * blocks, THREADS_PER_BLOCK>>>(devIdxs, devSums, N);
		CHECK_ERROR(cudaGetLastError());
	}

#if DEBUG
	cudaMemcpy(idxs, devIdxs, devN*sizeof(unsigned int), cudaMemcpyDeviceToHost);
	for (int i = 0; i < devN; i++) {
		printf("%d ", idxs[i]);
	}printf("\n");
	getchar();
	cudaMemcpy(sums, devSums, count*sizeof(unsigned int), cudaMemcpyDeviceToHost);
	for (int i = 0; i < count; i++) {
		printf("%d ", sums[i]);
	}printf("\n");
	getchar();
#endif
	return true;
}

static void freeCuda(bool onError) {

#define CUDA_FREE(ptr) if (ptr) {cudaFree(ptr); ptr = NULL;}
#define MEM_FREE(ptr) if (ptr) {free(ptr); ptr = NULL;}

	if (onError) CUDA_FREE(devIdxs);// cannot be freed because it is returned
	CUDA_FREE(devSums);

	if (levels > 1) MEM_FREE(blockCounts);

#if DEBUG
	MEM_FREE(sums);
	MEM_FREE(idxs);
#endif
}

unsigned int * prescan(int N, unsigned int * devArrIn) {
	blocks = (N % blockElements == 0) ? N / blockElements : N / blockElements + 1;
	levels = ceilf(logf(N) / logf(blockElements));
	devN = getLengthAsPowersTwoSum(N);

	if (!initCUDA()) return NULL;
	cudaMemcpy(devIdxs, devArrIn, N*sizeof(unsigned int), cudaMemcpyDeviceToDevice);

	if (!callPrescan()) return NULL;
	if (!callAdd(N)) return NULL;
	if (IS_ERROR(cudaDeviceSynchronize())) return NULL;
	freeCuda(false);

	return devIdxs;
}

