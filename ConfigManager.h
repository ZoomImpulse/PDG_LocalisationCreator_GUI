#pragma once

#include <QObject>
#include <QSettings>
#include <QVariant>

class ConfigManager : public QObject
{
    Q_OBJECT

public:
    explicit ConfigManager(QObject* parent = nullptr);

    // Saves a setting to the config file
    void saveSetting(const QString& key, const QVariant& value);

    // Loads a setting from the config file. Returns defaultValue if not found.
    QVariant loadSetting(const QString& key, const QVariant& defaultValue = QVariant()) const;

private:
    QSettings settings;
};