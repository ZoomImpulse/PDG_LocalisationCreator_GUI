#include "PDG_LocalisationCreator_GUI.h"
#include <QRadioButton>
#include <QMessageBox>
#include <QLabel>
#include <QDateTime>
#include <QDir>
#include <QRegularExpression> 
#include <QFileInfo> 
#include <QDebug> 

// Constructor: Initializes the main window, sets up UI, worker thread, and connects signals/slots.
PDG_LocalisationCreator_GUI::PDG_LocalisationCreator_GUI(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::PDG_LocalisationCreator_GUIClass())
{
    ui->setupUi(this);

    cleanOldLogs(); // Remove old log files at startup

    // Initialize the cleanup step flag
    isCleanupStep = false;

    // Set up the worker object and move it to a separate thread
    worker = new Worker();
    worker->moveToThread(&workerThread);

    // Create and add the status label to the layout
    statusLabel = new QLabel("Ready", this);
    ui->verticalLayout_2->insertWidget(2, ui->progressBar);
    ui->verticalLayout_2->insertWidget(3, statusLabel);

    // Ensure progress bar text is visible
    ui->progressBar->setTextVisible(true);

    // Connect signals from the worker thread to slots in this (main) thread
    connect(&workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(this, &PDG_LocalisationCreator_GUI::destroyed, this, [=]() {
        workerThread.quit();
        workerThread.wait();
        });
    connect(worker, &Worker::progressUpdated, this, &PDG_LocalisationCreator_GUI::handleProgressUpdate);
    connect(worker, &Worker::taskFinished, this, &PDG_LocalisationCreator_GUI::handleTaskFinished);
    connect(worker, &Worker::statusMessage, this, &PDG_LocalisationCreator_GUI::handleStatusMessage);

    // Connect worker's logMessage to the file writer slot
    connect(worker, &Worker::logMessage, this, &PDG_LocalisationCreator_GUI::writeToLogFile);

    workerThread.start(); // Start the worker thread
}

// Destructor: Ensures log file is closed and cleans up UI
PDG_LocalisationCreator_GUI::~PDG_LocalisationCreator_GUI()
{
    // If a log file stream is open, ensure it's closed
    if (logFileStream) {
        logFileStream->flush();
        logFileStream.reset(); // Closes the file
    }
    delete ui;
}

// Slot: Handles the unified run button click, sets up log file, disables UI, and starts creation task
void PDG_LocalisationCreator_GUI::on_unifiedRunButton_clicked()
{
    int modType = 0;
    if (ui->stnhRadioButton->isChecked()) modType = 1;
    else if (ui->swfrRadioButton->isChecked()) modType = 2;
    else if (ui->sgpRadioButton->isChecked()) modType = 3;

    if (modType > 0) {
        setUiEnabled(false);
        ui->progressBar->setValue(0);
        isCleanupStep = false; // Reset flag for the start of the sequence

        // Setup log file for this run
        QDir logDir("logs");
        if (!logDir.exists()) {
            logDir.mkpath("."); // Create "logs" directory if it doesn't exist
        }
        currentLogFileName = "logs/log_" + QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss") + ".txt";
        QFile* logFile = new QFile(currentLogFileName);
        if (!logFile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
            QMessageBox::critical(this, "Error", "Failed to open log file for writing: " + currentLogFileName);
            setUiEnabled(true);
            delete logFile;
            return;
        }
        logFileStream.reset(new QTextStream(logFile));
        logFileStream->setEncoding(QStringConverter::Utf8);
        logFileStream->setGenerateByteOrderMark(false);

        // Write initial log entries
        writeToLogFile("STARTING NEW LOCALISATION PROCESS");
        writeToLogFile("Log file: " + currentLogFileName);
        writeToLogFile("Selected Mod Type: " + QString::number(modType));
        writeToLogFile("Timestamp: " + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));

        statusLabel->setText("Starting creation task");
        // Start the creation task in the worker thread
        QMetaObject::invokeMethod(worker, "doCreateTask", Qt::QueuedConnection, Q_ARG(int, modType));
    }
}

// Slot: Updates the progress bar value
void PDG_LocalisationCreator_GUI::handleProgressUpdate(int value)
{
    ui->progressBar->setValue(value);
}

