#include "tasksys.h"
#include <algorithm>

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
        // bool wait = false;

        {   
            // if (readyQueue.empty()) {
            //     std::cout << "this works" << std::endl; // this is printed, which means readyQueue is not cleared
            // }
            // if (waitingQueue.empty()) {
            //     std::cout << "this shouldn't be the case\n"; // this is printed, 
            // }
            
            std::unique_lock<std::mutex> readyLock(readyQueueMutex);
            if (readyQueue.empty() && (waitingQueue.empty())) {
                // std::cout << "Wait till de\n";
                taskAvailable.wait(readyLock); // release the lock, and let the thread sleep if the readyQueue is empty
            }
            if (readyQueue.empty()) {
                std::unique_lock<std::mutex> waitingLock(waitingQueueMutex);
                while (!waitingQueue.empty()) {
                    const auto& nextTask = waitingQueue.top();
                    if (nextTask.dependTaskID > finishedTaskID) break; // haven't finished its dependency
                    // std::cout << "before adding tasks\n";
                    // std::cout << "nextTask.id before readyQueue emplace is " << nextTask.id << std::endl;
                    readyQueue.emplace(nextTask.id, nextTask.runnable, nextTask.numTotalTasks);
                    waitingQueue.pop();
                    // std::cout << "readyQueue.front().id afterrrrrrr emplace " << readyQueue.front().id << std::endl;
                    // every task.id is different, so shouln't have any data race here   
                    std::unique_lock<std::mutex> processLock(taskProcessMutex);
                    taskProcess[nextTask.id] = {0, nextTask.numTotalTasks}; // might need a taskMutex here //nextTaskID = nextTask.id + 1
                    taskAvailable.notify_one(); 
                    // std::cout << "Abort happened befre waitingQueue.pop()\n";
                    
                }
            }
            else {
                // std::cout << "now readyqueue is not empty, task.currentTask is " << task.currentTask << "; totaltasks is " << task.numTotalTasks << std::endl;
                task = readyQueue.front(); // this should be a reference pass, should not have double free error，segfault
                if (task.currentTask >= task.numTotalTasks) { // this was readyQueue.front().currentTask >= readyQueue.front().numTotalTasks
                    // std::cout << "now readyqueue is not empty, task.currentTask is " << task.currentTask << "; totaltasks is " << task.numTotalTasks << std::endl; // this is never printed, which means curretTask is not added 
                    // if (readyQueue.empty()) {std::cout << "shouldn't be empty";}
                    readyQueue.pop();  // 这里如果有些thread还在执行这个task的时候把它pop掉了可能有问题
                }
                else {
                    // task.currentTask++; // this was wrong, task is a copy of readyQueue.front(), so task.currentTask++ would not affect the real task in readyQueue.
                    // if (readyQueue.empty()) {std::cout << "shouldn't be empty";}
                    readyQueue.front().currentTask++;
                    // std::cout << "currentTask is " << task.currentTask << "; numtotalTasks is " << task.numTotalTasks << std::endl;
                    // std::cout << "task change might not affect readyQueue.front().task; " << "readyQueue.front.task.currentTask is " << readyQueue.front().currentTask << std::endl;
                    hasTask = true;
                }
            }
            

        }

        if (hasTask) {
 
            // std::cout << "Abortion happened before runTask\n";
            // std::cout << "Task.id is " << task.id << "; current task is" << task.currentTask << std::endl;
            
            task.runnable->runTask(task.currentTask, task.numTotalTasks);

            // Update the most recently finished taskid--finishedTaskID
            std::unique_lock<std::mutex> processLock(taskProcessMutex);
            // std::unique_lock<std::mutex> waitingLock(waitingQueueMutex); // if no this mutex, 
            // std::cout << "after runTask, befre taskProcess[task.id]\n";
            auto& [finished, total] = taskProcess[task.id];
            if (++finished == total) { // Task batch completed
                taskProcess.erase(task.id);
                finishedTaskID = std::max(finishedTaskID, task.id); // always track the most recently finished taskID so that we can check dependency 
                // std::cout << "finishedTaskID is " << finishedTaskID << std::endl;
                finishedCondition.notify_one();
            }

            // if (hasTask) {
            // try {
            //     // Add a final check to ensure `task` is still valid
            //     if (task.currentTask >= task.numTotalTasks) {
            //         std::cerr << "Task already completed or invalid. Skipping...\n";
            //         continue;
            //     }

            //     // Execute the task
            //     task.runnable->runTask(task.currentTask, task.numTotalTasks);

            //     // Update task progress
            //     std::unique_lock<std::mutex> processLock(taskProcessMutex);
            //     auto& [finished, total] = taskProcess[task.id];
            //     if (++finished == total) { // Task batch completed
            //         taskProcess.erase(task.id);
            //         finishedTaskID = std::max(finishedTaskID, task.id);
            //         finishedCondition.notify_one();
            //     }
            //     } catch (const std::exception& e) {
            //         std::cerr << "Exception occurred during task execution: " << e.what() << "\n";
            //     } catch (...) {
            //         std::cerr << "Unknown error occurred during task execution.\n";
            //     }
            // }
        }
    }
}


