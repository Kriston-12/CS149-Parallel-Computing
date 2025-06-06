# **Stanford CS149 - Parallel Computing (Personal Notes)**

This is a repository that records my understanding of **Stanford CS149: Parallel Computing**.

> **Note**: I am not a Stanford student, and I am using **WSL2 Ubuntu 22.04** as the environment for setup and development.

---

## **Assignment 1 - ISPC Installation Guide**

Follow the steps below to set up the environment and install the required tools for **Assignment 1**.

### **1. Install ISPC Compiler**
Run the following commands to download, extract, and install the **ISPC (Intel SPMD Program Compiler)**:

```bash
wget https://github.com/ispc/ispc/releases/download/v1.23.0/ispc-v1.23.0-linux.tar.gz
tar -xvf ispc-v1.23.0-linux.tar.gz
sudo mv ispc-v1.23.0-linux/bin/ispc /usr/local/bin
rm -rf ispc-v1.23.0-linux.tar.gz ispc-v1.23.0-linux
```

The most interesting part of this lab I found is the 3rd part of program 5--`Program 5: BLAS saxpy`. The computation task of this program is very simple, which you'll find using multi-core ISPC will barely improve program speed. This is due to memory bandwidth--most of the program time is spent on memory loading and storing, rather than the computation. The solution to this is to increase computation/memory ratio, namely the arithmetic intensity. I'd recommend 2 ways to solve this:

1. Using _mm_stream_si<X> from Intel Intrinsics (https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html).
   - **The reasoning:** Write granularity being too small leads to unnecessary reads.
   - This happens because the smallest unit for communication between the cache and memory is a cache line: 64 bytes. When the CPU attempts to write 4 bytes, since the remaining 60 bytes in the cache line haven't been modified, the cache doesn't know whether the CPU will use those 60 bytes in subsequent operations. Therefore, it has to read the entire 64-byte cache line from memory first, modify the 4 bytes with the data provided by the CPU, and then write the entire cache line back.
   - As a result, even though the CPU doesn't need to read the data, the cache still has to fetch it from memory, effectively doubling the bandwidth usage unnecessarily.

   To address the issue of unnecessary reads caused by small write granularity, you can use the _mm_stream_si32 instruction as an alternative to direct memory writes. This instruction enables streaming stores, allowing a 4-byte write operation to bypass the cache and directly enqueue into a temporary queue. Once the queue is filled with 64 bytes, it writes directly to memory, completely avoiding the need for unnecessary reads and saving bandwidth.

   However, _mm_stream_si32 only supports int as its parameter. If you need to use float, you’ll have to first cast the float pointer to an int pointer (e.g., using bitcast) and then pass the argument.

2. Using Loop tiling to exploit cache locality
   - **Problem without tiling:** If data is processed in a large loop, the memory accesses often result in cache misses because data required for later iterations might not fit in the cache and is evicted before reuse.
   - **How tiling helps:** By breaking the loop into smaller tiles that fit into the cache, tiling ensures that data loaded into the cache is reused multiple times before being evicted. This reduces cache misses and improves the effective memory bandwidth.

## **Assignment 2 - Building A Task Execution Library from the Ground Up**

Using `htop` is useful for debugging this lab.

When you run into deadlocks, there are usually 2 cases:

1. Logic error caused by data races
   - In this case, `htop` will tell you that your thread usage is very high (nearly 100%).

2. Condition variables missing signals
   - In this case, `htop` will show you that your thread usage is nearly 0, as threads are sleeping and waiting to be notified.

Test speed comparison of part_a

