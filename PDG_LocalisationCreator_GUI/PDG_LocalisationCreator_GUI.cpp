#include "PDG_LocalisationCreator_GUI.h"
#include <QRadioButton>
#include <QMessageBox>
#include <QLabel>
#include <QDateTime>
#include <QDir>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDebug>
#include <QFileDialog>
#include <QLineEdit>
#include <QPushButton>
#include "ConfigManager.h" // Assuming ConfigManager is included and defined

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
    statusLabel->setAlignment(Qt::AlignCenter);
    ui->verticalLayout_progressBar->insertWidget(0, statusLabel); // Insert at position 0 to put it above the progress bar

    // Ensure progress bar text is visible
    ui->progressBar->setTextVisible(true);

    // Initialize ConfigManager and load settings
    configManager = new ConfigManager(this); // Parented to GUI
    loadPathsFromConfig();

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
    // Save settings on exit (this might be redundant if saved on start, but good for robustness)
    savePathsToConfig();

    // If a log file stream is open, ensure it's closed
    delete ui;
}

// Slot: Handles the unified run button click, sets up log file, disables UI, and starts creation task
void PDG_LocalisationCreator_GUI::on_unifiedRunButton_clicked()
{
    int modType = 0;
    if (ui->stnhRadioButton->isChecked()) modType = 1;
    else if (ui->swfrRadioButton->isChecked()) modType = 2;
    // Removed SGP option: else if (ui->sgpRadioButton->isChecked()) modType = 3; 

    // Get the paths from QLineEdit fields
    QString inputPath = ui->inputPathLineEdit->text();
    QString outputPath = ui->outputPathLineEdit->text();
    QString vanillaPath = ui->vanillaPathLineEdit->text();

    // Basic validation: Check if paths are set
    if (inputPath.isEmpty() || outputPath.isEmpty() || vanillaPath.isEmpty()) {
        QMessageBox::warning(this, "Missing Paths", "Please select all Input, Output, and Vanilla Files directories before running.");
        return;
    }

    // Save paths automatically before starting the process
    savePathsToConfig();

    if (modType > 0) {
        setUiEnabled(false);
        statusLabel->setText("Starting process...");
        ui->progressBar->setValue(0);

        // Setup logging for this run
        QDateTime currentDateTime = QDateTime::currentDateTime();
        currentLogFileName = "logs/log_" + currentDateTime.toString("yyyy-MM-dd_hh-mm-ss") + ".txt";

        // Ensure log directory exists
        QDir logsDir("logs");
        if (!logsDir.exists()) {
            logsDir.mkpath("."); // Create "logs" directory
        }

        // Just write an initial message to ensure the file is created and accessible
        // The writeToLogFile function will handle opening/closing.
        writeToLogFile("--- Log Session Started: " + currentDateTime.toString(Qt::ISODate) + " ---");
        writeToLogFile("DEBUG GUI: Log file system initialized and ready.");

        // Write initial log entries
        writeToLogFile("STARTING NEW LOCALISATION PROCESS");
        writeToLogFile("Log file: " + currentLogFileName);
        writeToLogFile("Selected Mod Type: " + QString::number(modType));
        writeToLogFile("Input Path: " + inputPath);
        writeToLogFile("Output Path: " + outputPath);
        writeToLogFile("Vanilla Path: " + vanillaPath);
        writeToLogFile("Timestamp: " + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));


        statusLabel->setText("Starting creation task");
        // Start the creation task in the worker thread, passing the paths
        QMetaObject::invokeMethod(worker, "doCreateTask", Qt::QueuedConnection,
            Q_ARG(int, modType),
            Q_ARG(QString, inputPath),
            Q_ARG(QString, outputPath),
            Q_ARG(QString, vanillaPath));
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

    if (!isCleanupStep) { // If the creation task just finished
        if (success) {
            int modType = 0; // Get selected mod type again for cleanup task
            if (ui->stnhRadioButton->isChecked()) modType = 1;
            else if (ui->swfrRadioButton->isChecked()) modType = 2;
            // Removed SGP option: else if (ui->sgpRadioButton->isChecked()) modType = 3; 

            // Get the paths from QLineEdit fields for cleanup task
            QString inputPath = ui->inputPathLineEdit->text();
            QString outputPath = ui->outputPathLineEdit->text();
            QString vanillaPath = ui->vanillaPathLineEdit->text();

            isCleanupStep = true;
            statusLabel->setText("Creation complete. Starting cleanup.");
            ui->progressBar->setValue(0);

            writeToLogFile("DEBUG GUI: Signalling doCleanupTask to worker.");

            QMetaObject::invokeMethod(worker, "doCleanupTask", Qt::QueuedConnection,
                Q_ARG(int, modType),
                Q_ARG(QString, inputPath),
                Q_ARG(QString, outputPath),
                Q_ARG(QString, vanillaPath));
        }
        else { // If creation failed
            setUiEnabled(true);
            ui->progressBar->setValue(0);
            statusLabel->setText("Task failed.");
            QMessageBox::critical(this, "Error", message + "\nCreation failed. Check the log file for details: " + currentLogFileName);
            isCleanupStep = false;
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
    // Use the stored currentLogFileName
    QFile logFile(currentLogFileName);
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&logFile);
        out.setEncoding(QStringConverter::Utf8); // Ensure fallback also uses UTF-8
        out << "[" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "] " << message << "\n";
        out.flush();
        logFile.close(); // Close the file immediately after writing
    }
    else {
        // If even this fails, print to debug console as a last resort
        qDebug() << "ERROR: Failed to open log file for writing: " << currentLogFileName << " Message: " << message;
    }
}

