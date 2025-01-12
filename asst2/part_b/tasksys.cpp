#include "tasksys.h"


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
    tasksWithDeps.reserve(num_threads * 10);
    tasksWithoutDeps.reserve(num_threads * 20);
    for (int i = 0; i < num_threads; ++i) {
        threadPool.emplace_back([this] () {
            while (true) {
                TaskID batchId = -1;
                int taskIndex = -1;
                Task* currentTask = nullptr;

                // std::cout<<"this is to show the threads are keeping spinning\n";//this stops at some points 
                {   
                    std::unique_lock<std::mutex> lock(readyQueueMutex);
                    if (readyQueue.empty()) { 
                        // completeAll.notify_one();
                        taskAvailable.wait(lock, [this]() {
                        // std::cout << "All threads are waiting here\n"; // this is printed at the end, which means missing signals.
                            return !readyQueue.empty() || stopFlag.load();
                        });
                    }

                    if (stopFlag) {
                        break;
                    }
                    // std::cout << "before readyQueue pop\n"; 
                    // auto [batch, index] = readyQueue.front(); // cpp 17 feature, does not work in cpp11
                    auto batchPair = readyQueue.front();
                    readyQueue.pop();
                    // assigning code below could be put out of the block for speeding.
                    batchId = batchPair.first;
                    taskIndex = batchPair.second;
                    
                }

                {   
                    std::unique_lock<std::mutex> lock(taskMutex);
                    currentTask = &(tasksWithoutDeps[batchId]);
                }

              

                if (currentTask) {
                    currentTask->runnable->runTask(taskIndex, currentTask->numTotalTasks);
                    ++(currentTask->completedCount);
                    // std::cout << "----------------------------\n";
                        
                    // if completedCount reaches totalTasks, update dependencies
                    if (currentTask->completedCount == currentTask->numTotalTasks) {
                        completeAll.notify_one(); //这里可能有问题，因为run函数中有多个completeAll在等待，但是获取lock的顺序是固定的，因为queue是FIFO，所以可能没问题
                        // std::cout << "completedCount reach total tasks\n";
                        
                        // std::unique_lock<std::mutex> lock(readyQueueMutex); // 只lock readyQueue的话，下面loop taskWithDeps的时候，如果run了一个async导致这个map被改了就会出问题了，比如跳过了新进入的tasksWithDeps导致dep没有被erase
                        ++totalCompletedBatches;
                        for (auto& taskPair : tasksWithDeps) { // for all tasks that dependent on the task with the batchID, erase the task from their dependencies
                            TaskID dependentBatchId = taskPair.first;  // Get the batch ID
                            auto& task = taskPair.second; 
                            taskMutex.lock(); // might not be necessary? 
                            if (task.deps.find(batchId) != task.deps.end()) {
                                task.deps.erase(batchId);   // a map erase a non existing id is safe
                                // taskMutex.lock(); // if lock here the same task might push same batchId multiple times, 这里lock我想了半天还是应该放外面
                                if (task.deps.empty()) {
                                    tasksWithDeps.erase(dependentBatchId);
                                    tasksWithoutDeps.emplace(
                                        dependentBatchId,
                                        Task(task.numTotalTasks, task.runnable, std::unordered_set<TaskID>())
                                    );  
                                    taskMutex.unlock();
                                    readyQueueMutex.lock();
                                    for (int i = 0; i < task.numTotalTasks; ++i) {
                                        // readyQueue.push({dependentBatchId, i});
                                        readyQueue.push(std::make_pair(dependentBatchId, i));
                                    }
                                    readyQueueMutex.unlock();
                                }
                                taskAvailable.notify_one(); 
                            }
                            
                        }
                    }
                }
            }    
        });
    }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {

    // std::cout << "It enteres the destructor" << std::endl;
    stopFlag.store(true);
    {
        std::lock_guard<std::mutex> lock(readyQueueMutex);
        taskAvailable.notify_all(); // Wake up all threads
    }
    for (auto& thread : threadPool) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {
    // for (int i = 0; i < num_total_tasks; i++) {
    //     runnable->runTask(i, num_total_tasks);
    // }
    // Add a new task batch to the metadata
    // tasks[batchId] = std::move(Task(num_total_tasks, runnable, {}));
    // tasks[batchId] = Task(num_total_tasks, runnable, std::unordered_set<TaskID>());
    // tasks.emplace(batchId, Task(num_total_tasks, runnable, {}));
    // tasks.emplace(batchId, Task(num_total_tasks, runnable, std::unordered_set<TaskID>()));

    
    // TaskID batchId = nextBatchId++;
    TaskID batchId = -1;
    {
    std::unique_lock<std::mutex> lock(taskMutex); 
    batchId = nextBatchId++; // make sure we are using the global nextBatchId, so each batchId is unique and hashable for tasks<batchId, task>
    

    // std::cout << "tasksWithoutDeps Emplace is called in run function\n";
    tasksWithoutDeps.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(batchId),
        std::forward_as_tuple(num_total_tasks, runnable, std::unordered_set<TaskID>())
    );
    }

    // tasksWithoutDeps.emplace(
    //     batchId,
    //     Task(num_total_tasks, runnable, std::unordered_set<TaskID>())
    // );

    // std::cout << "emplace call finish\n";
    {
    std::unique_lock<std::mutex> lock(readyQueueMutex);
    for (int i = 0; i < num_total_tasks; ++i) {
        // readyQueue.push({batchId, i}); // seems not viable in c++11
        readyQueue.emplace(batchId, i);
    }

    // const std::deque<std::pair<int, int>>& container = *(reinterpret_cast<const std::deque<std::pair<int, int>>*>(&readyQueue));

    taskAvailable.notify_all();
    // std::cout << "notify all threads\n";
    completeAll.wait(lock, [this, batchId]() { // 我现在在想这个wait好像没用啊
        return tasksWithoutDeps[batchId].completedCount == tasksWithoutDeps[batchId].numTotalTasks;
    });
    // std::cout << "reach the end of run function\n";
    }
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


    
    std::unordered_set<TaskID> dependencies(deps.begin(), deps.end()); // copy the dependencies, must copy bc we are using set, but the given parameter is vector

    // TaskID batchId = nextBatchId++;
    std::lock_guard<std::mutex> lock(taskMutex);
    TaskID batchId = nextBatchId++;
    

    // Store metadata for this task batch
    // tasks.emplace(batchId, Task(num_total_tasks, runnable, dependencies));

    if (deps.empty()) {

        // tasksWithoutDeps.emplace(
        // batchId,
        // Task(num_total_tasks, runnable, std::unordered_set<TaskID>())
        // );
        // No dependencies, mark tasks as ready
        tasksWithoutDeps.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(batchId),
        std::forward_as_tuple(num_total_tasks, runnable, std::unordered_set<TaskID>())
        );
        for (int i = 0; i < num_total_tasks; ++i) {
            // readyQueue.push({batchId, i});
            readyQueue.push(std::make_pair(batchId, i));
        }
        taskAvailable.notify_all();
    }
    else {
        // tasksWithDeps.emplace(
        // batchId,
        // Task(num_total_tasks, runnable, std::unordered_set<TaskID>())
        // );
        tasksWithDeps.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(batchId),
        std::forward_as_tuple(num_total_tasks, runnable, std::unordered_set<TaskID>())
        );
    } 
    return batchId;

}

void TaskSystemParallelThreadPoolSleeping::sync() {

    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //

    std::unique_lock<std::mutex> lock(taskMutex);
    completeAll.wait(lock, [this]() {
        // for (auto& taskPair : tasks) {
        //     if (taskPair.second.completedCount < taskPair.second.numTotalTasks) {
        //         return false;
        //     }
        // }
        return nextBatchId == totalCompletedBatches; // only return True if all tasks are finished
    });
    return;
}