```
kris@aa:~/cs149/CS149-Parallel-Computing/asst2/part_a$ python3 ../tests/run_test_harness.py 
runtasks_ref
Linux x86_64
================================================================================
Running task system grading harness... (11 total tests)
  - Detected CPU with 16 execution contexts
  - Task system configured to use at most 16 threads
================================================================================
================================================================================
Executing test: super_super_light...
Reference binary: ./runtasks_ref_linux
Results for: super_super_light
                                        STUDENT   REFERENCE   PERF?
[Serial]                                6.601     9.639       0.68  (OK)
[Parallel + Always Spawn]               264.232   268.134     0.99  (OK)
[Parallel + Thread Pool + Spin]         14.847    21.323      0.70  (OK)
[Parallel + Thread Pool + Sleep]        23.571    105.605     0.22  (OK)
================================================================================
Executing test: super_light...
Reference binary: ./runtasks_ref_linux
Results for: super_light
                                        STUDENT   REFERENCE   PERF?
[Serial]                                56.133    59.807      0.94  (OK)
[Parallel + Always Spawn]               251.294   266.791     0.94  (OK)
[Parallel + Thread Pool + Spin]         14.638    24.17       0.61  (OK)
[Parallel + Thread Pool + Sleep]        25.069    102.4       0.24  (OK)
================================================================================
Executing test: ping_pong_equal...
Reference binary: ./runtasks_ref_linux
Results for: ping_pong_equal
                                        STUDENT   REFERENCE   PERF?
[Serial]                                896.46    960.324     0.93  (OK)
[Parallel + Always Spawn]               284.536   288.605     0.99  (OK)
[Parallel + Thread Pool + Spin]         176.057   193.845     0.91  (OK)
[Parallel + Thread Pool + Sleep]        156.406   204.807     0.76  (OK)
================================================================================
Executing test: ping_pong_unequal...
Reference binary: ./runtasks_ref_linux
Results for: ping_pong_unequal
                                        STUDENT   REFERENCE   PERF?
[Serial]                                1313.716  1347.832    0.97  (OK)
[Parallel + Always Spawn]               301.383   303.742     0.99  (OK)
[Parallel + Thread Pool + Spin]         201.16    213.288     0.94  (OK)
[Parallel + Thread Pool + Sleep]        189.992   234.318     0.81  (OK)
================================================================================
Executing test: recursive_fibonacci...
Reference binary: ./runtasks_ref_linux
Results for: recursive_fibonacci
                                        STUDENT   REFERENCE   PERF?
[Serial]                                761.58    1331.707    0.57  (OK)
[Parallel + Always Spawn]               120.169   163.968     0.73  (OK)
[Parallel + Thread Pool + Spin]         122.442   165.791     0.74  (OK)
[Parallel + Thread Pool + Sleep]        112.116   160.936     0.70  (OK)
================================================================================
Executing test: math_operations_in_tight_for_loop...
Reference binary: ./runtasks_ref_linux
Results for: math_operations_in_tight_for_loop
                                        STUDENT   REFERENCE   PERF?
[Serial]                                469.277   474.109     0.99  (OK)
[Parallel + Always Spawn]               1311.629  1263.091    1.04  (OK)
[Parallel + Thread Pool + Spin]         135.315   159.262     0.85  (OK)
[Parallel + Thread Pool + Sleep]        130.08    508.85      0.26  (OK)
================================================================================
Executing test: math_operations_in_tight_for_loop_fewer_tasks...
Reference binary: ./runtasks_ref_linux
Results for: math_operations_in_tight_for_loop_fewer_tasks
                                        STUDENT   REFERENCE   PERF?
[Serial]                                476.13    490.583     0.97  (OK)
[Parallel + Always Spawn]               1305.835  1342.071    0.97  (OK)
[Parallel + Thread Pool + Spin]         142.326   169.681     0.84  (OK)
[Parallel + Thread Pool + Sleep]        159.812   512.793     0.31  (OK)
================================================================================
Executing test: math_operations_in_tight_for_loop_fan_in...
Reference binary: ./runtasks_ref_linux
Results for: math_operations_in_tight_for_loop_fan_in
                                        STUDENT   REFERENCE   PERF?
[Serial]                                244.624   245.249     1.00  (OK)
[Parallel + Always Spawn]               177.126   178.667     0.99  (OK)
[Parallel + Thread Pool + Spin]         44.256    50.519      0.88  (OK)
[Parallel + Thread Pool + Sleep]        45.356    71.074      0.64  (OK)
================================================================================
Executing test: math_operations_in_tight_for_loop_reduction_tree...
Reference binary: ./runtasks_ref_linux
Results for: math_operations_in_tight_for_loop_reduction_tree
                                        STUDENT   REFERENCE   PERF?
[Serial]                                244.413   245.409     1.00  (OK)
[Parallel + Always Spawn]               63.726    64.611      0.99  (OK)
[Parallel + Thread Pool + Spin]         42.192    44.164      0.96  (OK)
[Parallel + Thread Pool + Sleep]        37.373    48.794      0.77  (OK)
================================================================================
Executing test: spin_between_run_calls...
Reference binary: ./runtasks_ref_linux
Results for: spin_between_run_calls
                                        STUDENT   REFERENCE   PERF?
[Serial]                                267.088   479.333     0.56  (OK)
[Parallel + Always Spawn]               138.884   245.872     0.56  (OK)
[Parallel + Thread Pool + Spin]         261.635   357.128     0.73  (OK)
[Parallel + Thread Pool + Sleep]        194.592   244.642     0.80  (OK)
================================================================================
Executing test: mandelbrot_chunked...
Reference binary: ./runtasks_ref_linux
Results for: mandelbrot_chunked
                                        STUDENT   REFERENCE   PERF?
[Serial]                                305.804   309.453     0.99  (OK)
[Parallel + Always Spawn]               23.547    23.462      1.00  (OK)
[Parallel + Thread Pool + Spin]         25.556    25.535      1.00  (OK)
[Parallel + Thread Pool + Sleep]        25.483    23.151      1.10  (OK)
================================================================================
Overall performance results
[Serial]                                : All passed Perf
[Parallel + Always Spawn]               : All passed Perf
[Parallel + Thread Pool + Spin]         : All passed Perf
[Parallel + Thread Pool + Sleep]        : All passed Perf
```

