#ifndef _TASKSYS_H
#define _TASKSYS_H

#include "itasksys.h"
#include "pthread.h"
#include "stdlib.h"
#include <queue>
#include <algorithm>
#include <atomic>

#define TASKS_PER_THREAD 1

struct RunTask {
    IRunnable* runnable;
    int task_id;
    int num_total_tasks;
};

struct TaskArgsA1 {
    IRunnable* runnable;
    int thread_id;
    int num_total_tasks;
    std::atomic<int> *task_id;
};

struct TaskArgsA2 {
    int thread_id;
    bool *is_running;
    bool *done;
    std::queue<RunTask> *work_queue;
    pthread_mutex_t *mutex_lock;
};

struct TaskArgsA3 {
    int thread_id;                  // thread id
    int num_threads;                // total number of threads
    bool *done;                     // true if destructor called, false otherwise
    int *work_queue;                // task_id counter
    pthread_mutex_t *mutex_lock;    // shared mutex lock
//  pthread_mutex_t *thread_lock;   // thread wait locks
    pthread_cond_t *wake;           // thread sleep/done condition variable
    pthread_cond_t *all_done;       // thread completed 
    std::atomic<int> *tasks_done;   // counter for tasks done
    IRunnable **runnable;           // pointer to runnable object
    int *num_total_tasks;           // total number of tasks
};


void* runTaskWrapperA1(void * args);

void* runTaskWrapperA2(void * args);

void* runTaskWrapperA3(void * args);

/*
 * TaskSystemSerial: This class is the student's implementation of a
 * serial task execution engine.  See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemSerial: public ITaskSystem {
    public:
        TaskSystemSerial(int num_threads);
        ~TaskSystemSerial();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

/*
 * TaskSystemParallelSpawn: This class is the student's implementation of a
 * parallel task execution engine that spawns threads in every run()
 * call.  See definition of ITaskSystem in itasksys.h for documentation
 * of the ITaskSystem interface.
 */
class TaskSystemParallelSpawn: public ITaskSystem {
    private:
        int _num_threads;
        std::atomic<int> _task_id;

    public:
        TaskSystemParallelSpawn(int num_threads);
        ~TaskSystemParallelSpawn();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

/*
 * TaskSystemParallelThreadPoolSpinning: This class is the student's
 * implementation of a parallel task execution engine that uses a
 * thread pool. See definition of ITaskSystem in itasksys.h for
 * documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSpinning: public ITaskSystem {
    private:
        std::queue<RunTask> _work_queue;
        pthread_mutex_t _mutex_lock;
        bool *_is_running;
        bool *_done;
        TaskArgsA2 *_args;
        pthread_t *_thread_pool;
        int _num_threads;

    public:
        TaskSystemParallelThreadPoolSpinning(int num_threads);
        ~TaskSystemParallelThreadPoolSpinning();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

/*
 * TaskSystemParallelThreadPoolSleeping: This class is the student's
 * optimized implementation of a parallel task execution engine that uses
 * a thread pool. See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSleeping: public ITaskSystem {
    private:
        int _work_queue;
        pthread_mutex_t _mutex_lock;
//      pthread_mutex_t *_thread_locks;
        pthread_cond_t _wake;
        pthread_cond_t _all_done;
        bool *_done;
        TaskArgsA3 *_args;
        pthread_t *_thread_pool;
        int _num_threads;
        std::atomic<int> _tasks_done;
        IRunnable *_runnable;
        int _num_total_tasks;

    public:
        TaskSystemParallelThreadPoolSleeping(int num_threads);
        ~TaskSystemParallelThreadPoolSleeping();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

#endif
