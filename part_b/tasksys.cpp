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
 * TaskGraph implementation
 * ================================================================
 */

#define MAX_TASK_CHUNKING 4
#define NO_TASK -1
#define SHUTDOWN_TASK -2

TaskGraph::TaskGraph() {
    // State variables
    _task_id_counter = 0;
    _completed_tasks_counter = 0;

    // Mutex and condition variables
    pthread_mutex_init(&_tg_lock, NULL);
    pthread_cond_init(&_task_received, NULL);
    pthread_cond_init(&_all_tasks_done, NULL);

    // Init data structures
    //_task_graph = std::unordered_map<TaskID, TaskGraphNode>();
    _task_graph = std::vector<TaskGraphNode>();
    _ready_tasks = std::vector<TaskID>();
    _rt_n_done = 0;
}

TaskGraph::~TaskGraph() {
    // printf("TaskGraph::~TaskGraph: Cleaning up\n");

    // Destroy mutex and condition variables
    pthread_mutex_destroy(&_tg_lock);
    pthread_cond_destroy(&_task_received);
    pthread_cond_destroy(&_all_tasks_done);
}

TaskID TaskGraph::addTask(IRunnable* runnable, int num_total_tasks, const std::vector<TaskID>& deps,
                          int n_workers, WorkerArgsB *worker_args) {
    // Adds a new task to the graph, managing graph state and subscribers

    // Calculate subtask allocation
    int n_subtasks_per_worker = 1;
    if (num_total_tasks > n_workers) {
        n_subtasks_per_worker = num_total_tasks / n_workers;
        if (n_subtasks_per_worker > MAX_TASK_CHUNKING) {
            n_subtasks_per_worker = MAX_TASK_CHUNKING;
        }
    }
    // printf("TaskGraph::addTask: %d subtasks per worker\n", n_subtasks_per_worker);

    // Prep new task node
    TaskGraphNode tgn;
    tgn.work_unit = WorkUnit{0, 0, num_total_tasks-1, n_subtasks_per_worker, num_total_tasks, runnable};
    tgn.n_dependencies = deps.size();
    tgn.dependents = std::vector<TaskID>();
    tgn.n_subtasks_done = 0;
    tgn.done = false;

    // <CRITICAL_SECTION>
    pthread_mutex_lock(&_tg_lock);

    // Assign ID
    TaskID cur_tid = _task_id_counter++; // Returns current value, then increments
    tgn.work_unit.task_id = cur_tid;

    // Add node to graph
    //_task_graph[cur_tid] = tgn;
    _task_graph.push_back(tgn);

    // Process task's dependencies
    // printf("TaskGraph::addTask: Task %d has %d dependencies:\n    ", cur_tid, tgn.n_dependencies);
    for (TaskID dep : deps) {
        // Add dependent to dependency
        _task_graph[dep].dependents.push_back(cur_tid);
        // printf("%d, ", dep);

        // If dependency is done, decrement dependency count
        if (_task_graph[dep].done) {
            tgn.n_dependencies--;
        }
    }
    // printf("\n");


    // If task has no dependencies, add to ready tasks
    if (tgn.n_dependencies == 0) {
        _ready_tasks.push_back(cur_tid);
    }

    // printf("TaskGraph::addTask: Added task %d: %d subtasks, %d deps\n", cur_tid, num_total_tasks, tgn.n_dependencies);


    // Hand out work to idle workers, if any
    int cur_worker = 0;
    bool has_unassigned_work = true;
    
    // Find next available idle worker, assign next unit of work
    while (cur_worker < n_workers && has_unassigned_work) {
        // <WORKER-SPECIFIC_CRITICAL_SECTION> Grab current worker mutex
        pthread_mutex_lock(&worker_args[cur_worker].worker_mutex);

        // Found an idle worker!
        if (worker_args[cur_worker].is_idle) {
            // Get a unit of work for this worker
            WorkUnit wu = getNextWorkUnitInner(); // Also updates _ready_tasks
            if (wu.task_id == NO_TASK) {
                // printf("TaskGraph::addTask: No work to assign to worker %d\n", cur_worker);
                has_unassigned_work = false;
                pthread_mutex_unlock(&worker_args[cur_worker].worker_mutex); // (Break skips usual unlock)
                break; // Jump out of inner loop if no work
            } else {
                // printf("TaskGraph::addTask: Assigning task %d, subtask %d to worker %d\n", cur_tid, wu.subtask_id, cur_worker);
                worker_args[cur_worker].wu_mailbox = wu;
                worker_args[cur_worker].is_idle = false; // Flag immediately so successive adds can't overwrite
                pthread_cond_signal(&worker_args[cur_worker].worker_inbox); // Signal worker
            }
        }
        pthread_mutex_unlock(&worker_args[cur_worker].worker_mutex);
        // </WORKER-SPECIFIC_CRITICAL_SECTION> Unlock worker mutex

        cur_worker++;
    }

    pthread_mutex_unlock(&_tg_lock);
    // </CRITICAL_SECTION>

    return cur_tid;
}