## **Assignment 3 - A Simple CUDA Renderer**

Follow the steps below to set up the environment and install the required tools for **Assignment 1**.
### **You should run `nvidia-smi` to see if your graphic card could be recognized**
```bash
Wed Jan 22 19:29:19 2025       
+---------------------------------------------------------------------------------------+
| NVIDIA-SMI 545.35                 Driver Version: 546.30       CUDA Version: 12.3     |
|-----------------------------------------+----------------------+----------------------+
| GPU  Name                 Persistence-M | Bus-Id        Disp.A | Volatile Uncorr. ECC |
| Fan  Temp   Perf          Pwr:Usage/Cap |         Memory-Usage | GPU-Util  Compute M. |
|                                         |                      |               MIG M. |
|=========================================+======================+======================|
|   0  NVIDIA GeForce RTX 3060 ...    On  | 00000000:01:00.0  On |                  N/A |
| N/A   40C    P8              16W /  40W |   1156MiB /  6144MiB |      5%      Default |
|                                         |                      |                  N/A |
+-----------------------------------------+----------------------+----------------------+
                                                                                         
+---------------------------------------------------------------------------------------+
| Processes:                                                                            |
|  GPU   GI   CI        PID   Type   Process name                            GPU Memory |
|        ID   ID                                                             Usage      |
|=======================================================================================|
|    0   N/A  N/A        35      G   /Xwayland                                 N/A      |
+---------------------------------------------------------------------------------------+
```

### **You will only need to install cuda/nvcc compiler**
Run the following commands:
```bash
sudo apt install nvidia-cuda-toolkit
```
PS: I ran into errors using cuda version < 11.5. I recommend using a version >= 11.6

```bash
~/cs149/CS149-Parallel-Computing/asst3/scan$ nvcc -V
nvcc: NVIDIA (R) Cuda compiler driver
Copyright (c) 2005-2023 NVIDIA Corporation
Built on Wed_Nov_22_10:17:15_PST_2023
Cuda compilation tools, release 12.3, V12.3.107
Build cuda_12.3.r12.3/compiler.33567101_0
```

