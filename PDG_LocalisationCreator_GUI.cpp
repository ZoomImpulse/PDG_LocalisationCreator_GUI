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
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include "ConfigManager.h" // Assuming ConfigManager is included and defined
#include "SheetsSelectionDialog.h" // Assuming SheetsSelectionDialog is included and defined
#include "ProgressOverlay.h" // Extracted overlay classes

// Overlay classes moved to ProgressOverlay.h/cpp

// Constructor: Initializes the main window, sets up UI, worker thread, and connects signals/slots.
PDG_LocalisationCreator_GUI::PDG_LocalisationCreator_GUI(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::PDG_LocalisationCreator_GUIClass())
    , sheetsSelectionDialog(nullptr)
{
    ui->setupUi(this);


    cleanOldLogs(); // Remove old log files at startup

    // Initialize the cleanup step flag
    isCleanupStep = false;

    // Set up the worker object and move it to a separate thread
    worker = new Worker();
    worker->moveToThread(&workerThread);

    // Status text is shown only in the in-window progress overlay now

    // Initialize ConfigManager and load settings
    configManager = new ConfigManager(this); // Parented to GUI
    loadPathsFromConfig();
    // Load saved sheet selections JSON if present
    sheetsSelectionsJson = configManager->loadSetting("Sheets/SelectionsJson", "{}").toString();
    updateSheetsSummary();

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

// Keep overlay sized to window
void PDG_LocalisationCreator_GUI::resizeEvent(QResizeEvent* e)
{
    QMainWindow::resizeEvent(e);
    if (overlayWidget) {
        // Keep overlay sized to the central widget
        overlayWidget->resize(ui->centralWidget->size());
        overlayWidget->move(0, 0);
    }
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
    int modType = 1; // Only STNH is available now

    // Get the paths from QLineEdit fields
    QString outputPath = ui->outputPathLineEdit->text();
    QString vanillaPath = ui->vanillaPathLineEdit->text();

    // Basic validation: Check if paths are set
    if (outputPath.isEmpty() || vanillaPath.isEmpty()) {
        QMessageBox::warning(this, "Missing Paths", "Please select all Output, and Vanilla Files directories before running.");
        return;
    }

    // Save paths automatically before starting the process
    savePathsToConfig();

    setUiEnabled(false);
    // In-window overlay: create on first use
    if (!overlayWidget) {
        // Parent to central widget so it covers the content area exactly
        overlayWidget = new OverlayWidget(ui->centralWidget);
        overlayWidget->resize(ui->centralWidget->size());
        overlayWidget->move(0, 0);
        progressPanel = overlayWidget->panelWidget();

        // Wire panel signals
        connect(worker, &Worker::progressUpdated, progressPanel, &ProgressPanel::setOverallProgress, Qt::UniqueConnection);
        connect(worker, &Worker::statusMessage,  progressPanel, &ProgressPanel::setStatusText,       Qt::UniqueConnection);
        connect(worker, &Worker::fetchActive,    progressPanel, &ProgressPanel::setFetchingActive,   Qt::UniqueConnection);
        connect(worker, &Worker::processActive,  progressPanel, &ProgressPanel::setProcessingActive, Qt::UniqueConnection);
        // Allow user to dismiss overlay when finished
        connect(progressPanel, &ProgressPanel::dismissRequested, this, [this]() {
            if (overlayWidget) overlayWidget->hideOverlay();
            if (progressPanel) progressPanel->setDismissVisible(false);
        });
    }
    // Set initial state and show overlay
    if (progressPanel) {
        progressPanel->setStatusText("Starting…");
        progressPanel->setOverallProgress(0);
        progressPanel->setFetchingActive(false);
        progressPanel->setProcessingActive(false);
        progressPanel->setDismissVisible(false);
    }
    overlayWidget->showOverlay();

    // Setup logging for this run
    QDateTime currentDateTime = QDateTime::currentDateTime();
    currentLogFileName = "logs/log_" + currentDateTime.toString("yyyy-MM-dd_hh-mm-ss") + ".txt";

    // Ensure log directory exists
    QDir logsDir("logs");
    if (!logsDir.exists()) {
        logsDir.mkpath(".");
    }

    writeToLogFile("--- Log Session Started: " + currentDateTime.toString(Qt::ISODate) + " ---");
    writeToLogFile("DEBUG GUI: Log file system initialized and ready.");
    writeToLogFile("STARTING NEW LOCALISATION PROCESS");
    writeToLogFile("Log file: " + currentLogFileName);
    writeToLogFile("Selected Mod Type: " + QString::number(modType));
    writeToLogFile("Output Path: " + outputPath);
    writeToLogFile("Vanilla Path: " + vanillaPath);
    writeToLogFile("Timestamp: " + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));

    // Status shown via the progress overlay
    // Provide selections to worker (queued to its thread)
    QMetaObject::invokeMethod(worker, "setSelectionsJson", Qt::QueuedConnection, Q_ARG(QString, sheetsSelectionsJson));
    // Start the creation task in the worker thread, passing the paths
    QMetaObject::invokeMethod(worker, "doCreateTask", Qt::QueuedConnection,
        Q_ARG(int, modType),
        Q_ARG(QString, ""), // Input path is no longer needed, pass an empty string
        Q_ARG(QString, outputPath),
        Q_ARG(QString, vanillaPath));
}

