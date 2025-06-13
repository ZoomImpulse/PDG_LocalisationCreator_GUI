#include "ConfigManager.h"
#include <QCoreApplication> // For QCoreApplication::applicationDirPath()
#include <QDir>             // For QDir::separator()
#include <QDebug>           // For qDebug()

ConfigManager::ConfigManager(QObject* parent)
    : QObject(parent),
    // Initialize QSettings. You can choose QSettings::IniFormat for plain text.
    // The file will be in the application's directory.
    settings(QCoreApplication::applicationDirPath() + QDir::separator() + "config.ini", QSettings::IniFormat)
{
    qDebug() << "Config file path: " << settings.fileName();
}

void ConfigManager::saveSetting(const QString& key, const QVariant& value)
{
    settings.setValue(key, value);
    settings.sync(); // Ensure the changes are written to disk immediately
    qDebug() << "Saved setting: " << key << " = " << value;
}

QVariant ConfigManager::loadSetting(const QString& key, const QVariant& defaultValue) const
{
    QVariant value = settings.value(key, defaultValue);
    qDebug() << "Loaded setting: " << key << " = " << value << " (Default: " << defaultValue << ")";
    return value;
}