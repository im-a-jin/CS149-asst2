#ifndef _TASKSYS_H
#define _TASKSYS_H

#include "itasksys.h"
#include "stdlib.h"
#include "pthread.h"
#include <vector>
#include <queue>
#include <atomic>

typedef class TaskGraphNode TaskGraphNode;

struct TaskArgsB {
    int thread_id;                              // thread id
    int num_threads;                            // total number of threads
    bool *done;                                 // true if destructor called
    std::queue<TaskGraphNode *> *work_queue;    // task_id counter
    std::vector<TaskGraphNode *> *task_graph;   // task_id counter
    pthread_mutex_t *wq_lock;                   // task worker lock
    pthread_mutex_t *tg_lock;                   // task worker lock
    pthread_cond_t *wake;                       // thread sleep/done condition variable
    pthread_cond_t *all_done;                   // thread completed 
};

class TaskGraphNode {
    public:
        int node_id;
        int task_id;
        std::atomic<int> tasks_done;
        IRunnable *runnable;
        int num_total_tasks;
        int num_deps;
        std::vector<TaskID> deps_out;

        TaskGraphNode(int node_id, IRunnable *runnable, int num_total_tasks) {
            this->node_id = node_id;
            this->task_id = 0;
            this->tasks_done = 0;
            this->runnable = runnable;
            this->num_total_tasks = num_total_tasks;
            this->num_deps = 0;
        }
};

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
        std::queue<TaskGraphNode *> _work_queue;
        std::vector<TaskGraphNode *> _task_graph;
        pthread_mutex_t _wq_lock;
        pthread_mutex_t _tg_lock;
        pthread_cond_t _wake;
        pthread_cond_t _all_done;
        bool *_done;
        TaskArgsB *_args;
        pthread_t *_thread_pool;
        int _num_threads;

    public:
        TaskSystemParallelThreadPoolSleeping(int num_threads);
        ~TaskSystemParallelThreadPoolSleeping();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
        TaskID addTask(IRunnable* runnable, int num_total_tasks, const std::vector<TaskID>& deps);
        void blockUntilDone();
};

#endif
