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
                    std::cout << "before adding tasks\n";
                    readyQueue.emplace(nextTask.id, nextTask.runnable, nextTask.numTotalTasks);
                    taskProcess[nextTaskID] = {0, nextTask.numTotalTasks}; // might need a taskMutex here
                    waitingQueue.pop();
                }
            }
            
            if (readyQueue.empty()) {
                std::cout << "Wait till de\n";
                taskAvailable.wait(readyLock); // release the lock, and let the thread sleep if the readyQueue is empty
            }
        }

        if (!readyQueue.empty()) {
            task = readyQueue.front();
            if (readyQueue.front().currentTask >= readyQueue.front().numTotalTasks) {
                readyQueue.pop();
            }
            else {
                readyQueue.front().currentTask++;
                hasTask = true;
            }
        }

        if (hasTask) {
            task.runnable->runTask(task.currentTask, task.numTotalTasks);

            // Update the most recently finished taskid--finishedTaskID
            std::unique_lock<std::mutex> processLock(taskProcessMutex);
            auto& [finished, total] = taskProcess[task.id];
            if (++finished == total) { // Task batch completed
                taskProcess.erase(task.id);
                finishedTaskID = std::max(finishedTaskID, task.id);
                finishedCondition.notify_one();
            }
        }
    }
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads)
    : ITaskSystem(num_threads)
{
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
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {
    TaskID dependency = -1;
    if (!deps.empty()) {
        dependency = *std::max(deps.begin(), deps.end());
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
}
