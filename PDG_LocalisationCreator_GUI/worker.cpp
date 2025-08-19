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
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>
#include <QTimer>
#include <functional>
#include <cmath>


// A struct to hold the API call data for each file.
struct ApiData {
    QString webAppUrl;
    QString spreadsheetId;
    QJsonArray targetSheets;
};

// Constructor for Worker class
Worker::Worker(QObject* parent) : QObject(parent), networkManager(new QNetworkAccessManager(this)) {}

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
    emit statusMessage("Starting localisation creation...");

    // Define constants for the retry mechanism
    const int MAX_RETRIES = 3; // Try a total of 4 times (1 initial + 3 retries)
    const int BASE_RETRY_DELAY_MS = 1000; // Start with a 1-second delay

    // Clear Output folder before starting
    emit logMessage("Clearing contents of Output folder: " + outputPath);
    QDir outputDir(outputPath);
    if (!outputDir.exists()) {
        if (!outputDir.mkpath(".")) {
            emit logMessage("ERROR: Could not create Output folder at: " + outputPath);
            emit taskFinished(false, "Failed to prepare output directory.");
            return;
        }
    }
    outputDir.removeRecursively();
    outputDir.mkpath(".");
    emit logMessage(outputPath + " folder contents cleared.");

    // Prepare file name mappings for each mod type
    std::vector<std::pair<QString, QString>> filenames;
    QString modName = "STNH";
    emit logMessage("Selected STNH Localisation");
    filenames = {
        { "Main Localisation", "STH_main_l_<lang>.yml" },
        { "Ships Localisation", "STH_ships_l_<lang>.yml" },
        { "Modifiers Localisation", "STH_modifiers_l_<lang>.yml" },
        { "Events Localisation", "STH_events_l_<lang>.yml" },
        { "Tech Localisation", "STH_tech_l_<lang>.yml" },
        { "Synced Localisation", "STH_synced_l_<lang>.yml" }
    };

    // Map filenames to their corresponding API data
    QMap<QString, ApiData> apiMappings;
    apiMappings["Main Localisation"] = {
        "https://script.google.com/macros/s/AKfycbzAfQroJ3X4vCkn3NCwDy4WqRdgQs_lLpZ-QmOMsUQZ_lo_Lu8ddnbgoaiiGN6U3Nxk_w/exec",
        "1jQOrWJpAF_9TQVyrrOfxinyTTxvoDJg_E7BHUNEkoio",
        QJsonArray({83571973, 136902998, 329681197, 344846995, 459228252, 491211430, 671222437, 833441231, 962771099, 1302479661, 1329871994, 1435222409, 1444412922, 1485308419, 1487353902, 1711119669, 1846746232, 1947435442, 1981406910, 2031398540})
    };
    apiMappings["Ships Localisation"] = {
        "https://script.google.com/macros/s/AKfycbzAfQroJ3X4vCkn3NCwDy4WqRdgQs_lLpZ-QmOMsUQZ_lo_Lu8ddnbgoaiiGN6U3Nxk_w/exec",
        "19z068O5ARdrXLyswqTeDqcQdhAwA39kI8Gx_nhZPL3I",
        QJsonArray({392244419,932592689,1151223207,1410646937,1933627270, 1937160235,2001315937,2003155773})
    };
    apiMappings["Modifiers Localisation"] = {
        "https://script.google.com/macros/s/AKfycbzAfQroJ3X4vCkn3NCwDy4WqRdgQs_lLpZ-QmOMsUQZ_lo_Lu8ddnbgoaiiGN6U3Nxk_w/exec",
        "1TZylnt8An15CLYlQmy1tjUYvHgMQoosh_x1jC35HOck",
        QJsonArray({12059612,217543803,451677996,874386308,1083916510, 1142139456,1294141213,1625849031,1792340026,1878062444})
    };
    apiMappings["Events Localisation"] = {
        "https://script.google.com/macros/s/AKfycbzAfQroJ3X4vCkn3NCwDy4WqRdgQs_lLpZ-QmOMsUQZ_lo_Lu8ddnbgoaiiGN6U3Nxk_w/exec",
        "1YNdrUt0Ro1w6aiVZR0uSJnnulpzhh4thvy3K1-fJ_qA",
        QJsonArray({154369650,165229678,382025639,430653874,579345446, 587908368,765807036,1054822697,1127044865,1743990087,1758953694, 1781767846,2102085493,2102463089})
    };
    apiMappings["Tech Localisation"] = {
        "https://script.google.com/macros/s/AKfycbzAfQroJ3X4vCkn3NCwDy4WqRdgQs_lLpZ-QmOMsUQZ_lo_Lu8ddnbgoaiiGN6U3Nxk_w/exec",
        "15QcA1M4dX455UYD2GEv3tDJ3P4z3jhK7p5qPMTDFS60",
        QJsonArray({64606495,420715848,525230256,576741004,599811558})
    };
    apiMappings["Synced Localisation"] = {
        "https://script.google.com/macros/s/AKfycbzAfQroJ3X4vCkn3NCwDy4WqRdgQs_lLpZ-QmOMsUQZ_lo_Lu8ddnbgoaiiGN6U3Nxk_w/exec",
        "1MgcmiOr8OMqD6qo5EMwk3ymVenSqAS8MWdo33hKjIPk",
        QJsonArray({747046962,797234541,1545834876,1684242216,2022805766})
    };

    // --- ASYNCHRONOUS LOGIC ---

    int* activeRequests = new int(filenames.size());
    bool* overallSuccess = new bool(true);
    QMap<QString, QString>* fileStatus = new QMap<QString, QString>();

    auto updateStatusMessage = [this, fileStatus]() {
        QMap<QString, int> statusCounts;
        for (const auto& status : *fileStatus) {
            statusCounts[status]++;
        }
        int fetchingCount = statusCounts.value("Fetching", 0);
        int processingCount = statusCounts.value("Processing", 0);
        int completedCount = statusCounts.value("Completed", 0);
        int failedCount = statusCounts.value("Failed", 0);
        int totalCount = fileStatus->size();
        QString status = QString("Fetching: %1 | Processing: %2 | Completed: %3/%4")
            .arg(fetchingCount)
            .arg(processingCount)
            .arg(completedCount + failedCount)
            .arg(totalCount);
        if (failedCount > 0) {
            status += QString(" | Failed: %1").arg(failedCount);
        }
        emit statusMessage(status);
        };

    auto finalizeRequest = [=]() {
        (*activeRequests)--;
        emit progressUpdated(100 * (filenames.size() - *activeRequests) / filenames.size());

        if (*activeRequests == 0) {
            emit logMessage("\nAll API requests have been processed.");
            if (*overallSuccess) {
                emit statusMessage("Task finished successfully!");
                emit taskFinished(true, "Localisation files created successfully!");
            }
            else {
                emit statusMessage("Task finished with errors.");
                emit taskFinished(false, "Localisation creation finished with some errors.");
            }
            delete activeRequests;
            delete overallSuccess;
            delete fileStatus;
        }
        };

    std::function<void(const std::pair<QString, QString>&, const ApiData&, int)> performApiRequest;
    performApiRequest = [=, &performApiRequest](const std::pair<QString, QString>& filePair, const ApiData& apiData, int attemptNum) {
        const QString& currentFileName = filePair.first;

        QJsonObject jsonSettings;
        jsonSettings["exportType"] = "jsonFormat";
        jsonSettings["spreadsheetId"] = apiData.spreadsheetId;
        jsonSettings["exportSheets"] = "custom";
        jsonSettings["targetSheets"] = apiData.targetSheets;
        jsonSettings["minifyData"] = false;
        jsonSettings["exportBoolsAsInts"] = false;
        jsonSettings["ignoreEmptyCells"] = true;
        jsonSettings["includeFirstColumn"] = false;
        jsonSettings["nestedElements"] = false;
        jsonSettings["unwrapSingleRows"] = false;
        jsonSettings["collapseSingleRows"] = false;
        jsonSettings["ignoreColumnsWithPrefix"] = true;
        jsonSettings["ignorePrefix"] = "NOEX_";
        jsonSettings["unwrapSheetsWithPrefix"] = false;
        jsonSettings["unwrapPrefix"] = "US_";
        jsonSettings["collapseSheetsWithPrefix"] = false;
        jsonSettings["collapsePrefix"] = "CS_";
        QJsonObject jsonSubSettings;
        jsonSubSettings["forceString"] = false;
        jsonSubSettings["exportCellArray"] = false;
        jsonSubSettings["exportSheetArray"] = true;
        jsonSubSettings["exportValueArray"] = false;
        QJsonObject advancedSubSettings;
        advancedSubSettings["exportContentsAsArray"] = false;
        advancedSubSettings["exportCellObject"] = false;
        advancedSubSettings["emptyValueFormat"] = "null";
        advancedSubSettings["nullValueFormat"] = "null";
        advancedSubSettings["separatorChar"] = ",";
        advancedSubSettings["forceArray"] = false;
        advancedSubSettings["forceArrayPrefix"] = "JA_";
        advancedSubSettings["forceArrayNest"] = false;
        advancedSubSettings["forceNestedArrayPrefix"] = "NA_";
        jsonSubSettings["advanced"] = advancedSubSettings;
        jsonSettings["json"] = jsonSubSettings;

        QJsonDocument jsonDoc(jsonSettings);
        QByteArray encodedPayload = QUrl::toPercentEncoding(jsonDoc.toJson(QJsonDocument::Compact));
        QUrlQuery urlQuery;
        urlQuery.addQueryItem("settings", encodedPayload);
        QUrl url(apiData.webAppUrl);
        url.setQuery(urlQuery);
        QNetworkRequest request(url);
        QNetworkReply* reply = networkManager->get(request);

        connect(reply, &QNetworkReply::finished, this, [=]() {
            bool requestHandled = false;

            if (reply->error() == QNetworkReply::NoError) {
                (*fileStatus)[currentFileName] = "Processing";
                updateStatusMessage();
                emit logMessage("<- Received response for: " + currentFileName);

                QByteArray responseData = reply->readAll();
                QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
                std::unordered_map<std::string, std::vector<std::string>> translations;
                bool successThisRequest = true;

                if (responseDoc.isObject()) {
                    QJsonObject rootObject = responseDoc.object();
                    for (auto it = rootObject.begin(); it != rootObject.end(); ++it) {
                        if (it.value().isArray()) {
                            QJsonArray categoryArray = it.value().toArray();
                            for (const QJsonValue& itemValue : categoryArray) {
                                if (itemValue.isObject()) {
                                    QJsonObject itemObject = itemValue.toObject();
                                    for (auto locIt = itemObject.begin(); locIt != itemObject.end(); ++locIt) {
                                        QString key = locIt.key();
                                        QString value = locIt.value().toString();

                                        value.replace(QRegularExpression(R"(\s+)"), " ");
                                        value = value.trimmed();

                                        QRegularExpression re(R"(\(([^)]+)\))");
                                        QRegularExpressionMatch match = re.match(key);
                                        if (match.hasMatch()) {
                                            QString language = match.captured(1);
                                            if (language.compare("Braz_Por", Qt::CaseInsensitive) == 0) language = "braz_por";
                                            else language = language.toLower();
                                            if (!value.contains(" localisation (", Qt::CaseInsensitive)) {
                                                translations[language.toStdString()].push_back(value.toStdString());
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    for (const auto& entry : translations) {
                        QString language = QString::fromStdString(entry.first);
                        if (language.toLower() == "italian") continue;
                        QString langLower = language.toLower();
                        QDir currentOutputDir(outputPath);
                        currentOutputDir.mkpath(langLower);
                        QString outFileName = filePair.second;
                        outFileName.replace("<lang>", langLower);
                        QString fullOutputPath = currentOutputDir.filePath(langLower + "/" + outFileName);
                        QFile outputFile(fullOutputPath);
                        if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                            emit logMessage("ERROR: Could not write to file " + fullOutputPath);
                            successThisRequest = false;
                            continue;
                        }
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
                    }
                }
                else {
                    emit logMessage("ERROR: Unexpected JSON for " + currentFileName + ". Expected a JSON object.");
                    successThisRequest = false;
                }

                if (successThisRequest) {
                    (*fileStatus)[currentFileName] = "Completed";
                    emit logMessage("Successfully processed " + currentFileName);
                }
                else {
                    (*fileStatus)[currentFileName] = "Failed";
                    *overallSuccess = false;
                }
                requestHandled = true;
            }
            else {
                emit logMessage(QString("ERROR: Network request failed for %1 (Attempt %2/%3): %4")
                    .arg(currentFileName).arg(attemptNum + 1).arg(MAX_RETRIES + 1).arg(reply->errorString()));

                if (attemptNum < MAX_RETRIES) {
                    int delay = BASE_RETRY_DELAY_MS * static_cast<int>(std::pow(2, attemptNum));
                    emit logMessage(QString("-> Retrying in %1ms...").arg(delay));
                    QTimer::singleShot(delay, this, [=]() {
                        performApiRequest(filePair, apiData, attemptNum + 1);
                        });
                }
                else {
                    emit logMessage(QString("-> Maximum retries reached for %1. This file has failed.").arg(currentFileName));
                    (*fileStatus)[currentFileName] = "Failed";
                    *overallSuccess = false;
                    requestHandled = true;
                }
            }

            reply->deleteLater();
            updateStatusMessage();
            if (requestHandled) {
                finalizeRequest();
            }
            });
        };

    // 1. LAUNCH ALL API REQUESTS CONCURRENTLY (Initial call)
    for (const auto& filePair : filenames) {
        const QString& currentFileName = filePair.first;
        emit logMessage("-> Starting API request for: " + currentFileName);

        if (!apiMappings.contains(currentFileName)) {
            emit logMessage("ERROR: No API mapping found for file: " + currentFileName);
            (*fileStatus)[currentFileName] = "Failed";
            *overallSuccess = false;
            finalizeRequest();
            continue;
        }

        (*fileStatus)[currentFileName] = "Fetching";
        const ApiData& apiData = apiMappings.value(currentFileName);

        performApiRequest(filePair, apiData, 0);
    }
    updateStatusMessage();
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