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
*/
#include "FSMsequence.h"

#include "cuda_runtime.h"
#include "device_launch_parameters.h"

extern unsigned int * prescan(int N, state_t * devArrIn);

namespace FSMsequence {

#define THREADS_PER_BLOCK 512
#define MAX_BLOCKS 1024

#define IS_ERROR(error) isError(error, __FILE__, __LINE__)
#define CHECK_ERROR(error) if (isError(error, __FILE__, __LINE__)) return false;
#define RETURN_ON_ERROR(error) if (isError(error, __FILE__, __LINE__)) return;

	static void freeCuda();
	static bool isError(cudaError_t error, const char *file, int line) {
		if (error != cudaSuccess) {
			ERROR_MESSAGE("%s in %s, line %d", cudaGetErrorString(error), file, line);
			freeCuda();
			return true;
		}
		return false;
	}

	static output_t * devStateOutput = NULL;
	static output_t * devTransitionOutput = NULL;
	static state_t * devNextState = NULL;
	static state_t * devMapping = NULL;
	static state_t * devNextDistIdx = NULL, *outNextDistIdx = NULL;
	static state_t * devDistinguishedCount = NULL;
	static input_t P, *devDistinguishing = NULL, *outDistinguishing = NULL;
	static state_t N, M;

	// Parallel SF
	static seq_len_t * devDistSeqLen = NULL;
	
	// Parallel Queue
	static state_t * devPrevIdx = NULL, *devPrevIdxLen = NULL;
	static state_t * devUnchecked = NULL, *devLinkPrev = NULL;
	static input_t *devLinkIn = NULL;

#if SEQUENCES_PERFORMANCE_TEST
	static cudaEvent_t start, stop;
	extern float gpuLoadTime, gpuProcessTime, gpuTotalTime;
#endif // SEQUENCES_PERFORMANCE_TEST

	extern state_t getIdx(vector<state_t>& states, state_t stateId);

	static bool initCuda(DFSM* fsm, bool useQueue) {
		auto states = fsm->getStates();
		if (fsm->isOutputState()) {
			CHECK_ERROR(cudaMalloc((void**)&(devStateOutput), N * sizeof(output_t)));
			output_t * outputs = new output_t[N];
			for (state_t state = 0; state < N; state++) {
				outputs[state] = fsm->getOutput(states[state], STOUT_INPUT);
			}
			CHECK_ERROR(cudaMemcpy(devStateOutput, outputs, N*sizeof(output_t), cudaMemcpyHostToDevice));
			delete outputs;
		}
		if (fsm->isOutputTransition()) {
			CHECK_ERROR(cudaMalloc((void**)&(devTransitionOutput), N * P * sizeof(output_t)));
			output_t * outputs = new output_t[N*P];
			for (state_t state = 0; state < N; state++) {
				for (input_t input = 0; input < P; input++) {
					outputs[state*P + input] = fsm->getOutput(states[state], input);
				}
			}
			CHECK_ERROR(cudaMemcpy(devTransitionOutput, outputs, N*P*sizeof(output_t), cudaMemcpyHostToDevice));
			delete outputs;
		}
		CHECK_ERROR(cudaMalloc((void**)&(devNextState), N * P * sizeof(state_t)));
		state_t * nextStates = new state_t[N*P];
		for (state_t state = 0; state < N; state++) {
			for (input_t input = 0; input < P; input++) {
				nextStates[state*P + input] = getIdx(states, fsm->getNextState(states[state], input));
			}
		}
		CHECK_ERROR(cudaMemcpy(devNextState, nextStates, N*P*sizeof(state_t), cudaMemcpyHostToDevice));
		delete nextStates;

		CHECK_ERROR(cudaMalloc((void**)&(devNextDistIdx), M*sizeof(state_t)));
		CHECK_ERROR(cudaMemset(devNextDistIdx, int(NULL_STATE), M*sizeof(state_t)));
		outNextDistIdx = new state_t[M];
		CHECK_ERROR(cudaMalloc((void**)&(devDistinguishing), M*sizeof(input_t)));

		outDistinguishing = new input_t[M];

		CHECK_ERROR(cudaMalloc((void**)&(devMapping), M*sizeof(state_t)));
		state_t * mapping = new state_t[M];
		state_t idx = 0;
		for (state_t i = 0; i < N - 1; i++) {
			for (state_t j = i + 1; j < N; j++) {
				mapping[idx++] = i;
			}
		}
		CHECK_ERROR(cudaMemcpy(devMapping, mapping, M*sizeof(state_t), cudaMemcpyHostToDevice));
		delete mapping;

		CHECK_ERROR(cudaMalloc((void**)&(devDistinguishedCount), sizeof(state_t)));
		CHECK_ERROR(cudaMemset(devDistinguishedCount, 0, sizeof(state_t)));
		
		if (useQueue) {// Queue
			CHECK_ERROR(cudaMalloc((void**)&(devUnchecked), M*sizeof(state_t)));
			CHECK_ERROR(cudaMalloc((void**)&(devPrevIdx), (M + 1)*sizeof(state_t)));
			CHECK_ERROR(cudaMemset(devPrevIdx, 0, (M + 1)*sizeof(state_t)));
		} else {// SF
			CHECK_ERROR(cudaMalloc((void**)&(devDistSeqLen), M*sizeof(seq_len_t)));
			CHECK_ERROR(cudaMemset(devDistSeqLen, 0, M*sizeof(seq_len_t)));
		}
		return true;
	}

#define CUDA_FREE(ptr) if (ptr) {cudaFree(ptr); ptr = NULL;}

