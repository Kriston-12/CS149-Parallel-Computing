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

Test speed comparision of part_a
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
