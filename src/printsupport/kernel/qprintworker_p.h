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

#ifndef QPRINTWORKER_P_H
#define QPRINTWORKER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtPrintSupport/private/qtprintsupportglobal_p.h>

#ifndef QT_NO_PRINTER

QT_BEGIN_NAMESPACE

class QPrintTask : public QObject, public QRunnable
{
	Q_OBJECT
public:
    using TaskFunctor = std::function<QVariant ()>;
    using CompletionFunctor = std::function<void (const QVariant &, bool)>;

    QPrintTask(TaskFunctor task, CompletionFunctor completion);
    ~QPrintTask();

    void run() override;
    bool isFinished() const;

    void interrupt();
    void ensureBlockWaitSemaphore();
    bool blockWait(int timeout) const;
    int addRef();

Q_SIGNALS:
    void finished();

public Q_SLOTS:
    void notifyRelease();
    void notifyFinished();

private:
    TaskFunctor m_task;
    CompletionFunctor m_completion;
    bool m_finished;
    QVariant m_result;
    bool m_interrupted;
    QSemaphore *m_semFinished;
    QAtomicInt m_ref;
};

class QPrintWorkerThread : public QThread
{
    Q_OBJECT
public:
    QPrintWorkerThread();
    ~QPrintWorkerThread();

    void addTask(QPrintTask *task);
    void dequeueTask(QPrintTask *task);
    bool hasTask();

public Q_SLOTS:
    void requestQuit();

protected:
    void run() override;

private:
    QQueue<QPrintTask *> m_tasks;
    QSemaphore m_newTask;
    QMutex m_mutex;
    QAtomicInt m_quit;
};

class QPrintWorkerPrivate
{
public:
    QPrintWorkerPrivate();
    ~QPrintWorkerPrivate();
    bool runTask(QPrintTask *task, int timeout = -1);
    static QPrintWorkerThread *getWorkerThread();

    QEventLoop m_eventLoop;
    int m_blockWaitTime;
    QTimer m_timer;
    bool m_exceptionState;
};


QT_END_NAMESPACE

#endif // QT_NO_PRINTER

#endif // QPRINTWORKER_P_H
