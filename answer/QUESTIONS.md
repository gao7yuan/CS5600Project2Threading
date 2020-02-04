# Project 2 Questions

## Directions

Answer each question to the best of your ability; feel free to copy sections of
your code or link to it if it helps explain but answers such as "this line
guarantees that the highest priority thread is woken first" are not sufficient.

Your answers can be relatively short - around 100 words, and no more than 200,
should suffice per subsection.

To submit, edit this file and replace every `ANSWER_BEGIN` to `ANSWER_END` with
your answer. Your submission should not include **any** instances of
`ANSWER_BEGIN` or `ANSWER_END`; the text provided is meant to give you some tips
on how to begin answering.

For one point, remember to delete this section (Directions) before submitting.

## Scheduling

**Two tests have infinite loops with the scheduler you were provided. Explain
why `Running.PriorityOrder` will never terminate with the basic scheduler.**

ANSWER_BEGIN

If this question doesn't make sense at all, take a look at `test_config.h`.

Turning on verbose logging and reading it will help answer this question.

- Function `nextThreadToRun()` is called by the simulator every tick to get the next thread to run.
- Currently, `readyList` is simply a FIFO queue, and `nextThreadToRun()` simply looks for the first non-terminated
thread in the queue.
- When function `createAndSetThreadToRun()` is called, a new thread is created and added to `readyList`.
- Currently, without priority scheduling and priority donation implemented, according to `TEST(Running, PriorityOrder)`,
`NAME_LO_PRI` will be added to the readyList first and executed every tick until it is finished. However, when
executing function `recordThreadPriority()`, this thread will be stuck forever in the while loop, since `NAME_MID_PRI`
and `NAME_HI_PRI` will never have a chance to be added to new threads and thus `infoList->cont` will always be false.
- As a result, this test never terminates.

ANSWER_END

## Sleep

**Briefly describe what happens in your `sleep(...)` function.**

ANSWER_BEGIN

1. Record the tick when the function is called as the tick that the sleep is supposed to start - `start_tick`.
2. Calculate the tick that the sleep is going to end - `end_tick`.
3. Move current thread from somewhere in `readyList` to the end of `readyList` and call `stopExecutingThreadForCycle()`.
4. Use a while loop to check whether current tick has reached `end_tick`.
If it has, exit the while loop. Otherwise, keep looping.
5. Return `start_tick`.

ANSWER_END

**How does your `sleep(...)` function guarantee duration?**

ANSWER_BEGIN

What prevents a thread from waking up too early?
- The sleeping threads can be placed at the end of `readyList` to avoid being woken up too early.

What prevents a thread from _never_ waking up?
- The thread that is supposed to be woken up can be placed at the beginning of `readyList`.

ANSWER_END

**Do you have to check every sleeping thread every tick? Why not?**

ANSWER_BEGIN

You shouldn't have to check every thread every tick.

You should be able to take advantage of knowing how long a thread will sleep
when `sleep(...)` is called.

- No. A map `Map<Thread*, void*>` where the value is a pointer to the end tick of a sleeping thread can be kept
to keep track of whether a thread should be woken up or not.

ANSWER_END

## Locking

**How do you ensure that the highest priority thread waiting for a lock gets it
first?**

ANSWER_BEGIN

This is similar to how you should be handling `sleep(...)`ing threads.

- Make sure that the threads waiting for a lock are sorted in descending order of priority. Therefore, when the lock is
released, the thread with highest priority will get the lock.

ANSWER_END

## Priority Donation

**Describe the sequence of events before and during a priority donation.**

ANSWER_BEGIN

Your answer should talk about the order of framework functions called and how
priority donation opportunities are detected.

1. When a thread tries to get the lock, firstly `lockAttempted()` is called.
2. Inside `lockAttempted()`, if a low priority thread holds the lock, the high priority thread donates
its priority to the low priority thread.
2. Therefore, the low priority thread is put into schedule and would finish executing.
3. After that, the lock is released as `lockReleased()` is called.
4. Now we perform priority inversion so that the high priority thread gets its priority back, and
therefore is put into schedule and gets the lock as `lockAcquired()` is called.
5. High priority thread is executed and will eventually release the lock after it finishes.

ANSWER_END

**How is nested donation handled?**

ANSWER_BEGIN

Describe a system of three threads (A, B, C) and three locks (1, 2, 3).

Say the priorities of A, B, C are 1, 5, 9, respectively. A is holding lock 1, B is holding lock 2 waiting for lock1,
and C is holding lock 3 waiting for lock 2.

1. C has the highest priority, so C runs first.
2. C finds that B is holding lock 2 and has a lower priority, so C donates priority to B and B's priority becomes 9.
3. B runs and finds that A is holding lock 1 and has a lower priority. So B donates priority to A and A's priority
becomes 9 as well.
4. A runs and terminates. A releases lock 1. A's priority drops back to 1.
5. B gets lock 1 and runs. After B terminates, it releases lock 1 and lock 2, and its priority drops back to 5.
6. C gets lock 2 and runs. After C terminates, it releases lock 2 and lock 3. All threads are finished.


ANSWER_END

**How do you ensure that the highest priority thread waiting is called when the
lock is released?**

ANSWER_BEGIN

Apply your previous answer to your code.
- Make sure that the threads waiting for a lock are sorted in descending order of priority. Therefore, when the lock is
released, the thread with highest priority will get the lock and will be called.

ANSWER_END