// void TaskSystemParallelThreadPoolSleeping::workerThread() {
//     while (!killed) {
//         ReadyTask task;
//         bool hasTask = false;

//         try {
//             {   
//                 std::unique_lock<std::mutex> readyLock(readyQueueMutex);

//                 if (readyQueue.empty() && waitingQueue.empty()) {
//                     taskAvailable.wait(readyLock);
//                 }

//                 // 从等待队列转移任务到就绪队列
//                 if (readyQueue.empty()) {
//                     std::unique_lock<std::mutex> waitingLock(waitingQueueMutex);
//                     while (!waitingQueue.empty()) {
//                         const auto& nextTask = waitingQueue.top();
//                         if (nextTask.dependTaskID > finishedTaskID) break; // 依赖未完成
//                         readyQueue.emplace(nextTask.id, nextTask.runnable, nextTask.numTotalTasks);
//                         waitingQueue.pop();

//                         // 更新任务进度
//                         std::unique_lock<std::mutex> processLock(taskProcessMutex);
//                         taskProcess[nextTask.id] = {0, nextTask.numTotalTasks};
//                         taskAvailable.notify_one();
//                     }
//                 } else {
//                     // 获取当前任务
//                     task = readyQueue.front();
//                     if (task.currentTask >= task.numTotalTasks) {
//                         readyQueue.pop();
//                     } else {
//                         readyQueue.front().currentTask++;
//                         hasTask = true;
//                     }
//                 }
//             }

//             if (hasTask) {
//                 try {
//                     // 检查任务状态是否仍然有效
//                     if (task.currentTask >= task.numTotalTasks) {
//                         std::cerr << "任务已完成或无效，跳过...\n";
//                         continue;
//                     }

//                     // 执行任务
//                     task.runnable->runTask(task.currentTask, task.numTotalTasks);

//                     // 更新任务完成进度
//                     std::unique_lock<std::mutex> processLock(taskProcessMutex);
//                     auto& [finished, total] = taskProcess[task.id];
//                     if (++finished == total) { // 批任务完成
//                         taskProcess.erase(task.id);
//                         finishedTaskID = std::max(finishedTaskID, task.id);
//                         finishedCondition.notify_one();
//                     }
//                 } catch (const std::exception& e) {
//                     std::cerr << "执行任务时发生异常: " << e.what() << "\n";
//                 } catch (...) {
//                     std::cerr << "执行任务时发生未知错误。\n";
//                 }
//             }
//         } catch (const std::exception& e) {
//             std::cerr << "队列或任务操作时发生异常: " << e.what() << "\n";
//         } catch (...) {
//             std::cerr << "队列或任务操作时发生未知错误。\n";
//         }
//     }
// }


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

    // {
    //     std::unique_lock<std::mutex> lockReady(readyQueueMutex);
    //     std::queue<ReadyTask> emptyReadyQueue;
    //     std::swap(readyQueue, emptyReadyQueue); // 清空 readyQueue
    // }
    // {
    //     std::unique_lock<std::mutex> lockWaiting(waitingQueueMutex);
    //     std::priority_queue<WaitingTask> emptyWaitingQueue;
    //     std::swap(waitingQueue, emptyWaitingQueue); // 清空 waitingQueue
    // }

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
    // std::cout << "run function finished\n";
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {
    TaskID dependency = -1;
    if (!deps.empty()) {
        dependency = *std::max_element(deps.begin(), deps.end()); // max_element will return an interator, *element to get the integer value
        // std::cout << "dependency is " << dependency << std::endl; // this is not printed, which means segfault happened before this
    }

    {
        std::unique_lock<std::mutex> lock(waitingQueueMutex);
        // std::cout << "nextTaskID is " << nextTaskID << std::endl;
        waitingQueue.emplace(nextTaskID, dependency, runnable, num_total_tasks);
    }                                                    

    taskAvailable.notify_all();

    // {
    //     std::unique_lock<std::mutex> readyLock(readyQueueMutex);
    //     taskAvailable.notify_all();
    // }
    
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
