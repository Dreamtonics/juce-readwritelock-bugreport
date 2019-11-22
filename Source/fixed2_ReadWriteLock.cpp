/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2017 - ROLI Ltd.

   JUCE is an open source library subject to commercial or open-source
   licensing.

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

#include "fixed2_ReadWriteLock.h"

FixedReadWriteLock2::FixedReadWriteLock2() noexcept
{
    readerThreads.ensureStorageAllocated (16);
}

FixedReadWriteLock2::~FixedReadWriteLock2() noexcept
{
    jassert (readerThreads.size() == 0);
    jassert (numWriters == 0);
}

//==============================================================================
void FixedReadWriteLock2::enterRead() const noexcept
{
    while (! tryEnterRead())
        waitEventRead.wait (-1);
}

bool FixedReadWriteLock2::tryEnterRead() const noexcept
{
    const Thread::ThreadID threadId = Thread::getCurrentThreadId();

    const SpinLock::ScopedLockType sl (accessLock);

    for (int i = 0; i < readerThreads.size(); ++i)
    {
        ThreadRecursionCount& trc = readerThreads.getReference(i);

        if (trc.threadID == threadId)
        {
            trc.count++;
            return true;
        }
    }

    if (numWriters + numWaitingWriters == 0
         || (threadId == writerThreadId && numWriters > 0))
    {
        ThreadRecursionCount trc = { threadId, 1 };
        readerThreads.add (trc);
        return true;
    }

    return false;
}

void FixedReadWriteLock2::exitRead() const noexcept
{
    const Thread::ThreadID threadId = Thread::getCurrentThreadId();
    const SpinLock::ScopedLockType sl (accessLock);

    for (int i = 0; i < readerThreads.size(); ++i)
    {
        ThreadRecursionCount& trc = readerThreads.getReference(i);

        if (trc.threadID == threadId)
        {
            if (--(trc.count) == 0)
            {
                readerThreads.remove (i);
                waitEventRead.signal();
                waitEventWrite.signal();
            }

            return;
        }
    }

    jassertfalse; // unlocking a lock that wasn't locked..
}

//==============================================================================
void FixedReadWriteLock2::enterWrite() const noexcept
{
    const Thread::ThreadID threadId = Thread::getCurrentThreadId();
    const SpinLock::ScopedLockType sl (accessLock);

    while (! tryEnterWriteInternal (threadId))
    {
        ++numWaitingWriters;
        accessLock.exit();
        waitEventWrite.wait (-1);
        accessLock.enter();
        --numWaitingWriters;
    }
}

bool FixedReadWriteLock2::tryEnterWrite() const noexcept
{
    const SpinLock::ScopedLockType sl (accessLock);
    return tryEnterWriteInternal (Thread::getCurrentThreadId());
}

bool FixedReadWriteLock2::tryEnterWriteInternal (Thread::ThreadID threadId) const noexcept
{
    if (readerThreads.size() + numWriters == 0
         || threadId == writerThreadId
         || (readerThreads.size() == 1 && readerThreads.getReference(0).threadID == threadId))
    {
        writerThreadId = threadId;
        ++numWriters;
        return true;
    }

    return false;
}

void FixedReadWriteLock2::exitWrite() const noexcept
{
    const SpinLock::ScopedLockType sl (accessLock);

    // check this thread actually had the lock..
    jassert (numWriters > 0 && writerThreadId == Thread::getCurrentThreadId());

    if (--numWriters == 0)
    {
        writerThreadId = {};
        waitEventRead.signal();
        waitEventWrite.signal();
    }
}

