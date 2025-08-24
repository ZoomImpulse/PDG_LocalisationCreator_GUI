#pragma once

#include <QDialog>
#include <QMap>
#include <QList>
#include <QString>
#include <QTabWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

class ConfigManager;

class SheetsSelectionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SheetsSelectionDialog(ConfigManager* config, QWidget* parent = nullptr);

    // Returns a JSON string mapping category -> array of sheet ids
    QString selectionsJson() const;

    // Reset all checkbox states to the last saved configuration
    void resetToSavedSelections();

private slots:
    void onFetchFinished();
    void onSelectAll();
    void onSelectNone();
    void onFilterTextChanged(const QString& text);

private:
    struct CategoryInfo {
        QString displayName;
        QString webAppUrl;
        QString spreadsheetId;
        QListWidget* listWidget{nullptr};
        QLineEdit* filterEdit{nullptr};
        QPushButton* selectAllBtn{nullptr};
        QPushButton* selectNoneBtn{nullptr};
    };

    void buildUi();
    void fetchSheets();
    void applySavedSelections();

    QMap<QString, CategoryInfo> categories; // key: display name
    QTabWidget* tabs{nullptr};
    QPushButton* okBtn{nullptr};
    QPushButton* cancelBtn{nullptr};
    QLabel* statusLabel{nullptr};

    QNetworkAccessManager* nam{nullptr};
    QList<QNetworkReply*> pendingReplies;

    ConfigManager* configManager{nullptr};
};
