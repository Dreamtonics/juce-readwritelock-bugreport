#include "../JuceLibraryCode/JuceHeader.h"
#include "juce_ReadWriteLock.h"
#include "fixed_ReadWriteLock.h"
#include "fixed2_ReadWriteLock.h"

using namespace juce;

JUCEReadWriteLock buggyLock;
FixedReadWriteLock healthyLock;
FixedReadWriteLock2 healthyLock2;

std::atomic<int> numDuringRead;
std::atomic<int> numDuringWrite;

bool withDummyWorkload{true};

class TestBuggyReadThread : public Thread {
public:
  TestBuggyReadThread() : Thread("TestBuggyReadThread") {}

  void run() override {
    buggyLock.enterRead();
    numDuringRead ++;
    
    jassert(numDuringWrite == 0);
    if(withDummyWorkload) Thread::sleep(5);

    numDuringRead --;
    buggyLock.exitRead();
  }
};

class TestBuggyWriteThread : public Thread {
public:
  TestBuggyWriteThread() : Thread("TestBuggyWriteThread") {}

  void run() override {
    buggyLock.enterWrite();
    numDuringWrite ++;
    
    jassert(numDuringRead == 0 && numDuringWrite == 1);
    if(withDummyWorkload) Thread::sleep(5);

    numDuringWrite --;
    buggyLock.exitWrite();
  }
};

class TestHealthyReadThread : public Thread {
public:
  TestHealthyReadThread() : Thread("TestHealthyReadThread") {}

  void run() override {
    healthyLock.enterRead();
    numDuringRead ++;
    
    jassert(numDuringWrite == 0);
    if(withDummyWorkload) Thread::sleep(5);

    numDuringRead --;
    healthyLock.exitRead();
  }
};

class TestHealthyWriteThread : public Thread {
public:
  TestHealthyWriteThread() : Thread("TestHealthyWriteThread") {}

  void run() override {
    healthyLock.enterWrite();
    numDuringWrite ++;
    
    jassert(numDuringRead == 0 && numDuringWrite == 1);
    if(withDummyWorkload) Thread::sleep(5);

    numDuringWrite --;
    healthyLock.exitWrite();
  }
};

class TestHealthyReadThread2 : public Thread {
public:
  TestHealthyReadThread2() : Thread("TestHealthyReadThread2") {}

  void run() override {
    healthyLock2.enterRead();
    numDuringRead ++;
    
    jassert(numDuringWrite == 0);
    if(withDummyWorkload) Thread::sleep(5);

    numDuringRead --;
    healthyLock2.exitRead();
  }
};

class TestHealthyWriteThread2 : public Thread {
public:
  TestHealthyWriteThread2() : Thread("TestHealthyWriteThread2") {}

  void run() override {
    healthyLock2.enterWrite();
    numDuringWrite ++;
    
    jassert(numDuringRead == 0 && numDuringWrite == 1);
    if(withDummyWorkload) Thread::sleep(5);

    numDuringWrite --;
    healthyLock2.exitWrite();
  }
};

int main(int argc, char* argv[]) {
  if(argc != 4) {
    std::cout << "Usage: RWLockTest num-contenders num-trials rwlock-version"
              << std::endl;
    std::cout << "num-contenders: number of contending reader/writers." << std::endl;
    std::cout << "num-trials: number of runs for the stress test." << std::endl;
    std::cout << "rwlock-version: \"buggy\": use JUCE ReadWriteLock;" << std::endl
              << "                \"fixed\": use the first bug-fixed version." << std::endl
              << "                \"fixed2\": use the second bug-fixed version." << std::endl;
    return 1;
  }
  
  int numContenders = std::atoi(argv[1]);
  int numTrials = std::atoi(argv[2]);
  
  OwnedArray<Thread> testThreads;
  for(int i = 0; i < numContenders; i ++) {
    if(! std::strcmp("buggy", argv[3])) {
      testThreads.add(new TestBuggyReadThread());
      testThreads.add(new TestBuggyWriteThread());
    } else
    if(! std::strcmp("fixed", argv[3])) {
      testThreads.add(new TestHealthyReadThread());
      testThreads.add(new TestHealthyWriteThread());
    } else
    if(! std::strcmp("fixed2", argv[3])) {
      testThreads.add(new TestHealthyReadThread2());
      testThreads.add(new TestHealthyWriteThread2());
    }
  }
  
  for(int i = 0; i < numTrials; i ++) {
    double tSec;
    {
      ScopedTimeMeasurement measure(tSec);
      for(auto* t : testThreads)
        t -> startThread();

      for(auto* t : testThreads)
        t -> waitForThreadToExit(-1);
    }
    DBG(i << "\telapsed time = " << tSec * 1000.0 << " ms.");
  }
  
  return 0;
}
