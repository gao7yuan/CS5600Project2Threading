# Project 2 Questions

## Scheduling

**Two tests have infinite loops with the scheduler you were provided. Explain
why `Running.PriorityOrder` will never terminate with the basic scheduler.**

- Function `nextThreadToRun()` is called by the simulator every tick to get the next thread to run.
- Currently, `readyList` is simply a FIFO queue, and `nextThreadToRun()` simply looks for the first non-terminated
thread in the queue.
- When function `createAndSetThreadToRun()` is called, a new thread is created and added to `readyList`.
- Currently, without priority scheduling and priority donation implemented, according to `TEST(Running, PriorityOrder)`,
`NAME_LO_PRI` will be added to the readyList first and executed every tick until it is finished. However, when
executing function `recordThreadPriority()`, this thread will be stuck forever in the while loop, since `NAME_MID_PRI`
and `NAME_HI_PRI` will never have a chance to be added to new threads and thus `infoList->cont` will always be `false`.
- As a result, this function never terminates.

## Sleep

**Briefly describe what happens in your `sleep(...)` function.**

1. Record the tick when the function is called as the tick that the sleep is supposed to start - `startTick`.
2. Calculate the tick that the sleep is going to end - `wakeTick`.
3. Call `stopExecutingThreadForCycle()` until current tick has reached `wakeTick`.
4. Add this thread to `sleepThreadMap` and `sleepList` and remove it from `readyList`.
5. Return `startTick`.

**How does your `sleep(...)` function guarantee duration?**

What prevents a thread from waking up too early?
- The threads that are supposed to sleep are removed from `readyList` and kept in `sleepList`. They are never moved
back to `readyList` until they have reached their `wakeTick`.

What prevents a thread from _never_ waking up?
- The threads that are supposed to wake up are removed from `sleepList` and `sleepThreadMap` and added back to
`readyList`.

**Do you have to check every sleeping thread every tick? Why not?**

- No. `sleepList` is essentially a priority queue. Each tick, we check the threads in `sleepList` with smallest
`wakeTick` and see if they should be waken up.

## Locking

**How do you ensure that the highest priority thread waiting for a lock gets it
first?**

- `readyList` is essentially a priority queue. When looking for the next thread to run, we look for the thread with
highest priority to make sure that it gets run next and gets the lock.
- If the lock is held by a low-priority thread, we do priority donation in function `lockAttempted()` and set the
priority of the low-priority thread the same as the high-priority thread.
- In `readyList`, when we look for the next thread to run, we also check the `originalPriority` field of threads
having the same priority, in order to make sure that we run the one just got priority donation first so that it releases
the lock.
- After the lock is released, we perform priority inversion on the low-priority thread in `lockReleased()` so that the
high-priority thread waiting for the lock jumps to the head of `readyList` and will eventually get the lock.

## Priority Donation

**Describe the sequence of events before and during a priority donation.**

1. When a thread tries to get the lock, firstly `lockAttempted()` is called.
2. Inside `lockAttempted()`, if a low-priority thread holds the lock, the high-priority thread donates its priority to
the low priority thread.
2. Therefore, the low-priority thread jumps to the head of `readyList` and will be the next thread to run.
3. After that, in `lockReleased()`, the low-priority thread releases the lock, and its priority gets set back to
its original value.
4. Therefore, the high-priority thread jumps to the head of `readyList` and is put into schedule, and gets the lock as
`lockAcquired()` is called.
5. The high-priority thread is run and will eventually release the lock after it terminates.

**How is nested donation handled?**

Describe a system of three threads (A, B, C) and three locks (1, 2, 3).

Say the priorities of A, B, C are 1, 5, 9, respectively. A is holding lock 1, B is holding lock 2 waiting for lock 1,
and C is holding lock 3 waiting for lock 2.

1. C runs first and tries to get lock 2.
2. C finds that B is holding lock 2 and has a lower priority, so C donates priority to B and B's priority becomes 9.
3. B runs and tries to get lock 1.
4. B finds that A is holding lock 1 and has a lower priority. So B donates priority to A and A's priority becomes 9
as well.
5. A runs because A's original priority is 1, which is smaller than that of B and C. A releases lock 1. Priority
inversion is performed on A, so A's priority drops back to 1.
6. B gets lock 1 and runs because B's original priority is 5, which is smaller than that of C. B releases lock 1 and
lock 2, and its priority drops back to 5.
7. C gets lock 2 and runs. C is eventually terminated and releases lock 2 and lock 3.

**How do you ensure that the highest priority thread waiting is called when the
lock is released?**

- `readyList` is essentially a priority queue. When looking for the next thread to run, we look for the thread with
highest priority to make sure that it gets run next and gets the lock.
- If the lock is held by a low-priority thread, we do priority donation in function `lockAttempted()` and set the
priority of the low-priority thread the same as the high-priority thread.
- In `readyList`, when we look for the next thread to run, we also check the `originalPriority` field of threads
having the same priority, in order to make sure that we run the one just got priority donation first so that it releases
the lock.
- After the lock is released, we perform priority inversion on the low-priority thread in `lockReleased()` so that the
high-priority thread waiting for the lock jumps to the head of `readyList` and will eventually get the lock.
