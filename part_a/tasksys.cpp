#include "tasksys.h"

IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {}
ITaskSystem::~ITaskSystem() {}

void * runTaskWrapperA1(void * args) {
    TaskArgsA1 * taskArgs = (TaskArgsA1 *) args;
    int task_id = taskArgs->task_id->fetch_add(1);
    while (task_id < taskArgs->num_total_tasks) {
        (taskArgs->runnable)->runTask(task_id, taskArgs->num_total_tasks);
        task_id = taskArgs->task_id->fetch_add(1);
    }
    return NULL;
}

void * runTaskWrapperA2(void * args) {
    TaskArgsA2 *taskArgs = (TaskArgsA2 *) args;
    RunTask cur_task;

    while (!*(taskArgs->done)) {
        taskArgs->is_running[taskArgs->thread_id] = false;

        pthread_mutex_lock(taskArgs->mutex_lock);
        if (!taskArgs->work_queue->empty()) {
            taskArgs->is_running[taskArgs->thread_id] = true;
            cur_task = taskArgs->work_queue->front();

            if (cur_task.task_id + TASKS_PER_THREAD < cur_task.num_total_tasks) {
                (taskArgs->work_queue->front()).task_id++;
            } else {
                taskArgs->work_queue->pop();
            }
        }
        pthread_mutex_unlock(taskArgs->mutex_lock);

        if (taskArgs->is_running[taskArgs->thread_id]) {
            for (int i = 0; i < std::min(TASKS_PER_THREAD, cur_task.num_total_tasks-cur_task.task_id); i++) {
                cur_task.runnable->runTask(cur_task.task_id + i, cur_task.num_total_tasks);
            }
        }
    }

    return NULL;
}

void * runTaskWrapperA3(void * args) {
    TaskArgsA3 *taskArgs = (TaskArgsA3 *) args;
    int cur_task;

    while (!*(taskArgs->done)) {
        pthread_mutex_lock(taskArgs->mutex_lock);
        if (*(taskArgs->work_queue) < *(taskArgs->num_total_tasks)) {
            cur_task = (*(taskArgs->work_queue))++;
            pthread_mutex_unlock(taskArgs->mutex_lock);
            (*(taskArgs->runnable))->runTask(cur_task, *(taskArgs->num_total_tasks));
            taskArgs->tasks_done->fetch_add(1, std::memory_order_relaxed);
        } else {
            pthread_cond_signal(taskArgs->all_done);
            if (*(taskArgs->done)) {
                pthread_mutex_unlock(taskArgs->mutex_lock);
                return NULL;
            }
            pthread_cond_wait(taskArgs->wake, taskArgs->mutex_lock);
            pthread_mutex_unlock(taskArgs->mutex_lock);
        }
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
    TaskArgsA1 *args = (TaskArgsA1 *) malloc(_num_threads * sizeof(TaskArgsA1));
    _task_id = 0;

    for (int i = 0; i < _num_threads; i++) {
        args[i].runnable = runnable;
        args[i].thread_id = i;
        args[i].num_total_tasks = num_total_tasks;
        args[i].task_id = &_task_id;
        pthread_create(&threads[i], NULL, runTaskWrapperA1, &args[i]);
    }

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

    _done = (bool *) malloc(sizeof(bool));
    *_done = false;

    _thread_pool = (pthread_t *) malloc(_num_threads * sizeof(pthread_t));

    pthread_mutex_init(&_mutex_lock, NULL);

    _is_running = (bool *) malloc(_num_threads * sizeof(bool));

    _args = (TaskArgsA2 *) malloc(_num_threads * sizeof(TaskArgsA2));

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

    pthread_mutex_destroy(&_mutex_lock);

    // Free memory
    free(_done);
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
    RunTask cur_task = {runnable, 0, num_total_tasks};

    pthread_mutex_lock(&_mutex_lock);
    _work_queue.push(cur_task);
    pthread_mutex_unlock(&_mutex_lock);

    while (!(*_done)) {
        pthread_mutex_lock(&_mutex_lock);
        if (_work_queue.empty()) {
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
    _runnable = NULL;
    _num_total_tasks = 0;
    _work_queue = 0;
    _tasks_done = 0;

    _done = (bool *) malloc(sizeof(bool));
    *_done = false;

    _thread_pool = (pthread_t *) malloc(_num_threads * sizeof(pthread_t));

    pthread_mutex_init(&_mutex_lock, NULL);
//  _thread_locks = (pthread_mutex_t *) malloc(_num_threads * sizeof(pthread_mutex_t));
    pthread_cond_init(&_wake, NULL);
    pthread_cond_init(&_all_done, NULL);

    _args = (TaskArgsA3 *) malloc(_num_threads * sizeof(TaskArgsA3));

    // PThread create
    for (int i = 0; i < _num_threads; i++) {
//      pthread_mutex_init(&_thread_locks[i], NULL);
        _args[i].thread_id = i;
        _args[i].num_threads = _num_threads;
        _args[i].done = _done;
        _args[i].work_queue = &_work_queue;
        _args[i].mutex_lock = &_mutex_lock;
//      _args[i].thread_lock = &_thread_locks[i];
        _args[i].wake = &_wake;
        _args[i].all_done = &_all_done;
        _args[i].tasks_done = &_tasks_done;
        _args[i].runnable = &_runnable;
        _args[i].num_total_tasks = &_num_total_tasks;
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
    pthread_mutex_lock(&_mutex_lock);
    *_done = true;
    pthread_mutex_unlock(&_mutex_lock);
    pthread_cond_broadcast(&_wake);

    // Join threads
    for (int i = 0; i < _num_threads; i++) {
        pthread_join(_thread_pool[i], NULL);
    }

    pthread_mutex_destroy(&_mutex_lock);
//  for (int i = 0; i < _num_threads; i++) {
//      pthread_mutex_destroy(&_thread_locks[i]);
//  }
    pthread_cond_destroy(&_wake);
    pthread_cond_destroy(&_all_done);

    // Free memory
    free(_done);
//  free(_thread_locks);
    free(_thread_pool);
    free(_args);
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {
    //
    // TODO: CS149 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //
    _tasks_done = 0;

    pthread_mutex_lock(&_mutex_lock);
    _work_queue = 0;
    _runnable = runnable;
    _num_total_tasks = num_total_tasks;
    pthread_mutex_unlock(&_mutex_lock);
    pthread_cond_broadcast(&_wake);
    pthread_mutex_lock(&_mutex_lock);
    while (_tasks_done < _num_total_tasks) {
        pthread_cond_wait(&_all_done, &_mutex_lock);
    }
    _runnable = NULL;
    _num_total_tasks = 0;
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
