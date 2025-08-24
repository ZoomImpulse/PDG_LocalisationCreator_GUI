#include "SheetsSelectionDialog.h"
#include "ConfigManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonValue>
#include <QUrlQuery>
#include <QUrl>
#include <QListWidgetItem>
#include <QTimer>

static const char* CFG_PREFIX = "Sheets/";

SheetsSelectionDialog::SheetsSelectionDialog(ConfigManager* config, QWidget* parent)
    : QDialog(parent), configManager(config)
{
    setWindowTitle("Select Sheets");
    resize(800, 520);
    setMinimumWidth(720);
    nam = new QNetworkAccessManager(this);
    buildUi();

    // Define categories (display name -> webAppUrl + spreadsheetId)
    categories.clear();
    auto addCat = [this](const QString& name, const QString& url, const QString& ssid) {
        CategoryInfo info; info.displayName = name; info.webAppUrl = url; info.spreadsheetId = ssid; 
        categories.insert(name, info);
    };
    addCat("Main Localisation",    "https://script.google.com/macros/s/AKfycbzAfQroJ3X4vCkn3NCwDy4WqRdgQs_lLpZ-QmOMsUQZ_lo_Lu8ddnbgoaiiGN6U3Nxk_w/exec", "1jQOrWJpAF_9TQVyrrOfxinyTTxvoDJg_E7BHUNEkoio");
    addCat("Ships Localisation",   "https://script.google.com/macros/s/AKfycbzAfQroJ3X4vCkn3NCwDy4WqRdgQs_lLpZ-QmOMsUQZ_lo_Lu8ddnbgoaiiGN6U3Nxk_w/exec", "19z068O5ARdrXLyswqTeDqcQdhAwA39kI8Gx_nhZPL3I");
    addCat("Modifiers Localisation","https://script.google.com/macros/s/AKfycbzAfQroJ3X4vCkn3NCwDy4WqRdgQs_lLpZ-QmOMsUQZ_lo_Lu8ddnbgoaiiGN6U3Nxk_w/exec", "1TZylnt8An15CLYlQmy1tjUYvHgMQoosh_x1jC35HOck");
    addCat("Events Localisation",  "https://script.google.com/macros/s/AKfycbzAfQroJ3X4vCkn3NCwDy4WqRdgQs_lLpZ-QmOMsUQZ_lo_Lu8ddnbgoaiiGN6U3Nxk_w/exec", "1YNdrUt0Ro1w6aiVZR0uSJnnulpzhh4thvy3K1-fJ_qA");
    addCat("Tech Localisation",    "https://script.google.com/macros/s/AKfycbzAfQroJ3X4vCkn3NCwDy4WqRdgQs_lLpZ-QmOMsUQZ_lo_Lu8ddnbgoaiiGN6U3Nxk_w/exec", "15QcA1M4dX455UYD2GEv3tDJ3P4z3jhK7p5qPMTDFS60");
    addCat("Synced Localisation",  "https://script.google.com/macros/s/AKfycbzAfQroJ3X4vCkn3NCwDy4WqRdgQs_lLpZ-QmOMsUQZ_lo_Lu8ddnbgoaiiGN6U3Nxk_w/exec", "1MgcmiOr8OMqD6qo5EMwk3ymVenSqAS8MWdo33hKjIPk");

    // Create tabs and controls per category
    for (auto it = categories.begin(); it != categories.end(); ++it) {
        QWidget* tab = new QWidget(this);
        QVBoxLayout* v = new QVBoxLayout(tab);
        QHBoxLayout* tools = new QHBoxLayout();
        it->filterEdit = new QLineEdit(tab);
        it->filterEdit->setPlaceholderText("Filter sheets...");
        it->selectAllBtn = new QPushButton("Select All", tab);
        it->selectNoneBtn = new QPushButton("Select None", tab);
        tools->addWidget(it->filterEdit);
        tools->addWidget(it->selectAllBtn);
        tools->addWidget(it->selectNoneBtn);
        it->listWidget = new QListWidget(tab);
        it->listWidget->setSelectionMode(QAbstractItemView::NoSelection);
        v->addLayout(tools);
        v->addWidget(it->listWidget);
        tabs->addTab(tab, it.key());
        connect(it->filterEdit, &QLineEdit::textChanged, this, &SheetsSelectionDialog::onFilterTextChanged);
        connect(it->selectAllBtn, &QPushButton::clicked, this, &SheetsSelectionDialog::onSelectAll);
        connect(it->selectNoneBtn, &QPushButton::clicked, this, &SheetsSelectionDialog::onSelectNone);
        // Initially disabled until fetch completes
        it->filterEdit->setEnabled(false);
        it->selectAllBtn->setEnabled(false);
        it->selectNoneBtn->setEnabled(false);
    }

    applySavedSelections();
    // Disable interactions until fetching completes
    if (okBtn) okBtn->setEnabled(false);
    if (tabs) tabs->setEnabled(false);
    fetchSheets();
}

