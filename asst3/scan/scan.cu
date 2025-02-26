#include <stdio.h>

#include <cuda.h>
#include <cuda_runtime.h>

#include <driver_functions.h>

#include <thrust/scan.h>
#include <thrust/device_ptr.h>
#include <thrust/device_malloc.h>
#include <thrust/device_free.h>

#include "CycleTimer.h"

#define THREADS_PER_BLOCK 256

// We need to implement the cuda version of the code below 
// void exclusive_scan_iterative(int* start, int* end, int* output) {

//     int N = end - start;
//     memmove(output, start, N*sizeof(int));
    
//     // upsweep phase
//     // two_dplus1 means the index we are handling, two_d means the last index
//     // so output[i + two_dplus1 - 1] += output[i + two_d - 1] means we are adding element at last index to the current element
//     for (int two_d = 1; two_d <= N/2; two_d*=2) {
//         int two_dplus1 = 2*two_d;
//         parallel_for (int i = 0; i < N; i += two_dplus1) {
//             output[i+two_dplus1-1] += output[i+two_d-1];
//         }
//     }

//     output[N-1] = 0;

//     // downsweep phase
//     for (int two_d = N/2; two_d >= 1; two_d /= 2) {
//         int two_dplus1 = 2*two_d;
//         parallel_for (int i = 0; i < N; i += two_dplus1) {
//             int t = output[i+two_d-1];
//             output[i+two_d-1] = output[i+two_dplus1-1];
//             output[i+two_dplus1-1] += t;
//         }
//     }
// }

// helper function to round an integer up to the next power of 2
// For example, if we have n = 19, it rounds it up to 32. 
// 25 -> 32
// 3 -> 4
static inline int nextPow2(int n) {
    n--; 
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
}

// Called by CPU/host and executed on GPU/device, two_dplus1 is our jump here
__global__ void scan_upsweep(int N, int two_dplus1, int* result) {
    // threadId is consecutive, but we are using (threadId) * two_dplus1 to handle jump index for the algorithm. 
    // if len(result) = 32, two_d = 1, two_dplus1 = 2, threads per block = 8. Then our task length would be 32/two_dplus = 16
    // blockNum = task length / threads per block = 2
    // threadId will have a value of 0,1,2,3,4,5,6,7... 15, this operation is in parallel
    // result[jumpIndex] += result[jumpIndex - (two_dplus1 >> 1)] -- two_dplus1 >> 1 = two_d
    // would be result[1] += result[0], result[3] += result[2], result[5] += result[4].. result[31] += result[30].
    // The addition above is in parallel, and this is why we use cuda.  
    int threadId = blockIdx.x * blockDim.x + threadIdx.x;
    if (threadId < N) {
        int jumpIndex = (threadId + 1) * two_dplus1 - 1;
        // Index * twodplus1 determines which line of the algorithm architecture are we executing 
        result[jumpIndex] += result[jumpIndex - (two_dplus1 >> 1)];
    }
}

__global__ void scan_downsweep(int N, int two_dplus1, int* result) {
    // If you understand the example in the upsweep, the code below should be clear. 
    int threadId = blockIdx.x * blockDim.x + threadIdx.x;
    if (threadId < N) {
        int jumpIndex = (threadId + 1) * two_dplus1 - 1;
        int temp = result[jumpIndex - (two_dplus1 >> 1)];
        result[jumpIndex - (two_dplus1 >> 1)] = result[jumpIndex];
        result[jumpIndex] += temp;
    }    
}

