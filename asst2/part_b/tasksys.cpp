#include "tasksys.h"
#include <iostream>

IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {}
ITaskSystem::~ITaskSystem() {}

/*
 * ================================================================
 * Serial task system implementation
 * ================================================================
 */

const char* TaskSystemSerial::name() {
    return "Serial";
}

TaskSystemSerial::TaskSystemSerial(int num_threads): ITaskSystem(num_threads) {
}

TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable* runnable, int num_total_tasks) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                          const std::vector<TaskID>& deps) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemSerial::sync() {
    return;
}

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelSpawn::name() {
    return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads): ITaskSystem(num_threads) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                 const std::vector<TaskID>& deps) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemParallelSpawn::sync() {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads): ITaskSystem(num_threads) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSleeping::name() {
    return "Parallel + Thread Pool + Sleep";
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads)
    : ITaskSystem(num_threads)
{
    threadPool.reserve(num_threads);
    for (int i = 0; i < num_threads; ++i) {
        threadPool.emplace_back([this] () {
            while (true) {
                TaskID batchId = -1;
                int taskIndex = -1;
                Task* currentTask = nullptr;
        
                {
                    std::unique_lock<std::mutex> lock(taskMutex);
                    taskAvailable.wait(lock, [this]() {
                        return !readyQueue.empty() || stopFlag.load();
                    });

                    if (stopFlag) {
                        break;
                    }

                    // auto [batch, index] = readyQueue.front(); // cpp 17 feature, not sure if works in cpp11
                    auto batchPair = readyQueue.front();
                    readyQueue.pop();
                    // assigning code below could be put out of the block for speeding.
                    batchId = batchPair.first;
                    taskIndex = batchPair.second;
                    currentTask = &tasks[taskIndex];
                }

                if (stopFlag) {
                    break;
                }

                if (currentTask) {
                    currentTask->runnable->runTask(taskIndex, currentTask->numTotalTasks);
                    ++(currentTask->completedCount);

                    // if completedCount reaches totalTasks, update dependencies
                    if (currentTask->completedCount == currentTask->numTotalTasks) {
                        for (auto& taskPair : tasks) { // for all tasks that dependent on the task with the batchID, erase the task from their dependencies
                            TaskID dependentBatchId = taskPair.first;  // Get the batch ID
                            auto& task = taskPair.second; 
                            // taskMutex.lock(); might not be necessary? since there is no same bathId
                            task.deps.erase(batchId); 
                            if (task.deps.empty()) {  // if no dependency, put them in the readyQueue, here do need a lock
                                taskMutex.lock();
                                for (int i = 0; i < task.numTotalTasks; ++i) {
                                    // readyQueue.push({dependentBatchId, i});
                                    readyQueue.push(std::make_pair(dependentBatchId, i));
                                }
                                taskMutex.unlock();
                                taskAvailable.notify_all();
                            }
                        }
                    }
                }
            }    
        });
    }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    //
    // TODO: CS149 student implementations may decide to perform cleanup
    // operations (such as thread pool shutdown construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    std::cout << "It enteres the destructor" << std::endl;
    stopFlag.store(true);
    {
        std::lock_guard<std::mutex> lock(taskMutex);
        taskAvailable.notify_all(); // Wake up all threads
    }
    for (auto& thread : threadPool) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {


    //
    // TODO: CS149 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //
    // run func should not change a lot from part A.
    // for (int i = 0; i < num_total_tasks; i++) {
    //     runnable->runTask(i, num_total_tasks);
    // }

    
    TaskID batchId = nextBatchId++; // make sure we are using the global nextBatchId, so each batchId is unique and hashable for tasks<batchId, task>
    std::unique_lock<std::mutex> lock(taskMutex); 
    
    // Add a new task batch to the metadata
    // tasks[batchId] = std::move(Task(num_total_tasks, runnable, {}));
    // tasks[batchId] = Task(num_total_tasks, runnable, {});

    // tasks.emplace(batchId, Task(num_total_tasks, runnable, {}));
    // tasks.emplace(batchId, Task(num_total_tasks, runnable, std::unordered_set<TaskID>()));
    tasks.emplace(
    std::piecewise_construct,
    std::forward_as_tuple(batchId),
    std::forward_as_tuple(num_total_tasks, runnable, std::unordered_set<TaskID>())
    );
    
    for (int i = 0; i < num_total_tasks; ++i) {
        // readyQueue.push({batchId, i}); // seems not viable in c++11
        readyQueue.push(std::make_pair(batchId, i));
    }

    taskAvailable.notify_all();

    completeAll.wait(lock, [this, batchId]() { // batchId should be added in the capture list to be captured.
        return tasks[batchId].completedCount == tasks[batchId].numTotalTasks;
    });
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {


    //
    // TODO: CS149 students will implement this method in Part B.
    //
    
    // for (int i = 0; i < num_total_tasks; i++) {
    //     runnable->runTask(i, num_total_tasks);
    // }

    // I let run to be a func that is run without dependency
    // so in this function, I'll check dependency


    TaskID batchId = nextBatchId++;
    std::unordered_set<TaskID> dependencies(deps.begin(), deps.end()); // copy the dependencies

    std::lock_guard<std::mutex> lock(taskMutex);

    // Store metadata for this task batch
    // tasks.emplace(batchId, Task(num_total_tasks, runnable, dependencies));
    tasks.emplace(
    std::piecewise_construct,
    std::forward_as_tuple(batchId),
    std::forward_as_tuple(num_total_tasks, runnable, dependencies)
    );

    if (deps.empty()) {
        // No dependencies, mark tasks as ready
        for (int i = 0; i < num_total_tasks; ++i) {
            // readyQueue.push({batchId, i});
            readyQueue.push(std::make_pair(batchId, i));
        }
        taskAvailable.notify_all();
    } 
    return batchId;


    // return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync() {

    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //

    std::unique_lock<std::mutex> lock(taskMutex);
    completeAll.wait(lock, [this]() {
        for (auto& taskPair : tasks) {
            if (taskPair.second.completedCount < taskPair.second.numTotalTasks) {
                return false;
            }
        }
        return true; // only return True if all tasks are finished
    });
    return;
}