WorkUnit TaskGraph::markCompleteGetNext(WorkUnit wu_done, pthread_mutex_t *worker_lock, bool *idle_flag) {
    // Receives the completion of a subtask, updates graph state,
    // and returns the next unit of work to be done, if any
    // (If no work, task_id will be NO_TASK)

    // If we've run out of work, set the worker's idle flag


    TaskID task = wu_done.task_id;
    // int subtask_id = wu_done.subtask_id;
    // int num_subtasks_to_run = wu_done.num_subtasks_to_run;
    // printf("TaskGraph::markComplete: Task %d, subtask %d-%d is done\n", task, subtask_id, subtask_id + num_subtasks_to_run - 1);


    // <CRITICAL_SECTION>
    pthread_mutex_lock(&_tg_lock);

    // Look up the task graph node
    TaskGraphNode& tgn = _task_graph[task];
    WorkUnit wu;

    // Update subtask count
    tgn.n_subtasks_done += wu_done.num_subtasks_to_run;

    // Was this the last subtask?
    if (tgn.n_subtasks_done == tgn.work_unit.num_total_tasks) {
        // Manage task completion
        tgn.done = true;
        _completed_tasks_counter++;
        // printf("TaskGraph::markComplete: Task %d is done\n", task);

        // Process dependents
        for (TaskID dep : tgn.dependents) {
            // Decrement dependency count
            _task_graph[dep].n_dependencies--;

            // printf("TaskGraph::markComplete: Task %d has %d dependencies left\n", dep, _task_graph[dep].n_dependencies);

            // If dependency is done, add to ready tasks
            if (_task_graph[dep].n_dependencies == 0) {
                _ready_tasks.push_back(dep);
                // printf("TaskGraph::markComplete: Task %d is now ready\n", dep);
            }
        }

        // Was this the last subtask of the last task?
        if (_completed_tasks_counter == _task_id_counter) {
            // Broadcast that all tasks are done
            pthread_cond_broadcast(&_all_tasks_done);
        }
    }

    // Find new work
    wu = getNextWorkUnitInner();

    // Flag idle worker if we're out of work
    if (wu.task_id == NO_TASK) {
        pthread_mutex_lock(worker_lock);
        *idle_flag = true;
        pthread_mutex_unlock(worker_lock);
    }

    pthread_mutex_unlock(&_tg_lock);
    // </CRITICAL_SECTION>

    return wu;
}


WorkUnit TaskGraph::getStarterWorkUnit(
    pthread_mutex_t *worker_lock, pthread_cond_t *worker_inbox,
    bool *idle_flag, WorkUnit *wu_mailbox) {
    // Called by workers who have absolutely no work:
    // Either they just started, or their current thread of work ran out
    // (implying _ready_tasks is empty)

    // This uses workers' individual personal locks to avoid everyone 
    // contending for main lock when new work arrives.

    // Since it doesn't edit the task graph, it doesn't need the main lock.
    WorkUnit wu;

    // <WORKER-SPECIFIC_CRITICAL_SECTION>
    pthread_mutex_lock(worker_lock);

    while (true) { // Loop to handle spurious wakeups
        // Is there something already allocated for me?
        if (wu_mailbox->task_id != NO_TASK) {
            // printf("TaskGraph::getStarterWorkUnit: Worker %d found work in mailbox\n", pthread_self());
            // Yes: Take it and empty mailbox
            wu = *wu_mailbox;
            *wu_mailbox = WorkUnit{NO_TASK, 0, 0, 0, 0, NULL};
            break;
        } else {
            // printf("TaskGraph::getStarterWorkUnit: Worker %d found no work in mailbox\n", pthread_self());
            // Nothing allocated for me, wait for more work
            // Subscribe to _task_received signal, wait for work
            pthread_cond_wait(worker_inbox, worker_lock);
            // printf("TaskGraph::getStarterWorkUnit: Worker %d woke up\n", pthread_self());
        }
    }

    assert(wu.task_id != NO_TASK);

    pthread_mutex_unlock(worker_lock);
    // </WORKER-SPECIFIC_CRITICAL_SECTION>

    return wu;
}