	static void freeCuda() {
		CUDA_FREE(devNextState);
		CUDA_FREE(devStateOutput);
		CUDA_FREE(devTransitionOutput);
		CUDA_FREE(devNextDistIdx);
		CUDA_FREE(devDistinguishing);
		CUDA_FREE(devMapping);
		// Queue
		CUDA_FREE(devPrevIdx);
		CUDA_FREE(devUnchecked);
		CUDA_FREE(devLinkIn);
		CUDA_FREE(devLinkPrev);
		CUDA_FREE(devPrevIdxLen);			
		// SF
		CUDA_FREE(devDistSeqLen);
		
		if (outDistinguishing) {
			delete outDistinguishing;
			outDistinguishing = NULL;
		}
		if (outNextDistIdx) {
			delete outNextDistIdx;
			outNextDistIdx = NULL;
		}
	}

	// <--- SF's kernels --->

	__global__ void distinguishByStateOutputs(state_t M, state_t N, input_t P, state_t * distinguishedCount,
		state_t * mapping, state_t * nextDistIdx, input_t * distinguishing, state_t * distSeqLen, output_t * output) {
		state_t idx = threadIdx.x + blockIdx.x * blockDim.x;
		int distinguished = 0;
		if (idx < M) {
			state_t i = mapping[idx];
			state_t j = idx + 1 + i*(i + 3) / 2 - i*N;
			if (output[i] != output[j]) {
				distinguishing[idx] = STOUT_INPUT;
				nextDistIdx[idx] = idx;
				distSeqLen[idx] = 1;
				distinguished = 1;
			}
		}
		distinguished = __syncthreads_count(distinguished);
		if (threadIdx.x == 0) {
			atomicAdd(distinguishedCount, state_t(distinguished));
		}
	}

	__global__ void distinguishByTransitionOutputs(state_t M, state_t N, input_t P, state_t * distinguishedCount,
		state_t * mapping, state_t * nextDistIdx, input_t * distinguishing, state_t * distSeqLen, output_t * output) {
		state_t idx = threadIdx.x + blockIdx.x * blockDim.x;
		int distinguished = 0;
		if ((idx < M) && (nextDistIdx[idx] == NULL_STATE)) {
			state_t i = mapping[idx];
			state_t j = idx + 1 + i*(i + 3) / 2 - i*N;
			for (input_t input = 0; input < P; input++) {
				//printf("%d on %d: %d->%d %d->%d\n", idx, input, i, output[i*P + input], j, output[j*P + input]);
				if (output[i*P + input] != output[j*P + input]) {
					distinguishing[idx] = input;
					nextDistIdx[idx] = idx;
					distSeqLen[idx] = 1;
					distinguished = 1;
					break;
				}
			}
		}
		distinguished = __syncthreads_count(distinguished);
		if (threadIdx.x == 0) {
			atomicAdd(distinguishedCount, state_t(distinguished));
		}
	}