// Enables or disables UI controls for mod selection and actions
void PDG_LocalisationCreator_GUI::setUiEnabled(bool enabled)
{
    ui->modSelectionBox->setEnabled(enabled);
    ui->actionBox->setEnabled(enabled);
    ui->inputPathLineEdit->setEnabled(enabled);
    ui->inputPathButton->setEnabled(enabled);
    ui->outputPathLineEdit->setEnabled(enabled);
    ui->outputPathButton->setEnabled(enabled);
    ui->vanillaPathLineEdit->setEnabled(enabled);
    ui->vanillaPathButton->setEnabled(enabled);
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

// New Slot: Handle Input Path selection
void PDG_LocalisationCreator_GUI::on_inputPathButton_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Input Directory", ui->inputPathLineEdit->text());
    if (!dir.isEmpty()) {
        ui->inputPathLineEdit->setText(QDir::toNativeSeparators(dir));

    }
}

// New Slot: Handle Output Path selection
void PDG_LocalisationCreator_GUI::on_outputPathButton_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Output Directory", ui->outputPathLineEdit->text());
    if (!dir.isEmpty()) {
        ui->outputPathLineEdit->setText(QDir::toNativeSeparators(dir));
    }
}

// New Slot: Handle Vanilla Path selection
void PDG_LocalisationCreator_GUI::on_vanillaPathButton_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Vanilla Files Directory", ui->vanillaPathLineEdit->text());
    if (!dir.isEmpty()) {
        ui->vanillaPathLineEdit->setText(QDir::toNativeSeparators(dir));
    }
}

// New: Load paths from config file and update UI
void PDG_LocalisationCreator_GUI::loadPathsFromConfig()
{
    ui->inputPathLineEdit->setText(configManager->loadSetting("Paths/InputPath", "").toString());
    ui->outputPathLineEdit->setText(configManager->loadSetting("Paths/OutputPath", "").toString());
    ui->vanillaPathLineEdit->setText(configManager->loadSetting("Paths/VanillaPath", "").toString());
}

// New: Saves current paths from UI to config file
void PDG_LocalisationCreator_GUI::savePathsToConfig()
{
    configManager->saveSetting("Paths/InputPath", ui->inputPathLineEdit->text());
    configManager->saveSetting("Paths/OutputPath", ui->outputPathLineEdit->text());
    configManager->saveSetting("Paths/VanillaPath", ui->vanillaPathLineEdit->text());
    qDebug() << "Saved configuration paths.";
}