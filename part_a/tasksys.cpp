#include "tasksys.h"
#include <chrono>
#include <iostream>

IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {}
ITaskSystem::~ITaskSystem() {}

void * runTaskWrapperA1(void * args) {
    TaskArgsA1 * taskArgs = (TaskArgsA1 *) args;
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
    int cur_task;

    // // Time measurement
    // auto start = std::chrono::high_resolution_clock::now();
    // double outer_run_time = 0.0;
    // double inner_run_time = 0.0;
    // double exec_run_time = 0.0;

    while (!*(taskArgs->done)) {
        // auto outer_start = std::chrono::high_resolution_clock::now();
        pthread_mutex_lock(taskArgs->mutex_lock);
        // auto inner_start = std::chrono::high_resolution_clock::now();
        taskArgs->is_running[taskArgs->thread_id] = false;

        if (taskArgs->task->task_id < taskArgs->task->num_total_tasks) {
            taskArgs->is_running[taskArgs->thread_id] = true;
            cur_task = taskArgs->task->task_id;
            taskArgs->task->task_id += 1;
        }
        // auto inner_end = std::chrono::high_resolution_clock::now();
        // auto inner_duration = std::chrono::duration_cast<std::chrono::microseconds>(inner_end - inner_start);
        // inner_run_time += inner_duration.count() / 1000.0;
        pthread_mutex_unlock(taskArgs->mutex_lock);
        // auto outer_end = std::chrono::high_resolution_clock::now();
        // auto outer_duration = std::chrono::duration_cast<std::chrono::microseconds>(outer_end - outer_start);
        // outer_run_time += outer_duration.count() / 1000.0;

        if (taskArgs->is_running[taskArgs->thread_id]) {
            // auto exec_start = std::chrono::high_resolution_clock::now();
            taskArgs->task->runnable->runTask(cur_task, taskArgs->task->num_total_tasks);
            // auto exec_end = std::chrono::high_resolution_clock::now();
            // auto exec_duration = std::chrono::duration_cast<std::chrono::microseconds>(exec_end - exec_start);
            // exec_run_time += exec_duration.count() / 1000.0;
        }

    }

    // auto end = std::chrono::high_resolution_clock::now();
    // auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    // double run_time = duration.count() / 1000.0;
    // printf("Thread %d total run time ms: %f\n", taskArgs->thread_id, run_time);
    // printf("Thread %d outer run time ms: %f\n", taskArgs->thread_id, outer_run_time);
    // printf("Thread %d inner run time ms: %f\n", taskArgs->thread_id, inner_run_time);
    // printf("Thread %d exec run time ms: %f\n", taskArgs->thread_id, exec_run_time);
    
    return NULL;
}

