#pragma once

#include <QWidget>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QPushButton>

// Lightweight progress panel embedded in an overlay
class ProgressPanel : public QWidget {
    Q_OBJECT
public:
    explicit ProgressPanel(QWidget* parent = nullptr);

    void setOverallProgress(int value);
    void setStatusText(const QString& text);
    void setFetchingActive(bool active);
    void setProcessingActive(bool active);
    void setDismissVisible(bool visible);

signals:
    void dismissRequested();

private:
    QLabel* header = nullptr;
    QProgressBar* overall = nullptr;
    QProgressBar* fetchBar = nullptr;
    QProgressBar* processBar = nullptr;
    QLabel* fetchLabel = nullptr;
    QLabel* processLabel = nullptr;
    QPushButton* dismissBtn = nullptr;
};

// Full-window overlay that centers a ProgressPanel and fades in/out
class OverlayWidget : public QWidget {
    Q_OBJECT
public:
    explicit OverlayWidget(QWidget* parent = nullptr);

    ProgressPanel* panelWidget() const { return panel; }
    void showOverlay();
    void hideOverlay();

protected:
    void paintEvent(QPaintEvent*) override;

private:
    ProgressPanel* panel = nullptr;
    QGraphicsOpacityEffect* effect = nullptr;
    QPropertyAnimation* anim = nullptr;
};