	__global__ void distinguishByNextStates(state_t M, state_t N, input_t P, seq_len_t len, state_t * distinguishedCount,
		state_t * mapping, state_t * nextDistIdx, input_t * distinguishing, state_t * distSeqLen, state_t * nextState) {
		state_t idx = threadIdx.x + blockIdx.x * blockDim.x;
		int distinguished = 0;
		if ((idx < M) && (nextDistIdx[idx] == NULL_STATE)) {
			state_t i = mapping[idx];
			state_t j = idx + 1 + i*(i + 3) / 2 - i*N;
			state_t nextStateI, nextStateJ, nextIdx;
			for (input_t input = 0; input < P; input++) {
				nextStateI = nextState[i*P + input];
				nextStateJ = nextState[j*P + input];
				//printf("%d on %d: %d->%d %d->%d\n", idx, input, i, nextStateI, j, nextStateJ);
				if (nextStateI != nextStateJ) {
					nextIdx = (nextStateI < nextStateJ) ?
						(nextStateI * N + nextStateJ - 1 - (nextStateI * (nextStateI + 3)) / 2) :
						(nextStateJ * N + nextStateI - 1 - (nextStateJ * (nextStateJ + 3)) / 2);
					if ((nextDistIdx[nextIdx] != NULL_STATE) && (distSeqLen[nextIdx] == len)) {
						distinguishing[idx] = input;
						nextDistIdx[idx] = nextIdx;
						distSeqLen[idx] = len + 1;
						distinguished = 1;
						break;
					}
				}
			}
		}
		distinguished = __syncthreads_count(distinguished);
		if (threadIdx.x == 0) {
			atomicAdd(distinguishedCount, state_t(distinguished));
		}
	}

	// <--- Queue's kernels --->

	__global__ void distinguishByOutputOrLink(state_t M, state_t N, input_t P, state_t * distinguishedCount,
		state_t * mapping, state_t * nextDistIdx, input_t * distinguishing,
		output_t * stateOutput, output_t * transitionOutput, state_t * nextState, state_t * unchecked, state_t * prevIdx) {

		state_t idx = threadIdx.x + blockIdx.x * blockDim.x;
		if (idx < M) {
			state_t i = mapping[idx];
			state_t j = idx + 1 + i*(i + 3) / 2 - i*N;
			if ((stateOutput != NULL) && (stateOutput[i] != stateOutput[j])) {
				distinguishing[idx] = STOUT_INPUT;
				nextDistIdx[idx] = idx;
				int uncheckedIdx = atomicAdd(distinguishedCount, 1);
				unchecked[uncheckedIdx] = idx;
			}
			else {
				int distinguished = 0;
				if (transitionOutput != NULL) {
					for (input_t input = 0; input < P; input++) {
						//printf("%d on %d: %d->%d %d->%d\n", idx, input, i, output[i*P + input], j, output[j*P + input]);
						if (transitionOutput[i*P + input] != transitionOutput[j*P + input]) {
							distinguishing[idx] = input;
							nextDistIdx[idx] = idx;
							int uncheckedIdx = atomicAdd(distinguishedCount, 1);
							unchecked[uncheckedIdx] = idx;
							distinguished = 1;
							break;
						}
					}
				}
				if (!distinguished) {
					state_t nextStateI, nextStateJ, nextIdx;
					for (input_t input = 0; input < P; input++) {
						nextStateI = nextState[i*P + input];
						nextStateJ = nextState[j*P + input];
						//printf("%d on %d: %d->%d %d->%d\n", idx, input, i, nextStateI, j, nextStateJ);
						if (nextStateI != nextStateJ) {
							nextIdx = (nextStateI < nextStateJ) ?
								(nextStateI * N + nextStateJ - 1 - (nextStateI * (nextStateI + 3)) / 2) :
								(nextStateJ * N + nextStateI - 1 - (nextStateJ * (nextStateJ + 3)) / 2);
							if (idx != nextIdx) {
								atomicAdd(prevIdx + nextIdx, 1);
							}
						}
					}
				}
			}
		}
	}
	
