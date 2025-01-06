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
                TaskID taskId = -1;
                IRunnable* currentRunnable = nullptr;
                int numTotalTasks = 0;
               
                {
                    std::unique_lock<std::mutex> lock(taskMutex);
                    taskAvailable.wait(lock, [this]() {
                        return !readyTasks.empty() || stopFlag.load();
                    });

                    if (stopFlag) {
                        break;
                    }

                    if (!readyTasks.empty()) {
                        // std::cout << "Segfault might happen here\n";
                        taskId = readyTasks.front();
                        readyTasks.pop();

                        // Retrieve the runnable and number of tasks for this taskId

                        currentRunnable = taskMetadata[taskId].runnable;
                        numTotalTasks = taskMetadata[taskId].numTotalTasks;
                    }
                }

                if (stopFlag) {
                    break;
                }
                
                std::cout << "It reaches here just before task running\n";
                if (taskId != -1) {
                    // Execute the task
                    std::cout << "task running errors, \n";
                    if (!currentRunnable) {
                        std::cout << "currentRunnable is nullptr\n"; 
                    }
                    currentRunnable->runTask(taskId, numTotalTasks);
                    std::cout << "it cannot reach here\n";
                    completedTasks.fetch_add(1);

                    {
                        std::lock_guard<std::mutex> lock(taskMutex);
                        // completedTasks.fetch_add(1);
                        std::cout << "It reaches after running tasks\n";
                        // Notify dependent tasks
                        for (auto& taskPair : taskDependencies) {
                            auto& dependentTask = taskPair.first;
                            auto& dependencies = taskPair.second;
                            std::cout << "Enters here just before clear the dependencies\n";
                            dependencies.erase(taskId);
                            if (dependencies.empty() && unfinishedTaskCount[dependentTask] > 0) {
                                readyTasks.push(dependentTask);
                                unfinishedTaskCount[dependentTask] = 0; // Mark as ready
                                taskAvailable.notify_one();
                            }
                        }
                    }

                    // If all tasks are done, notify sync
                    if (static_cast<size_t>(completedTasks) >= taskMetadata.size()) {
                        completeAll.notify_all();
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

    
    TaskID baseTaskId = nextTaskId++;
    std::unique_lock<std::mutex> lock(taskMutex); 
    
    
    for (int i = 0; i < num_total_tasks; ++i) {  // this might be slow, since need to push all numbers
        readyTasks.push(baseTaskId + i); // All tasks are ready
        taskMetadata[baseTaskId + i] = {runnable, num_total_tasks};
    }

    // lock.unlock(); // not sure if this works yet
    std::cout << "It enters the run functin that notifies all\n" << std::endl;
    taskAvailable.notify_all();

    completeAll.wait(lock, [this]() {
        return static_cast<size_t>(completedTasks) >= taskMetadata.size();
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


    TaskID taskId = nextTaskId++;

    std::lock_guard<std::mutex> lock(taskMutex);

    // Store metadata for this task batch
    taskMetadata[taskId] = {runnable, num_total_tasks};

    if (deps.empty()) {
        // No dependencies, mark tasks as ready
        for (int i = 0; i < num_total_tasks; ++i) {
            readyTasks.push(taskId + i);
        }
        taskAvailable.notify_all();
    } else {
        // Record dependencies for this task batch
        for (int i = 0; i < num_total_tasks; ++i) {
            taskDependencies[taskId + i] = std::unordered_set<TaskID>(deps.begin(), deps.end());
            unfinishedTaskCount[taskId + i] = deps.size();
        }
    }

    return taskId;


    // return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync() {

    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //

    std::unique_lock<std::mutex> lock(taskMutex);
    completeAll.wait(lock, [this]() {
        return readyTasks.empty() && static_cast<size_t>(completedTasks) >= taskMetadata.size();
    });
    return;
}
