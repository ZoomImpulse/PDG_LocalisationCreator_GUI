// ---------------------------------------------------------------- //
//                         worker.cpp (Source File)                 //
// ---------------------------------------------------------------- //
// This is the implementation of the Worker class. The logic from   //
// your Create.cpp and Cleanup.cpp files has been moved here and    //
// adapted to use Qt classes (QFile, QDir, etc.) and to emit        //
// signals for UI updates instead of writing to the console.        //
// ---------------------------------------------------------------- //
#include "worker.h"
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDebug>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

Worker::Worker(QObject* parent) : QObject(parent) {}

void Worker::doCreateTask(int modType)
{
    runCreateProcess(modType);
}

void Worker::doCleanupTask(int modType)
{
    runCleanupProcess(modType);
}

// --- Logic from Create.cpp ---
void Worker::runCreateProcess(int modType)
{
    emit progressUpdated(0);
    emit statusMessage("Starting localisation creation"); // User-friendly status

    emit logMessage("Clearing contents of Output folder");
    emit statusMessage("Clearing previous output");
    QDir outputDir("Output");

    // Ensure the Output directory exists first
    if (!outputDir.exists()) {
        if (!QDir::current().mkpath("Output")) {
            emit logMessage("ERROR: Could not create Output folder!");
            emit taskFinished(false, "Failed to prepare output directory.");
            return; // Critical error, stop here
        }
    }

    // Remove contents of the Output directory
    QStringList entries = outputDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
    for (const QString& entry : entries) {
        QString fullPath = outputDir.filePath(entry);
        QFileInfo fileInfo(fullPath);

        if (fileInfo.isDir()) {
            QDir subDir(fullPath);
            if (!subDir.removeRecursively()) {
                emit logMessage("WARNING: Could not remove directory: " + fullPath);
            }
            else {
                emit logMessage("Removed directory: " + fullPath);
            }
        }
        else if (fileInfo.isFile()) {
            if (!QFile::remove(fullPath)) {
                emit logMessage("WARNING: Could not remove file: " + fullPath);
            }
            else {
                emit logMessage("Removed file: " + fullPath);
            }
        }
    }
    emit logMessage("Output folder contents cleared.");

    std::vector<std::pair<QString, QString>> filenames;
    QDir::current().mkpath("Input");
    QDir::current().mkpath("Output");

    QString modName;
    if (modType == 1) {
        modName = "STNH";
        emit logMessage("Selected STNH Localisation");
        filenames = {
            { "ST New Horizons - Main Localisation.json", "STH_main_l_<lang>.yml" },
            { "ST New Horizons - Ships Localisation.json", "STH_ships_l_<lang>.yml" },
            { "ST New Horizons - Modifiers Localisation.json", "STH_modifiers_l_<lang>.yml" },
            { "ST New Horizons - Events Localisation.json", "STH_events_l_<lang>.yml" },
            { "ST New Horizons - Tech Localisation.json", "STH_tech_l_<lang>.yml" },
            { "ST New Horizons - Synced Localisation.json", "STH_synced_l_<lang>.yml" }
        };
    }
    else if (modType == 2) {
        modName = "SWFR";
        emit logMessage("Selected SWFR Localisation");
        filenames = {
            { "SW Fallen Republic - Main Localisation.json", "SWFR_main_l_<lang>.yml" },
            { "SW Fallen Republic - Ships Localisation.json", "SWFR_ships_l_<lang>.yml" },
            { "SW Fallen Republic - Modifiers Localisation.json", "SWFR_modifiers_l_<lang>.yml" },
            { "SW Fallen Republic - Events Localisation.json", "SWFR_events_l_<lang>.yml" },
            { "SW Fallen Republic - Tech Localisation.json", "SWFR_tech_l_<lang>.yml" },
            { "SW Fallen Republic - Synced Localisation.json", "SWFR_synced_l_<lang>.yml" }
        };
    }
    else if (modType == 3) {
        modName = "SGP";
        emit logMessage("Selected SGP Localisation");
        filenames = {
            { "SGP - Main Localisation.json", "SGP_main_l_<lang>.yml" },
        };
    }

    emit statusMessage("Processing " + modName + " localisation files");

    QRegularExpression translationExp("\"[^(]+\\(([^)]+)\\)\" *: *\"(.*?)(?:\\\\\"|$)\""); // Changed regex

    bool success = true;
    for (const auto& filePair : filenames) {
        QString inputPath = "Input/" + filePair.first;
        emit logMessage("\n-> Processing: " + inputPath);
        emit statusMessage("Reading " + filePair.first);

        QFile inputFile(inputPath);

        if (!inputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            emit logMessage("ERROR: Could not open file: " + inputPath);
            emit taskFinished(false, "Failed to open input file: " + filePair.first);
            return; // Exit on first critical error
        }

        qint64 size = inputFile.size();
        std::unordered_map<std::string, std::vector<std::string>> translations;

        QTextStream in(&inputFile);
        while (!in.atEnd()) {
            QString line = in.readLine();
            QRegularExpressionMatch match = translationExp.match(line);
            if (match.hasMatch()) {
                std::string lang = match.captured(1).toStdString();
                QString rawText = match.captured(2); // Get the captured text as QString
                // Replace escaped quotes with literal quotes, then append the final missing double quote
                std::string text = rawText.replace(QStringLiteral("\\\""), QStringLiteral("\"")).toStdString() + "\"";
                translations[lang].push_back(text);
            }
            if (size > 0) {
                emit progressUpdated(inputFile.pos() * 100 / size);
            }
        }
        inputFile.close();
        emit logMessage("Loaded translations from JSON.");

        for (const auto& entry : translations) {
            QString language = QString::fromStdString(entry.first);
            if (language.toLower() == "italian") {
                emit logMessage("Skipping Italian translations.");
                continue;
            }

            QString langLower = language.toLower();
            QDir::current().mkpath("Output/" + langLower);

            QString outFileName = filePair.second;
            outFileName.replace("<lang>", langLower);

            QFile outputFile("Output/" + langLower + "/" + outFileName);
            if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                emit logMessage("ERROR: Could not write to file " + outFileName);
                success = false; // Mark failure but try to continue
                continue;
            }

            emit statusMessage("Writing file for " + language + "...");
            QTextStream out(&outputFile);
            out.setEncoding(QStringConverter::Utf8);
            out.setGenerateByteOrderMark(true);

            out << "l_" << langLower << ":\n";
            for (const auto& line : entry.second) {
                out << " " << QString::fromStdString(line) << "\n";
            }
            outputFile.close();
        }
    }
    if (success) {
        emit taskFinished(true, "Localisation files created successfully!");
    }
    else {
        emit taskFinished(false, "Localisation creation finished with some errors.");
    }
}