	__global__ void prevStateLink(state_t M, state_t N, input_t P,
		state_t * mapping, state_t * nextDistIdx, state_t * nextState,
		state_t * prevIdx, state_t * prevIdxLen, input_t * linkIn, state_t * linkPrev) {

		state_t idx = threadIdx.x + blockIdx.x * blockDim.x;
		if ((idx < M) && (nextDistIdx[idx] == NULL_STATE)) {
			state_t i = mapping[idx];
			state_t j = idx + 1 + i*(i + 3) / 2 - i*N;
			state_t nextStateI, nextStateJ, nextIdx;
			for (input_t input = 0; input < P; input++) {
				nextStateI = nextState[i*P + input];
				nextStateJ = nextState[j*P + input];
				//printf("%d on %d: %d->%d %d->%d\n", idx, input, i, nextStateI, j, nextStateJ);
				if (nextStateI != nextStateJ) {
					nextIdx = (nextStateI < nextStateJ) ?
						(nextStateI * N + nextStateJ - 1 - (nextStateI * (nextStateI + 3)) / 2) :
						(nextStateJ * N + nextStateI - 1 - (nextStateJ * (nextStateJ + 3)) / 2);
					if (idx != nextIdx) {
						state_t basePrevIdx = atomicAdd(prevIdx + nextIdx, 1);
						basePrevIdx += prevIdxLen[nextIdx];
						linkIn[basePrevIdx] = input;
						linkPrev[basePrevIdx] = idx;
					}
				}
			}
		}
	}

	__global__ void processUnchecked(state_t M, state_t * distinguishedCount,
		state_t * nextDistIdx, input_t * distinguishing, state_t * unchecked,
		state_t * prevIdx, state_t * prevIdxLen, input_t * linkIn, state_t * linkPrev) {

		int base, count = 0;
		do {
			base = count;
			count = *distinguishedCount;
			__syncthreads();
			while (int(threadIdx.x) < count - base) {
				//printf("%d in %d (%d-%d)\n", threadIdx.x, count - base, count, base);
				state_t nextIdx = unchecked[base + threadIdx.x];
				state_t size = prevIdx[nextIdx];
				for (state_t k = 0; k < size; k++) {
					state_t prev = linkPrev[prevIdxLen[nextIdx] + k];
					state_t val = atomicCAS(nextDistIdx + prev, NULL_STATE, nextIdx);
					if (val == NULL_STATE) {
						distinguishing[prev] = linkIn[prevIdxLen[nextIdx] + k];
						int uncheckedIdx = atomicAdd(distinguishedCount, 1);
						unchecked[uncheckedIdx] = prev;
					}
				}
				base += blockDim.x;
			}
			__syncthreads();
		} while (*distinguishedCount < M);
	}

	// <--- common functions --->

	static bool getSequences(DFSM * fsm, vector<sequence_in_t> & seq) {
#if SEQUENCES_PERFORMANCE_TEST
		CHECK_ERROR(cudaEventRecord(stop, 0));
		CHECK_ERROR(cudaEventSynchronize(stop));
		CHECK_ERROR(cudaEventElapsedTime(&gpuProcessTime, start, stop));
#endif // SEQUENCES_PERFORMANCE_TEST

		cudaMemcpy(outDistinguishing, devDistinguishing, M*sizeof(input_t), cudaMemcpyDeviceToHost);
		cudaMemcpy(outNextDistIdx, devNextDistIdx, M*sizeof(state_t), cudaMemcpyDeviceToHost);

		state_t nextIdx;
		seq.resize(M);
		for (state_t idx = 0; idx < M; idx++) {
			seq[idx].clear();
			nextIdx = idx;
			seq[idx].push_back(outDistinguishing[nextIdx]);
			while (nextIdx != outNextDistIdx[nextIdx]) {
				nextIdx = outNextDistIdx[nextIdx];
				if (fsm->isOutputTransition() || (outDistinguishing[nextIdx] != STOUT_INPUT)) // filter last STOUT for Moore and DFA
					seq[idx].push_back(outDistinguishing[nextIdx]);
			}
		}

		freeCuda();

#if SEQUENCES_PERFORMANCE_TEST
		CHECK_ERROR(cudaEventRecord(stop, 0));
		CHECK_ERROR(cudaEventSynchronize(stop));
		CHECK_ERROR(cudaEventElapsedTime(&gpuTotalTime, start, stop));
		CHECK_ERROR(cudaEventDestroy(start));
		CHECK_ERROR(cudaEventDestroy(stop));
#endif // SEQUENCES_PERFORMANCE_TEST
		return true;
	}