void SheetsSelectionDialog::buildUi()
{
    QVBoxLayout* root = new QVBoxLayout(this);
    statusLabel = new QLabel("Fetching sheets...", this);
    tabs = new QTabWidget(this);
    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addStretch();
    cancelBtn = new QPushButton("Cancel", this);
    okBtn = new QPushButton("OK", this);
    buttons->addWidget(cancelBtn);
    buttons->addWidget(okBtn);
    root->addWidget(statusLabel);
    root->addWidget(tabs, 1);
    root->addLayout(buttons);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void SheetsSelectionDialog::fetchSheets()
{
    // One request per category for simplicity
    for (auto it = categories.begin(); it != categories.end(); ++it) {
        QUrl url(it->webAppUrl);
        QUrlQuery q;
        QJsonArray ids; ids.append(it->spreadsheetId);
        QJsonDocument d(ids);
        q.addQueryItem("action", "listSheets");
        q.addQueryItem("ids", QString::fromUtf8(d.toJson(QJsonDocument::Compact)));
        url.setQuery(q);
        QNetworkRequest req(url);
        QNetworkReply* rep = nam->get(req);
        pendingReplies.append(rep);
        connect(rep, &QNetworkReply::finished, this, &SheetsSelectionDialog::onFetchFinished);
    }
}

void SheetsSelectionDialog::onFetchFinished()
{
    QNetworkReply* rep = qobject_cast<QNetworkReply*>(sender());
    if (!rep) return;
    pendingReplies.removeAll(rep);

    if (rep->error() != QNetworkReply::NoError) {
        statusLabel->setText("Error fetching sheets: " + rep->errorString());
        rep->deleteLater();
        return;
    }

    QByteArray data = rep->readAll();
    rep->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        statusLabel->setText("Unexpected API response");
        return;
    }
    QJsonObject obj = doc.object();
    QJsonArray spreadsheets = obj.value("spreadsheets").toArray();
    for (const QJsonValue& v : spreadsheets) {
        QJsonObject sObj = v.toObject();
        const QString ssid = sObj.value("spreadsheetId").toString();
        const QString ssName = sObj.value("spreadsheetName").toString();
        QJsonArray sheets = sObj.value("sheets").toArray();

        // Find matching category by spreadsheetId
        for (auto it = categories.begin(); it != categories.end(); ++it) {
            if (it->spreadsheetId == ssid && it->listWidget) {
                it->listWidget->clear();
                for (const QJsonValue& sv : sheets) {
                    QJsonObject sh = sv.toObject();
                    const QString name = sh.value("name").toString();
                    const qint64 id = sh.value("id").toVariant().toLongLong();
                    // Display only the sheet name; keep ID in item data
                    QListWidgetItem* item = new QListWidgetItem(name, it->listWidget);
                    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                    item->setData(Qt::UserRole + 1, id);
                    // Pre-check from saved config
                    QString key = QString("%1%2/SelectedIds").arg(CFG_PREFIX).arg(it.key());
                    QVariant saved = configManager ? configManager->loadSetting(key) : QVariant();
                    const QStringList savedIds = saved.toStringList();
                    item->setCheckState(savedIds.contains(QString::number(id)) ? Qt::Checked : Qt::Unchecked);
                }
            }
        }
    }

    if (pendingReplies.isEmpty()) {
        statusLabel->setText("Select sheets and press OK");
        if (tabs) tabs->setEnabled(true);
        if (okBtn) okBtn->setEnabled(true);
        // Re-enable per-tab controls
        for (auto it = categories.begin(); it != categories.end(); ++it) {
            if (it->filterEdit) it->filterEdit->setEnabled(true);
            if (it->selectAllBtn) it->selectAllBtn->setEnabled(true);
            if (it->selectNoneBtn) it->selectNoneBtn->setEnabled(true);
        }
    }
}