// Slot: Updates the progress bar value
void PDG_LocalisationCreator_GUI::handleProgressUpdate(int value)
{
    if (progressPanel)  progressPanel->setOverallProgress(value);
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
            int modType = 1;

            // Get the paths from QLineEdit fields for cleanup task
            QString inputPath = ""; // Input path is no longer needed
            QString outputPath = ui->outputPathLineEdit->text();
            QString vanillaPath = ui->vanillaPathLineEdit->text();

            isCleanupStep = true;
            // Status shown via the progress overlay
            // progress bar removed from main window UI

            writeToLogFile("DEBUG GUI: Signalling doCleanupTask to worker.");

            QMetaObject::invokeMethod(worker, "doCleanupTask", Qt::QueuedConnection,
                Q_ARG(int, modType),
                Q_ARG(QString, inputPath),
                Q_ARG(QString, outputPath),
                Q_ARG(QString, vanillaPath));
        }
        else { // If creation failed
            setUiEnabled(true);
            QMessageBox::critical(this, "Error", message + "\nCreation failed. Check the log file for details: " + currentLogFileName);
            isCleanupStep = false;
            if (overlayWidget) { overlayWidget->hideOverlay(); }
        }
    }
    else { // If the cleanup task just finished
        setUiEnabled(true);

        if (!success) {
            QMessageBox::critical(this, "Error", message + "\nCleanup finished with errors. Check the log file for details: " + currentLogFileName);
            isCleanupStep = false; // Reset for next run
            if (overlayWidget) { overlayWidget->hideOverlay(); }
            return;
        }

        // Success path: keep overlay until user dismisses it
        if (progressPanel) {
            progressPanel->setStatusText("Completed successfully");
            progressPanel->setOverallProgress(100);
            progressPanel->setFetchingActive(false);
            progressPanel->setProcessingActive(false);
            progressPanel->setDismissVisible(true);
        }

        isCleanupStep = false; // Reset for next run
    }
}

// Slot: Updates the status label with a message from the worker
void PDG_LocalisationCreator_GUI::handleStatusMessage(const QString& message)
{
    if (progressPanel)  progressPanel->setStatusText(message);
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
    // No UI log streaming anymore
}

// Enables or disables UI controls for mod selection and actions
void PDG_LocalisationCreator_GUI::setUiEnabled(bool enabled)
{
    ui->actionBox->setEnabled(enabled);
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
    ui->outputPathLineEdit->setText(configManager->loadSetting("Paths/OutputPath", "").toString());
    ui->vanillaPathLineEdit->setText(configManager->loadSetting("Paths/VanillaPath", "").toString());
}

// New: Saves current paths from UI to config file
void PDG_LocalisationCreator_GUI::savePathsToConfig()
{
    configManager->saveSetting("Paths/OutputPath", ui->outputPathLineEdit->text());
    configManager->saveSetting("Paths/VanillaPath", ui->vanillaPathLineEdit->text());
    qDebug() << "Saved configuration paths.";
}