	void getStatePairsShortestSeparatingSequences_ParallelSF(DFSM * fsm, vector<sequence_in_t> & seq) {
		N = fsm->getNumberOfStates();
		P = fsm->getNumberOfInputs();
		M = ((N - 1) * N) / 2;
		seq.clear();
		if (M > MAX_BLOCKS * THREADS_PER_BLOCK) {
			ERROR_MESSAGE("%s::getStatePairsShortestSeparatingSequences_ParallelSF - too many states (%d), max is %d",
				machineTypeNames[fsm->getType()], M, MAX_BLOCKS * THREADS_PER_BLOCK);
			return; 
		}
#if SEQUENCES_PERFORMANCE_TEST
		RETURN_ON_ERROR(cudaEventCreate(&start));
		RETURN_ON_ERROR(cudaEventCreate(&stop));
		RETURN_ON_ERROR(cudaEventRecord(start, 0));
#endif // SEQUENCES_PERFORMANCE_TEST

		if (!initCuda(fsm, false)) return;

#if SEQUENCES_PERFORMANCE_TEST
		RETURN_ON_ERROR(cudaEventRecord(stop, 0));
		RETURN_ON_ERROR(cudaEventSynchronize(stop));
		RETURN_ON_ERROR(cudaEventElapsedTime(&gpuLoadTime, start, stop));
#endif // SEQUENCES_PERFORMANCE_TEST

		unsigned int threads, blocks;
		threads = (M < THREADS_PER_BLOCK) ? M : THREADS_PER_BLOCK;
		blocks = M / threads + (M % threads > 0);
		
#if SEQUENCES_PERFORMANCE_TEST
		RETURN_ON_ERROR(cudaEventRecord(start, 0));
#endif // SEQUENCES_PERFORMANCE_TEST

		if (fsm->isOutputState()) {
			distinguishByStateOutputs<<<blocks, threads>>>(M, N, P, devDistinguishedCount,
				devMapping, devNextDistIdx, devDistinguishing, devDistSeqLen, devStateOutput);
			RETURN_ON_ERROR(cudaGetLastError());
			RETURN_ON_ERROR(cudaDeviceSynchronize());
		}
		if (fsm->isOutputTransition()) {
			distinguishByTransitionOutputs<<<blocks, threads>>>(M, N, P, devDistinguishedCount,
				devMapping, devNextDistIdx, devDistinguishing, devDistSeqLen, devTransitionOutput);
			RETURN_ON_ERROR(cudaGetLastError());
			RETURN_ON_ERROR(cudaDeviceSynchronize());
		}
		
		state_t count;
		cudaMemcpy(&count, devDistinguishedCount, sizeof(state_t), cudaMemcpyDeviceToHost);
		//printf("distinguished: %d\n", count);

		seq_len_t len = 0;
		while (count < M) {
			distinguishByNextStates<<<blocks, threads>>>(M, N, P, ++len, devDistinguishedCount,
				devMapping, devNextDistIdx, devDistinguishing, devDistSeqLen, devNextState);
			cudaDeviceSynchronize();
			cudaMemcpy(&count, devDistinguishedCount, sizeof(state_t), cudaMemcpyDeviceToHost);
			//printf("distinguished: %d\n", count);
			//getchar();
		}

		if (!getSequences(fsm, seq)) seq.clear();
	}

