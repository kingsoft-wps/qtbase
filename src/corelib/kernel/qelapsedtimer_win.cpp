/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qelapsedtimer.h"
#include "qdeadlinetimer.h"
#include "qdeadlinetimer_p.h"
#include <qt_windows.h>

typedef ULONGLONG (WINAPI *PtrGetTickCount64)(void);
#if defined(Q_OS_WINRT)
    static const PtrGetTickCount64 ptrGetTickCount64 = &GetTickCount64;
#else
    static PtrGetTickCount64 ptrGetTickCount64 = 0;
#endif

QT_BEGIN_NAMESPACE

// Result of QueryPerformanceFrequency, 0 indicates that the high resolution timer is unavailable
static quint64 counterFrequency = 0;

static void resolveLibs()
{
    static bool done = false;
    if (done)
        return;

#if !defined(Q_OS_WINRT)
    // try to get GetTickCount64 from the system
    HMODULE kernel32 = GetModuleHandleW(L"kernel32");
    if (!kernel32)
        return;
    ptrGetTickCount64 = (PtrGetTickCount64)GetProcAddress(kernel32, "GetTickCount64");
#endif // !Q_OS_WINRT

    // Retrieve the number of high-resolution performance counter ticks per second
    LARGE_INTEGER frequency;
    if (!QueryPerformanceFrequency(&frequency)) {
        qFatal("QueryPerformanceFrequency failed, even though Microsoft documentation promises it wouldn't.");
        counterFrequency = 0;
    } else {
        counterFrequency = frequency.QuadPart;
    }

    done = true;
}

static inline qint64 ticksToNanoseconds(qint64 ticks)
{
    if (counterFrequency > 0) {
        // QueryPerformanceCounter uses an arbitrary frequency
        qint64 seconds = ticks / counterFrequency;
        qint64 nanoSeconds = (ticks - seconds * counterFrequency) * 1000000000 / counterFrequency;
        return seconds * 1000000000 + nanoSeconds;
    }
    // GetTickCount(64) returns milliseconds
    return ticks * 1000000;
}


static quint64 getTickCount()
{
    resolveLibs();

    // This avoids a division by zero and disables the high performance counter if it's not available
    if (counterFrequency > 0) {
        LARGE_INTEGER counter;

        bool ok = QueryPerformanceCounter(&counter);
        Q_ASSERT_X(ok, "QElapsedTimer::start()",
                   "QueryPerformanceCounter failed, although QueryPerformanceFrequency succeeded.");
        Q_UNUSED(ok);
        return counter.QuadPart;
    }

#ifndef Q_OS_WINRT
    if (ptrGetTickCount64)
        return ptrGetTickCount64();

    static quint32 highdword = 0;
    static quint32 lastval = 0;
    quint32 val = GetTickCount();
    if (val < lastval)
        ++highdword;
    lastval = val;
    return val | (quint64(highdword) << 32);
#else // !Q_OS_WINRT
    // ptrGetTickCount64 is always set on WinRT but GetTickCount is not available
    return GetTickCount64();
#endif // Q_OS_WINRT
}

quint64 qt_msectime()
{
    return ticksToNanoseconds(getTickCount()) / 1000000;
}

QElapsedTimer::ClockType QElapsedTimer::clockType() Q_DECL_NOTHROW
{
    resolveLibs();

    return counterFrequency > 0 ? PerformanceCounter : TickCounter;
}

bool QElapsedTimer::isMonotonic() Q_DECL_NOTHROW
{
    return true;
}

void QElapsedTimer::start() Q_DECL_NOTHROW
{
    t1 = getTickCount();
    t2 = 0;
}

qint64 QElapsedTimer::restart() Q_DECL_NOTHROW
{
    qint64 oldt1 = t1;
    t1 = getTickCount();
    t2 = 0;
    return ticksToNanoseconds(t1 - oldt1) / 1000000;
}

qint64 QElapsedTimer::nsecsElapsed() const Q_DECL_NOTHROW
{
    qint64 elapsed = getTickCount() - t1;
    return ticksToNanoseconds(elapsed);
}

qint64 QElapsedTimer::elapsed() const Q_DECL_NOTHROW
{
    qint64 elapsed = getTickCount() - t1;
    return ticksToNanoseconds(elapsed) / 1000000;
}

qint64 QElapsedTimer::msecsSinceReference() const Q_DECL_NOTHROW
{
    return ticksToNanoseconds(t1) / 1000000;
}

qint64 QElapsedTimer::msecsTo(const QElapsedTimer &other) const Q_DECL_NOTHROW
{
    qint64 difference = other.t1 - t1;
    return ticksToNanoseconds(difference) / 1000000;
}

qint64 QElapsedTimer::secsTo(const QElapsedTimer &other) const Q_DECL_NOTHROW
{
    return msecsTo(other) / 1000;
}

bool operator<(const QElapsedTimer &v1, const QElapsedTimer &v2) Q_DECL_NOTHROW
{
    return (v1.t1 - v2.t1) < 0;
}

QDeadlineTimer QDeadlineTimer::current(Qt::TimerType timerType) Q_DECL_NOTHROW
{
    Q_STATIC_ASSERT(!QDeadlineTimerNanosecondsInT2);
    QDeadlineTimer result;
    result.t1 = ticksToNanoseconds(getTickCount());
    result.type = timerType;
    return result;
}

QT_END_NAMESPACE
