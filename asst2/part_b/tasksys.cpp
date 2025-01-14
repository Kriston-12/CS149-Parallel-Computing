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
/*
 * Worker Thread logic
 */
void TaskSystemParallelThreadPoolSleeping::workerThread() {
    while (!killed) {
        ReadyTask task;
        bool hasTask = false;

        {
            std::unique_lock<std::mutex> readyLock(readyQueueMutex);
            if (readyQueue.empty()) {
                std::unique_lock<std::mutex> waitingLock(waitingQueueMutex);
                while (!waitingQueue.empty()) {
                    const auto& nextTask = waitingQueue.top();
                    if (nextTask.dependTaskID > finishedTaskID) break; // haven't finished its dependency
                    // std::cout << "before adding tasks\n";
                    readyQueue.emplace(nextTask.id, nextTask.runnable, nextTask.numTotalTasks);
                    taskAvailable.notify_one(); 
                    // every task.id is different, so shouln't have any data race here   
                    taskProcess[nextTask.id] = {0, nextTask.numTotalTasks}; // might need a taskMutex here //nextTaskID = nextTask.id + 1
                    // std::cout << "Abort happened befre waitingQueue.pop()\n";
                    waitingQueue.pop();
                }
            }
            
            if (readyQueue.empty()) {
                std::cout << "Wait till de\n";
                taskAvailable.wait(readyLock); // release the lock, and let the thread sleep if the readyQueue is empty
            }
        }

        if (!readyQueue.empty()) {
            std::unique_lock<std::mutex> readyLock(readyQueueMutex);
            task = readyQueue.front(); // this should be a reference pass, should not have double free error，segfault
            if (task.currentTask >= task.numTotalTasks) { // this was readyQueue.front().currentTask >= readyQueue.front().numTotalTasks
                readyQueue.pop();  // 这里如果有些thread还在执行这个task的时候把它pop掉了可能有问题
            }
            else {
                task.currentTask++; // this was  readyQueue.front().currentTask++
                hasTask = true;
            }
        }

        if (hasTask) {
 
            // std::cout << "Abortion happened before runTask\n";
            std::cout << "Task.id is " << task.id << "; current task is" << task.currentTask << std::endl;
            task.runnable->runTask(task.currentTask, task.numTotalTasks);

            // Update the most recently finished taskid--finishedTaskID
            std::unique_lock<std::mutex> processLock(taskProcessMutex);
            // std::unique_lock<std::mutex> waitingLock(waitingQueueMutex); // if no this mutex, 
            // std::cout << "after runTask, befre taskProcess[task.id]\n";
            auto& [finished, total] = taskProcess[task.id];
            if (++finished == total) { // Task batch completed
                taskProcess.erase(task.id);
                finishedTaskID = std::max(finishedTaskID, task.id); // always track the most recently finished taskID so that we can check dependency 
                std::cout << "finishedTaskID is " << finishedTaskID << std::endl;
                finishedCondition.notify_one();
            }
        }
    }
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads)
    : ITaskSystem(num_threads)
{   
   killed.store(false);
//    finishedTaskID = -1;
//    nextTaskID = 0;
   threadPool.reserve(num_threads);
   for (int i = 0; i < num_threads; ++i) {
    threadPool.emplace_back(&TaskSystemParallelThreadPoolSleeping::workerThread, this);
   }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {

    // std::cout << "It enteres the destructor" << std::endl;
    killed.store(true);
    taskAvailable.notify_all();
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
    std::vector<TaskID> noDeps;
    runAsyncWithDeps(runnable, num_total_tasks, noDeps);
    sync();  // much cleaner
    std::cout << "run function finished\n";
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {
    TaskID dependency = -1;
    if (!deps.empty()) {
        dependency = *std::max(deps.begin(), deps.end()); // we use the greatest integer among all dependencies as the dependency
    }

    {
        std::unique_lock<std::mutex> lock(waitingQueueMutex);
        waitingQueue.emplace(nextTaskID, dependency, runnable, num_total_tasks);
    }                                                    

    taskAvailable.notify_all();
    return nextTaskID++;

}

void TaskSystemParallelThreadPoolSleeping::sync() {

    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //

    std::unique_lock<std::mutex> lock(taskProcessMutex);
    finishedCondition.wait(lock, [this]() {return finishedTaskID + 1 == nextTaskID;});

    std::cout << "thread does not reach here\n"; // it does reach here
}