WorkUnit TaskGraph::getNextWorkUnitInner() {
    // Helper function to get the next unit of work to be done, if any
    // Returns wu with task_id of -1 if no work to do.
    // Never blocks.
    // Assumes shared lock is in place.
    WorkUnit wu;

    if (_ready_tasks.size() == _rt_n_done) { // e.g. got 3, done 3
        // No work on queue: Return sentinel
        wu.task_id = NO_TASK;
    } else {
        // Get next task ID
        int task_id = _ready_tasks[_rt_n_done]; // #done is also the index of the next task
        WorkUnit& tg_wu = _task_graph[task_id].work_unit;

        // Setup work unit
        int n_subtasks_left = tg_wu.subtask_id_hi - tg_wu.subtask_id_lo + 1; // e.g. 4 tasks = (0,3)

        wu.task_id = tg_wu.task_id; // Must get this from tg_wu for special shutdown case
        wu.subtask_id_lo = tg_wu.subtask_id_lo;
        wu.subtask_id_hi = tg_wu.subtask_id_hi;
        wu.num_subtasks_to_run = (n_subtasks_left < tg_wu.num_subtasks_to_run) ? n_subtasks_left : tg_wu.num_subtasks_to_run;
        wu.num_total_tasks = tg_wu.num_total_tasks;
        wu.runnable = tg_wu.runnable;

        // Increment graph subtask_id
        int add_lo = tg_wu.num_subtasks_to_run / 2;
        // For odd subtask counts, alternate between adding to lo and hi
        if (tg_wu.num_subtasks_to_run % 2 == 1) {
            if (tg_wu.subtask_id_lo % 2 == 0 && tg_wu.subtask_id_hi % 2 == 0) {
                add_lo++;
            } // add_hi is implicit
        }
        tg_wu.subtask_id_lo += add_lo;
        tg_wu.subtask_id_hi -= tg_wu.num_subtasks_to_run - add_lo;

        // Pop task from ready tasks if we just assigned the last subtask
        int n_assigned = tg_wu.subtask_id_lo + (tg_wu.num_total_tasks - (tg_wu.subtask_id_hi+1));
        if (n_assigned >= tg_wu.num_total_tasks) {
            _ready_tasks[_rt_n_done] = NO_TASK;
            _rt_n_done++;
        }
    }

    return wu;
}

void TaskGraph::blockUntilEmpty() {
    // Called by consumers like sync() to sleep until all tasks are done.
    // When it returns, we guarantee that all tasks are done.

    // <CRITICAL_SECTION>
    pthread_mutex_lock(&_tg_lock);

    // Wait for all tasks to be done
    while (true) { // Loop to handle spurious wakeups
        if (_completed_tasks_counter == _task_id_counter) {
            // All tasks are done
            break;
        }
        pthread_cond_wait(&_all_tasks_done, &_tg_lock);
    }

    pthread_mutex_unlock(&_tg_lock);
    // </CRITICAL_SECTION>
}

void TaskGraph::shutdown(WorkerArgsB *worker_args) {
    // Called by main to put graph into shutdown state
    // Ensures queue is empty, then adds sentinel to work queue
    // and broadcasts to all threads

    // Ensure queue is empty
    this->blockUntilEmpty();

    // Setup special sentinel work unit
    // Thread workers exit when they receive this task ID
    WorkUnit wu_shutdown;
    wu_shutdown.task_id = SHUTDOWN_TASK;
    wu_shutdown.subtask_id_lo = 0;
    wu_shutdown.num_total_tasks = INT_MAX;

    // Put sentinel task on work queue for non-idle workers
    // <CRITICAL_SECTION>
    pthread_mutex_lock(&_tg_lock);
    _task_graph[0].work_unit = wu_shutdown;
    _task_graph[0].n_subtasks_done = 0;
    _task_graph[0].done = false;
    _ready_tasks.push_back(0);
    pthread_mutex_unlock(&_tg_lock);
    // </CRITICAL_SECTION>

    // Put sentinel task in mailbox for idle workers
    for (int i = 0; i < n_workers; i++) {
        pthread_mutex_lock(&worker_args[i].worker_mutex);
        worker_args[i].wu_mailbox = wu_shutdown;
        worker_args[i].is_idle = false; // Flag immediately so successive adds can't overwrite
        pthread_cond_signal(&worker_args[i].worker_inbox); // Signal worker
        pthread_mutex_unlock(&worker_args[i].worker_mutex);
    }
}







