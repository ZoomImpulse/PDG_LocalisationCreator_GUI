// ---------------------------------------------------------------- //
//                          worker.h (Header File)                  //
// ---------------------------------------------------------------- //
// This header declares the Worker class, which runs the heavy file //
// processing tasks in a separate thread to prevent the UI from     //
// freezing. It communicates with the main window via signals.      //
// ---------------------------------------------------------------- //
#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>
#include <QMutex>         // NEW: For synchronization
#include <QWaitCondition> // NEW: For waiting/waking threads

class Worker : public QObject
{
    Q_OBJECT

public:
    explicit Worker(QObject* parent = nullptr);

public slots:
    void doCreateTask(int modType);
    void doCleanupTask(int modType);

signals:
    void logMessage(const QString& message);
    void statusMessage(const QString& message);
    void progressUpdated(int value);
    void taskFinished(bool success, const QString& message);

private:
    void runCreateProcess(int modType);
    void runCleanupProcess(int modType);

    bool m_outputFolderClearConfirmed; // NEW: Flag to store the user's decision
    QMutex m_mutex;                    // NEW: Mutex for synchronization
    QWaitCondition m_condition;        // NEW: Condition for waiting/waking the worker thread
};