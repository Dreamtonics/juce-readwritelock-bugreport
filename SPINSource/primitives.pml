typedef critical_section {
  bit set;
}

inline enter_critical_section(cs) {
  atomic {
    ! cs.set
    -> cs.set = true;
  }
}

inline exit_critical_section(cs) {
  cs.set = false;
}

bit proc_wake_flag;

inline wait() {
  printf("Waiting (pid = %d)\n", _pid);
  atomic {
    (proc_wake_flag)
    -> proc_wake_flag = false;
  }
}

inline signal() {
  proc_wake_flag = true;
}
