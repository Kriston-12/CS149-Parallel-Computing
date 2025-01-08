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

/*
 * TaskSystemParallelThreadPoolSleeping: This class is the student's
 * optimized implementation of a parallel task execution engine that uses
 * a thread pool. See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSleeping: public ITaskSystem {
    std::vector<std::thread> threadPool; 
    std::mutex taskMutex;
    std::condition_variable taskAvailable;
    IRunnable* currentRunnable;
    std::atomic<int> currentTaskId{0};
    std::atomic<int> totalTasks{0};
    std::atomic<bool> stopFlag{false};
    std::atomic<int> completedTasks{0};
    std::condition_variable completeAll;

    // Part B
    
    struct Task {
        // TaskID basedId;
        std::atomic<int> numTotalTasks;
        IRunnable * runnable;
        // std::shared_ptr<IRunnable> runnable;
        std::unordered_set<TaskID> deps; //Dependencies (other tasks that this task is dependent on)
        std::atomic<int> completedCount{0}; // if this is equal to numTotalTasks, then this task is completed.

        Task() = default;
        Task(int numTotalTasks, IRunnable * runnable, const std::unordered_set<TaskID>& deps): numTotalTasks(numTotalTasks), runnable(runnable), deps(deps) {
            std::cout << "task constructor is called\n";
            if (runnable == nullptr) {
                std::cout << "nullptr construction, error!!\n";
            }
        }

        // default move constructor and move assignment
        // Task(Task&&) = default;
        Task(Task&& other) noexcept
        :numTotalTasks(other.numTotalTasks.load()),
        runnable(std::move(other.runnable)),
        deps(std::move(other.deps)),
        completedCount(other.completedCount.load()) {
            other.runnable = nullptr;  // Clear the source's runnable pointer
        }

        Task& operator=(Task&& other) noexcept {
            if (this != &other) {
                numTotalTasks = other.numTotalTasks.load();
                runnable = std::move(other.runnable);
                deps = std::move(other.deps);
                completedCount = other.completedCount.load();
                other.runnable = nullptr;
            }
            return *this;
        }

        // Delete copy constructor and copy assignment operator
        // Task(const Task&) = delete;
        // Task& operator=(const Task&) = delete;
    };

    std::unordered_map<TaskID, Task> tasks;      // Map of BatchId to Task, I wanna do it Task->TaskID, but not hashable even after defining ==operator 
    std::queue<std::pair<TaskID, int>> readyQueue;        //Queue of (batchID, taskIndex)
    TaskID nextBatchId{0};       // Incremental TaskID generator

    public:
        TaskSystemParallelThreadPoolSleeping(int num_threads);
        ~TaskSystemParallelThreadPoolSleeping();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

#endif
