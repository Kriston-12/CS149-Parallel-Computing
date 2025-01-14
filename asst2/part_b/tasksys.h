#ifndef _TASKSYS_H
#define _TASKSYS_H

#include "itasksys.h"
#include <atomic>
#include <mutex>
#include <condition_variable>  
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <vector>
#include <iostream>

/*
 * TaskSystemSerial: This class is the student's implementation of a
 * serial task execution engine.  See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemSerial: public ITaskSystem {
    public:
        TaskSystemSerial(int num_threads);
        ~TaskSystemSerial();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

/*
 * TaskSystemParallelSpawn: This class is the student's implementation of a
 * parallel task execution engine that spawns threads in every run()
 * call.  See definition of ITaskSystem in itasksys.h for documentation
 * of the ITaskSystem interface.
 */
class TaskSystemParallelSpawn: public ITaskSystem {
    public:
        TaskSystemParallelSpawn(int num_threads);
        ~TaskSystemParallelSpawn();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

/*
 * TaskSystemParallelThreadPoolSpinning: This class is the student's
 * implementation of a parallel task execution engine that uses a
 * thread pool. See definition of ITaskSystem in itasksys.h for
 * documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSpinning: public ITaskSystem {
    public:
        TaskSystemParallelThreadPoolSpinning(int num_threads);
        ~TaskSystemParallelThreadPoolSpinning();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

// WaitingTask - Represents a task waiting for dependencies to complete
struct WaitingTask {
    TaskID id;
    TaskID dependTaskID{-1};
    IRunnable* runnable;
    int numTotalTasks;

    WaitingTask(TaskID id, TaskID dependency, IRunnable* runnable, int numTotalTasks)
    : id(id), dependTaskID(dependency), runnable(runnable), numTotalTasks(numTotalTasks){}

    // Prioritize tasks with smaller depdendent Task ID 
    //-- used for priority queue to pop up (if returns True, the queue will priorize the second parameter(other)), otherwise the first
    bool operator<(const WaitingTask& other) const {
        return dependTaskID > other.dependTaskID;
    }
};

// ReadyTask - represents a task ready to execute

struct ReadyTask {
    TaskID id;  // Every task has different taskID, taskId starts from 0, all the way to total number rof tasks - 1
    IRunnable* runnable;
    int currentTask{0};
    int numTotalTasks;

    ReadyTask() = default;
    ReadyTask(TaskID id, IRunnable* runnable, int numTotalTasks)
    : id(id), runnable(runnable), numTotalTasks(numTotalTasks) {}

    // ReadyTask& operator=(const ReadyTask& other) {
    //     // std::cout << "copy operator is called\n";
    //     if (this != &other) {
    //         id = other.id;
    //         runnable = other.runnable;
    //         numTotalTasks = other.numTotalTasks;
    //         currentTask = other.currentTask;
    //     }
    //     return *this;
    // }
};

/*
 * TaskSystemParallelThreadPoolSleeping: This class is the student's
 * optimized implementation of a parallel task execution engine that uses
 * a thread pool. See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSleeping: public ITaskSystem {
    
    std::atomic<bool> killed{false}; //notify workerthreads when tasks are done

    std::unordered_map<TaskID, std::pair<int, int>> taskProcess; // {TaskID -> finishedCount, totalCount}
    std::mutex taskProcessMutex;

    // TaskID management
    TaskID finishedTaskID{-1};
    TaskID nextTaskID{0};
    std::mutex taskIDMutex;

    // Waiting queue for tasks
    std::priority_queue<WaitingTask> waitingQueue;
    std::mutex waitingQueueMutex;

    // Ready queue for tasks ready to run
    std::queue<ReadyTask> readyQueue; 
    std::mutex readyQueueMutex;

    // The worker threadPool 
    std::vector<std::thread> threadPool; 

    std::condition_variable taskAvailable;
    std::condition_variable finishedCondition;

    public:
        TaskSystemParallelThreadPoolSleeping(int num_threads);
        ~TaskSystemParallelThreadPoolSleeping();
        void workerThread();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

#endif