	void getStatePairsShortestSeparatingSequences_ParallelQueue(DFSM * fsm, vector<sequence_in_t> & seq) {
		N = fsm->getNumberOfStates();
		P = fsm->getNumberOfInputs();
		M = ((N - 1) * N) / 2;
		seq.clear();
		if (M > MAX_BLOCKS * THREADS_PER_BLOCK) {
			ERROR_MESSAGE("%s::getStatePairsShortestSeparatingSequences_ParallelQueue - too many states (%d), max is %d",
				machineTypeNames[fsm->getType()], M, MAX_BLOCKS * THREADS_PER_BLOCK);
			return;
		}
#if SEQUENCES_PERFORMANCE_TEST
		RETURN_ON_ERROR(cudaEventCreate(&start));
		RETURN_ON_ERROR(cudaEventCreate(&stop));
		RETURN_ON_ERROR(cudaEventRecord(start, 0));
#endif // SEQUENCES_PERFORMANCE_TEST

		if (!initCuda(fsm, true)) return;

#if SEQUENCES_PERFORMANCE_TEST
		RETURN_ON_ERROR(cudaEventRecord(stop, 0));
		RETURN_ON_ERROR(cudaEventSynchronize(stop));
		RETURN_ON_ERROR(cudaEventElapsedTime(&gpuLoadTime, start, stop));
#endif // SEQUENCES_PERFORMANCE_TEST

		unsigned int threads, blocks;
		threads = (M < THREADS_PER_BLOCK) ? M : THREADS_PER_BLOCK;
		blocks = M / threads + (M % threads > 0);

#if SEQUENCES_PERFORMANCE_TEST
		RETURN_ON_ERROR(cudaEventRecord(start, 0));
#endif // SEQUENCES_PERFORMANCE_TEST

		distinguishByOutputOrLink<<<blocks, threads>>>(M, N, P, devDistinguishedCount,
				devMapping, devNextDistIdx, devDistinguishing,
				devStateOutput, devTransitionOutput, devNextState, devUnchecked, devPrevIdx);
		RETURN_ON_ERROR(cudaGetLastError());
		RETURN_ON_ERROR(cudaDeviceSynchronize());

		state_t count;
		cudaMemcpy(&count, devDistinguishedCount, sizeof(state_t), cudaMemcpyDeviceToHost);
		//printf("distinguished: %d\n", count);

		if (count < M) {
#if DEBUG
			state_t * tmp = (state_t*)malloc((M + 1)*sizeof(state_t));
			cudaMemcpy(tmp, devPrevIdx, (M + 1)*sizeof(state_t), cudaMemcpyDeviceToHost);
			for (int i = 0; i <= M; i++) {
				printf("%d ", tmp[i]);
			}printf("\n");
			getchar();
#endif
			devPrevIdxLen = prescan(M + 1, devPrevIdx);
			if (devPrevIdxLen == NULL) {
				freeCuda();
				return;
			}
#if DEBUG
			cudaMemcpy(tmp, devPrevIdxLen, (M + 1)*sizeof(state_t), cudaMemcpyDeviceToHost);
			for (int i = 0; i <= M; i++) {
				printf("%d ", tmp[i]);
			}printf("\n");
			getchar();
#endif
			state_t linkSize;
			cudaMemcpy(&linkSize, devPrevIdxLen + M, sizeof(state_t), cudaMemcpyDeviceToHost);

			RETURN_ON_ERROR(cudaMemset(devPrevIdx, 0, M*sizeof(state_t)));
			RETURN_ON_ERROR(cudaMalloc((void**)&(devLinkIn), linkSize*sizeof(input_t)));
			RETURN_ON_ERROR(cudaMalloc((void**)&(devLinkPrev), linkSize*sizeof(state_t)));

			prevStateLink<<<blocks, threads>>>(M, N, P, devMapping, devNextDistIdx,
				devNextState, devPrevIdx, devPrevIdxLen, devLinkIn, devLinkPrev);
			RETURN_ON_ERROR(cudaGetLastError());
			RETURN_ON_ERROR(cudaDeviceSynchronize());
#if DEBUG
			cudaMemcpy(tmp, devPrevIdx, (M + 1)*sizeof(state_t), cudaMemcpyDeviceToHost);
			for (int i = 0; i <= M; i++) {
				printf("%d ", tmp[i]);
			}printf("\n");
			cudaMemcpy(tmp, devPrevIdxLen, (M + 1)*sizeof(state_t), cudaMemcpyDeviceToHost);
			for (int i = 0; i <= M; i++) {
				printf("%d ", tmp[i]);
			}printf("\n");
			getchar();
			free(tmp);

			tmp = (state_t*)malloc(linkSize*sizeof(state_t));
			cudaMemcpy(tmp, devLinkPrev, linkSize*sizeof(state_t), cudaMemcpyDeviceToHost);
			for (int i = 0; i < linkSize; i++) {
				printf("%d ", tmp[i]);
			}printf("\n");
			cudaMemcpy(tmp, devLinkIn, linkSize*sizeof(state_t), cudaMemcpyDeviceToHost);
			for (int i = 0; i < linkSize; i++) {
				printf("%d ", tmp[i]);
			}printf("\n");
			getchar();
			free(tmp);
#endif
			processUnchecked<<<1, min(THREADS_PER_BLOCK, M)>>>(M, devDistinguishedCount,
				devNextDistIdx, devDistinguishing, devUnchecked, devPrevIdx, devPrevIdxLen, devLinkIn, devLinkPrev);
			RETURN_ON_ERROR(cudaGetLastError());
#if DEBUG
			cudaMemcpy(&count, devDistinguishedCount, sizeof(state_t), cudaMemcpyDeviceToHost);
			printf("distinguished: %d\n", count);
			getchar();
#endif
		}

		if (!getSequences(fsm, seq)) seq.clear();
	}
}
