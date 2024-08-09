/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#include "qprintworker.h"
#include "qprintworker_p.h"
#include <QtCore/private/qthread_p.h>

#ifndef QT_NO_PRINTER

QT_BEGIN_NAMESPACE

Q_GLOBAL_STATIC(QPrintWorker, globalWorker);

struct QPrintTaskDeleter {
    static void cleanup(QPrintTask *task) {
        if (task)
            task->notifyRelease();
    }
};

QPrintTask::QPrintTask(TaskFunctor task, CompletionFunctor completion)
    : m_task(task)
    , m_completion(completion)
    , m_finished(false)
    , m_interrupted(false)
    , m_result(false)
    , m_semFinished(nullptr)
    , m_ref(0)
{
}

QPrintTask::~QPrintTask()
{
    if (m_semFinished)
        delete m_semFinished;
}

void QPrintTask::run()
{
    m_result = m_task();

    QMetaObject::invokeMethod(this, "notifyFinished", Qt::QueuedConnection);
    if (m_semFinished)
        m_semFinished->release();
}

bool QPrintTask::isFinished() const
{
    return m_finished;
}

void QPrintTask::interrupt()
{
    m_interrupted = true;
}

void QPrintTask::ensureBlockWaitSemaphore()
{
    if (!m_semFinished)
        m_semFinished = new QSemaphore;
}

bool QPrintTask::blockWait(int timeout) const
{
    if (m_semFinished)
        return m_semFinished->tryAcquire(1, timeout);
    return false;
}

int QPrintTask::addRef()
{
    if (autoDelete())
        m_ref.ref();

    return m_ref.load();
}

void QPrintTask::notifyRelease()
{
    if (autoDelete()) {
        if (!m_ref.deref())
            deleteLater();
    }
}

void QPrintTask::notifyFinished()
{
    if (QPrintWorker::instance()->isExceptionState())
        return;
    if (m_finished)
        return;
    m_finished = true;
    if (m_completion) {
        m_completion(m_result, m_interrupted);
    }

    emit finished();
}

QPrintWorkerThread::QPrintWorkerThread()
    : QThread()
{
}

QPrintWorkerThread::~QPrintWorkerThread()
{
}

void QPrintWorkerThread::addTask(QPrintTask *task)
{
    QMutexLocker locker(&m_mutex);
    if (task)
        task->addRef();
    m_tasks.enqueue(task);
    m_newTask.release(1);
}

void QPrintWorkerThread::dequeueTask(QPrintTask *task)
{
    QMutexLocker locker(&m_mutex);
    if (m_tasks.head() == task) {
        m_tasks.dequeue();
        QMetaObject::invokeMethod(task, "notifyRelease", Qt::QueuedConnection);
    }
}

bool QPrintWorkerThread::hasTask()
{
    QMutexLocker locker(&m_mutex);
    return !m_tasks.isEmpty();
}

void QPrintWorkerThread::run()
{
    while (!m_quit.load()) {
        m_newTask.acquire(1);
        QPrintTask *task = nullptr;

        {
            QMutexLocker locker(&m_mutex);
            if (!m_tasks.isEmpty()) {
                task = m_tasks.head();
            }
        }

        if (task) {
            task->run();
            dequeueTask(task);
        }
    }
}

void QPrintWorkerThread::requestQuit()
{
    m_quit.ref();
    addTask(nullptr);
    if (wait(5000)) {
        for (QPrintTask *t : m_tasks) {
            delete t;
        }
        m_tasks.clear();
        deleteLater();
    }
}

QPrintWorkerPrivate::QPrintWorkerPrivate()
    : m_blockWaitTime(0),
      m_exceptionState(false)
{
}

QPrintWorkerPrivate::~QPrintWorkerPrivate()
{
}

bool QPrintWorkerPrivate::runTask(QPrintTask *task, int timeout)
{
    task->addRef();
    QScopedPointer<QPrintTask, QPrintTaskDeleter> ptrTask(task);
    QPrintWorkerThread *thread = getWorkerThread();
    if (!thread)
        return false;

    const bool blockWait = (timeout > 0 && timeout <= m_blockWaitTime) ? true : false;
    if (blockWait)
        ptrTask->ensureBlockWaitSemaphore();

    thread->addTask(ptrTask.get());
    if (!timeout)
        return true;

    if (blockWait) {
        const bool success = ptrTask->blockWait(timeout);
        if (success)
            ptrTask->notifyFinished();
        else
            ptrTask->interrupt();
        return ptrTask->isFinished();
    }

    QObject::connect(ptrTask.get(), SIGNAL(finished()), &m_eventLoop, SLOT(quit()));

    if (timeout > 0) {
        m_timer.setSingleShot(true);
        QObject::connect(&m_timer, &QTimer::timeout, &m_eventLoop, [this]() { m_eventLoop.exit(1); });
        m_timer.start(timeout);
    }

    int code = m_eventLoop.exec();
    if (code)
        ptrTask->interrupt();

    m_timer.stop();
    QObject::disconnect(&m_timer, nullptr, &m_eventLoop, nullptr);
    QObject::disconnect(ptrTask.get(), nullptr, &m_eventLoop, nullptr);

    return ptrTask->isFinished();
}

QPrintWorkerThread *QPrintWorkerPrivate::getWorkerThread()
{
    static QPrintWorkerThread *workerThread = nullptr;
    if (!workerThread) {
        workerThread = new QPrintWorkerThread();

        QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [&]() {
            if (workerThread) {
                workerThread->requestQuit();
                workerThread = nullptr;
            }
        });
        workerThread->start();
    }

    return workerThread;
}


QPrintWorker::QPrintWorker() : d_ptr(new QPrintWorkerPrivate())
{
}

QPrintWorker::~QPrintWorker()
{
}

QPrintWorker *QPrintWorker::instance()
{
    return globalWorker();
}

bool QPrintWorker::runTask(TaskFunctor task, CompletionFunctor completion, int timeout)
{
    Q_D(QPrintWorker);
    return d->runTask(new QPrintTask(task, completion), timeout);
}

bool QPrintWorker::hasTask() const
{
    Q_D(const QPrintWorker);
    if (!d->getWorkerThread())
        return false;
    return d->getWorkerThread()->hasTask();
}

void QPrintWorker::exitWaiting()
{
    Q_D(QPrintWorker);
    if (d->m_eventLoop.isRunning())
        d->m_eventLoop.exit(1);
}

bool QPrintWorker::isWaiting() const
{
    Q_D(const QPrintWorker);
    return d->m_eventLoop.isRunning();
}

void QPrintWorker::setBlockWaitTime(int waitTime)
{
    Q_D(QPrintWorker);
    d->m_blockWaitTime = waitTime;
}

void QPrintWorker::tryRecoverForException()
{
    Q_D(QPrintWorker);
    d->m_exceptionState = true;

    QThreadData *data = QThreadData::current(false);
    if (data && data->eventLoops.removeOne(&d->m_eventLoop)) {
        --data->loopLevel;
    }
}

bool QPrintWorker::isExceptionState() const
{
    Q_D(const QPrintWorker);
    return d->m_exceptionState;
}

QT_END_NAMESPACE

#endif // QT_NO_PRINTER