/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

void* threadWorkerB(void *args) {
    // Unpack arguments
    WorkerArgsB *wa = (WorkerArgsB *) args;
    TaskGraph *tg = wa->tg;
    int thread_id = wa->thread_id;
    bool *idle_flag = &(wa->is_idle);
    WorkUnit *wu_mailbox = &(wa->wu_mailbox);
    pthread_mutex_t *my_mutex = &wa->worker_mutex;
    pthread_cond_t *my_inbox = &wa->worker_inbox;

    WorkUnit wu;
    wu.task_id = NO_TASK;

    while (true) {
        if (wu.task_id == NO_TASK) {
            wu = tg->getStarterWorkUnit(my_mutex, my_inbox, idle_flag, wu_mailbox);
        } else if (wu.task_id == SHUTDOWN_TASK) {
            // Received done sentinel, exit
            // printf("threadWorkerB: Received done sentinel, exiting\n");
            break;
        } else {
            // Run the task and mark it complete
            for (int i = 0; i < wu.num_subtasks_to_run; i++) {
                if (i % 2 == 0) {
                    wu.runnable->runTask(wu.subtask_id_lo + i/2, wu.num_total_tasks);
                } else {
                    wu.runnable->runTask(wu.subtask_id_hi - i/2, wu.num_total_tasks);
                }
                
                // printf("threadWorkerB: thread %d, runTask: Task %d, subtask %d done\n", thread_id, wu.task_id, wu.subtask_id+i);
            }
            wu = tg->markCompleteGetNext(wu, my_mutex, idle_flag);
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

    // Setup thread pool
    _num_threads = num_threads;
    _thread_pool = (pthread_t *) malloc(_num_threads * sizeof(pthread_t));
    _tg.n_workers = num_threads; // To help chunk work

    // Worker arguments 
    _worker_args = (WorkerArgsB *) malloc(_num_threads * sizeof(WorkerArgsB));

    // PThread create
    for (int i = 0; i < _num_threads; i++) {
        _worker_args[i].tg = &_tg;
        _worker_args[i].thread_id = i;
        _worker_args[i].is_idle = true;
        _worker_args[i].wu_mailbox = WorkUnit{NO_TASK, 0, 0, 0, 0, NULL};
        pthread_mutex_init(&_worker_args[i].worker_mutex, NULL);
        pthread_cond_init(&_worker_args[i].worker_inbox, NULL);
        pthread_create(&_thread_pool[i], NULL, threadWorkerB, (void *) &_worker_args[i]);
    }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    //
    // TODO: CS149 student implementations may decide to perform cleanup
    // operations (such as thread pool shutdown construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //

    // Send shutdown signal
    _tg.shutdown(_worker_args);

    // Join threads
    for (int i = 0; i < _num_threads; i++) {
        pthread_join(_thread_pool[i], NULL);
        pthread_cond_destroy(&_worker_args[i].worker_inbox);
        pthread_mutex_destroy(&_worker_args[i].worker_mutex);
    }

    // Free memory
    free(_worker_args);
    free(_thread_pool);

    // printf("TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping: Freeing thread pool\n");
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {


    //
    // TODO: CS149 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    _tg.addTask(runnable, num_total_tasks, std::vector<TaskID>(), _num_threads, _worker_args);
    _tg.blockUntilEmpty();
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {


    //
    // TODO: CS149 students will implement this method in Part B.
    //

    // Add task to graph
    return _tg.addTask(runnable, num_total_tasks, deps, _num_threads, _worker_args);
}

void TaskSystemParallelThreadPoolSleeping::sync() {

    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //

    // Block until all tasks are done
    _tg.blockUntilEmpty();

    return;
}
