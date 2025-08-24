#pragma once

#include <QtWidgets/QMainWindow>
#include <QThread>
#include "ui_PDG_LocalisationCreator_GUI.h"
#include "worker.h"
#include <QTextStream>
#include <QScopedPointer>
#include "ConfigManager.h" // New: Include the ConfigManager header
#include "SheetsSelectionDialog.h" // Include the SheetsSelectionDialog header

QT_BEGIN_NAMESPACE
namespace Ui { class PDG_LocalisationCreator_GUIClass; };
QT_END_NAMESPACE

class OverlayWidget;  // forward declaration for in-window overlay
class ProgressPanel;  // forward declaration for reusable progress panel

// Main window class for the localisation creator GUI application.
class PDG_LocalisationCreator_GUI : public QMainWindow
{
    Q_OBJECT

public:
    // Constructor: sets up the UI and worker thread.
    PDG_LocalisationCreator_GUI(QWidget* parent = nullptr);
    // Destructor: cleans up resources and closes log files.
    ~PDG_LocalisationCreator_GUI();

protected:
    void resizeEvent(QResizeEvent* e) override; // keep overlay covering the window

signals:
    // Signal emitted when the output folder confirmation is needed.
    void outputFolderConfirmed(bool confirmed);

private slots:
    // Slot: handles the unified run button click event.
    void on_unifiedRunButton_clicked();
    // Slot: updates the progress bar value.
    void handleProgressUpdate(int value);
    // Slot: handles completion of worker tasks.
    void handleTaskFinished(bool success, const QString& message);
    // Slot: updates status in the progress dialog.
    void handleStatusMessage(const QString& message);
    // Slot: writes a message to the log file.
    void writeToLogFile(const QString& message);

    // New Slots for folder selection
    void on_outputPathButton_clicked();
    void on_vanillaPathButton_clicked();

    // New: open the sheets selection dialog
    void on_selectSheetsButton_clicked();

private:
    // Enables or disables UI controls during processing.
    void setUiEnabled(bool enabled);
    // Removes old log files from the logs directory.
    void cleanOldLogs();
    // New: Loads paths from config file and updates UI
    void loadPathsFromConfig();
    // New: Saves current paths from UI to config file
    void savePathsToConfig();

    // New: Update the summary label for selected sheets
    void updateSheetsSummary();

    Ui::PDG_LocalisationCreator_GUIClass* ui; // Pointer to the UI elements.
    QThread workerThread;                     // Thread for running the worker object.
    Worker* worker;                           // Pointer to the background worker.
    bool isCleanupStep;                       // Flag to track if the cleanup step is running.
    QString currentLogFileName;               // Name of the current log file.

    ConfigManager* configManager;             // New: Instance of ConfigManager

    // New: cached selections JSON (category -> [sheetIds])
    QString sheetsSelectionsJson;

    // New: SheetsSelectionDialog instance
    SheetsSelectionDialog* sheetsSelectionDialog;

    // New: In-window overlay and panel (Option B)
    OverlayWidget* overlayWidget = nullptr;
    ProgressPanel* progressPanel = nullptr;
};