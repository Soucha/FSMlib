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

#define IS_ERROR(error) isError(error, __FILE__, __LINE__, dev)
#define CHECK_ERROR(error) if (isError(error, __FILE__, __LINE__, dev)) return false;
#define RETURN_ON_ERROR(error) if (isError(error, __FILE__, __LINE__, dev)) return sequence_vec_t();

	struct dev_ptrs_t {
		output_t * StateOutput = nullptr;
		output_t * TransitionOutput = nullptr;
		state_t * NextState = nullptr;
		state_t * Mapping = nullptr;
		state_t * NextDistIdx = nullptr;
		state_t * DistinguishedCount = nullptr;
		input_t * Distinguishing = nullptr;
		
		// Parallel SF
		seq_len_t * DistSeqLen = nullptr;

		// Parallel Queue
		state_t * PrevIdx = nullptr, * PrevIdxLen = nullptr;
		state_t * Unchecked = nullptr, * LinkPrev = nullptr;
		input_t * LinkIn = nullptr;
	};

	static void freeCuda(dev_ptrs_t& dev);
	static bool isError(cudaError_t error, const char *file, int line, dev_ptrs_t& dev) {
		if (error != cudaSuccess) {
			ERROR_MESSAGE("%s in %s, line %d", cudaGetErrorString(error), file, line);
			freeCuda(dev);
			return true;
		}
		return false;
	}

#if SEQUENCES_PERFORMANCE_TEST
	static cudaEvent_t start, stop;
	extern float gpuLoadTime, gpuProcessTime, gpuTotalTime;
#endif // SEQUENCES_PERFORMANCE_TEST

	static bool initCuda(const unique_ptr<DFSM>& fsm, bool useQueue, dev_ptrs_t& dev) {
		state_t N = fsm->getNumberOfStates();
		input_t P = fsm->getNumberOfInputs();
		state_t M = ((N - 1) * N) / 2;
		if (fsm->isOutputState()) {
			CHECK_ERROR(cudaMalloc((void**)&(dev.StateOutput), N * sizeof(output_t)));
			output_t * outputs = new output_t[N];
			for (state_t state = 0; state < N; state++) {
				outputs[state] = fsm->getOutput(state, STOUT_INPUT);
			}
			CHECK_ERROR(cudaMemcpy(dev.StateOutput, outputs, N*sizeof(output_t), cudaMemcpyHostToDevice));
			delete outputs;
		}
		if (fsm->isOutputTransition()) {
			CHECK_ERROR(cudaMalloc((void**)&(dev.TransitionOutput), N * P * sizeof(output_t)));
			output_t * outputs = new output_t[N*P];
			for (state_t state = 0; state < N; state++) {
				for (input_t input = 0; input < P; input++) {
					outputs[state*P + input] = fsm->getOutput(state, input);
				}
			}
			CHECK_ERROR(cudaMemcpy(dev.TransitionOutput, outputs, N*P*sizeof(output_t), cudaMemcpyHostToDevice));
			delete outputs;
		}
		CHECK_ERROR(cudaMalloc((void**)&(dev.NextState), N * P * sizeof(state_t)));
		state_t * nextStates = new state_t[N*P];
		for (state_t state = 0; state < N; state++) {
			for (input_t input = 0; input < P; input++) {
				nextStates[state*P + input] = fsm->getNextState(state, input);
			}
		}
		CHECK_ERROR(cudaMemcpy(dev.NextState, nextStates, N*P*sizeof(state_t), cudaMemcpyHostToDevice));
		delete nextStates;

		CHECK_ERROR(cudaMalloc((void**)&(dev.NextDistIdx), M*sizeof(state_t)));
		CHECK_ERROR(cudaMemset(dev.NextDistIdx, int(NULL_STATE), M*sizeof(state_t)));
		CHECK_ERROR(cudaMalloc((void**)&(dev.Distinguishing), M*sizeof(input_t)));

		CHECK_ERROR(cudaMalloc((void**)&(dev.Mapping), M*sizeof(state_t)));
		state_t * mapping = new state_t[M];
		state_t idx = 0;
		for (state_t i = 0; i < N - 1; i++) {
			for (state_t j = i + 1; j < N; j++) {
				mapping[idx++] = i;
			}
		}
		CHECK_ERROR(cudaMemcpy(dev.Mapping, mapping, M*sizeof(state_t), cudaMemcpyHostToDevice));
		delete mapping;

		CHECK_ERROR(cudaMalloc((void**)&(dev.DistinguishedCount), sizeof(state_t)));
		CHECK_ERROR(cudaMemset(dev.DistinguishedCount, 0, sizeof(state_t)));
		
		if (useQueue) {// Queue
			CHECK_ERROR(cudaMalloc((void**)&(dev.Unchecked), M*sizeof(state_t)));
			CHECK_ERROR(cudaMalloc((void**)&(dev.PrevIdx), (M + 1)*sizeof(state_t)));
			CHECK_ERROR(cudaMemset(dev.PrevIdx, 0, (M + 1)*sizeof(state_t)));
		} else {// SF
			CHECK_ERROR(cudaMalloc((void**)&(dev.DistSeqLen), M*sizeof(seq_len_t)));
			CHECK_ERROR(cudaMemset(dev.DistSeqLen, 0, M*sizeof(seq_len_t)));
		}
		return true;
	}

