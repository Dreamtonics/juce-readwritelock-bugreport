#define NUM_CONTENDERS 2

rwlock test_rwlock;
byte num_finished_rw_procs = 0;

inline read_block(rwl) {
  enter_read(rwl);

  printf("Reading (counter = %d)\n", rwl.num_readers);
  assert(rwl.num_writers == 0);
  
  exit_read(rwl);
}

inline write_block(rwl) {
  enter_write(rwl);

  printf("Writing\n");
  assert(rwl.num_readers == 0);
  
  exit_write(rwl);
}

proctype proc_read_block() {
  read_block(test_rwlock);
  num_finished_rw_procs ++;
}

proctype proc_write_block() {
  write_block(test_rwlock);
  num_finished_rw_procs ++;
}

init {
  byte i = 0;
  for(i : 0 .. NUM_CONTENDERS - 1) {
    run proc_write_block();
    run proc_read_block();
  }
  
  num_finished_rw_procs == NUM_CONTENDERS * 2;
}
