#include "primitives.pml"

typedef rwlock {
  critical_section access_lock;
  byte num_writers;
  byte num_readers;
}

inline enter_read(rwl) {
  printf("Entering read (pid = %d).\n", _pid);

  enter_critical_section(rwl.access_lock);

  do
  :: rwl.num_writers == 0
  -> rwl.num_readers ++;
     break;
  :: else
  -> exit_critical_section(rwl.access_lock);
     wait();
     enter_critical_section(rwl.access_lock);
  od;
  
  exit_critical_section(rwl.access_lock);
}

inline exit_read(rwl) {
  enter_critical_section(rwl.access_lock);
  rwl.num_readers --;
  signal();
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
  -> exit_critical_section(rwl.access_lock);
     wait();
     enter_critical_section(rwl.access_lock);
  od;
  
  exit_critical_section(rwl.access_lock);
}

inline exit_write(rwl) {
  enter_critical_section(rwl.access_lock);
  rwl.num_writers --;
  signal();
  exit_critical_section(rwl.access_lock);
  printf("Write exited.\n");
}

#include "testcase.pml"