// exclusive_scan --
//
// Implementation of an exclusive scan on global memory array `input`,
// with results placed in global memory `result`.
//
// N is the logical size of the input and output arrays, however 
// students can assume that both the start and result arrays we
// allocated with next power-of-two sizes as described by the comments
// in cudaScan().  This is helpful, since your parallel scan
// will likely write to memory locations beyond N, but of course not
// greater than N rounded up to the next power of 2.
//
// Also, as per the comments in cudaScan(), you can implement an
// "in-place" scan, since the timing harness makes a copy of input and
// places it in result
void exclusive_scan(int* input, int N, int* result) // N is the logical size of input and output, 
{ 

    // CS149 TODO:
    //
    // Implement your exclusive scan implementation here.  Keep in
    // mind that although the arguments to this function are device
    // allocated arrays, this is a function that is running in a thread
    // on the CPU.  Your implementation will need to make multiple calls
    // to CUDA kernel functions (that you must write) to implement the
    // scan.
    int range = nextPow2(N);

    for (int two_dplus1 = 2; two_dplus1 <= range / 2; two_dplus1 *= 2) {

        // If we have remainder, we need to do an extra round, which we use "+1" below to represent it
        int upSweepTasksLen = (range + two_dplus1 - 1) / two_dplus1;
        // int upSweepTasksLen = range % two_dplus1 ? range / two_dplus1 + 1 : range / two_dplus1;

        // This is a similar handle to upSweepTasksLen
        int blocksNum = (upSweepTasksLen + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK;
        // int blocksNum = upSweepTasksLen % THREADS_PER_BLOCK ? upSweepTasksLen / THREADS_PER_BLOCK + 1 : upSweepTasksLen / THREADS_PER_BLOCK;

        scan_upsweep<<<blocksNum, THREADS_PER_BLOCK>>>(upSweepTasksLen, two_dplus1, result);
        cudaDeviceSynchronize(); // Must synchronize to ensure previous line is completly handled
    }

    // result[range - 1] = 0;  // This is invalid for device memory
    cudaMemset(&result[range - 1], 0, sizeof(int));
    // cudaMemset(result + (range - 1), 0, sizeof(int));
    cudaDeviceSynchronize(); 

    for (int two_dplus1 = range; two_dplus1 >= 2; two_dplus1 /= 2) {
        
        // These are the same as in upsweep
        int downSweepTasksLen = (range + two_dplus1 - 1) / two_dplus1;
        int blocksNum = (downSweepTasksLen + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK;

        scan_downsweep<<<blocksNum, THREADS_PER_BLOCK>>>(downSweepTasksLen, two_dplus1, result);
        cudaDeviceSynchronize();
    }
}


//
// cudaScan --
//
// This function is a timing wrapper around the student's
// implementation of scan - it copies the input to the GPU
// and times the invocation of the exclusive_scan() function
// above. Students should not modify it.
double cudaScan(int* inarray, int* end, int* resultarray)
{
    int* device_result;
    int* device_input;
    int N = end - inarray;  

    // This code rounds the arrays provided to exclusive_scan up
    // to a power of 2, but elements after the end of the original
    // input are left uninitialized and not checked for correctness.
    //
    // Student implementations of exclusive_scan may assume an array's
    // allocated length is a power of 2 for simplicity. This will
    // result in extra work on non-power-of-2 inputs, but it's worth
    // the simplicity of a power of two only solution.

    int rounded_length = nextPow2(end - inarray);
    
    cudaMalloc((void **)&device_result, sizeof(int) * rounded_length);
    cudaMalloc((void **)&device_input, sizeof(int) * rounded_length);

    // For convenience, both the input and output vectors on the
    // device are initialized to the input values. This means that
    // students are free to implement an in-place scan on the result
    // vector if desired.  If you do this, you will need to keep this
    // in mind when calling exclusive_scan from find_repeats.
    cudaMemcpy(device_input, inarray, (end - inarray) * sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(device_result, inarray, (end - inarray) * sizeof(int), cudaMemcpyHostToDevice);

    double startTime = CycleTimer::currentSeconds();

    exclusive_scan(device_input, N, device_result);

    // Wait for completion
    cudaDeviceSynchronize();
    double endTime = CycleTimer::currentSeconds();
       
    cudaMemcpy(resultarray, device_result, (end - inarray) * sizeof(int), cudaMemcpyDeviceToHost);

    double overallDuration = endTime - startTime;
    return overallDuration; 
}


// cudaScanThrust --
//
// Wrapper around the Thrust library's exclusive scan function
// As above in cudaScan(), this function copies the input to the GPU
// and times only the execution of the scan itself.
//
// Students are not expected to produce implementations that achieve
// performance that is competition to the Thrust version, but it is fun to try.
double cudaScanThrust(int* inarray, int* end, int* resultarray) {

    int length = end - inarray;
    thrust::device_ptr<int> d_input = thrust::device_malloc<int>(length);
    thrust::device_ptr<int> d_output = thrust::device_malloc<int>(length);
    
    cudaMemcpy(d_input.get(), inarray, length * sizeof(int), cudaMemcpyHostToDevice);

    double startTime = CycleTimer::currentSeconds();

    thrust::exclusive_scan(d_input, d_input + length, d_output);

    cudaDeviceSynchronize();
    double endTime = CycleTimer::currentSeconds();
   
    cudaMemcpy(resultarray, d_output.get(), length * sizeof(int), cudaMemcpyDeviceToHost);

    thrust::device_free(d_input);
    thrust::device_free(d_output);

    double overallDuration = endTime - startTime;
    return overallDuration; 
}


// find_repeats --
__global__ void findRepeatsSet(int* a, int* device_input, int length) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    a[index] = (index < length - 1 && device_input[index] == device_input[index + 1]) ? 1 : 0;
}
//
// Given an array of integers `device_input`, returns an array of all
// indices `i` for which `device_input[i] == device_input[i+1]`.
//
// Returns the total number of pairs found
int find_repeats(int* device_input, int length, int* device_output) {

    // CS149 TODO:
    //
    // Implement this function. You will probably want to
    // make use of one or more calls to exclusive_scan(), as well as
    // additional CUDA kernel launches.
    //    
    // Note: As in the scan code, the calling code ensures that
    // allocated arrays are a power of 2 in size, so you can use your
    // exclusive_scan function with them. However, your implementation
    // must ensure that the results of find_repeats are correct given
    // the actual array length.

    // The goal is to find total number of device_input[i] == device_input[i+1]
    // Strategy is to use an array to indicate if the condition is met. If it is, set a[i + 1] = 1, else 0
    int range = nextPow2(length);
    int blockNum = (range + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK;

    int *a;
    cudaMalloc(&a, range * sizeof(bool));
    findRepeatsSet<<<blockNum, std::min(range, THREADS_PER_BLOCK)>>>(a, device_input, length);
    cudaDeviceSynchronize();

    // The first parameter is never used.
    exclusive_scan(a, length, a);

    int result;
    cudaMemcpy(&result, &a[length - 1], sizeof(int), cudaMemcpyDeviceToHost);

    cudaFree(a);

    return 0; 
}


//
// cudaFindRepeats --
//
// Timing wrapper around find_repeats. You should not modify this function.
double cudaFindRepeats(int *input, int length, int *output, int *output_length) {

    int *device_input;
    int *device_output;
    int rounded_length = nextPow2(length);
    
    cudaMalloc((void **)&device_input, rounded_length * sizeof(int));
    cudaMalloc((void **)&device_output, rounded_length * sizeof(int));
    cudaMemcpy(device_input, input, length * sizeof(int), cudaMemcpyHostToDevice);

    cudaDeviceSynchronize();
    double startTime = CycleTimer::currentSeconds();
    
    int result = find_repeats(device_input, length, device_output);

    cudaDeviceSynchronize();
    double endTime = CycleTimer::currentSeconds();

    // set output count and results array
    *output_length = result;
    cudaMemcpy(output, device_output, length * sizeof(int), cudaMemcpyDeviceToHost);

    cudaFree(device_input);
    cudaFree(device_output);

    float duration = endTime - startTime; 
    return duration;
}



void printCudaInfo()
{
    int deviceCount = 0;
    cudaError_t err = cudaGetDeviceCount(&deviceCount);

    printf("---------------------------------------------------------\n");
    printf("Found %d CUDA devices\n", deviceCount);

    for (int i=0; i<deviceCount; i++)
    {
        cudaDeviceProp deviceProps;
        cudaGetDeviceProperties(&deviceProps, i);
        printf("Device %d: %s\n", i, deviceProps.name);
        printf("   SMs:        %d\n", deviceProps.multiProcessorCount);
        printf("   Global mem: %.0f MB\n",
               static_cast<float>(deviceProps.totalGlobalMem) / (1024 * 1024));
        printf("   CUDA Cap:   %d.%d\n", deviceProps.major, deviceProps.minor);
    }
    printf("---------------------------------------------------------\n"); 
}
