#include "worker.h"
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDebug>
#include <QCoreApplication>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <regex>
#include <algorithm>

// Constructor for Worker class
Worker::Worker(QObject* parent) : QObject(parent) {}

// Slot to start the creation process for a given mod type
void Worker::doCreateTask(int modType, const QString& inputPath, const QString& outputPath, const QString& vanillaPath)
{
    runCreateProcess(modType, inputPath, outputPath, vanillaPath);
}

// Slot to start the cleanup process for a given mod type
void Worker::doCleanupTask(int modType, const QString& inputPath, const QString& outputPath, const QString& vanillaPath)
{
    runCleanupProcess(modType, inputPath, outputPath, vanillaPath);
}

// Main logic for creating localisation files based on modType
void Worker::runCreateProcess(int modType, const QString& inputPath, const QString& outputPath, const QString& vanillaPath)
{
    emit progressUpdated(0);
    emit statusMessage("Starting localisation creation");

    // Clear Output folder before starting
    emit logMessage("Clearing contents of Output folder: " + outputPath);
    emit statusMessage("Clearing previous output");
    QDir outputDir(outputPath);

    // Create Output directory if it doesn't exist
    if (!outputDir.exists()) {
        if (!outputDir.mkpath(".")) { // Use mkpath(".") on the QDir object itself
            emit logMessage("ERROR: Could not create Output folder at: " + outputPath);
            emit taskFinished(false, "Failed to prepare output directory.");
            return;
        }
    }

    // Remove all files and subdirectories in Output
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
    emit logMessage(outputPath + " folder contents cleared.");

    // Prepare file name mappings for each mod type
    std::vector<std::pair<QString, QString>> filenames;
    // Ensure input and output directories exist (output already checked)
    QDir inputDir(inputPath);
    if (!inputDir.exists()) {
        if (!inputDir.mkpath(".")) {
            emit logMessage("ERROR: Could not create Input folder at: " + inputPath);
            emit taskFinished(false, "Failed to prepare input directory.");
            return;
        }
    }

    QString modName = "STNH";
    emit logMessage("Selected STNH Localisation");
    filenames = {
        { "Star Trek_ New Horizons - Main Localisation.json", "STH_main_l_<lang>.yml" },
        { "Star Trek_ New Horizons - Ships Localisation.json", "STH_ships_l_<lang>.yml" },
        { "Star Trek_ New Horizons - Modifiers Localisation.json", "STH_modifiers_l_<lang>.yml" },
        { "Star Trek_ New Horizons - Events Localisation.json", "STH_events_l_<lang>.yml" },
        { "Star Trek_ New Horizons - Tech Localisation.json", "STH_tech_l_<lang>.yml" },
        { "Star Trek_ New Horizons - Synced Localisation.json", "STH_synced_l_<lang>.yml" }
    };

    emit statusMessage("Processing " + modName + " localisation files");

    // Regex for extracting translation entries and escaping
    std::regex translationExp("\"[^(]+\\(([^)]+)\\)\" *: *\"( *[^:]+: *[^]*)\"");
    std::regex escapeExp("\\\\([^])");

    bool success = true;
    // Process each input file and generate output files for each language
    for (const auto& filePair : filenames) {
        QString currentInputPath = QDir(inputPath).filePath(filePair.first);
        emit logMessage("\n-> Processing: " + currentInputPath);
        emit statusMessage("Reading " + filePair.first);

        QFile inputFile(currentInputPath);

        if (!inputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            emit logMessage("ERROR: Could not open file: " + currentInputPath);
            emit taskFinished(false, "Failed to open input file: " + filePair.first);
            return;
        }

        qint64 size = inputFile.size();
        std::unordered_map<std::string, std::vector<std::string>> translations;
        std::smatch matches;

        QTextStream in(&inputFile);
        while (!in.atEnd()) {
            QString line = in.readLine();
            std::string stdLine = line.toStdString();
            if (std::regex_search(stdLine, matches, translationExp)) {
                std::string lang = matches[1].str();
                std::string text = std::regex_replace(matches[2].str(), escapeExp, "$1");
                translations[lang].push_back(text);
            }
            if (size > 0) {
                emit progressUpdated(inputFile.pos() * 100 / size);
            }
        }
        inputFile.close();
        emit logMessage("Loaded translations from JSON.");

        // Write output files for each language, skipping Italian
        for (const auto& entry : translations) {
            QString language = QString::fromStdString(entry.first);
            if (language.toLower() == "italian") {
                emit logMessage("Skipping Italian translations.");
                continue;
            }

            QString langLower = language.toLower();
            QDir currentOutputDir(outputPath);
            currentOutputDir.mkpath(langLower); // Create language subfolder in output

            QString outFileName = filePair.second;
            outFileName.replace("<lang>", langLower);

            QString fullOutputPath = currentOutputDir.filePath(langLower + "/" + outFileName);
            QFile outputFile(fullOutputPath);
            if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                emit logMessage("ERROR: Could not write to file " + fullOutputPath);
                success = false;
                continue;
            }

            emit statusMessage("Writing file for " + language + "...");
            QTextStream out(&outputFile);
            out.setEncoding(QStringConverter::Utf8);
            out.setGenerateByteOrderMark(true);

            out << "l_" << langLower << ":\n";
            std::vector<std::string> sortedLines = entry.second;
            std::sort(sortedLines.begin(), sortedLines.end());

            for (const auto& line : sortedLines) {
                out << " " << QString::fromStdString(line) << "\n";
            }
            outputFile.close();
            emit logMessage("Wrote file: " + fullOutputPath);
        }
    }
    // Emit final result
    if (success) {
        emit taskFinished(true, "Localisation files created successfully!");
    }
    else {
        emit taskFinished(false, "Localisation creation finished with some errors.");
    }
}
// Main logic for cleaning up and updating localisation files based on modType
void Worker::runCleanupProcess(int modType, const QString& inputPath, const QString& outputPath, const QString& vanillaPath)
{
    emit progressUpdated(0);
    emit statusMessage("Starting localization cleanup and update");
    emit logMessage("Running cleanup process (writing cleaned vanilla to Output)...");

    // The keys to be removed from vanilla files, hardcoded
    std::unordered_set<std::string> keysToRemove = {
        "DIFFICULTY_ADMIRAL",
        "DIFFICULTY_CADET",
        "DIFFICULTY_CAPTAIN",
        "DIFFICULTY_CIVILIAN",
        "DIFFICULTY_COMMODORE",
        "DIFFICULTY_ENSIGN",
        "DIFFICULTY_GRAND_ADMIRAL"
    };

    // List of supported languages (Italian is skipped)
    std::vector<QString> languages = {
        "braz_por", "english", "french", "german", "polish", "russian",
        // "italian",
        "spanish"
    };

    // Define the file templates based on modType - used only for First Pass (loading mod tags)
    QMap<QString, QStringList> modFilesTemplates;
    QString modName = "STNH";
    emit logMessage("Selected STNH Cleanup");
    modFilesTemplates.insert(outputPath + "/<lang>/STH_main_l_<lang>.yml", QStringList());
    modFilesTemplates.insert(outputPath + "/<lang>/STH_ships_l_<lang>.yml", QStringList());
    modFilesTemplates.insert(outputPath + "/<lang>/STH_modifiers_l_<lang>.yml", QStringList());
    modFilesTemplates.insert(outputPath + "/<lang>/STH_tech_l_<lang>.yml", QStringList());
    modFilesTemplates.insert(outputPath + "/<lang>/STH_events_l_<lang>.yml", QStringList());
    modFilesTemplates.insert(outputPath + "/<lang>/STH_synced_l_<lang>.yml", QStringList());

    emit logMessage("DEBUG: modFilesTemplates size after initialization: " + QString::number(modFilesTemplates.size()) + " for modType " + QString::number(modType));

    std::regex keyExp("^ +(.+?):[0-9]? +\"([^\\\"]*)\"");
    std::regex emptystringExp("^( +.+?:[0-9]? +)\\\"\\\"$");
    std::smatch matches;

    std::unordered_map<QString, std::unordered_set<std::string>> usedTags;

    emit statusMessage("Loading existing localization tags from mod output files...");
    emit logMessage("Loading tags from mod output files for cleanup...");

    // First pass: Load existing localization tags from the mod's output files
    // Calculate progress for this section
    int currentProgress = 0;
    int progressPerLanguage = (modFilesTemplates.keys().size() > 0) ? (20 / languages.size()) : 0; // Allocate 20% for this phase

    for (const auto& lang : languages) {
        QString langLower = lang.toLower();
        int tagsLoadedForLang = 0;
        for (const QString& outputPathTemplate : modFilesTemplates.keys()) {
            QString outputPathWithLang = outputPathTemplate;
            outputPathWithLang.replace("<lang>", langLower);

            QFile outputFile(outputPathWithLang);
            if (!outputFile.exists()) {
                emit logMessage("INFO: Mod output file does not exist for loading tags: " + outputPathWithLang);
                continue;
            }

            if (!outputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                emit logMessage("ERROR: Could not open mod output file for reading tags: " + outputPathWithLang);
                continue;
            }

            QTextStream in(&outputFile);
            while (!in.atEnd()) {
                QString line = in.readLine();
                std::string stdLine = line.toStdString();
                if (std::regex_search(stdLine, matches, keyExp)) {
                    std::string tag = matches[1].str();
                    usedTags[lang].insert(tag);
                    tagsLoadedForLang++;
                }
            }
            outputFile.close();
            emit logMessage("INFO: Loaded " + QString::number(tagsLoadedForLang) + " tags from " + outputPathWithLang + " for " + lang + ".");
            tagsLoadedForLang = 0;
        }
        emit logMessage("INFO: Total unique tags loaded for " + lang + ": " + QString::number(usedTags[lang].size()));
        currentProgress += progressPerLanguage;
        emit progressUpdated(qMin(currentProgress, 20)); // Cap at 20% for this phase
    }
    emit logMessage("SUMMARY: Loaded tags for " + QString::number(usedTags.size()) + " languages in total from mod output.");
    emit progressUpdated(20); // Ensure it's at 20% after the first pass

    // Second pass: Process ALL vanilla files and write cleaned versions to the Output folder
    bool success = true;
    int progressPerVanillaFile = (languages.size() * modFilesTemplates.keys().size() > 0) ? (70 / (languages.size() * modFilesTemplates.keys().size())) : 0; // Allocate 70% for this phase (20-90)
    int filesProcessed = 0;

    for (const auto& lang : languages) {
        if (lang.toLower() == "italian") {
            emit logMessage("Skipping Italian language for cleanup.");
            continue;
        }

        QDir vanillaLangDir(vanillaPath + "/" + lang);
        if (!vanillaLangDir.exists()) {
            emit logMessage("WARNING: Vanilla language directory does not exist: " + vanillaLangDir.path());
            continue;
        }

        QDir outputLangDir(outputPath + "/" + lang);
        if (!outputLangDir.exists()) {
            outputLangDir.mkpath(".");
        }


        QStringList vanillaFiles = vanillaLangDir.entryList(QStringList() << "*.yml", QDir::Files, QDir::Name);

        for (const QString& vanillaFileName : vanillaFiles) {
            if (vanillaFileName.startsWith("name_lists_") || vanillaFileName.startsWith("random_names_")) {
                continue;
            }

            QString vanillaInputPath = vanillaLangDir.filePath(vanillaFileName);
            QFile vanillaFile(vanillaInputPath);
            if (!vanillaFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                emit logMessage("ERROR: Could not open vanilla file: " + vanillaInputPath);
                success = false;
                continue;
            }

            QTextStream inVanilla(&vanillaFile);
            QStringList fileData;
            bool fileChanged = false;

            while (!inVanilla.atEnd()) {
                QString line = inVanilla.readLine();
                std::string stdLine = line.toStdString();

                if (std::regex_search(stdLine, matches, keyExp)) {
                    std::string tag = matches[1].str();
                    // Check if the tag is either a mod tag OR a hardcoded key to remove
                    if ((usedTags.find(lang) != usedTags.end() && usedTags[lang].find(tag) != usedTags[lang].end()) || keysToRemove.count(tag) > 0) {
                        fileChanged = true;
                    }
                    else {
                        fileData.append(line);
                    }
                }
                else {
                    fileData.append(line);
                }
            }
            vanillaFile.close();

            if (fileChanged) {
                QString cleanedOutputPath = outputLangDir.filePath(vanillaFileName);
                QFile cleanedOutputFile(cleanedOutputPath);
                if (!cleanedOutputFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
                    emit logMessage("ERROR: Could not write cleaned file: " + cleanedOutputPath);
                    success = false;
                    continue;
                }

                QTextStream outCleaned(&cleanedOutputFile);
                outCleaned.setEncoding(QStringConverter::Utf8);
                outCleaned.setGenerateByteOrderMark(true);

                for (const QString& line : fileData) {
                    std::string processedLine = std::regex_replace(line.toStdString(), emptystringExp, "$1\"\\n\"");
                    outCleaned << QString::fromStdString(processedLine) << "\n";
                }
                cleanedOutputFile.close();
                emit logMessage("UPDATED: " + cleanedOutputPath);
            }
            filesProcessed++;
            emit progressUpdated(20 + qMin(filesProcessed * progressPerVanillaFile, 70)); // Progress from 20% to 90%
        }
    }
    emit progressUpdated(90); // Ensure it's at 90% before copying name lists

    emit statusMessage("Copying name lists");
    emit logMessage("Copying name_lists and random_names to Output folder...");
    // Copy name_lists and random_names folders for each language
    for (const auto& lang : languages) {
        QStringList subfoldersToCopy = { "name_lists", "random_names" };
        for (const auto& subfolder : subfoldersToCopy) {
            QDir sourceDir(vanillaPath + "/" + lang + "/" + subfolder);
            if (sourceDir.exists()) {
                QDir destDir(outputPath + "/" + lang + "/" + subfolder);
                if (!destDir.exists()) destDir.mkpath(".");
                else {
                    QStringList oldFiles = destDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
                    for (const QString& oldFile : oldFiles) {
                        QFile::remove(destDir.filePath(oldFile));
                    }
                }

                QStringList files = sourceDir.entryList(QDir::Files);
                for (const auto& file : files) {
                    if (!QFile::copy(sourceDir.filePath(file), destDir.filePath(file))) {
                        emit logMessage("WARNING: Failed to copy " + sourceDir.filePath(file) + " to " + destDir.filePath(file) + " (May already exist or permissions issue).");
                    }
                }
                emit logMessage("Copied " + subfolder + " for " + lang + " to Output.");
            }
        }
    }

    // Copy content of "static_localisation" folder to the Output folder,
    // ensuring files go into the correct language subdirectories.
    emit statusMessage("Copying static localisation files");
    emit logMessage("Copying files from 'static_localisation' into language subfolders in Output...");

    // Get the base path for static localisation
    QDir staticLocalisationBaseDir(QCoreApplication::applicationDirPath() + "/static_localisation");

    // Iterate through each language folder in the static localisation directory
    QStringList staticLangFolders = staticLocalisationBaseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& langFolder : staticLangFolders) {
        QString sourceLangPath = staticLocalisationBaseDir.filePath(langFolder);
        QDir sourceDir(sourceLangPath);

        // Define the destination path for this language
        QDir destDir(outputPath + "/" + langFolder);
        if (!destDir.exists()) {
            destDir.mkpath(".");
        }

        // Get the list of files to copy from the source language folder
        QStringList filesToCopy = sourceDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
        for (const QString& file : filesToCopy) {
            QString sourceFilePath = sourceDir.filePath(file);
            QString destFilePath = destDir.filePath(file);

            // Overwrite existing files
            if (QFile::exists(destFilePath)) {
                QFile::remove(destFilePath);
            }

            if (!QFile::copy(sourceFilePath, destFilePath)) {
                emit logMessage("WARNING: Failed to copy " + sourceFilePath + " to " + destFilePath + " (Permissions issue).");
                success = false;
            }
            else {
                emit logMessage("Copied " + file + " to " + destDir.path() + ".");
            }
        }
    }

    if (staticLangFolders.isEmpty()) {
        emit logMessage("INFO: 'static_localisation' folder is empty or not found. Skipping copy.");
    }

    emit progressUpdated(100);

    if (success) {
        emit taskFinished(true, "Cleanup and update task completed successfully, cleaned vanilla files are in Output!");
    }
    else {
        emit taskFinished(false, "Cleanup and update task finished with some errors.");
    }
}