void * runTaskWrapperA3(void * args) {
    // Time measurement
    auto start = std::chrono::high_resolution_clock::now();
    double outer_run_time = 0.0;
    double inner_run_time = 0.0;
    double exec_run_time = 0.0;


    TaskArgsA3 *taskArgs = (TaskArgsA3 *) args;
    RunTask cur_task, next_task;
    bool runnable;

    while (!*(taskArgs->done)) {
        runnable = false;
        auto outer_start = std::chrono::high_resolution_clock::now();
        pthread_mutex_lock(taskArgs->mutex_lock);
        auto inner_start = std::chrono::high_resolution_clock::now();
        if (!taskArgs->work_queue->empty()) {
            cur_task = taskArgs->work_queue->front();
            taskArgs->work_queue->pop();
            next_task = {cur_task.runnable, cur_task.task_id + 1, cur_task.num_total_tasks};
            taskArgs->work_queue->push(next_task);
            pthread_cond_signal(taskArgs->queue_add);
            if (cur_task.task_id < cur_task.num_total_tasks) {
                runnable = true;
            } else {
                if (cur_task.task_id == cur_task.num_total_tasks + taskArgs->num_threads - 1) {
                    pthread_cond_signal(taskArgs->all_threads_done);
                }
                pthread_cond_wait(taskArgs->reset, taskArgs->mutex_lock);
            }
        } else {
            pthread_cond_wait(taskArgs->queue_add, taskArgs->mutex_lock);
        }
        auto inner_end = std::chrono::high_resolution_clock::now();
        auto inner_duration = std::chrono::duration_cast<std::chrono::microseconds>(inner_end - inner_start);
        inner_run_time += inner_duration.count() / 1000.0;
        pthread_mutex_unlock(taskArgs->mutex_lock);
        auto outer_end = std::chrono::high_resolution_clock::now();
        auto outer_duration = std::chrono::duration_cast<std::chrono::microseconds>(outer_end - outer_start);
        outer_run_time += outer_duration.count() / 1000.0;

        if (runnable) {
            auto exec_start = std::chrono::high_resolution_clock::now();
            cur_task.runnable->runTask(cur_task.task_id, cur_task.num_total_tasks);
            auto exec_end = std::chrono::high_resolution_clock::now();
            auto exec_duration = std::chrono::duration_cast<std::chrono::microseconds>(exec_end - exec_start);
            exec_run_time += exec_duration.count() / 1000.0;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double run_time = duration.count() / 1000.0;
    printf("Thread %d total run time ms: %f\n", taskArgs->thread_id, run_time);
    printf("Thread %d outer run time ms: %f\n", taskArgs->thread_id, outer_run_time);
    printf("Thread %d inner run time ms: %f\n", taskArgs->thread_id, inner_run_time);
    printf("Thread %d exec run time ms: %f\n", taskArgs->thread_id, exec_run_time);
   

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
    TaskArgsA1 *args = (TaskArgsA1 *) malloc(_num_threads * sizeof(TaskArgsA1));

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

    _task = (RunTask *) malloc(sizeof(RunTask));
    _done = (bool *) malloc(sizeof(bool));
    *_done = false;

    _thread_pool = (pthread_t *) malloc(_num_threads * sizeof(pthread_t));
    _is_running = (bool *) malloc(_num_threads * sizeof(bool));
    _args = (TaskArgsA2 *) malloc(_num_threads * sizeof(TaskArgsA2));

    pthread_mutex_init(&_mutex_lock, NULL);

    // PThread create
    for (int i = 0; i < _num_threads; i++) {
        _args[i].thread_id = i;
        _args[i].is_running = _is_running;
        _args[i].done = _done;
        _args[i].task = _task;
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

    pthread_mutex_destroy(&_mutex_lock);

    // Free memory
    free(_done);
    free(_thread_pool);
    free(_is_running);
    free(_task);
    free(_args);
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {
    //
    // TODO: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //
    _task->runnable = runnable;
    _task->num_total_tasks = num_total_tasks;
    // auto start = std::chrono::high_resolution_clock::now();


    RunTask first_task = {runnable, 0, num_total_tasks};

    pthread_mutex_lock(&_mutex_lock);
    _task->task_id = 0;
    pthread_mutex_unlock(&_mutex_lock);

    while (!(*_done)) {
        pthread_mutex_lock(&_mutex_lock);
        if (_task->task_id == _task->num_total_tasks) {
            bool any_running = false;
            for (int i = 0; i < _num_threads; i++) {
                any_running |= _is_running[i];
            }

            if (!any_running) {
                pthread_mutex_unlock(&_mutex_lock);
                break;
            }
        }
        pthread_mutex_unlock(&_mutex_lock);
    }

    // auto end = std::chrono::high_resolution_clock::now();
    // auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);    
    // double run_time = duration.count() / 1000.0;
    // _time_in_run += run_time;
    n_run_calls++;
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

    _done = (bool *) malloc(sizeof(bool));
    *_done = false;

    _thread_pool = (pthread_t *) malloc(_num_threads * sizeof(pthread_t));

    pthread_mutex_init(&_mutex_lock, NULL);
    pthread_cond_init(&_queue_add, NULL);
    pthread_cond_init(&_all_threads_done, NULL);
    pthread_cond_init(&_reset, NULL);

    _args = (TaskArgsA3 *) malloc(_num_threads * sizeof(TaskArgsA3));

    // PThread create
    for (int i = 0; i < _num_threads; i++) {
        _args[i].thread_id = i;
        _args[i].num_threads = _num_threads;
        _args[i].done = _done;
        _args[i].work_queue = &_work_queue;
        _args[i].mutex_lock = &_mutex_lock;
        _args[i].queue_add = &_queue_add;
        _args[i].all_threads_done = &_all_threads_done;
        _args[i].reset = &_reset;
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
    *_done = true;
    pthread_cond_broadcast(&_reset);

    // Join threads
    for (int i = 0; i < _num_threads; i++) {
        pthread_join(_thread_pool[i], NULL);
    }

    pthread_mutex_destroy(&_mutex_lock);
    pthread_cond_destroy(&_queue_add);
    pthread_cond_destroy(&_all_threads_done);
    pthread_cond_destroy(&_reset);

    free(_done);
    free(_thread_pool);
    free(_args);
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {
    //
    // TODO: CS149 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //
    pthread_cond_broadcast(&_reset);

    RunTask first_task = {runnable, 0, num_total_tasks};

    pthread_mutex_lock(&_mutex_lock);
    _work_queue.push(first_task);
    pthread_cond_signal(&_queue_add);
    pthread_cond_wait(&_all_threads_done, &_mutex_lock);
    if (!_work_queue.empty()) {
        _work_queue.pop();
    }
    pthread_mutex_unlock(&_mutex_lock);
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
