#include "../JuceLibraryCode/JuceHeader.h"
#include "juce_ReadWriteLock.h"
#include "fixed_ReadWriteLock.h"

using namespace juce;

JUCEReadWriteLock buggyLock;
FixedReadWriteLock healthyLock;

std::atomic<int> numDuringRead;
std::atomic<int> numDuringWrite;

class TestBuggyReadThread : public Thread {
public:
  TestBuggyReadThread() : Thread("TestBuggyReadThread") {}

  void run() override {
    buggyLock.enterRead();
    numDuringRead ++;
    
    jassert(numDuringWrite == 0);

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

    numDuringWrite --;
    healthyLock.exitWrite();
  }
};

int main(int argc, char* argv[]) {
  if(argc != 4) {
    std::cout << "Usage: RWLockTest num-contenders num-trials buggy-or-healthy"
              << std::endl;
    std::cout << "num-contenders: number of contending reader/writers." << std::endl;
    std::cout << "num-trials: number of runs for the stress test." << std::endl;
    std::cout << "buggy-or-healthy: if \"buggy\", use JUCE ReadWriteLock;" << std::endl
              << "                  otherwise, use the bug-fixed version." << std::endl;
    return 1;
  }
  
  int numContenders = std::atoi(argv[1]);
  int numTrials = std::atoi(argv[2]);
  bool useBuggyRWLock = ! std::strcmp("buggy", argv[3]);
  
  OwnedArray<Thread> testThreads;
  for(int i = 0; i < numContenders; i ++) {
    if(useBuggyRWLock) {
      testThreads.add(new TestBuggyReadThread());
      testThreads.add(new TestBuggyWriteThread());
    } else {
      testThreads.add(new TestHealthyReadThread());
      testThreads.add(new TestHealthyWriteThread());
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
    if(i % 10 == 0)
      DBG(i);
  }
  
  return 0;
}
