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
#include <regex>

// Constructor for Worker class
Worker::Worker(QObject* parent) : QObject(parent) {}

// Slot to start the creation process for a given mod type
void Worker::doCreateTask(int modType)
{
    runCreateProcess(modType);
}

// Slot to start the cleanup process for a given mod type
void Worker::doCleanupTask(int modType)
{
    runCleanupProcess(modType);
}

// Main logic for creating localisation files based on modType
void Worker::runCreateProcess(int modType)
{
    emit progressUpdated(0);
    emit statusMessage("Starting localisation creation"); 

    // Clear Output folder before starting
    emit logMessage("Clearing contents of Output folder");
    emit statusMessage("Clearing previous output");
    QDir outputDir("Output");

    // Create Output directory if it doesn't exist
    if (!outputDir.exists()) {
        if (!QDir::current().mkpath("Output")) {
            emit logMessage("ERROR: Could not create Output folder!");
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
    emit logMessage("Output folder contents cleared.");

    // Prepare file name mappings for each mod type
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

    // Regex for extracting translation entries and escaping
    std::regex translationExp("\"[^(]+\\(([^)]+)\\)\" *: *\"( *[^:]+: *[^]*)\"");
    std::regex escapeExp("\\\\([^])"); 

    bool success = true;
    // Process each input file and generate output files for each language
    for (const auto& filePair : filenames) {
        QString inputPath = "Input/" + filePair.first;
        emit logMessage("\n-> Processing: " + inputPath);
        emit statusMessage("Reading " + filePair.first);

        QFile inputFile(inputPath);

        if (!inputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            emit logMessage("ERROR: Could not open file: " + inputPath);
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
            QDir::current().mkpath("Output/" + langLower);

            QString outFileName = filePair.second;
            outFileName.replace("<lang>", langLower);

            QFile outputFile("Output/" + langLower + "/" + outFileName);
            if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                emit logMessage("ERROR: Could not write to file " + outFileName);
                success = false;
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
    // Emit final result
    if (success) {
        emit taskFinished(true, "Localisation files created successfully!");
    }
    else {
        emit taskFinished(false, "Localisation creation finished with some errors.");
    }
}

// Main logic for cleaning up and updating localisation files based on modType
void Worker::runCleanupProcess(int modType)
{
    emit progressUpdated(0);
    emit statusMessage("Starting localization cleanup and update");
    emit logMessage("Running cleanup process...");

    // List of supported languages (Italian is skipped)
    std::vector<QString> languages = {
        "braz_por", "english", "french", "german", "polish", "russian",
        // "italian",
        "spanish"
    };

    // Define the file templates based on modType
    QMap<QString, QStringList> modFilesTemplates;
    QString modName;
    if (modType == 1) {
        modName = "STNH";
        emit logMessage("Selected STNH Cleanup");
        modFilesTemplates["Output/<lang>/STH_main_l_<lang>.yml"] << "VanillaFiles/<lang>/vanilla_main_l_<lang>.yml";
        modFilesTemplates["Output/<lang>/STH_ships_l_<lang>.yml"] << "VanillaFiles/<lang>/vanilla_ships_l_<lang>.yml";
        modFilesTemplates["Output/<lang>/STH_modifiers_l_<lang>.yml"] << "VanillaFiles/<lang>/vanilla_modifiers_l_<lang>.yml";
        modFilesTemplates["Output/<lang>/STH_tech_l_<lang>.yml"] << "VanillaFiles/<lang>/vanilla_tech_l_<lang>.yml";
        modFilesTemplates["Output/<lang>/STH_events_l_<lang>.yml"] << "VanillaFiles/<lang>/vanilla_events_l_<lang>.yml";
    }
    else if (modType == 2) {
        modName = "SWFR";
        emit logMessage("Selected SWFR Cleanup");
        modFilesTemplates["Output/<lang>/SWFR_main_l_<lang>.yml"] << "VanillaFiles/<lang>/vanilla_main_l_<lang>.yml";
        modFilesTemplates["Output/<lang>/SWFR_ships_l_<lang>.yml"] << "VanillaFiles/<lang>/vanilla_ships_l_<lang>.yml";
        modFilesTemplates["Output/<lang>/SWFR_modifiers_l_<lang>.yml"] << "VanillaFiles/<lang>/vanilla_modifiers_l_<lang>.yml";
        modFilesTemplates["Output/<lang>/SWFR_tech_l_<lang>.yml"] << "VanillaFiles/<lang>/vanilla_tech_l_<lang>.yml";
        modFilesTemplates["Output/<lang>/SWFR_events_l_<lang>.yml"] << "VanillaFiles/<lang>/vanilla_events_l_<lang>.yml";
    }
    else if (modType == 3) {
        modName = "SGP";
        emit logMessage("Selected SGP Cleanup");
        modFilesTemplates["Output/<lang>/SGP_main_l_<lang>.yml"] << "VanillaFiles/<lang>/vanilla_main_l_<lang>.yml";
    }

    // Use std::regex for key extraction, consistent with Cleanup.cpp's approach
    std::regex keyExp("([A-Z0-9_\\\\\\.:-]+):0 \\\""); 
    std::smatch matches;

    std::unordered_map<QString, std::unordered_set<std::string>> usedTags;

    emit statusMessage("Loading existing localization tags...");
    emit logMessage("Loading tags from output files...");

    // First pass: Load existing localization tags from the mod's output files
    for (const auto& lang : languages) {
        QString langLower = lang.toLower();
        for (const QString& outputPathTemplate : modFilesTemplates.keys()) {
            QString outputPath = outputPathTemplate;
            outputPath.replace("<lang>", langLower);

            QFile outputFile(outputPath);
            if (!outputFile.exists()) {
                // If the output file doesn't exist, there are no tags to load from it for this language
                continue;
            }

            if (!outputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                emit logMessage("WARNING: Could not open output file for reading: " + outputPath);
                continue;
            }

            QTextStream in(&outputFile);
            while (!in.atEnd()) {
                QString line = in.readLine();
                std::string stdLine = line.toStdString();
                if (std::regex_search(stdLine, matches, keyExp)) {
                    std::string tag = matches[1].str(); 
                    usedTags[lang].insert(tag);
                }
            }
            outputFile.close();
        }
    }
    emit logMessage("Loaded " + QString::number(usedTags.size()) + " languages' tags.");

    emit statusMessage("Updating localization files from vanilla...");
    emit logMessage("Processing vanilla files and filtering...");

    // Second pass: Process vanilla files and create updated output files
    bool success = true;
    for (const auto& lang : languages) {
        QString langLower = lang.toLower();
        if (langLower == "italian") {
            emit logMessage("Skipping Italian language for cleanup.");
            continue;
        }

        for (const auto& outputPathTemplate : modFilesTemplates.keys()) {
            QString outputPath = outputPathTemplate;
            outputPath.replace("<lang>", langLower);

            QStringList vanillaInputPaths = modFilesTemplates.value(outputPathTemplate); 
            if (vanillaInputPaths.isEmpty()) continue; 

            QString vanillaInputPath = vanillaInputPaths.first();
            vanillaInputPath.replace("<lang>", langLower);

            QFile vanillaFile(vanillaInputPath);
            if (!vanillaFile.exists()) {
                emit logMessage("WARNING: Vanilla input file does not exist: " + vanillaInputPath);
                continue;
            }
            if (!vanillaFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                emit logMessage("ERROR: Could not open vanilla file for reading: " + vanillaInputPath);
                success = false;
                continue;
            }

            // Prepare the output directory
            QDir outputDir;
            outputDir.mkpath(QFileInfo(outputPath).path());

            QFile outputFile(outputPath + ".temp"); // Write to a temp file first
            if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                emit logMessage("ERROR: Could not create temporary output file: " + outputPath + ".temp");
                success = false;
                vanillaFile.close();
                continue;
            }

            QTextStream inVanilla(&vanillaFile);
            QTextStream outOutput(&outputFile);
            outOutput.setEncoding(QStringConverter::Utf8);
            outOutput.setGenerateByteOrderMark(true);

            outOutput << "l_" << langLower << ":\n"; // Write YAML header

            while (!inVanilla.atEnd()) {
                QString line = inVanilla.readLine();
                std::string stdLine = line.toStdString(); 
                if (std::regex_search(stdLine, matches, keyExp)) {
                    std::string tag = matches[1].str();
                    // Only include lines whose tags are NOT found in usedTags (i.e., new vanilla tags)
                    if (usedTags.find(lang) == usedTags.end() || usedTags[lang].find(tag) == usedTags[lang].end()) {
                        outOutput << line << "\n";
                    }
                }
                else {
                    // Include lines that don't match the regex (e.g., comments, empty lines, other structures)
                    outOutput << line << "\n";
                }
            }
            vanillaFile.close();
            outputFile.close();

            // Replace original file with temp file
            if (QFile::remove(outputPath)) {
                if (!QFile::rename(outputPath + ".temp", outputPath)) {
                    emit logMessage("ERROR: Could not rename temp file to " + outputPath);
                    success = false;
                }
            }
            else {
                // Original file didn't exist or couldn't be removed, try to just rename
                if (!QFile::rename(outputPath + ".temp", outputPath)) {
                    emit logMessage("ERROR: Could not rename temp file to " + outputPath);
                    success = false;
                }
            }
            emit logMessage("Updated: " + outputPath);
        }
    }

    emit statusMessage("Copying name lists");
    emit logMessage("\nCopying name_lists and random_names"); 
    // Copy name_lists and random_names folders for each language
    for (const auto& lang : languages) {
        QStringList subfoldersToCopy = { "name_lists", "random_names" };
        for (const auto& subfolder : subfoldersToCopy) {
            QDir sourceDir("VanillaFiles/" + lang + "/" + subfolder);
            if (sourceDir.exists()) {
                QDir destDir("Output/" + lang + "/" + subfolder);
                if (!destDir.exists()) destDir.mkpath("."); // Create destination path
                else {
                    // Clear existing files in destDir to ensure clean copy
                    QStringList oldFiles = destDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
                    for (const QString& oldFile : oldFiles) {
                        QFile::remove(destDir.filePath(oldFile));
                    }
                }

                QStringList files = sourceDir.entryList(QDir::Files);
                for (const auto& file : files) {
                    // Use QFile::copy which is platform-independent
                    if (!QFile::copy(sourceDir.filePath(file), destDir.filePath(file))) {
                        emit logMessage("WARNING: Failed to copy " + sourceDir.filePath(file) + " to " + destDir.filePath(file) + " (May already exist or permissions issue).");
                    }
                }
                emit logMessage("Copied " + subfolder + " for " + lang);
            }
        }
    }
    emit progressUpdated(100);

    // Emit final result
    if (success) {
        emit taskFinished(true, "Cleanup and update task completed successfully!");
    }
    else {
        emit taskFinished(false, "Cleanup and update task finished with some errors.");
    }
}