// New: open the sheets selection dialog when button clicked
void PDG_LocalisationCreator_GUI::on_selectSheetsButton_clicked()
{
    // Reuse a single dialog instance to avoid re-fetching sheets; reset to saved before showing
    if (!sheetsSelectionDialog) {
        sheetsSelectionDialog = new SheetsSelectionDialog(configManager, this);
    }
    sheetsSelectionDialog->resetToSavedSelections();
    if (sheetsSelectionDialog->exec() == QDialog::Accepted) {
        sheetsSelectionsJson = sheetsSelectionDialog->selectionsJson();
        // Persist selections JSON
        configManager->saveSetting("Sheets/SelectionsJson", sheetsSelectionsJson);
        updateSheetsSummary();
    }
}

// New: update the summary label with counts per category
void PDG_LocalisationCreator_GUI::updateSheetsSummary()
{
    QJsonDocument doc = QJsonDocument::fromJson(sheetsSelectionsJson.toUtf8());
    if (!doc.isObject()) {
        ui->sheetsSummaryLabel->setTextFormat(Qt::RichText);
        ui->sheetsSummaryLabel->setAlignment(Qt::AlignHCenter);
        ui->sheetsSummaryLabel->setWordWrap(true);
        ui->sheetsSummaryLabel->setText("No sheets selected");
        ui->sheetsSummaryLabel->setToolTip(QString());
        return;
    }
    QJsonObject obj = doc.object();
    // Build full text and a nicer HTML with badge-like spans
    struct Item { QString key; int count; };
    QList<Item> items;
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (!it.value().isArray()) continue;
        const qsizetype cnt = it.value().toArray().size();
        items.append({ it.key(), static_cast<int>(cnt) });
    }
    if (items.isEmpty()) {
        ui->sheetsSummaryLabel->setTextFormat(Qt::RichText);
        ui->sheetsSummaryLabel->setAlignment(Qt::AlignHCenter);
        ui->sheetsSummaryLabel->setWordWrap(true);
        ui->sheetsSummaryLabel->setText("No sheets selected");
        ui->sheetsSummaryLabel->setToolTip(QString());
        return;
    }

    // Optional: map long names to shorter aliases for compactness
    auto alias = [](const QString& k) -> QString {
        if (k.startsWith("Main", Qt::CaseInsensitive)) return "Main";
        if (k.startsWith("Ships", Qt::CaseInsensitive)) return "Ships";
        if (k.startsWith("Modifiers", Qt::CaseInsensitive)) return "Modifiers";
        if (k.startsWith("Events", Qt::CaseInsensitive)) return "Events";
        if (k.startsWith("Tech", Qt::CaseInsensitive)) return "Tech";
        if (k.startsWith("Synced", Qt::CaseInsensitive)) return "Synced";
        return k;
    };

    // Build pill badges using RichText HTML
    QString html;
    html += "<div style=\"text-align:center;\">";
    for (int i = 0; i < items.size(); ++i) {
        const auto& it = items.at(i);
        const QString pill = QString(
            "<span style='display:inline-block; margin:2px 4px; padding:2px 8px;"
            " border:1px solid rgba(128,128,128,0.6); border-radius:12px;"
            " background: rgba(128,128,128,0.15); font-size:9pt;'>%1:&thinsp;"
            "<span style=\"font-weight:600; padding-left:2px;\">%2</span></span>")
            .arg(alias(it.key))
            .arg(it.count);
        html += pill;
        html += "&nbsp;&nbsp;"; // explicit spacing since QLabel ignores CSS margins
    }
    html += "</div>";

    ui->sheetsSummaryLabel->setToolTip(QString());
    ui->sheetsSummaryLabel->setTextFormat(Qt::RichText);
    ui->sheetsSummaryLabel->setAlignment(Qt::AlignHCenter);
    ui->sheetsSummaryLabel->setWordWrap(true);
    ui->sheetsSummaryLabel->setText(html);
}