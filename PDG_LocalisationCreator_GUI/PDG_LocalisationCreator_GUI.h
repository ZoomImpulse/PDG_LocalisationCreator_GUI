// ---------------------------------------------------------------- //
//                  PDG_LocalisationCreator_GUI.h (Header File)                      //
// ---------------------------------------------------------------- //
// This file declares the MainWindow class, which is the main       //
// application window. It manages the UI elements, connects signals //
// from buttons to slots, and handles the background worker thread. //
// ---------------------------------------------------------------- //
#pragma once

#include <QtWidgets/QMainWindow>
#include <QThread>
#include "ui_PDG_LocalisationCreator_GUI.h"
#include "worker.h"
#include <QLabel>
#include <QTextStream>
#include <QScopedPointer>

QT_BEGIN_NAMESPACE
namespace Ui { class PDG_LocalisationCreator_GUIClass; };
QT_END_NAMESPACE

class PDG_LocalisationCreator_GUI : public QMainWindow
{
    Q_OBJECT

public:
    PDG_LocalisationCreator_GUI(QWidget* parent = nullptr);
    ~PDG_LocalisationCreator_GUI();

signals: // NEW: Add this signals section
    void outputFolderConfirmed(bool confirmed); // Signal to send confirmation back to worker

private slots:
    void on_unifiedRunButton_clicked();
    void handleProgressUpdate(int value);
    void handleTaskFinished(bool success, const QString& message);
    void handleStatusMessage(const QString& message);
    void writeToLogFile(const QString& message);

private:
    void setUiEnabled(bool enabled);
    void cleanOldLogs();
    Ui::PDG_LocalisationCreator_GUIClass* ui;
    QThread workerThread;
    Worker* worker;
    QLabel* statusLabel;
    bool isCleanupStep;
    QScopedPointer<QTextStream> logFileStream;
    QString currentLogFileName;
};