void SheetsSelectionDialog::applySavedSelections()
{
    for (auto it = categories.begin(); it != categories.end(); ++it) {
        Q_UNUSED(it);
        // Items are checked during population based on saved config
    }
}

void SheetsSelectionDialog::resetToSavedSelections()
{
    // Restore check states for each category from saved configuration
    for (auto it = categories.begin(); it != categories.end(); ++it) {
        if (!it->listWidget) continue;
        const QString key = QString("%1%2/SelectedIds").arg(CFG_PREFIX).arg(it.key());
        const QStringList savedIds = configManager ? configManager->loadSetting(key).toStringList() : QStringList();
        for (int i = 0; i < it->listWidget->count(); ++i) {
            QListWidgetItem* item = it->listWidget->item(i);
            if (!(item->flags() & Qt::ItemIsUserCheckable)) continue;
            const qint64 id = item->data(Qt::UserRole + 1).toLongLong();
            const bool checked = savedIds.contains(QString::number(id));
            item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        }
        // Clear filter and unhide everything
        if (it->filterEdit) it->filterEdit->clear();
        for (int i = 0; i < it->listWidget->count(); ++i) {
            it->listWidget->item(i)->setHidden(false);
        }
    }
}

void SheetsSelectionDialog::onSelectAll()
{
    // Determine current tab
    int idx = tabs->currentIndex();
    if (idx < 0) return;
    CategoryInfo& info = categories[tabs->tabText(idx)];
    if (!info.listWidget) return;
    for (int i = 0; i < info.listWidget->count(); ++i) {
        QListWidgetItem* item = info.listWidget->item(i);
        if (item->flags() & Qt::ItemIsUserCheckable)
            item->setCheckState(Qt::Checked);
    }
}

void SheetsSelectionDialog::onSelectNone()
{
    int idx = tabs->currentIndex();
    if (idx < 0) return;
    CategoryInfo& info = categories[tabs->tabText(idx)];
    if (!info.listWidget) return;
    for (int i = 0; i < info.listWidget->count(); ++i) {
        QListWidgetItem* item = info.listWidget->item(i);
        if (item->flags() & Qt::ItemIsUserCheckable)
            item->setCheckState(Qt::Unchecked);
    }
}

void SheetsSelectionDialog::onFilterTextChanged(const QString& text)
{
    int idx = tabs->currentIndex();
    if (idx < 0) return;
    CategoryInfo& info = categories[tabs->tabText(idx)];
    if (!info.listWidget) return;
    const QString t = text.trimmed();
    for (int i = 0; i < info.listWidget->count(); ++i) {
        QListWidgetItem* item = info.listWidget->item(i);
        // Only filter user-selectable items
        if (!(item->flags() & Qt::ItemIsUserCheckable)) continue;
        bool match = t.isEmpty() || item->text().contains(t, Qt::CaseInsensitive);
        item->setHidden(!match);
    }
}

QString SheetsSelectionDialog::selectionsJson() const
{
    QJsonObject obj;
    for (auto it = categories.begin(); it != categories.end(); ++it) {
        if (!it->listWidget) continue;
        QJsonArray ids;
        QStringList saveIds;
        for (int i = 0; i < it->listWidget->count(); ++i) {
            QListWidgetItem* item = it->listWidget->item(i);
            if (!(item->flags() & Qt::ItemIsUserCheckable)) continue; // skip header
            if (item->checkState() == Qt::Checked) {
                const qint64 id = item->data(Qt::UserRole + 1).toLongLong();
                ids.append(QJsonValue(static_cast<double>(id)));
                saveIds << QString::number(id);
            }
        }
        obj.insert(it.key(), ids);
        // Persist
        if (configManager) {
            const QString key = QString("%1%2/SelectedIds").arg(CFG_PREFIX).arg(it.key());
            configManager->saveSetting(key, saveIds);
        }
    }
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}
