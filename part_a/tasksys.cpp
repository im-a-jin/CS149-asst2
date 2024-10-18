#include "tasksys.h"


IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {}
ITaskSystem::~ITaskSystem() {}

void * runTaskWrapperA1(void * args) {
    TaskArgs * taskArgs = (TaskArgs *) args;
    int thread_id = taskArgs->thread_id;
    int num_total_tasks = taskArgs->num_total_tasks;
    int num_threads = taskArgs->num_threads;
    for (int i = thread_id; i < num_total_tasks; i += num_threads) {
      (taskArgs->runnable)->runTask(i, num_total_tasks);
    }
    return NULL;
}

void * runTaskWrapperA2(void * args) {
    TaskArgsA2 *taskArgs = (TaskArgsA2 *) args;
    while (!*(taskArgs->done)) {

        // lock mutex
            // default isRunning to off
            // is there anything on the queue?
                // switch my isRunning on (threadId indexed)
                // yes: pop it and hold on to the var (locally)
        // unlock mutex

        // if local var, runtask
            // runtask
    
    }

    return NULL;
}

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

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads): ITaskSystem(num_threads) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //

    _num_threads = num_threads;
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {
    //
    // TODO: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    pthread_t *threads = (pthread_t *) malloc(_num_threads * sizeof(pthread_t));
    TaskArgs *args = (TaskArgs *) malloc(_num_threads * sizeof(TaskArgs));

    for (int i = 0; i < _num_threads; i++) {
        args[i].runnable = runnable;
        args[i].thread_id = i;
        args[i].num_total_tasks = num_total_tasks;
        args[i].num_threads = _num_threads;
        pthread_create(&threads[i], NULL, runTaskWrapperA1, &args[i]);
    }
    // runnable->runTask(0, num_total_tasks);

    for (int i = 0; i < _num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(args);
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

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads): ITaskSystem(num_threads) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //

    _num_threads = num_threads;

    // Initialize threads - alloc memory
    _thread_pool = (pthread_t *) malloc(_num_threads * sizeof(pthread_t));

    // Initialize mutex
    pthread_mutex_init(&_mutex_lock, NULL);

    // Setup "isRunning" array
    _is_running = (bool *) calloc(false, _num_threads * sizeof(bool));


    // Setup args
    _args = (TaskArgsA2 *)malloc(sizeof(TaskArgsA2));
    _args->done = &_done;
    _args->work_queue = &_work_queue;
    _args->mutex_lock = &_mutex_lock;

    // PThread create
    for (int i = 0; i < _num_threads; i++) {
        pthread_create(&_thread_pool[i], NULL, runTaskWrapperA2, _args);
    }
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {

    _done = true;

    // Join threads
    for (int i = 0; i < _num_threads; i++) {
        pthread_join(_thread_pool[i], NULL);
    }

    // Free memory
    free(_thread_pool);
    free(_is_running);
    free(_args);
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {


    //
    // TODO: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    // Lock queue

    // Pour everything into the queue
    // for (int i = 0; i < num_total_tasks; i++) {
    //     runnable->runTask(i, num_total_tasks);
    // }

    // Unlock queue

    while (!_done) {
        // Lock mutex
        pthread_mutex_lock(&_mutex_lock);

        // Is the queue empty?
        // AND is nobody running anything?
            // If so, break

        // Unlock mutex
        pthread_mutex_unlock(&_mutex_lock);
    }
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
