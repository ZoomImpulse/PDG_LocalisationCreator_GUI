#include "ProgressOverlay.h"
#include <QEasingCurve>
#include <QPainter>

// ---------------- ProgressPanel ----------------
ProgressPanel::ProgressPanel(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(8);

    header = new QLabel("Working…", this);
    QFont f = header->font();
    f.setPointSize(std::max(9, f.pointSize()));
    f.setWeight(QFont::DemiBold);
    header->setFont(f);
    header->setAlignment(Qt::AlignHCenter);
    header->setWordWrap(true);
    root->addWidget(header);

    overall = new QProgressBar(this);
    overall->setRange(0, 100);
    overall->setValue(0);
    root->addWidget(overall);

    // Grid to align labels and bars vertically
    auto* grid = new QGridLayout();
    grid->setColumnStretch(0, 0);
    grid->setColumnStretch(1, 1);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(6);

    fetchLabel = new QLabel("Fetching...", this);
    fetchLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    fetchBar = new QProgressBar(this);
    fetchBar->setRange(0, 0); // indeterminate

    processLabel = new QLabel("Processing...", this);
    processLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    processBar = new QProgressBar(this);
    processBar->setRange(0, 0); // indeterminate

    // Start hidden until active
    fetchLabel->setVisible(false);
    fetchBar->setVisible(false);
    processLabel->setVisible(false);
    processBar->setVisible(false);

    grid->addWidget(fetchLabel,   0, 0);
    grid->addWidget(fetchBar,     0, 1);
    grid->addWidget(processLabel, 1, 0);
    grid->addWidget(processBar,   1, 1);

    root->addLayout(grid);

    // Dismiss button, hidden by default; becomes visible on completion
    dismissBtn = new QPushButton("Close", this);
    dismissBtn->setVisible(false);
    dismissBtn->setDefault(true);
    QObject::connect(dismissBtn, &QPushButton::clicked, this, [this]() { emit dismissRequested(); });
    root->addWidget(dismissBtn, 0, Qt::AlignHCenter);

    // Card-like styling via stylesheet (ensure styled background is drawn)
    setObjectName("card");
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(
        "QWidget#card{"
        " border: 1px solid rgba(255,255,255,90);"
        " border-radius: 8px;"
        " background: rgba(20,20,20,220);"
        "}"
        " QProgressBar{min-height:10px;}"
    );
}

void ProgressPanel::setOverallProgress(int value) {
    if (value < 0) { overall->setRange(0, 0); }
    else { overall->setRange(0, 100); overall->setValue(value); }
}

void ProgressPanel::setStatusText(const QString& text) { header->setText(text); }

void ProgressPanel::setFetchingActive(bool active) {
    if (fetchLabel) fetchLabel->setVisible(active);
    if (fetchBar)   fetchBar->setVisible(active);
}

void ProgressPanel::setProcessingActive(bool active) {
    if (processLabel) processLabel->setVisible(active);
    if (processBar)   processBar->setVisible(active);
}

void ProgressPanel::setDismissVisible(bool visible) {
    if (dismissBtn) dismissBtn->setVisible(visible);
}

// ---------------- OverlayWidget ----------------
OverlayWidget::OverlayWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NoSystemBackground, false);
    setAttribute(Qt::WA_TransparentForMouseEvents, false); // block clicks
    setAutoFillBackground(false);
    setVisible(false);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    lay->setAlignment(Qt::AlignCenter);

    panel = new ProgressPanel(this);
    panel->setFixedWidth(420);
    lay->addWidget(panel, 0, Qt::AlignCenter);

    // Fade effect only on the panel, keep the dim background fully opaque
    effect = new QGraphicsOpacityEffect(panel);
    panel->setGraphicsEffect(effect);
    effect->setOpacity(0.0);

    anim = new QPropertyAnimation(effect, "opacity", this);
    anim->setDuration(160);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::InOutSine);
}

void OverlayWidget::showOverlay() {
    if (!parentWidget()) return;
    resize(parentWidget()->size());
    move(0, 0);
    setVisible(true);
    raise();
    anim->stop();
    anim->setDirection(QAbstractAnimation::Forward);
    anim->start();
}

void OverlayWidget::hideOverlay() {
    anim->stop();
    anim->setDirection(QAbstractAnimation::Backward);
    QObject::disconnect(anim, nullptr, this, nullptr);
    connect(anim, &QPropertyAnimation::finished, this, [this]() {
        if (anim->direction() == QAbstractAnimation::Backward) setVisible(false);
    });
    anim->start();
}

void OverlayWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(0, 0, 0, 160));
}