In this lab, we will implement cuda scan (prefix sum), see the image below for algorithm detail
![image](https://github.com/user-attachments/assets/6e75874d-b5f0-4217-b1cf-e80e60b79f1f)
Below is the C++ implementation 
```cpp
void exclusive_scan_iterative(int* start, int* end, int* output) {

    int N = end - start;
    memmove(output, start, N*sizeof(int));
    
    // upsweep phase
    for (int two_d = 1; two_d <= N/2; two_d*=2) {
        int two_dplus1 = 2*two_d;
        parallel_for (int i = 0; i < N; i += two_dplus1) {
            output[i+two_dplus1-1] += output[i+two_d-1];
        }
    }

    output[N-1] = 0;

    // downsweep phase
    for (int two_d = N/2; two_d >= 1; two_d /= 2) {
        int two_dplus1 = 2*two_d;
        parallel_for (int i = 0; i < N; i += two_dplus1) {
            int t = output[i+two_d-1];
            output[i+two_d-1] = output[i+two_dplus1-1];
            output[i+two_dplus1-1] += t;
        }
    }
}
```
Below is my CUDA implementation with explanantion in comments
```CUDA
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
```
Below is test output
```bash
~/cs149/CS149-Parallel-Computing/asst3/scan$ ./cudaScan -n 2000000
---------------------------------------------------------
Found 1 CUDA devices
Device 0: NVIDIA GeForce RTX 3060 Laptop GPU
   SMs:        30
   Global mem: 6144 MB
   CUDA Cap:   8.6
---------------------------------------------------------
Array size: 2000000
Student GPU time: 2.240 ms
Scan outputs are correct!

~/cs149/CS149-Parallel-Computing/asst3/scan$ ./checker.py scan   
Test: scan

--------------
Running tests:
--------------

Element Count: 1000000
Correctness passed!
Student Time: 1.988
Ref Time: 1.84

Element Count: 10000000
Correctness passed!
Student Time: 7.869
Ref Time: 9.648

Element Count: 20000000
Correctness passed!
Student Time: 12.439
Ref Time: 16.689

Element Count: 40000000
Correctness passed!
Student Time: 23.041
Ref Time: 33.832

-------------------------
Scan Score Table:
-------------------------
-------------------------------------------------------------------------
| Element Count   | Ref Time        | Student Time    | Score           |
-------------------------------------------------------------------------
| 1000000         | 1.84            | 1.988           | 1.25            |
| 10000000        | 9.648           | 7.869           | 1.25            |
| 20000000        | 16.689          | 12.439          | 1.25            |
| 40000000        | 33.832          | 23.041          | 1.25            |
-------------------------------------------------------------------------
|                                   | Total score:    | 5.0/5.0         |
-------------------------------------------------------------------------
```

## **Assignment 4 - NanoGPT149**
In this assignment, we need to implement FLASH ATTENTION.
![image](https://github.com/user-attachments/assets/d284a254-e36e-40b8-b9b9-23322ca4bc78)


Run the following commands to create a python virtual environment called torch-env and install torch:
```bash
python3 -m venv torch-env
source torch-env/bin/activate
pip install --upgrade pip
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu121
```

Result of Part 4--Implementation of flash attention (non-parallized version)
```bash
Compiling code into a PyTorch module...


Running Part 4 Test: Flash Attention

-----RUNNING REFERENCE IMPLEMENTATION-----

WARNING:2025-05-15 15:40:52 4888:4888 init.cpp:155] function cbapi->getCuptiStatus() failed with error CUPTI_ERROR_NOT_INITIALIZED (15)
WARNING:2025-05-15 15:40:52 4888:4888 init.cpp:156] CUPTI initialization failed - CUDA profiler activities will be missing
INFO:2025-05-15 15:40:52 4888:4888 init.cpp:158] If you see CUPTI_ERROR_INSUFFICIENT_PRIVILEGES, refer to https://developer.nvidia.com/nvidia-development-tools-solutions-err-nvgpuctrperm-cupti
STAGE:2025-05-15 15:40:52 4888:4888 ActivityProfilerController.cpp:312] Completed Stage: Warm Up
STAGE:2025-05-15 15:40:53 4888:4888 ActivityProfilerController.cpp:318] Completed Stage: Collection
STAGE:2025-05-15 15:40:53 4888:4888 ActivityProfilerController.cpp:322] Completed Stage: Post Processing
manual attention == pytorch attention True
Manual Execution Time:  0.4985060691833496 

-------------------------------  ------------  ------------  ------------  ------------  ------------  ------------  ------------  ------------  
                           Name    Self CPU %      Self CPU   CPU total %     CPU total  CPU time avg       CPU Mem  Self CPU Mem    # of Calls  
-------------------------------  ------------  ------------  ------------  ------------  ------------  ------------  ------------  ------------  
                    aten::zeros         0.35%       1.742ms         0.42%       2.097ms     149.786us       9.16 Mb       1.00 Kb            14  
                    aten::empty         0.01%      52.000us         0.01%      52.000us       3.714us       9.13 Mb       9.13 Mb            14  
                model_inference         0.01%      43.000us       100.00%     498.551ms     498.551ms     512.00 Kb    -680.00 Kb             1  
    REFERENCE - FLASH ATTENTION        97.40%     485.611ms        99.88%     497.935ms     497.935ms     512.00 Kb      -8.00 Mb             1  
                    aten::zero_         0.14%     697.000us         2.21%      11.022ms      29.789us      34.00 Kb      34.00 Kb           370  
                    aten::fill_         2.09%      10.406ms         2.09%      10.406ms      78.241us           0 b           0 b           133  
-------------------------------  ------------  ------------  ------------  ------------  ------------  ------------  ------------  ------------  
Self CPU time total: 498.551ms

REFERENCE - FLASH ATTENTION statistics
cpu time:  497.935ms
mem usage:  524288 bytes
-----RUNNING STUDENT IMPLEMENTATION-----

STAGE:2025-05-15 15:40:58 4888:4888 ActivityProfilerController.cpp:312] Completed Stage: Warm Up
STAGE:2025-05-15 15:40:58 4888:4888 ActivityProfilerController.cpp:318] Completed Stage: Collection
STAGE:2025-05-15 15:40:58 4888:4888 ActivityProfilerController.cpp:322] Completed Stage: Post Processing
manual attention == pytorch attention True
Manual Execution Time:  0.15375876426696777 

-----------------------------  ------------  ------------  ------------  ------------  ------------  ------------  ------------  ------------  
                         Name    Self CPU %      Self CPU   CPU total %     CPU total  CPU time avg       CPU Mem  Self CPU Mem    # of Calls  
-----------------------------  ------------  ------------  ------------  ------------  ------------  ------------  ------------  ------------  
                  aten::empty         0.02%      36.000us         0.02%      36.000us       2.769us       1.38 Mb       1.38 Mb            13  
                  aten::zeros         0.02%      36.000us         0.09%     141.000us      11.750us       1.16 Mb     263.00 Kb            12  
                  aten::clone         0.03%      41.000us         0.27%     421.000us     210.500us       1.00 Mb           0 b             2  
              model_inference         0.11%     166.000us       100.00%     153.807ms     153.807ms     512.00 Kb    -687.00 Kb             1  
    STUDENT - FLASH ATTENTION        99.50%     153.033ms        99.82%     153.531ms     153.531ms     512.00 Kb      -1.00 Mb             1  
                aten::flatten         0.02%      32.000us         0.14%     215.000us      14.333us     512.00 Kb           0 b            15  
             aten::empty_like         0.02%      27.000us         0.02%      31.000us      31.000us     512.00 Kb           0 b             1  
          aten::empty_strided         0.01%      19.000us         0.01%      19.000us      19.000us     512.00 Kb     512.00 Kb             1  
                  aten::zero_         0.01%      19.000us         0.05%      74.000us       6.167us      37.00 Kb      37.00 Kb            12  
                  aten::fill_         0.04%      55.000us         0.04%      55.000us      18.333us           0 b           0 b             3  
-----------------------------  ------------  ------------  ------------  ------------  ------------  ------------  ------------  ------------  
Self CPU time total: 153.807ms

STUDENT - FLASH ATTENTION statistics
cpu time:  153.531ms
mem usage:  524288 bytes
```

## **Assignment 5 - Big Graph Processing in OpenMP**
We will parallize bfs(breath-first-search algorithm) using OpenMP.

Result:
```bash
--------------------------------------------------------------------------
SCORES :                    |   Top-Down    |   Bott-Up    |    Hybrid    |
--------------------------------------------------------------------------
grid1000x1000.graph         |      2.00 / 2 |     3.00 / 3 |     3.00 / 3 |
--------------------------------------------------------------------------
soc-livejournal1_68m.graph  |      2.00 / 2 |     2.85 / 3 |     3.00 / 3 |
--------------------------------------------------------------------------
com-orkut_117m.graph        |      2.00 / 2 |     3.00 / 3 |     3.00 / 3 |
--------------------------------------------------------------------------
random_500m.graph           |      7.00 / 7 |     8.00 / 8 |     8.00 / 8 |
--------------------------------------------------------------------------
rmat_200m.graph             |      7.00 / 7 |     6.18 / 8 |     8.00 / 8 |
--------------------------------------------------------------------------
TOTAL                                                      |  68.03 / 70 |
--------------------------------------------------------------------------
```
