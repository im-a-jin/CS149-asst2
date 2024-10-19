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
    RunTask cur_task, next_task;
    // Print pointer to done

    while (!*(taskArgs->done)) {

        pthread_mutex_lock(taskArgs->mutex_lock);
        // default isRunning to off
        taskArgs->is_running[taskArgs->thread_id] = false;

        // is there anything on the queue?
        if (!taskArgs->work_queue->empty()) {
            // switch my isRunning on (threadId indexed)
            taskArgs->is_running[taskArgs->thread_id] = true;

            // pop the queue and hold on to the var (locally)
            cur_task = taskArgs->work_queue->front();
            taskArgs->work_queue->pop();

            // add 1 to taskId and push to queue
            if (cur_task.task_id + 1 < cur_task.num_total_tasks) {
                next_task = {cur_task.runnable, cur_task.task_id + 1, cur_task.num_total_tasks};
                taskArgs->work_queue->push(next_task);
            }

        }

        pthread_mutex_unlock(taskArgs->mutex_lock);

        // if local var, runtask
        if (taskArgs->is_running[taskArgs->thread_id]) {
            cur_task.runnable->runTask(cur_task.task_id, cur_task.num_total_tasks);
        }
    }

    return NULL;
}

void * runTaskWrapperA3(void * args) {
    TaskArgsA3 *taskArgs = (TaskArgsA3 *) args;
    RunTask cur_task, next_task;
    bool runnable;

    while (true) {
        runnable = false;
        pthread_mutex_lock(taskArgs->mutex_lock);
        if (!taskArgs->work_queue->empty()) {
            cur_task = taskArgs->work_queue->front();
            taskArgs->work_queue->pop();
            next_task = {cur_task.runnable, cur_task.task_id + 1, cur_task.num_total_tasks};
            taskArgs->work_queue->push(next_task);
            if (cur_task.task_id < cur_task.num_total_tasks) {
                runnable = true;
                pthread_cond_signal(taskArgs->queue_add);
            } else {
                if (cur_task.task_id == cur_task.num_total_tasks + taskArgs->num_threads - 1) {
                    pthread_cond_signal(taskArgs->all_threads_done);
                }
                pthread_cond_wait(taskArgs->run_complete, taskArgs->mutex_lock);
            }
        } else {
            pthread_cond_wait(taskArgs->queue_add, taskArgs->mutex_lock);
        }
        pthread_mutex_unlock(taskArgs->mutex_lock);

        if (runnable) {
            // printf("Thread %d running task %d of %d\n", taskArgs->thread_id, cur_task.task_id, cur_task.num_total_tasks);
            cur_task.runnable->runTask(cur_task.task_id, cur_task.num_total_tasks);
        }
    }
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

    _done = (bool *)malloc(sizeof(bool));
    *_done = false;


    // Initialize threads - alloc memory
    _thread_pool = (pthread_t *) malloc(_num_threads * sizeof(pthread_t));

    // Initialize mutex
    pthread_mutex_init(&_mutex_lock, NULL);

    // Setup "isRunning" array
    _is_running = (bool *) calloc(false, _num_threads * sizeof(bool));


    // Setup args
    _args = (TaskArgsA2 *)malloc(_num_threads*sizeof(TaskArgsA2));

    // PThread create
    for (int i = 0; i < _num_threads; i++) {
        _args[i].thread_id = i;
        _args[i].is_running = _is_running;
        _args[i].done = _done;
        _args[i].work_queue = &_work_queue;
        _args[i].mutex_lock = &_mutex_lock;
        pthread_create(&_thread_pool[i], NULL, runTaskWrapperA2, &_args[i]);
    }
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {

    // Print pointer to done
    *_done = true;

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

    // Pour first task into queue
    RunTask first_task = {runnable, 0, num_total_tasks};

    pthread_mutex_lock(&_mutex_lock);
    _work_queue.push(first_task);
    pthread_mutex_unlock(&_mutex_lock);


    // Unlock queue
    while (!_done) {
        // Lock mutex
        pthread_mutex_lock(&_mutex_lock);

        // Is the queue empty?
        if (_work_queue.empty()) {
            bool any_running = false;
            // AND is nobody running anything?
            for (int i=0; i<_num_threads; i++) {
                any_running &= _is_running[i];
                // if (_is_running[i]) {
                // }
            }

            // If so, break
            if (!any_running) {
                break;
            }
        }

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

    _num_threads = num_threads;

    _thread_pool = (pthread_t *) malloc(_num_threads * sizeof(pthread_t));

    pthread_mutex_init(&_mutex_lock, NULL);
    pthread_cond_init(&_queue_add, NULL);
    pthread_cond_init(&_all_threads_done, NULL);
    pthread_cond_init(&_run_complete, NULL);

    _args = (TaskArgsA3 *)malloc(_num_threads*sizeof(TaskArgsA3));

    // PThread create
    for (int i = 0; i < _num_threads; i++) {
        _args[i].thread_id = i;
        _args[i].num_threads = _num_threads;
        _args[i].work_queue = &_work_queue;
        _args[i].mutex_lock = &_mutex_lock;
        _args[i].queue_add = &_queue_add;
        _args[i].all_threads_done = &_all_threads_done;
        _args[i].run_complete = &_run_complete;
        pthread_create(&_thread_pool[i], NULL, runTaskWrapperA3, &_args[i]);
    }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    //
    // TODO: CS149 student implementations may decide to perform cleanup
    // operations (such as thread pool shutdown construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //

    free(_thread_pool);
    free(_args);
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {
    //
    // TODO: CS149 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //
    RunTask first_task = {runnable, 0, num_total_tasks};

    pthread_mutex_lock(&_mutex_lock);
    _work_queue.push(first_task);
    pthread_cond_signal(&_queue_add);
    pthread_mutex_unlock(&_mutex_lock);

    pthread_mutex_lock(&_mutex_lock);
    pthread_cond_wait(&_all_threads_done, &_mutex_lock);
    if (!_work_queue.empty()) {
        _work_queue.pop();
    }
    pthread_mutex_unlock(&_mutex_lock);
    
    pthread_cond_broadcast(&_run_complete);
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
