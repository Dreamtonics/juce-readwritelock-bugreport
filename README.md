Report: fixing race condition in ReadWriteLock (JUCE)
===

This repository demonstrates that `ReadWriteLock` from JUCE (C++ library for cross-platform Audio applications) has a bug that causes ~100 ms delay in `enterRead()` and `enterWrite()` due to an unwoken `WaitableEvent`. C++ source code for replicating the bug is provided along side two suggested fixes. This repository also documents the use of formal methods to discover the bug, alongside a more human-understandable explanation of the failure case.

This bug affects all JUCE versions and is platform-independent. It has not been discovered probably due to the `WaitableEvent`'s 100 millisecond timeout.

Replication
---

* `Source/juce_ReadWriteLock.cpp` is a copy of JUCE `ReadWriteLock` with the `wait(100)` in `enterRead()` and `enterWrite()` changed to `wait(-1)` to disable the timeout.

* `Source/fixed_ReadWriteLock.cpp` is a bug fix with the same timeout-disabled wait. This bug fix works by removing `ReadWriteLock::numWaitingWriters`.

* `Source/fixed2_ReadWriteLock.cpp` is an alternative bug fix solution. This one works by adding another `WaitableEvent` to `ReadWriteLock`. Now there are two `WaitableEvent`s: one for read and another for write.

Build the JUCE console application, then run
```
$ ./RWLockTest
JUCE v5.4.5
Usage: RWLockTest num-contenders num-trials rwlock-version
num-contenders: number of contending reader/writers.
num-trials: number of runs for the stress test.
rwlock-version: "buggy": use JUCE ReadWriteLock;
                "fixed": use the first bug-fixed version.
                "fixed2": use the second bug-fixed version.
```
The tool is a stress test of spawning many contending readers and writes on the same `ReadWriteLock`. Arguments are self-explanatory.

```sh
$ ./RWLockTest 2 100 buggy
JUCE v5.4.5
0	elapsed time = 20.962 ms.
1	elapsed time = 20.922 ms.
2	elapsed time = 16.678 ms.
# stops here
```

Note: this is just an example, the actual number of trials it takes to stall is random. To increase the probability of stall, increase `num-contenders`.

Here is what happens with the fixed version:

```sh
$ ./RWLockTest 2 100 fixed # or ./RWLockTest 2 100 fixed2
JUCE v5.4.5
0	elapsed time = 16.833 ms.
1	elapsed time = 20.988 ms.
2	elapsed time = 16.667 ms.
3	elapsed time = 16.718 ms.
4	elapsed time = 20.899 ms.
5	elapsed time = 16.814 ms.
...
96	elapsed time = 20.873 ms.
97	elapsed time = 20.943 ms.
98	elapsed time = 20.873 ms.
99	elapsed time = 16.789 ms.
```

Verifying ReadWriteLock using SPIN
---