// --- Logic from Cleanup.cpp ---
void Worker::runCleanupProcess(int modType)
{
    using namespace std;
    emit progressUpdated(0);
    emit logMessage("Starting Cleanup Process"); // Detailed log
    emit statusMessage("Starting localisation cleanup"); // User-friendly status

    vector<QString> languages = {
        "braz_por", "english", "french", "german", "polish", "russian", "spanish"
    };

    vector<QString> modFilesTemplates;
    if (modType == 1) modFilesTemplates = { "STH_main_l_<lang>.yml", "STH_ships_l_<lang>.yml", "STH_modifiers_l_<lang>.yml", "STH_tech_l_<lang>.yml", "STH_events_l_<lang>.yml" };
    else if (modType == 2) modFilesTemplates = { "SWFR_main_l_<lang>.yml", "SWFR_ships_l_<lang>.yml", "SWFR_modifiers_l_<lang>.yml", "SWFR_tech_l_<lang>.yml", "SWFR_events_l_<lang>.yml" };
    else if (modType == 3) modFilesTemplates = { "SGP_main_l_<lang>.yml" };

    QRegularExpression keyExp("^ +(.+?):[0-9]? +\"([^\"\\\\]*)\"");
    unordered_map<QString, unordered_set<string>> usedTags;

    emit statusMessage("Loading existing localisation tags");
    emit logMessage("Loading Tags"); // Detailed log
    bool success = true;
    for (const auto& lang : languages) {
        for (const auto& fileTemplate : modFilesTemplates) {
            QString filename = QString(fileTemplate).replace("<lang>", lang);
            QFile file("Output/" + lang + "/" + filename);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                // Not a critical error, might be a file that doesn't exist yet
                emit logMessage("Skipped (file not found): " + filename);
                continue;
            }
            QTextStream in(&file);
            while (!in.atEnd()) {
                string line = in.readLine().toStdString();
                QRegularExpressionMatch match = keyExp.match(QString::fromStdString(line));
                if (match.hasMatch()) {
                    usedTags[lang].insert(match.captured(1).toStdString());
                }
            }
            file.close();
            emit logMessage("[LOADED] " + filename);
        }
    }
    emit logMessage("Tag loading complete");
    emit progressUpdated(25);

    emit statusMessage("Updating files from VanillaFiles");
    emit logMessage("\nUpdating files from VanillaFiles"); // Detailed log
    QDir vanillaDir("VanillaFiles");
    if (!vanillaDir.exists()) {
        emit logMessage("ERROR: 'VanillaFiles' folder not found!");
        emit taskFinished(false, "Required 'VanillaFiles' folder not found. Please create it.");
        return;
    }

    QStringList langDirs = vanillaDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    int langCount = 0;
    for (const auto& lang : langDirs) {
        if (std::find(languages.begin(), languages.end(), lang) == languages.end()) {
            continue;
        }

        QDir sourceDir("VanillaFiles/" + lang);
        QDir destDir("Output/" + lang);
        if (!destDir.exists()) destDir.mkpath(".");

        QStringList files = sourceDir.entryList(QStringList() << "*.yml", QDir::Files);

        for (const auto& fileName : files) {
            QFile inFile("VanillaFiles/" + lang + "/" + fileName);
            QFile outFile("Output/" + lang + "/" + fileName);

            if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                emit logMessage("ERROR: Could not open vanilla file: " + lang + "/" + fileName);
                success = false;
                continue;
            }

            vector<string> fileData;
            bool fileChanged = false;

            QTextStream in(&inFile);
            while (!in.atEnd()) {
                string line = in.readLine().toStdString();
                QRegularExpressionMatch match = keyExp.match(QString::fromStdString(line));
                if (match.hasMatch()) {
                    if (usedTags[lang].find(match.captured(1).toStdString()) == usedTags[lang].end()) {
                        fileData.push_back(line);
                    }
                    else {
                        fileChanged = true; // This line was removed
                    }
                }
                else {
                    fileData.push_back(line);
                }
            }
            inFile.close();

            if (fileChanged) {
                if (outFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                    QTextStream out(&outFile);
                    out.setEncoding(QStringConverter::Utf8);
                    out.setGenerateByteOrderMark(true);
                    for (const auto& line : fileData) {
                        out << QString::fromStdString(line) << "\n";
                    }
                    outFile.close();
                    emit logMessage(lang + "/" + fileName + " updated");
                }
                else {
                    emit logMessage("ERROR: Could not write updated file: " + lang + "/" + fileName);
                    success = false;
                }
            }
        }
        langCount++;
        emit progressUpdated(25 + (langCount * 50 / langDirs.size()));
    }

    emit statusMessage("Copying name lists");
    emit logMessage("\nCopying name_lists and random_names"); // Detailed log
    for (const auto& lang : languages) {
        QStringList subfoldersToCopy = { "name_lists", "random_names" };
        for (const auto& subfolder : subfoldersToCopy) {
            QDir sourceDir("VanillaFiles/" + lang + "/" + subfolder);
            if (sourceDir.exists()) {
                QDir destDir("Output/" + lang + "/" + subfolder);
                if (!destDir.exists()) destDir.mkpath(".");

                QStringList files = sourceDir.entryList(QDir::Files);
                for (const auto& file : files) {
                    if (!QFile::copy(sourceDir.filePath(file), destDir.filePath(file))) {
                        // If copy fails, it might be due to file already existing, or permissions.
                        // For a robust app, you'd add QIODevice::CopyOverwrite, but for now log it.
                        emit logMessage("WARNING: Failed to copy " + sourceDir.filePath(file) + " to " + destDir.filePath(file));
                    }
                }
                emit logMessage("Copied " + subfolder + " for " + lang);
            }
        }
    }
    emit progressUpdated(100);

    if (success) {
        emit taskFinished(true, "Cleanup and update task completed successfully!");
    }
    else {
        emit taskFinished(false, "Cleanup and update task finished with some errors.");
    }
}