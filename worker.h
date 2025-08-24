#pragma once
class QNetworkReply;

#include <QObject>
#include <QString>
#include <QVector>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QWaitCondition>
#include <QNetworkAccessManager>
#include <QMutexLocker>
#include <atomic>

// Worker class handles background localisation creation and cleanup tasks in a separate thread.
class Worker : public QObject
{
    Q_OBJECT

public:
    // Constructor
    explicit Worker(QObject* parent = nullptr);

public slots:
    // Starts the localisation creation process for the selected mod type.
    void doCreateTask(int modType, const QString& inputPath, const QString& outputPath, const QString& vanillaPath);
    // Starts the cleanup and update process for the selected mod type.
    void doCleanupTask(int modType, const QString& inputPath, const QString& outputPath, const QString& vanillaPath);

    // Request cooperative cancellation (abort in-flight network replies)
    void requestCancel();

    // Provide selections JSON (category -> [sheetIds])
    void setSelectionsJson(const QString& selectionsJson) { m_selectionsJson = selectionsJson; }

signals:
    // Emitted to log a message (for file or UI logging).
    void logMessage(const QString& message);
    // Emitted to update the status message in the UI.
    void statusMessage(const QString& message);
    // Emitted to update the progress bar in the UI.
    void progressUpdated(int value);
    // Emitted when a task finishes, indicating success or failure and a message.
    void taskFinished(bool success, const QString& message);

    // Phase activity signals
    void fetchActive(bool active);
    void processActive(bool active);

private:
    // Internal method to perform the localisation creation logic.
    void runCreateProcess(int modType, const QString& inputPath, const QString& outputPath, const QString& vanillaPath);
    // Internal method to perform the cleanup and update logic.
    void runCleanupProcess(int modType, const QString& inputPath, const QString& outputPath, const QString& vanillaPath);

    bool m_outputFolderClearConfirmed; // Flag to confirm output folder was cleared (not used in current logic).
    QMutex m_mutex;                    // Mutex for thread safety (reserved for future use).
    QWaitCondition m_condition;        // Wait condition for thread synchronization (reserved for future use).
    QNetworkAccessManager* networkManager;

    QString m_selectionsJson;          // Cached selections JSON from UI
    std::atomic<bool> m_cancelRequested { false };
    QList<QNetworkReply*> m_activeReplies; // track in-flight requests for immediate abort
};