#define CUDA_FREE(ptr) if (ptr) {cudaFree(ptr); ptr = nullptr;}

	static void freeCuda(dev_ptrs_t& dev) {
		CUDA_FREE(dev.NextState);
		CUDA_FREE(dev.StateOutput);
		CUDA_FREE(dev.TransitionOutput);
		CUDA_FREE(dev.NextDistIdx);
		CUDA_FREE(dev.Distinguishing);
		CUDA_FREE(dev.Mapping);
		// Queue
		CUDA_FREE(dev.PrevIdx);
		CUDA_FREE(dev.Unchecked);
		CUDA_FREE(dev.LinkIn);
		CUDA_FREE(dev.LinkPrev);
		CUDA_FREE(dev.PrevIdxLen);			
		// SF
		CUDA_FREE(dev.DistSeqLen);
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
			if ((stateOutput) && (stateOutput[i] != stateOutput[j])) {
				distinguishing[idx] = STOUT_INPUT;
				nextDistIdx[idx] = idx;
				int uncheckedIdx = atomicAdd(distinguishedCount, 1);
				unchecked[uncheckedIdx] = idx;
			}
			else {
				int distinguished = 0;
				if (transitionOutput) {
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

	static sequence_vec_t getSequences(const unique_ptr<DFSM>& fsm, dev_ptrs_t& dev, state_t& M, bool useStout) {
#if SEQUENCES_PERFORMANCE_TEST
		RETURN_ON_ERROR(cudaEventRecord(stop, 0));
		RETURN_ON_ERROR(cudaEventSynchronize(stop));
		RETURN_ON_ERROR(cudaEventElapsedTime(&gpuProcessTime, start, stop));
#endif // SEQUENCES_PERFORMANCE_TEST

		state_t *outNextDistIdx = new state_t[M];
		input_t * outDistinguishing = new input_t[M];
		cudaMemcpy(outDistinguishing, dev.Distinguishing, M*sizeof(input_t), cudaMemcpyDeviceToHost);
		cudaMemcpy(outNextDistIdx, dev.NextDistIdx, M*sizeof(state_t), cudaMemcpyDeviceToHost);

		state_t nextIdx;
		sequence_vec_t seq(M);
		for (state_t idx = 0; idx < M; idx++) {
			seq[idx].clear();
			nextIdx = idx;
			seq[idx].push_back(outDistinguishing[nextIdx]);
			while (nextIdx != outNextDistIdx[nextIdx]) {
				nextIdx = outNextDistIdx[nextIdx];
				if (outDistinguishing[nextIdx] != STOUT_INPUT) {
					if (useStout && seq[idx].back() != STOUT_INPUT) seq[idx].push_back(STOUT_INPUT);
					seq[idx].push_back(outDistinguishing[nextIdx]);
				} else if (fsm->isOutputTransition()) // filter last STOUT for Moore and DFA
					seq[idx].push_back(outDistinguishing[nextIdx]);
			}
		}
		delete outDistinguishing;
		delete outNextDistIdx;
		freeCuda(dev);

#if SEQUENCES_PERFORMANCE_TEST
		RETURN_ON_ERROR(cudaEventRecord(stop, 0));
		RETURN_ON_ERROR(cudaEventSynchronize(stop));
		RETURN_ON_ERROR(cudaEventElapsedTime(&gpuTotalTime, start, stop));
		RETURN_ON_ERROR(cudaEventDestroy(start));
		RETURN_ON_ERROR(cudaEventDestroy(stop));
#endif // SEQUENCES_PERFORMANCE_TEST
		return seq;
	}

	sequence_vec_t getStatePairsShortestSeparatingSequences_ParallelSF(const unique_ptr<DFSM>& fsm, bool omitUnnecessaryStoutInputs) {
		RETURN_IF_UNREDUCED(fsm, "FSMsequence::getStatePairsShortestSeparatingSequences_ParallelSF", sequence_vec_t());
		state_t N = fsm->getNumberOfStates();
		input_t P = fsm->getNumberOfInputs();
		state_t M = ((N - 1) * N) / 2;
		if (M > MAX_BLOCKS * THREADS_PER_BLOCK) {
			ERROR_MESSAGE("%s::getStatePairsShortestSeparatingSequences_ParallelSF - too many states (%d), max is %d",
				machineTypeNames[fsm->getType()], M, MAX_BLOCKS * THREADS_PER_BLOCK);
			return sequence_vec_t();
		}
		dev_ptrs_t dev;
#if SEQUENCES_PERFORMANCE_TEST
		RETURN_ON_ERROR(cudaEventCreate(&start));
		RETURN_ON_ERROR(cudaEventCreate(&stop));
		RETURN_ON_ERROR(cudaEventRecord(start, 0));
#endif // SEQUENCES_PERFORMANCE_TEST

		if (!initCuda(fsm, false, dev)) return sequence_vec_t();

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
			distinguishByStateOutputs<<<blocks, threads>>>(M, N, P, dev.DistinguishedCount,
				dev.Mapping, dev.NextDistIdx, dev.Distinguishing, dev.DistSeqLen, dev.StateOutput);
			RETURN_ON_ERROR(cudaGetLastError());
			RETURN_ON_ERROR(cudaDeviceSynchronize());
		}
		if (fsm->isOutputTransition()) {
			distinguishByTransitionOutputs<<<blocks, threads>>>(M, N, P, dev.DistinguishedCount,
				dev.Mapping, dev.NextDistIdx, dev.Distinguishing, dev.DistSeqLen, dev.TransitionOutput);
			RETURN_ON_ERROR(cudaGetLastError());
			RETURN_ON_ERROR(cudaDeviceSynchronize());
		}
		
		state_t count;
		cudaMemcpy(&count, dev.DistinguishedCount, sizeof(state_t), cudaMemcpyDeviceToHost);
		//printf("distinguished: %d\n", count);

		seq_len_t len = 0;
		while (count < M) {
			distinguishByNextStates<<<blocks, threads>>>(M, N, P, ++len, dev.DistinguishedCount,
				dev.Mapping, dev.NextDistIdx, dev.Distinguishing, dev.DistSeqLen, dev.NextState);
			RETURN_ON_ERROR(cudaDeviceSynchronize());
			cudaMemcpy(&count, dev.DistinguishedCount, sizeof(state_t), cudaMemcpyDeviceToHost);
			//printf("distinguished: %d\n", count);
			//getchar();
		}
		return getSequences(fsm, dev, M, !omitUnnecessaryStoutInputs && fsm->isOutputState());
	}

	sequence_vec_t getStatePairsShortestSeparatingSequences_ParallelQueue(const unique_ptr<DFSM>& fsm, bool omitUnnecessaryStoutInputs) {
		RETURN_IF_UNREDUCED(fsm, "FSMsequence::getStatePairsShortestSeparatingSequences_ParallelQueue", sequence_vec_t());
		state_t N = fsm->getNumberOfStates();
		input_t P = fsm->getNumberOfInputs();
		state_t M = ((N - 1) * N) / 2;
		if (M > MAX_BLOCKS * THREADS_PER_BLOCK) {
			ERROR_MESSAGE("%s::getStatePairsShortestSeparatingSequences_ParallelQueue - too many states (%d), max is %d",
				machineTypeNames[fsm->getType()], M, MAX_BLOCKS * THREADS_PER_BLOCK);
			return sequence_vec_t();
		}
		dev_ptrs_t dev;
#if SEQUENCES_PERFORMANCE_TEST
		RETURN_ON_ERROR(cudaEventCreate(&start));
		RETURN_ON_ERROR(cudaEventCreate(&stop));
		RETURN_ON_ERROR(cudaEventRecord(start, 0));
#endif // SEQUENCES_PERFORMANCE_TEST

		if (!initCuda(fsm, true, dev)) return sequence_vec_t();

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

		distinguishByOutputOrLink<<<blocks, threads>>>(M, N, P, dev.DistinguishedCount,
				dev.Mapping, dev.NextDistIdx, dev.Distinguishing,
				dev.StateOutput, dev.TransitionOutput, dev.NextState, dev.Unchecked, dev.PrevIdx);
		RETURN_ON_ERROR(cudaGetLastError());
		RETURN_ON_ERROR(cudaDeviceSynchronize());

		state_t count;
		cudaMemcpy(&count, dev.DistinguishedCount, sizeof(state_t), cudaMemcpyDeviceToHost);
		//printf("distinguished: %d\n", count);

		if (count < M) {
#if DEBUG
			state_t * tmp = (state_t*)malloc((M + 1)*sizeof(state_t));
			cudaMemcpy(tmp, dev.PrevIdx, (M + 1)*sizeof(state_t), cudaMemcpyDeviceToHost);
			for (int i = 0; i <= M; i++) {
				printf("%d ", tmp[i]);
			}printf("\n");
			getchar();
#endif
			dev.PrevIdxLen = prescan(M + 1, dev.PrevIdx);
			if (!dev.PrevIdxLen) {
				freeCuda(dev);
				return sequence_vec_t();
			}
#if DEBUG
			cudaMemcpy(tmp, dev.PrevIdxLen, (M + 1)*sizeof(state_t), cudaMemcpyDeviceToHost);
			for (int i = 0; i <= M; i++) {
				printf("%d ", tmp[i]);
			}printf("\n");
			getchar();
#endif
			state_t linkSize;
			cudaMemcpy(&linkSize, dev.PrevIdxLen + M, sizeof(state_t), cudaMemcpyDeviceToHost);

			RETURN_ON_ERROR(cudaMemset(dev.PrevIdx, 0, M*sizeof(state_t)));
			RETURN_ON_ERROR(cudaMalloc((void**)&(dev.LinkIn), linkSize*sizeof(input_t)));
			RETURN_ON_ERROR(cudaMalloc((void**)&(dev.LinkPrev), linkSize*sizeof(state_t)));

			prevStateLink<<<blocks, threads>>>(M, N, P, dev.Mapping, dev.NextDistIdx,
				dev.NextState, dev.PrevIdx, dev.PrevIdxLen, dev.LinkIn, dev.LinkPrev);
			RETURN_ON_ERROR(cudaGetLastError());
			RETURN_ON_ERROR(cudaDeviceSynchronize());
#if DEBUG
			cudaMemcpy(tmp, dev.PrevIdx, (M + 1)*sizeof(state_t), cudaMemcpyDeviceToHost);
			for (int i = 0; i <= M; i++) {
				printf("%d ", tmp[i]);
			}printf("\n");
			cudaMemcpy(tmp, dev.PrevIdxLen, (M + 1)*sizeof(state_t), cudaMemcpyDeviceToHost);
			for (int i = 0; i <= M; i++) {
				printf("%d ", tmp[i]);
			}printf("\n");
			getchar();
			free(tmp);

			tmp = (state_t*)malloc(linkSize*sizeof(state_t));
			cudaMemcpy(tmp, dev.LinkPrev, linkSize*sizeof(state_t), cudaMemcpyDeviceToHost);
			for (int i = 0; i < linkSize; i++) {
				printf("%d ", tmp[i]);
			}printf("\n");
			cudaMemcpy(tmp, dev.LinkIn, linkSize*sizeof(state_t), cudaMemcpyDeviceToHost);
			for (int i = 0; i < linkSize; i++) {
				printf("%d ", tmp[i]);
			}printf("\n");
			getchar();
			free(tmp);
#endif
			processUnchecked<<<1, min(THREADS_PER_BLOCK, M)>>>(M, dev.DistinguishedCount,
				dev.NextDistIdx, dev.Distinguishing, dev.Unchecked, dev.PrevIdx, dev.PrevIdxLen, dev.LinkIn, dev.LinkPrev);
			RETURN_ON_ERROR(cudaGetLastError());
#if DEBUG
			cudaMemcpy(&count, dev.DistinguishedCount, sizeof(state_t), cudaMemcpyDeviceToHost);
			printf("distinguished: %d\n", count);
			getchar();
#endif
		}
		return getSequences(fsm, dev, M, !omitUnnecessaryStoutInputs && fsm->isOutputState());
	}
}
