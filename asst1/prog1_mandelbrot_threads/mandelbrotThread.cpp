#include <stdio.h>
#include <thread>

#include "CycleTimer.h"

typedef struct {
    float x0, x1;
    float y0, y1;
    unsigned int width;
    unsigned int height;
    int maxIterations;
    int* output;
    int threadId;
    int numThreads;
} WorkerArgs;


extern void mandelbrotSerial(
    float x0, float y0, float x1, float y1,
    int width, int height,
    int startRow, int numRows,
    int maxIterations,
    int output[]);


extern void mandelbrotSerialInterleaved(
    float x0, float y0, float x1, float y1,
    int width, int height,
    int startRow, int interleavedRows,
    int maxIterations,
    int output[]    
);


//
// workerThreadStart --
//
// Thread entrypoint.
void workerThreadStart(WorkerArgs * const args) {


    // the strategy used here is: separate the image by rows. For example, with a image of 100 x 100, 
    // let it draw the upper half (50 x 100) and lower half (50 x 100) concurrently
    // int startRow = args->height / args->numThreads * args->threadId;
    // mandelbrotSerial(args->x0, args->y0, args->x1, args->y1, args->width, args->height, startRow,
    // args->threadId == args->numThreads - 1 ? args->height - startRow : args->height / args->numThreads,
    // args->maxIterations, args->output);

    // now draws with 8 threads (I have 16 threads intotal), below is the vserion of Q3 
    // int threadWidth = args->height / args->numThreads;
    // switch (args->threadId)
    // {
    // case 0:
    //     mandelbrotSerial(args->x0, args->y0, args->x1, args->y1, args->width, args->height, 0, threadWidth, args->maxIterations, args->output);
    //     break;
    // case 1:
    //     mandelbrotSerial(args->x0, args->y0, args->x1, args->y1, args->width, args->height, threadWidth, threadWidth, args->maxIterations, args->output);
    //     break;
    // case 2:
    //     mandelbrotSerial(args->x0, args->y0, args->x1, args->y1, args->width, args->height, threadWidth * 2, threadWidth, args->maxIterations, args->output);
    //     break;    
    // case 3:
    //     mandelbrotSerial(args->x0, args->y0, args->x1, args->y1, args->width, args->height, threadWidth * 3, threadWidth, args->maxIterations, args->output);
    //     break;    
    // case 4:
    //     mandelbrotSerial(args->x0, args->y0, args->x1, args->y1, args->width, args->height, threadWidth * 4, threadWidth, args->maxIterations, args->output);
    //     break;    
    // case 5:
    //     mandelbrotSerial(args->x0, args->y0, args->x1, args->y1, args->width, args->height, threadWidth * 5, threadWidth, args->maxIterations, args->output);
    //     break;
    // case 6:
    //     mandelbrotSerial(args->x0, args->y0, args->x1, args->y1, args->width, args->height, threadWidth * 6, threadWidth, args->maxIterations, args->output);
    //     break;
    // case 7:
    //     mandelbrotSerial(args->x0, args->y0, args->x1, args->y1, args->width, args->height, threadWidth * 7, threadWidth, args->maxIterations, args->output);
    //     break;    
    // default:
    //     break;
    // }

    // Interleaved speedup. Thread 0 will handle row 0, 8, 16... T1 will handle 1, 9...
    mandelbrotSerialInterleaved(args->x0, args->y0, args->x1, args->y1, args->width, args->height, 
    args->threadId, args->numThreads, args->maxIterations, args->output);
}

//
// MandelbrotThread --
//
// Multi-threaded implementation of mandelbrot set image generation.
// Threads of execution are created by spawning std::threads.
void mandelbrotThread(
    int numThreads,
    float x0, float y0, float x1, float y1,
    int width, int height,
    int maxIterations, int output[])
{
    static constexpr int MAX_THREADS = 32;

    if (numThreads > MAX_THREADS)
    {
        fprintf(stderr, "Error: Max allowed threads is %d\n", MAX_THREADS);
        exit(1);
    }

    // Creates thread objects that do not yet represent a thread.
    std::thread workers[MAX_THREADS];
    WorkerArgs args[MAX_THREADS];

    for (int i=0; i<numThreads; i++) {
      
        // TODO FOR CS149 STUDENTS: You may or may not wish to modify
        // the per-thread arguments here.  The code below copies the
        // same arguments for each thread
        args[i].x0 = x0;
        args[i].y0 = y0;
        args[i].x1 = x1;
        args[i].y1 = y1;
        args[i].width = width;
        args[i].height = height;
        args[i].maxIterations = maxIterations;
        args[i].numThreads = numThreads;
        args[i].output = output;
      
        args[i].threadId = i;
    }

    // Spawn the worker threads.  Note that only numThreads-1 std::threads
    // are created and the main application thread is used as a worker
    // as well.
    for (int i=1; i<numThreads; i++) {
        // double startTime = CycleTimer::currentSeconds();
        workers[i] = std::thread(workerThreadStart, &args[i]);
        // double endTime = CycleTimer::currentSeconds();
        // printf("Thread %d takes\t\t[%.3f] ms\n", args[i].threadId, (endTime - startTime ) * 1000);
    }

    // double startTime = CycleTimer::currentSeconds();
    workerThreadStart(&args[0]);
    // double endTime = CycleTimer::currentSeconds();

    // printf("Thread 0 takes\t\t[%.3f] ms\n",  endTime - startTime);

    // join worker threads
    for (int i=1; i<numThreads; i++) {
        workers[i].join();
    }
}