> [Spin](http://spinroot.com/spin/whatispin.html) is a popular open-source software verification tool, used by thousands of people worldwide. The tool can be used for the formal verification of multi-threaded software applications. The tool was developed at Bell Labs in the Unix group of the Computing Sciences Research Center, starting in 1980.

To readers unfamiliar with model checking techniques, SPIN performs an exhaustive search on **all** possible interleavings of "instructions" from different threads and try to find errors.

Run-time thread safety checkers such as Clang's ThreadSanitizer only detects data race (not all race conditions, for example deadlocks) and only do so on one such interleaving for each run of the program. In other words, TSan can miss a lot of cases and hence false negatives.

A full-state check in SPIN can prove the mathematical correctness of a concurrency design, albeit with the downside that such an exhaustive search is often computationally intractable. The developer also needs to take great care of correctly re-implementing the program in SPIN's own domain-specific language, Promela.

`SPINSource/JUCEReadWriteLock.pml` is a Promela implementation of JUCE `ReadWriteLock` with several simplifications:
* We only consider the case with one `ReadWriteLock` (and hence one globally shared `WaitableEvent`).
* We do not consider `tryEnterRead()` and `tryEnterWrite()`.
* The `ReadWriteLock` is no longer recursive (which means you can't enter read/write within a read/write scope).
   * Thanks to this simplification we can downgrade `ReadWriteLock::readerThreads` from an array to a simple `num_readers` counter.

`SPINSource/testcase.pml` defines a test with two pairs of concurrent readers and writers. So there are four threads trying to acquire and then release the same lock. This is the minmal setup to replicate the bug. You can change `NUM_CONTENDERS` macro to test cases with more readers and writers, although it gets exponentially more time and memory consuming. For example, when `NUM_CONTENDERS = 4` it consumes 2.7 GB memory and takes 30 seconds on an i7-8700K.

Run the following command to simulate the testcase for once using the JUCE `ReadWriteLock`.
```
$ spin -run JUCEReadWriteLock.pml 
pan:1: invalid end state (at depth 60)
pan: wrote JUCEReadWriteLock.pml.trail

(Spin Version 6.5.0 -- 1 July 2019)
Warning: Search not completed
	+ Partial Order Reduction

Full statespace search for:
	never claim         	- (none specified)
	assertion violations	+
	cycle checks       	- (disabled by -DSAFETY)
	invalid end states	+

State-vector 52 byte, depth reached 76, errors: 1
      800 states, stored
      416 states, matched
     1216 transitions (= stored+matched)
        0 atomic steps
hash conflicts:         0 (resolved)

Stats on memory usage (in Megabytes):
    0.061	equivalent memory usage for states (stored*(State-vector + overhead))
    0.279	actual memory usage for states
  128.000	memory used for hash table (-w24)
    0.534	memory used for DFS stack (-m10000)
  128.730	total actual memory usage

pan: elapsed time 0 seconds
```

SPIN detects an error almost immediately: the program gets stuck.

A trail file describing the path of execution to replicate the problem is generated. Use `spin -t` to inspect the error by running the program based on the trail file.
```
$ spin -t JUCEReadWriteLock.pml 
          Entering write (pid = 1).
              Entering read (pid = 2).
                  Entering write (pid = 3).
                      Entering read (pid = 4).
                      Reading (counter = 1)
                      Read exited.
                  Writing
              Waiting (pid = 2)
              Waiting (pid = 2)
          Waiting (pid = 1)
                  Write exited.
              Waiting (pid = 2)
spin: trail ends after 61 steps
#processes: 3
		proc_wake_flag = 0
		test_rwlock.access_lock.set = 0
		test_rwlock.num_waiting_writers = 1
		test_rwlock.num_writers = 0
		test_rwlock.num_readers = 0
		num_finished_rw_procs = 2
 61:	proc  2 (proc_read_block:1) primitives.pml:20 (state 15)
 61:	proc  1 (proc_write_block:1) primitives.pml:20 (state 16)
 61:	proc  0 (:init::1) testcase.pml:41 (state 11)
5 processes created
```

The last few lines beginning with `61:` indicate that the program halts with
* process 2 (reader thread) at `primitives.pml` line 20
* process 1 (writer thread) at `primitives.pml` line 20
* process 0 (main) at `testcase.pml` line 41

`primitives.pml` line 20 is part of the `WaitableEvent::wait()` implementation.

We can also see that in the final state, `num_waiting_writers` is set to 1.

Use `spin -t -v` to show detailed execution path. Because the output is very long, only interesting lines are shown below.
```
 spin -t -v JUCEReadWriteLock.pml 
using statement merging
...
                      Read exited.
 26:	proc  4 (proc_read_block:1) JUCEReadWriteLock.pml:37 (state 43)	[printf('Read exited.\\n')]
...
 30:	proc  3 (proc_write_block:1) JUCEReadWriteLock.pml:45 (state 6)	[(((test_rwlock.num_readers+test_rwlock.num_writers)==0))]
 31:	proc  3 (proc_write_block:1) JUCEReadWriteLock.pml:46 (state 7)	[test_rwlock.num_writers = (test_rwlock.num_writers+1)]
...
 36:	proc  2 (proc_read_block:1) JUCEReadWriteLock.pml:19 (state 9)	[else]
              Waiting (pid = 2)
...
 38:	proc  2 (proc_read_block:1) primitives.pml:19 (state 12)	[printf('Waiting (pid = %d)\\n',_pid)]
 39:	proc  2 (proc_read_block:1) primitives.pml:21 (state 13)	[(proc_wake_flag)]	<merge 0 now @14>
 39:	proc  2 (proc_read_block:1) primitives.pml:22 (state 14)	[proc_wake_flag = 0]
...
 41:	proc  2 (proc_read_block:1) JUCEReadWriteLock.pml:19 (state 9)	[else]
...
              Waiting (pid = 2)
 43:	proc  2 (proc_read_block:1) primitives.pml:19 (state 12)	[printf('Waiting (pid = %d)\\n',_pid)]
...
 45:	proc  1 (proc_write_block:1) JUCEReadWriteLock.pml:48 (state 9)	[else]
 46:	proc  1 (proc_write_block:1) JUCEReadWriteLock.pml:49 (state 10)	[test_rwlock.num_waiting_writers = (test_rwlock.num_waiting_writers+1)]
          Waiting (pid = 1)
...
 48:	proc  1 (proc_write_block:1) primitives.pml:19 (state 13)	[printf('Waiting (pid = %d)\\n',_pid)]
...
 50:	proc  3 (proc_write_block:1) JUCEReadWriteLock.pml:61 (state 35)	[test_rwlock.num_writers = (test_rwlock.num_writers-1)]
 51:	proc  3 (proc_write_block:1) JUCEReadWriteLock.pml:63 (state 36)	[((test_rwlock.num_writers==0))]
 52:	proc  3 (proc_write_block:1) primitives.pml:27 (state 37)	[proc_wake_flag = 1]
...
                  Write exited.
 54:	proc  3 (proc_write_block:1) JUCEReadWriteLock.pml:68 (state 45)	[printf('Write exited.\\n')]
...
 57:	proc  2 (proc_read_block:1) primitives.pml:21 (state 13)	[(proc_wake_flag)]	<merge 0 now @14>
 57:	proc  2 (proc_read_block:1) primitives.pml:22 (state 14)	[proc_wake_flag = 0]
...
 59:	proc  2 (proc_read_block:1) JUCEReadWriteLock.pml:19 (state 9)	[else]
...
              Waiting (pid = 2)
 61:	proc  2 (proc_read_block:1) primitives.pml:19 (state 12)	[printf('Waiting (pid = %d)\\n',_pid)]
spin: trail ends after 61 steps
```

To explain what's going on in a more human-readable manner:

1. (step 30) Writer (proc 3) tries to acquire the lock. At this time, both `num_readers` and `num_waiting_writers` are zero. The acquisation is successful and Writer (proc 3) starts writing.
2. (step 45) Writer (proc 1) tries to acquire the lock but finds that `num_writers` is non-zero. It then increments `num_waiting_writers` to 1 and starts waiting.
3. (step 50) Writer (proc 3) finishes so it decrements `num_writers` to 0 and sends a signal.
4. (step 57) Reader (proc 2) intercepts the signal and wakes up.
5. (step 59) Reader (proc 2) then tries to acquire the lock again, but finds that `num_waiting_writers` is 1. This is because in step 45 Writer (proc 1) entered waiting state.
6. Now Reader (proc 2) is waiting for `num_waiting_writers` to return to zero while Writer (proc 1) has its conditions met but **there's no one to wake it up**.

It then becomes easy to see that to fix the bug, either remove the waiting writers counter or to let readers and writers use different `WaitableEvent`.

```
$ spin -run FixedReadWriteLock.pml

(Spin Version 6.5.0 -- 1 July 2019)
	+ Partial Order Reduction

Full statespace search for:
	never claim         	- (none specified)
	assertion violations	+
	cycle checks       	- (disabled by -DSAFETY)
	invalid end states	+

State-vector 60 byte, depth reached 80, errors: 0
    13577 states, stored
    13050 states, matched
    26627 transitions (= stored+matched)
        0 atomic steps
hash conflicts:        34 (resolved)

Stats on memory usage (in Megabytes):
    1.139	equivalent memory usage for states (stored*(State-vector + overhead))
    0.963	actual memory usage for states (compression: 84.49%)
         	state-vector as stored = 46 byte + 28 byte overhead
  128.000	memory used for hash table (-w24)
    0.534	memory used for DFS stack (-m10000)
  129.413	total actual memory usage


unreached in proctype proc_read_block
	(0 of 47 states)
unreached in proctype proc_write_block
	(0 of 47 states)
unreached in init
	(0 of 12 states)

pan: elapsed time 0 seconds
```

```
$ spin -run FixedReadWriteLock2.pml

(Spin Version 6.5.0 -- 1 July 2019)
	+ Partial Order Reduction

Full statespace search for:
	never claim         	- (none specified)
	assertion violations	+
	cycle checks       	- (disabled by -DSAFETY)
	invalid end states	+

State-vector 60 byte, depth reached 90, errors: 0
    22751 states, stored
    22180 states, matched
    44931 transitions (= stored+matched)
        0 atomic steps
hash conflicts:         9 (resolved)

Stats on memory usage (in Megabytes):
    1.909	equivalent memory usage for states (stored*(State-vector + overhead))
    1.450	actual memory usage for states (compression: 75.96%)
         	state-vector as stored = 39 byte + 28 byte overhead
  128.000	memory used for hash table (-w24)
    0.534	memory used for DFS stack (-m10000)
  129.901	total actual memory usage


unreached in proctype proc_read_block
	(0 of 49 states)
unreached in proctype proc_write_block
	(0 of 51 states)
unreached in init
	(0 of 12 states)

pan: elapsed time 0.01 seconds
```
