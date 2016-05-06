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

struct dev_prescan_t {
	int blocks, levels, count, startIdx;
	unsigned int *blockCounts = nullptr, *Idxs = nullptr, *Sums = nullptr, N;
};

#if DEBUG
static unsigned int  *idxs = nullptr, *sums = nullptr;
#endif
#define IS_ERROR(error) isError(error, __FILE__, __LINE__, dev)
#define CHECK_ERROR(error) if (isError(error, __FILE__, __LINE__, dev)) return false;

static void freeCuda(dev_prescan_t& dev, bool onError = true);
static bool isError(cudaError_t error, const char *file, int line, dev_prescan_t& dev) {
	if (error != cudaSuccess) {
		ERROR_MESSAGE("prescan: %s in %s at line %d\n", cudaGetErrorString(error), file, line);
		freeCuda(dev);
		return true;
	}
	return false;
}

int getLengthAsPowersTwoSum(int n) {
	return (n / blockElements)*blockElements + (1 << (int)ceil(log((double)(n % blockElements)) / log(2)));
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

static bool initCUDA(dev_prescan_t& dev) {
	CHECK_ERROR(cudaMalloc((void**)&(dev.Idxs), dev.N*sizeof(unsigned int)));
	CHECK_ERROR(cudaMemset(dev.Idxs, 0, dev.N*sizeof(unsigned int)));

	dev.blockCounts = (unsigned int*)malloc(dev.levels*sizeof(unsigned int));

	dev.count = getLengthAsPowersTwoSum(dev.blocks);
	dev.blockCounts[0] = dev.count;
	if ((dev.count - 1) / blockElements >= 1) {// how many times must be prescan run
		int dx = dev.count;
		int i = 1;
		while (dx > 1) {
			dx = dx / blockElements + (dx % blockElements > 0);
			dx = getLengthAsPowersTwoSum(dx);
			dev.blockCounts[i++] = dx;
			dev.count += dx;
		}
	}
	else {
		dev.count++;
		dev.blockCounts[1] = 1;
	}
#if DEBUG
	for (int i = 0; i < dev.levels; i++) {
		printf("%d ", dev.blockCounts[i]);
	}printf("\n");
	getchar();

	idxs = (unsigned int*)malloc(dev.N*sizeof(unsigned int));
	sums = (unsigned int*)malloc(dev.count*sizeof(unsigned int));
#endif
	CHECK_ERROR(cudaMalloc((void**)&(dev.Sums), dev.count*sizeof(unsigned int)));
	CHECK_ERROR(cudaMemset(dev.Sums, 0, dev.count*sizeof(unsigned int)));
	return true;
}

static bool callPrescan(dev_prescan_t& dev) {
	prescan<<<dev.blocks, THREADS_PER_BLOCK, 2 * THREADS_PER_BLOCK*sizeof(unsigned int)>>>(dev.Idxs, dev.Idxs, dev.Sums, dev.N);

#if DEBUG
	CHECK_ERROR(cudaDeviceSynchronize());
	cudaMemcpy(idxs, dev.Idxs, dev.N*sizeof(unsigned int), cudaMemcpyDeviceToHost);
	for (int i = 0; i < dev.N; i++)	{
		printf("%d ", idxs[i]);
	}printf("\n");
	getchar();
#endif

	int dx, size, actBlocks;
	dev.count = dev.blockCounts[0];
	dev.startIdx = 0;
	for (int level = 1; level < dev.levels; level++) {// prescan of sums at each level
		dx = dev.blockCounts[level];
		size = dev.count - dev.startIdx;
		actBlocks = size / blockElements + (size % blockElements > 0);
#if DEBUG
		printf("%d %d %d %d %d\n", dx, dev.count, dev.startIdx, size, actBlocks);
#endif
		CHECK_ERROR(cudaDeviceSynchronize());
		prescan<<<actBlocks, THREADS_PER_BLOCK, 2 * THREADS_PER_BLOCK*sizeof(unsigned int)>>>(
			dev.Sums + dev.startIdx, dev.Sums + dev.startIdx, dev.Sums + dev.count, size);
		CHECK_ERROR(cudaGetLastError());
		dev.startIdx = dev.count;
		dev.count += dx;
	}

#if DEBUG
	cudaMemcpy(sums, dev.Sums, dev.count*sizeof(unsigned int), cudaMemcpyDeviceToHost);
	for (int i = 0; i < dev.count; i++)
	{
		printf("%d ", sums[i]);
	}printf("\n");
	getchar();
#endif
	return true;
}

static bool callAdd(int N, dev_prescan_t& dev) {
	int dx, level = dev.levels;
	if (level > 1) {
		dev.count -= dev.blockCounts[--level];
		dev.startIdx -= dev.blockCounts[level - 1];
	}
	while (level > 1) {
		dx = dev.blockCounts[--level];
		dev.count -= dx;
		dev.startIdx -= dev.blockCounts[level - 1];
#if DEBUG
		printf("%d %d %d\n", dx, count, startIdx);
#endif
		CHECK_ERROR(cudaDeviceSynchronize());
		add<<<2 * dx, THREADS_PER_BLOCK>>>(dev.Sums + dev.startIdx, dev.Sums + dev.count, dev.count - dev.startIdx);
		CHECK_ERROR(cudaGetLastError());
	}
	if (dev.blocks > 1) {// if all data are in one block, indexes are correct. otherwise add prescan sums
		CHECK_ERROR(cudaDeviceSynchronize());
		add<<<2 * dev.blocks, THREADS_PER_BLOCK>>>(dev.Idxs, dev.Sums, N);
		CHECK_ERROR(cudaGetLastError());
	}

#if DEBUG
	cudaMemcpy(idxs, dev.Idxs, dev.N*sizeof(unsigned int), cudaMemcpyDeviceToHost);
	for (int i = 0; i < dev.N; i++) {
		printf("%d ", idxs[i]);
	}printf("\n");
	getchar();
	cudaMemcpy(sums, dev.Sums, dev.count*sizeof(unsigned int), cudaMemcpyDeviceToHost);
	for (int i = 0; i < dev.count; i++) {
		printf("%d ", sums[i]);
	}printf("\n");
	getchar();
#endif
	return true;
}

static void freeCuda(dev_prescan_t& dev, bool onError) {

#define CUDA_FREE(ptr) if (ptr) {cudaFree(ptr); ptr = nullptr;}
#define MEM_FREE(ptr) if (ptr) {free(ptr); ptr = nullptr;}

	if (onError) CUDA_FREE(dev.Idxs);// cannot be freed because it is returned
	CUDA_FREE(dev.Sums);

	if (dev.levels > 1) MEM_FREE(dev.blockCounts);

#if DEBUG
	MEM_FREE(sums);
	MEM_FREE(idxs);
#endif
}

unsigned int * prescan(int N, unsigned int * devArrIn) {
	dev_prescan_t dev;
	dev.blocks = (N % blockElements == 0) ? N / blockElements : N / blockElements + 1;
	dev.levels = (int)ceil(log((double)N) / log((double)blockElements));
	dev.N = getLengthAsPowersTwoSum(N);

	if (!initCUDA(dev)) return nullptr;
	cudaMemcpy(dev.Idxs, devArrIn, N*sizeof(unsigned int), cudaMemcpyDeviceToDevice);

	if (!callPrescan(dev)) return nullptr;
	if (!callAdd(N, dev)) return nullptr;
	if (IS_ERROR(cudaDeviceSynchronize())) return nullptr;
	freeCuda(dev, false);

	return dev.Idxs;
}

