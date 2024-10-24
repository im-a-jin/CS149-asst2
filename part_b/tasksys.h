#ifndef _TASKSYS_H
#define _TASKSYS_H

#include "itasksys.h"
#include <unordered_map>
#include <cassert>
#include <climits>
#include "pthread.h"
#include <algorithm>


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



typedef int SubTaskID;

struct WorkUnit {
    TaskID task_id; // -1 if no work to do
    std::vector<SubTaskID> subtask_ids;
    int subtask_idx; // Invariant: Holds index of next unassigned subtask ID
    int num_subtasks_to_run; // May want to run >1 subtasks at a time
    int num_total_tasks;
    IRunnable* runnable;
};

struct TaskGraphNode {
    WorkUnit work_unit;
    std::vector<TaskID> dependents; // Tasks that depend on me
    int n_dependencies; // Tasks that I depend on
    int n_subtasks_done;
    bool done;
};

struct WorkerArgsB;

/*
 * TaskGraph: This class handles the task graph which underlies
 * TaskSystemParallelThreadPoolSleeping.
 * 
 * All public operations acquire _tg_lock and release when done.
 * Private operations assume lock is in place.
 */
class TaskGraph {
    public:
        // Basics
        TaskGraph();
        ~TaskGraph();

        // Configure number of workers to chunk work for
        int n_workers; 

        // Adding always broadcasts
        TaskID addTask(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps,
                                int n_workers, WorkerArgsB *worker_args);

        // Mark current unit of work done, return next unit of work, if any
        WorkUnit markCompleteGetNext(WorkUnit wu, pthread_mutex_t *worker_lock, bool *idle_flag);

        // Either returns sync or cond_waits
        WorkUnit getStarterWorkUnit(pthread_mutex_t *worker_lock, pthread_cond_t *worker_inbox,
            bool *idle_flag, WorkUnit *wu_mailbox);

        // Consumers use this to listen for done state (sync)
        void blockUntilEmpty();

        // Called by main to get all workers to return
        void shutdown(WorkerArgsB *worker_args);

    private:
        int _task_id_counter;
        int _completed_tasks_counter;

        // Internal graph storage object
        //std::unordered_map<TaskID, TaskGraphNode> _task_graph;
        std::vector<TaskGraphNode> _task_graph;

        // Push tasks on back, pop from front
        // Invariant: Tasks popped as soon as the last subtask is assigned
        std::vector<TaskID> _ready_tasks; 
        int _rt_n_done; // Number of tasks done in ready_tasks

        // Assumes lock is in place
        WorkUnit getNextWorkUnitInner(); 

        // Locks, conditions
        pthread_mutex_t _tg_lock; // Must have this to update tg in any way
        // pthread_cond_t _task_received; // If there's no work, idle threads subscribe to this and wait for broadcast
        pthread_cond_t _all_tasks_done; // For sync call

};

struct WorkerArgsB {
    TaskGraph *tg;
    int thread_id;
    pthread_mutex_t worker_mutex;
    pthread_cond_t worker_inbox;
    bool is_idle; // Invariant: Only change if holding worker_mutex
    WorkUnit wu_mailbox; // Invariant: Only change if holding worker_mutex
};

void* threadWorkerB(void *args);

/*
 * TaskSystemParallelThreadPoolSleeping: This class is the student's
 * optimized implementation of a parallel task execution engine that uses
 * a thread pool. See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSleeping: public ITaskSystem {
    public:
        TaskSystemParallelThreadPoolSleeping(int num_threads);
        ~TaskSystemParallelThreadPoolSleeping();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();

    private:
        TaskGraph _tg;
        pthread_t * _thread_pool;
        int _num_threads;
        WorkerArgsB *_worker_args;



};

#endif
