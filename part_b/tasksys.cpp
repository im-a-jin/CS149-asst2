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

TaskGraph::TaskGraph() {
    // State variables
    _task_id_counter = 0;
    _completed_tasks_counter = 0;

    // Mutex and condition variables
    pthread_mutex_init(&_tg_lock, NULL);
    pthread_cond_init(&_task_received, NULL);
    pthread_cond_init(&_all_tasks_done, NULL);

    // Init data structures
    _task_graph = std::unordered_map<TaskID, TaskGraphNode>();
    _ready_tasks = std::deque<TaskID>();
}

TaskGraph::~TaskGraph() {
    // printf("TaskGraph::~TaskGraph: Cleaning up\n");

    // Destroy mutex and condition variables
    pthread_mutex_destroy(&_tg_lock);
    pthread_cond_destroy(&_task_received);
    pthread_cond_destroy(&_all_tasks_done);
}

TaskID TaskGraph::addTask(IRunnable* runnable, int num_total_tasks, const std::vector<TaskID>& deps) {
    // Adds a new task to the graph, managing graph state and subscribers

    // Prep new task node
    TaskGraphNode tgn;
    tgn.work_unit = WorkUnit{0, 0, num_total_tasks, runnable};
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
    _task_graph[cur_tid] = tgn;

    // Process task's dependencies
    for (TaskID dep : deps) {
        // Add dependent to dependency
        _task_graph[dep].dependents.push_back(cur_tid);

        // If dependency is done, decrement dependency count
        if (_task_graph[dep].done) {
            tgn.n_dependencies--;
        }
    }

    // If task has no dependencies, add to ready tasks
    if (tgn.n_dependencies == 0) {
        _ready_tasks.push_back(cur_tid);
    }

    // printf("TaskGraph::addTask: Added task %d: %d tasks, %d deps\n", cur_tid, num_total_tasks, tgn.n_dependencies);

    // Broadcast that a task was added
    pthread_cond_broadcast(&_task_received);

    pthread_mutex_unlock(&_tg_lock);
    // </CRITICAL_SECTION>

    return cur_tid;
}

WorkUnit TaskGraph::markComplete(TaskID task, int subtask_id) {
    // Receives the completion of a subtask, updates graph state,
    // and returns the next unit of work to be done, if any
    // (If no work, task_id will be -1)

    // printf("TaskGraph::markComplete: Task %d, subtask %d\n", task, subtask_id);

    // <CRITICAL_SECTION>
    pthread_mutex_lock(&_tg_lock);

    // Look up the task graph node
    TaskGraphNode& tgn = _task_graph[task];
    WorkUnit wu;

    // Update subtask count
    tgn.n_subtasks_done++;

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

    pthread_mutex_unlock(&_tg_lock);
    // </CRITICAL_SECTION>

    return wu;
}

WorkUnit TaskGraph::getNextWorkUnit() {
    // Returns the next unit of work to be done, if any
    // (If no work, task_id will be -1)
    WorkUnit wu;

    // <CRITICAL_SECTION>
    pthread_mutex_lock(&_tg_lock);

    while (true) { // Loop to handle spurious wakeups
        // Run once first for the sync case
        wu = getNextWorkUnitInner();
        if (wu.task_id != -1) {
            // Found work, return it
            break;
        } else {
            // Subscribe to _task_received broadcast, wait for work
            pthread_cond_wait(&_task_received, &_tg_lock);
        }
    }

    pthread_mutex_unlock(&_tg_lock);
    // </CRITICAL_SECTION>

    assert(wu.task_id != -1);
    return wu;
}

WorkUnit TaskGraph::getNextWorkUnitInner() {
    // Helper function to get the next unit of work to be done, if any
    // Returns wu with task_id of -1 if no work to do.
    // Assumes lock is in place.

    WorkUnit wu;

    if (_ready_tasks.empty()) {
        // No work to do: Return sentinel
        wu.task_id = -1;
    } else {
        // Get next task ID
        int task_id = _ready_tasks.front();
        WorkUnit& tg_wu = _task_graph[task_id].work_unit;

        // Setup work unit
        wu.task_id = task_id;
        wu.subtask_id = tg_wu.subtask_id;
        wu.num_total_tasks = tg_wu.num_total_tasks;
        wu.runnable = tg_wu.runnable;

        // Increment graph subtask_id
        tg_wu.subtask_id++;

        // Pop task from ready tasks if we just assigned the last subtask
        if (tg_wu.subtask_id == tg_wu.num_total_tasks) {
            _ready_tasks.pop_front();
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

void TaskGraph::shutdown() {
    // Called by main to put graph into shutdown state
    // Ensures queue is empty, then adds sentinel to work queue
    // and broadcasts to all threads

    // Ensure queue is empty
    this->blockUntilEmpty();

    // Setup special sentinel work unit
    // Thread workers exit when they receive this task ID
    WorkUnit wu_shutdown;
    wu_shutdown.task_id = -2;
    wu_shutdown.subtask_id = 0;
    wu_shutdown.num_total_tasks = 10000;

    // <CRITICAL_SECTION>
    pthread_mutex_lock(&_tg_lock);

    // Add sentinel to work queue, broadcast to all threads
    _ready_tasks.push_back(-2);
    pthread_cond_broadcast(&_task_received);

    pthread_mutex_unlock(&_tg_lock);
    // </CRITICAL_SECTION>
}







/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

void* threadWorkerB(void *args) {
    TaskGraph *tg = (TaskGraph *) args;
    WorkUnit wu;
    wu.task_id = -1;

    while (true) {
        if (wu.task_id == -1) {
            // Get the next work unit
            wu = tg->getNextWorkUnit();
        } else if (wu.task_id == -2) {
            // Received done sentinel, exit
            // printf("threadWorkerB: Received done sentinel, exiting\n");
            break;
        } else {
            // Run the task and mark it complete
            wu.runnable->runTask(wu.subtask_id, wu.num_total_tasks);
            wu = tg->markComplete(wu.task_id, wu.subtask_id);
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

    // _args = (TaskArgsA3 *) malloc(_num_threads * sizeof(TaskArgsA3));

    // PThread create
    for (int i = 0; i < _num_threads; i++) {
        pthread_create(&_thread_pool[i], NULL, threadWorkerB, (void *) &_tg);
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
    _tg.shutdown();

    // Join threads
    for (int i = 0; i < _num_threads; i++) {
        pthread_join(_thread_pool[i], NULL);
    }

    // Free memory
    free(_thread_pool);

    // printf("TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping: Freeing thread pool\n");
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {


    //
    // TODO: CS149 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    _tg.addTask(runnable, num_total_tasks, std::vector<TaskID>());
    _tg.blockUntilEmpty();
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {


    //
    // TODO: CS149 students will implement this method in Part B.
    //

    // Add task to graph
    _tg.addTask(runnable, num_total_tasks, deps);

    return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync() {

    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //

    // Block until all tasks are done
    _tg.blockUntilEmpty();

    return;
}
