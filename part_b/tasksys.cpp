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

void *runTaskWrapperB(void *args) {
    TaskArgsB *taskArgs = (TaskArgsB *) args;
    TaskGraphNode *cur_task;
    int task_id;

    while (!*(taskArgs->done)) {
        pthread_mutex_lock(taskArgs->wq_lock);
        if (!taskArgs->work_queue->empty()) {
            cur_task = taskArgs->work_queue->front();
            task_id = cur_task->task_id;
            if (cur_task->task_id++ == cur_task->num_total_tasks-1) {
                taskArgs->work_queue->pop();
            }
            pthread_mutex_unlock(taskArgs->wq_lock);
            cur_task->runnable->runTask(task_id, cur_task->num_total_tasks);

            if (cur_task->tasks_done.fetch_add(1, std::memory_order_relaxed) == cur_task->num_total_tasks-1) {
                std::vector<TaskID> ids;
                pthread_mutex_lock(taskArgs->tg_lock);
                for (TaskID id : cur_task->deps_out) {
                    if (--(*(taskArgs->task_graph))[id]->num_deps == 0) {
                        ids.push_back(id);
                    }
                }
                pthread_mutex_unlock(taskArgs->tg_lock);
                
                pthread_mutex_lock(taskArgs->wq_lock);
                for (TaskID id : ids) {
                    taskArgs->work_queue->push((*(taskArgs->task_graph))[id]);
                }
                pthread_mutex_unlock(taskArgs->wq_lock);
                pthread_cond_broadcast(taskArgs->wake);
            }
        } else {
            pthread_cond_signal(taskArgs->all_done);
            if (*(taskArgs->done)) {
                pthread_mutex_unlock(taskArgs->mutex_lock);
                return NULL;
            }
            pthread_cond_wait(taskArgs->wake, taskArgs->wq_lock);
            pthread_mutex_unlock(taskArgs->wq_lock);
        }
    }

    return NULL;
}

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

    pthread_mutex_init(&_wq_lock, NULL);
    pthread_mutex_init(&_tg_lock, NULL);
    pthread_cond_init(&_wake, NULL);
    pthread_cond_init(&_all_done, NULL);

    _args = (TaskArgsB *) malloc(_num_threads * sizeof(TaskArgsB));

    // PThread create
    for (int i = 0; i < _num_threads; i++) {
        _args[i].num_threads = _num_threads;
        _args[i].done = _done;
        _args[i].work_queue = &_work_queue;
        _args[i].task_graph = &_task_graph;
        _args[i].wq_lock = &_wq_lock;
        _args[i].tg_lock = &_tg_lock;
        _args[i].wake = &_wake;
        _args[i].all_done = &_all_done;
        pthread_create(&_thread_pool[i], NULL, runTaskWrapperB, &_args[i]);
    }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    //
    // TODO: CS149 student implementations may decide to perform cleanup
    // operations (such as thread pool shutdown construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    pthread_mutex_lock(&_wq_lock);
    *_done = true;
    pthread_mutex_unlock(&_wq_lock);
    pthread_cond_broadcast(&_wake);

    for (TaskGraphNode *tgn : _task_graph) {
        delete tgn;
    }

    // Join threads
    for (int i = 0; i < _num_threads; i++) {
        pthread_join(_thread_pool[i], NULL);
    }

    pthread_mutex_destroy(&_wq_lock);
    pthread_mutex_destroy(&_tg_lock);
    pthread_cond_destroy(&_wake);
    pthread_cond_destroy(&_all_done);

    // Free memory
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

    addTask(runnable, num_total_tasks, std::vector<TaskID>());
    return blockUntilDone();
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks, const std::vector<TaskID>& deps) {
    //
    // TODO: CS149 students will implement this method in Part B.
    //

    return addTask(runnable, num_total_tasks, deps);
}

void TaskSystemParallelThreadPoolSleeping::sync() {
    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //

    return blockUntilDone();
}

TaskID TaskSystemParallelThreadPoolSleeping::addTask(IRunnable* runnable, int num_total_tasks, const std::vector<TaskID>& deps) {
    TaskID node_id = _task_graph.size();
    TaskGraphNode *tgn = new TaskGraphNode(node_id, runnable, num_total_tasks);
    tgn->num_deps = 0;

    pthread_mutex_lock(&_tg_lock);
    for (TaskID id : deps) {
        tgn->num_deps += int(_task_graph[id]->tasks_done < _task_graph[id]->num_total_tasks);
        _task_graph[id]->deps_out.push_back(node_id);
    }
    _task_graph.push_back(tgn);
    pthread_mutex_unlock(&_tg_lock);

    if (tgn->num_deps == 0) {
        pthread_mutex_lock(&_wq_lock);
        _work_queue.push(tgn);
        pthread_mutex_unlock(&_wq_lock);
        pthread_cond_broadcast(&_wake);
    }

    return node_id;
}

void TaskSystemParallelThreadPoolSleeping::blockUntilDone() {
    bool run_complete = false;

    pthread_mutex_lock(&_tg_lock);
    while (!(*_done)) {
        run_complete = true;
        for (TaskGraphNode *tgn : _task_graph) {
            run_complete &= tgn->tasks_done == tgn->num_total_tasks;
        }
        if (run_complete) {
            break;
        }
        pthread_cond_wait(&_all_done, &_tg_lock);
    }
    pthread_mutex_unlock(&_tg_lock);

    return; 
}
