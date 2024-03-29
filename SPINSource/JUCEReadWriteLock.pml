#include "primitives.pml"

typedef rwlock {
  critical_section access_lock;
  waitable_event wait_event;
  byte num_waiting_writers;
  byte num_writers;
  byte num_readers;
}

inline enter_read(rwl) {
  printf("Entering read (pid = %d).\n", _pid);

  enter_critical_section(rwl.access_lock);

  do
  :: rwl.num_writers + rwl.num_waiting_writers == 0
  -> rwl.num_readers ++;
     break;
  :: else
  -> exit_critical_section(rwl.access_lock);
     wait(rwl.wait_event);
     enter_critical_section(rwl.access_lock);
  od;
  
  exit_critical_section(rwl.access_lock);
}

inline exit_read(rwl) {
  enter_critical_section(rwl.access_lock);
  rwl.num_readers --;
  if
  :: rwl.num_readers == 0
  -> signal(rwl.wait_event);
  :: else -> skip;
  fi;
  exit_critical_section(rwl.access_lock);
  printf("Read exited.\n");
}

inline enter_write(rwl) {
  printf("Entering write (pid = %d).\n", _pid);
  enter_critical_section(rwl.access_lock);

  do
  :: rwl.num_readers + rwl.num_writers == 0
  -> rwl.num_writers ++;
     break;
  :: else
  -> rwl.num_waiting_writers ++;
     exit_critical_section(rwl.access_lock);
     wait(rwl.wait_event);
     enter_critical_section(rwl.access_lock);
     rwl.num_waiting_writers --;
  od;
  
  exit_critical_section(rwl.access_lock);
}

inline exit_write(rwl) {
  enter_critical_section(rwl.access_lock);
  rwl.num_writers --;
  if
  :: rwl.num_writers == 0
  -> signal(rwl.wait_event);
  :: else -> skip;
  fi;
  exit_critical_section(rwl.access_lock);
  printf("Write exited.\n");
}

#include "testcase.pml"
