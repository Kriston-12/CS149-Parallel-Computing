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
    // You do not need to implement this method.
    return 0;
}

void TaskSystemSerial::sync() {
    // You do not need to implement this method.
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

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads): 
    ITaskSystem(num_threads), 
    numThreads(num_threads), 
    /*threadPool(std::make_unique<std::vector<std::thread>>())--c++11 does not have make_unique, only c++14 later has*/
    threadPool(new std::vector<std::thread>)  {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    // for smart pointer, using ->
    threadPool->reserve(num_threads);

}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {
    // TODO: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    // for (int i = 0; i < num_total_tasks; i++) {
    //     runnable->runTask(i, num_total_tasks);
    // }

    std::atomic<int> taskIndex(0); // atomic variable shared by all threads, 
                                   // if it reaches num_total_tasks, then all assigned tasks are distributed and done by assigned threads

    // runnable represents the test class, runTask is a function of this class
    auto threadTask = [&]() {
        while (true) {
            int i = taskIndex.fetch_add(1);
            if (i >= num_total_tasks) break;
            runnable->runTask(i, num_total_tasks);
        }
    };

    for (int i = 0; i < numThreads; ++i) {
        threadPool->emplace_back(threadTask);
    }
    
    for (auto& thread : *threadPool) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    threadPool->clear();
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                 const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelSpawn::sync() {
    // You do not need to implement this method.
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

// Function that lets each threads wait until tasks come in to notify them
void TaskSystemParallelThreadPoolSpinning::workerThreadLoop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(taskMutex); // lock the mutex for conditional variable usage

            // if the current task is done or the tasks queue is empty, assign task then, otherwise wait
            taskCondition.wait(lock, [this]() {return done || !tasks.empty();}); 

            if (done && tasks.empty()) {
                break;
            }
            
            // assign first task 
            task = std::move(tasks.front()); 
            tasks.pop();
            ++activeTasks;
        }

        // let the thread execute this task if it is available
        task(); 

        { // if thread reaches here, meaning it completed the assigned task 
            std::unique_lock<std::mutex> lock(taskMutex);
            --activeTasks;
            if (tasks.empty() && activeTasks == 0) {
                taskCompleteCondition.notify_all();
            }
        }
    }
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads): 
        ITaskSystem(num_threads), 
        numThreads(num_threads),
        done(false) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    threadPool.reserve(numThreads);
    for (int i = 0; i < numThreads; ++i) {
        threadPool.emplace_back(&TaskSystemParallelThreadPoolSpinning::workerThreadLoop, this);
    }
    
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
    {
        std::unique_lock<std::mutex> lock(taskMutex);
        done = true; // let all other threads run their tasks cuz the thread is about to exit
        taskCondition.notify_all();
    }

    for (std::thread& t: threadPool) {
        if (t.joinable()) {
            t.join();
        }
    }
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {
    //
    // TODO: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    // for (int i = 0; i < num_total_tasks; i++) {
    //     runnable->runTask(i, num_total_tasks);
    // }

    {
        std::unique_lock<std::mutex> lock(taskMutex); // this lock is to ensure 
        for (int i = 0; i < num_total_tasks; ++i) {
            // enqueue functions to be run in the tasks queue
            tasks.emplace([=] () {
                runnable->runTask(i, num_total_tasks);
            });
        }
    }

    // after assigning, notifying all threads
    taskCondition.notify_all();

    std::unique_lock<std::mutex> lock(taskMutex);
    taskCompleteCondition.wait(lock, [this]() {return tasks.empty() && activeTasks == 0;});
}


TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    // You do not need to implement this method.
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

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads): ITaskSystem(num_threads) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    //
    // TODO: CS149 student implementations may decide to perform cleanup
    // operations (such as thread pool shutdown construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {


    //
    // TODO: CS149 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {


    //
    // TODO: CS149 students will implement this method in Part B.
    //

    return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync() {

    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //

    return;
}