// Slot: Handles task completion, manages log, UI state, and triggers cleanup if needed
void PDG_LocalisationCreator_GUI::handleTaskFinished(bool success, const QString& message)
{
    // Log final status before closing stream
    writeToLogFile("Task Sequence Finished");
    writeToLogFile("Success: " + QString(success ? "True" : "False"));
    writeToLogFile("Final Message: " + message);
    if (!success) {
        writeToLogFile("Please check this log file for detailed errors.");
    }

    // Ensure log file stream is flushed and closed
    if (logFileStream) {
        logFileStream->flush();
        logFileStream.reset();
    }

    if (!isCleanupStep) { // If the creation task just finished
        if (success) {
            int modType = 0; // Get selected mod type again for cleanup task
            if (ui->stnhRadioButton->isChecked()) modType = 1;
            else if (ui->swfrRadioButton->isChecked()) modType = 2;
            else if (ui->sgpRadioButton->isChecked()) modType = 3;

            isCleanupStep = true; // Set flag for cleanup task
            statusLabel->setText("Creation complete. Starting cleanup.");
            ui->progressBar->setValue(0); // Reset progress bar for cleanup
            // Start the cleanup task in the worker thread
            QMetaObject::invokeMethod(worker, "doCleanupTask", Qt::QueuedConnection, Q_ARG(int, modType));
        }
        else {
            setUiEnabled(true); // Re-enable UI if creation failed
            ui->progressBar->setValue(0); // Reset progress bar on failure
            statusLabel->setText("Task failed.");
            QMessageBox::critical(this, "Error", message + "\nCreation failed. Check the log file for details: " + currentLogFileName);
            isCleanupStep = false; // Reset for next run
        }
    }
    else { // If the cleanup task just finished
        setUiEnabled(true);
        ui->progressBar->setValue(100);

        if (success) {
            statusLabel->setText("All tasks completed successfully!");
        }
        else {
            statusLabel->setText("All tasks finished with errors.");
            QMessageBox::critical(this, "Error", message + "\nCleanup finished with errors. Check the log file for details: " + currentLogFileName);
        }
        isCleanupStep = false; // Reset for next run
    }
}

// Slot: Updates the status label with a message from the worker
void PDG_LocalisationCreator_GUI::handleStatusMessage(const QString& message)
{
    statusLabel->setText(message);
}

// Slot: Writes a message to the log file with a timestamp
void PDG_LocalisationCreator_GUI::writeToLogFile(const QString& message)
{
    if (logFileStream) {
        *logFileStream << "[" << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "] " << message << "\n";
        logFileStream->flush();
    }
}

// Enables or disables UI controls for mod selection and actions
void PDG_LocalisationCreator_GUI::setUiEnabled(bool enabled)
{
    ui->modSelectionBox->setEnabled(enabled);
    ui->actionBox->setEnabled(enabled);
}

// Removes old log files from the logs directory, keeping only today's logs
void PDG_LocalisationCreator_GUI::cleanOldLogs()
{
    QDir logsDir("logs");
    if (!logsDir.exists()) {
        qDebug() << "Log directory 'logs' does not exist. No old logs to clean.";
        return;
    }

    QDate currentDate = QDate::currentDate();
    // Filter for files starting with "log_" and ending with ".txt"
    QStringList logFiles = logsDir.entryList(QStringList() << "log_*.txt", QDir::Files);

    qDebug() << "Starting log cleanup. Current date: " << currentDate.toString("yyyy-MM-dd");

    for (const QString& fileName : logFiles) {
        QRegularExpression regex("log_(\\d{4}-\\d{2}-\\d{2})_\\d{2}-\\d{2}-\\d{2}\\.txt");
        QRegularExpressionMatch match = regex.match(fileName);

        if (match.hasMatch()) {
            QString dateString = match.captured(1); // Get the captured date group
            QDate fileDate = QDate::fromString(dateString, "yyyy-MM-dd");

            if (fileDate.isValid() && fileDate != currentDate) {
                if (logsDir.remove(fileName)) {
                    qDebug() << "Cleaned up old log file: " << fileName;
                }
                else {
                    qDebug() << "Failed to clean up log file: " << fileName << " (Permissions or file in use?)";
                }
            }
            else if (!fileDate.isValid()) {
                qDebug() << "Skipping file with invalid date format: " << fileName;
            }
        }
        else {
            qDebug() << "Skipping file not matching log pattern: " << fileName;
        }
    }
    qDebug() << "Log cleanup finished.";
}