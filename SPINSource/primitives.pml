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

typedef waitable_event {
  bit wake_flag;
}

inline wait(e) {
  printf("Waiting (pid = %d)\n", _pid);
  atomic {
    (e.wake_flag)
    -> e.wake_flag = false;
  }
}

inline signal(e) {
  e.wake_flag = true;
}
