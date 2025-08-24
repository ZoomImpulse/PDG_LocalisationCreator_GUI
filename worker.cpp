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
#include <QElapsedTimer>


// A struct to hold the API call data for each file.
struct ApiData {
    QString webAppUrl;
    QString spreadsheetId;
    QJsonArray targetSheets;
};

// Constructor for Worker class
Worker::Worker(QObject* parent) : QObject(parent), networkManager(new QNetworkAccessManager(this)) {}

// Request cooperative cancellation (abort in-flight network replies)
void Worker::requestCancel()
{
    m_cancelRequested.store(true);
    QMutexLocker locker(&m_mutex);
    for (QNetworkReply* r : m_activeReplies) {
        if (r) r->abort();
    }
}

// Slot to start the creation process for a given mod type
void Worker::doCreateTask(int modType, const QString& inputPath, const QString& outputPath, const QString& vanillaPath)
{
    m_cancelRequested.store(false);
    runCreateProcess(modType, inputPath, outputPath, vanillaPath);
}

// Slot to start the cleanup process for a given mod type
void Worker::doCleanupTask(int modType, const QString& inputPath, const QString& outputPath, const QString& vanillaPath)
{
    m_cancelRequested.store(false);
    runCleanupProcess(modType, inputPath, outputPath, vanillaPath);
}

// Main logic for creating localisation files based on modType
void Worker::runCreateProcess(int modType, const QString& inputPath, const QString& outputPath, const QString& vanillaPath)
{
    QElapsedTimer totalTimerCreate; totalTimerCreate.start();
    emit progressUpdated(0);
    emit statusMessage("Starting localisation creation...");

    // Define constants for the retry mechanism
    const int MAX_RETRIES = 3; // Try a total of 4 times (1 initial + 3 retries)
    const int BASE_RETRY_DELAY_MS = 1000; // Start with a 1-second delay

    // Clear Output folder before starting
    emit logMessage("INFO: Clearing contents of Output folder: " + outputPath);
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
    emit logMessage("INFO: " + outputPath + " folder contents cleared.");

    // Progress calibration across phases
    const int PREP_PROGRESS = 5;          // after setup
    const int API_PROGRESS_RANGE = 90;    // main API work spans 5..95
    const int FINALIZE_PROGRESS = 100;    // final step sets to 100
    emit progressUpdated(PREP_PROGRESS);

    // Prepare file name mappings for each mod type
    std::vector<std::pair<QString, QString>> filenames;
    QString modName = "STNH";
    emit logMessage("INFO: Selected STNH Localisation");
    filenames = {
        { "Main Localisation", "STH_main_l_<lang>.yml" },
        { "Ships Localisation", "STH_ships_l_<lang>.yml" },
        { "Modifiers Localisation", "STH_modifiers_l_<lang>.yml" },
        { "Events Localisation", "STH_events_l_<lang>.yml" },
        { "Tech Localisation", "STH_tech_l_<lang>.yml" },
        { "Synced Localisation", "STH_synced_l_<lang>.yml" }
    };

    // Map filenames to their corresponding API data (targetSheets intentionally left empty; must be provided by user selection)
    QMap<QString, ApiData> apiMappings;
    apiMappings["Main Localisation"] = {
        "https://script.google.com/macros/s/AKfycbzAfQroJ3X4vCkn3NCwDy4WqRdgQs_lLpZ-QmOMsUQZ_lo_Lu8ddnbgoaiiGN6U3Nxk_w/exec",
        "1jQOrWJpAF_9TQVyrrOfxinyTTxvoDJg_E7BHUNEkoio",
        QJsonArray()
    };
    apiMappings["Ships Localisation"] = {
        "https://script.google.com/macros/s/AKfycbzAfQroJ3X4vCkn3NCwDy4WqRdgQs_lLpZ-QmOMsUQZ_lo_Lu8ddnbgoaiiGN6U3Nxk_w/exec",
        "19z068O5ARdrXLyswqTeDqcQdhAwA39kI8Gx_nhZPL3I",
        QJsonArray()
    };
    apiMappings["Modifiers Localisation"] = {
        "https://script.google.com/macros/s/AKfycbzAfQroJ3X4vCkn3NCwDy4WqRdgQs_lLpZ-QmOMsUQZ_lo_Lu8ddnbgoaiiGN6U3Nxk_w/exec",
        "1TZylnt8An15CLYlQmy1tjUYvHgMQoosh_x1jC35HOck",
        QJsonArray()
    };
    apiMappings["Events Localisation"] = {
        "https://script.google.com/macros/s/AKfycbzAfQroJ3X4vCkn3NCwDy4WqRdgQs_lLpZ-QmOMsUQZ_lo_Lu8ddnbgoaiiGN6U3Nxk_w/exec",
        "1YNdrUt0Ro1w6aiVZR0uSJnnulpzhh4thvy3K1-fJ_qA",
        QJsonArray()
    };
    apiMappings["Tech Localisation"] = {
        "https://script.google.com/macros/s/AKfycbzAfQroJ3X4vCkn3NCwDy4WqRdgQs_lLpZ-QmOMsUQZ_lo_Lu8ddnbgoaiiGN6U3Nxk_w/exec",
        "15QcA1M4dX455UYD2GEv3tDJ3P4z3jhK7p5qPMTDFS60",
        QJsonArray()
    };
    apiMappings["Synced Localisation"] = {
        "https://script.google.com/macros/s/AKfycbzAfQroJ3X4vCkn3NCwDy4WqRdgQs_lLpZ-QmOMsUQZ_lo_Lu8ddnbgoaiiGN6U3Nxk_w/exec",
        "1MgcmiOr8OMqD6qo5EMwk3ymVenSqAS8MWdo33hKjIPk",
        QJsonArray()
    };

    // Require user-provided selections; error out if none
    if (m_selectionsJson.trimmed().isEmpty()) {
        emit statusMessage("No sheets selected. Please open 'Select Sheets' and choose at least one.");
        emit taskFinished(false, "No sheets selected.");
        return;
    }

    QJsonDocument selDoc = QJsonDocument::fromJson(m_selectionsJson.toUtf8());
    if (!selDoc.isObject()) {
        emit statusMessage("Invalid selections data. Please reselect sheets.");
        emit taskFinished(false, "Invalid selections JSON.");
        return;
    }
    {
        QJsonObject selObj = selDoc.object();
        for (auto it = selObj.begin(); it != selObj.end(); ++it) {
            if (!it.value().isArray()) continue;
            const QString category = it.key();
            if (apiMappings.contains(category)) {
                QJsonArray ids = it.value().toArray();
                if (!ids.isEmpty()) {
                    apiMappings[category].targetSheets = ids;
                }
            }
        }
    }
    // Log selection summary
    {
        QStringList catSummaries;
        for (auto it = apiMappings.begin(); it != apiMappings.end(); ++it) {
            QStringList idStrs;
            for (const auto& v : it->targetSheets) idStrs << v.toVariant().toString();
            if (!idStrs.isEmpty()) {
                catSummaries << (it.key() + ": [" + idStrs.join(", ") + "]");
            }
        }
        if (!catSummaries.isEmpty()) {
            emit logMessage("INFO: Selected sheets — " + catSummaries.join(", "));
        }
    }
    // Validate that at least one category has target sheets
    bool anySelected = false;
    for (auto it = apiMappings.begin(); it != apiMappings.end(); ++it) {
        if (!it->targetSheets.isEmpty()) { anySelected = true; break; }
    }
    if (!anySelected) {
        emit statusMessage("No sheets selected for any category. Please choose at least one sheet.");
        emit taskFinished(false, "No target sheets selected.");
        return;
    }

    // --- ASYNCHRONOUS LOGIC ---

    int* activeRequests = new int(static_cast<int>(filenames.size()));
    bool* overallSuccess = new bool(true);
    QMap<QString, QString>* fileStatus = new QMap<QString, QString>();
    int* totalRetries = new int(0);
    int* totalFilesSucceeded = new int(0);
    int* totalFilesFailed = new int(0);

    auto updateStatusMessage = [this, fileStatus]() {
        QMap<QString, int> statusCounts;
        for (const auto& status : *fileStatus) {
            statusCounts[status]++;
        }
        int fetchingCount = statusCounts.value("Fetching", 0);
        int processingCount = statusCounts.value("Processing", 0);
        // Simplified header text per request
        if (fetchingCount > 0 || processingCount > 0) {
            emit statusMessage("Fetching and processing data...");
        }
        emit fetchActive(fetchingCount > 0);
        emit processActive(processingCount > 0);
        };

    auto finalizeRequest = [=]() {
        (*activeRequests)--;
        int completed = (int)filenames.size() - *activeRequests;
        int scaled = PREP_PROGRESS + (completed * API_PROGRESS_RANGE) / (int)filenames.size();
        if (scaled > 95) scaled = 95; // cap before finalization
        emit progressUpdated(scaled);

        if (*activeRequests == 0) {
            emit logMessage("INFO: All API requests have been processed.");
            if (m_cancelRequested.load()) {
                emit statusMessage("Cancelled by user.");
                emit taskFinished(false, "Operation cancelled.");
            }
            else if (*overallSuccess) {
                emit statusMessage("Task finished successfully!");
                emit progressUpdated(FINALIZE_PROGRESS);
                emit taskFinished(true, "Localisation files created successfully!");
            }
            else {
                emit statusMessage("Task finished with errors.");
                emit progressUpdated(FINALIZE_PROGRESS);
                emit taskFinished(false, "Localisation creation finished with some errors.");
            }
            emit logMessage(QString("SUMMARY: Create process duration: %1 ms; files ok: %2, failed: %3, retries: %4")
                .arg(totalTimerCreate.elapsed()).arg(*totalFilesSucceeded).arg(*totalFilesFailed).arg(*totalRetries));
            delete activeRequests;
            delete overallSuccess;
            delete fileStatus;
            delete totalRetries;
            delete totalFilesSucceeded;
            delete totalFilesFailed;
        }
        };

    std::function<void(const std::pair<QString, QString>&, const ApiData&, int)> performApiRequest;
    performApiRequest = [=, &performApiRequest](const std::pair<QString, QString>& filePair, const ApiData& apiData, int attemptNum) {
        const QString& currentFileName = filePair.first;
        QElapsedTimer* requestTimer = new QElapsedTimer();
        requestTimer->start();

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
        {
            QMutexLocker locker(&m_mutex);
            m_activeReplies.append(reply);
        }

        connect(reply, &QNetworkReply::finished, this, [=]() {
            bool requestHandled = false;

            if (reply->error() == QNetworkReply::NoError) {
                (*fileStatus)[currentFileName] = "Processing";
                updateStatusMessage();
                emit logMessage("INFO: Received response for: " + currentFileName);

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
                    if (translations.empty()) {
                        emit logMessage("WARNING: No translations received for " + currentFileName);
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
                        int entriesWrittenThisLang = 0;
                        for (const auto& line : sortedLines) {
                            out << " " << QString::fromStdString(line) << "\n";
                            entriesWrittenThisLang++;
                        }
                        outputFile.close();
                        emit logMessage(QString("INFO: Wrote %1 entries to %2").arg(entriesWrittenThisLang).arg(fullOutputPath));
                    }
                }
                else {
                    emit logMessage("ERROR: Unexpected JSON for " + currentFileName + ". Expected a JSON object.");
                    successThisRequest = false;
                }

                if (successThisRequest) {
                    (*fileStatus)[currentFileName] = "Completed";
                    emit logMessage("INFO: Successfully processed " + currentFileName);
                    (*totalFilesSucceeded)++;
                }
                else {
                    (*fileStatus)[currentFileName] = "Failed";
                    *overallSuccess = false;
                    (*totalFilesFailed)++;
                }
                requestHandled = true;
            }
            else {
                emit logMessage(QString("ERROR: Network request failed for %1 (Attempt %2/%3): %4")
                    .arg(currentFileName).arg(attemptNum + 1).arg(MAX_RETRIES + 1).arg(reply->errorString()));

                if (!m_cancelRequested.load() && attemptNum < MAX_RETRIES) {
                    int delay = BASE_RETRY_DELAY_MS * static_cast<int>(std::pow(2, attemptNum));
                    emit logMessage(QString("INFO: Retrying in %1ms...").arg(delay));
                    QTimer::singleShot(delay, this, [=]() {
                        (*totalRetries)++;
                        performApiRequest(filePair, apiData, attemptNum + 1);
                        });
                }
                else {
                    if (m_cancelRequested.load()) {
                        emit logMessage(QString("INFO: Cancellation active, not retrying %1.").arg(currentFileName));
                    } else {
                        emit logMessage(QString("WARNING: Maximum retries reached for %1. This file has failed.").arg(currentFileName));
                    }
                    (*fileStatus)[currentFileName] = "Failed";
                    *overallSuccess = false;
                    requestHandled = true;
                }
            }

            {
                QMutexLocker locker(&m_mutex);
                m_activeReplies.removeAll(reply);
            }
            emit logMessage(QString("DEBUG: API request for '%1' took %2 ms").arg(currentFileName).arg(requestTimer->elapsed()));
            delete requestTimer;
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
        emit logMessage("INFO: Starting API request for: " + currentFileName);

        if (!apiMappings.contains(currentFileName)) {
            emit logMessage("ERROR: No API mapping found for file: " + currentFileName);
            (*fileStatus)[currentFileName] = "Failed";
            *overallSuccess = false;
            finalizeRequest();
            continue;
        }

        if (m_cancelRequested.load()) {
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
    QElapsedTimer totalTimerCleanup; totalTimerCleanup.start();
    emit progressUpdated(0);
    emit statusMessage("Starting localization cleanup and update");
    emit logMessage("INFO: Running cleanup process (writing cleaned vanilla to Output)...");
    // Log of cleanup config will be printed after languages are defined

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

    emit logMessage("INFO: Cleanup config — vanilla=" + vanillaPath + ", output=" + outputPath + ", langs=" + QString::number(static_cast<int>(languages.size())));

    // Define the file templates based on modType - used only for First Pass (loading mod tags)
    QMap<QString, QStringList> modFilesTemplates;
    QString modName = "STNH";
    emit logMessage("INFO: Selected STNH Cleanup");
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

    emit statusMessage("Loading existing keys from output files for cleanup...");
    emit logMessage("INFO: Loading existing keys from output files for cleanup...");

    // First pass: Load existing localization tags from the mod's output files
    // Calculate progress for this section
    int currentProgress = 0;
    int progressPerLanguage = (modFilesTemplates.keys().size() > 0) ? (20 / static_cast<int>(languages.size())) : 0; // Allocate 20% for this phase

    for (const auto& lang : languages) {
        if (m_cancelRequested.load()) {
            emit statusMessage("Cancelling…");
            emit taskFinished(false, "Operation cancelled.");
            return;
        }
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
    long long totalKeysRemoved = 0;
    int progressPerVanillaFile = (static_cast<int>(languages.size()) * modFilesTemplates.keys().size() > 0) ? (70 / (static_cast<int>(languages.size()) * modFilesTemplates.keys().size())) : 0; // Allocate 70% for this phase (20-90)
    int filesProcessed = 0;

    for (const auto& lang : languages) {
        QElapsedTimer langTimer; langTimer.start();
        if (lang.toLower() == "italian") {
            emit logMessage("INFO: Skipping Italian language for cleanup.");
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

        int filesProcessedForLang = 0;
        long long keysRemovedForLang = 0;
        for (const QString& vanillaFileName : vanillaFiles) {
            if (m_cancelRequested.load()) {
                emit statusMessage("Cancelling…");
                emit taskFinished(false, "Operation cancelled.");
                return;
            }
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
            int removedInThisFile = 0;

            while (!inVanilla.atEnd()) {
                QString line = inVanilla.readLine();
                std::string stdLine = line.toStdString();

                if (std::regex_search(stdLine, matches, keyExp)) {
                    std::string tag = matches[1].str();
                    // Check if the tag is either a mod tag OR a hardcoded key to remove
                    if ((usedTags.find(lang) != usedTags.end() && usedTags[lang].find(tag) != usedTags[lang].end()) || keysToRemove.count(tag) > 0) {
                        fileChanged = true;
                        removedInThisFile++;
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
                emit logMessage(QString("INFO: UPDATED %1 (removed %2 keys)").arg(cleanedOutputPath).arg(removedInThisFile));
                keysRemovedForLang += removedInThisFile;
                totalKeysRemoved += removedInThisFile;
            }
            else {
                emit logMessage("INFO: No changes — skipped write for " + vanillaFileName);
            }
            filesProcessed++;
            filesProcessedForLang++;
            emit progressUpdated(20 + qMin(filesProcessed * progressPerVanillaFile, 70)); // Progress from 20% to 90%
        }
        emit logMessage(QString("INFO: Cleanup summary for %1 — processed: %2 files, removed: %3 keys")
            .arg(lang).arg(filesProcessedForLang).arg(keysRemovedForLang));
        emit logMessage(QString("DEBUG: Cleanup for language '%1' took %2 ms").arg(lang).arg(langTimer.elapsed()));
    }
    emit progressUpdated(90); // Ensure it's at 90% before copying name lists

    emit statusMessage("Copying name lists");
    emit logMessage("Copying name_lists and random_names to Output folder...");
    // Copy name_lists and random_names folders for each language
    for (const auto& lang : languages) {
        if (m_cancelRequested.load()) {
            emit statusMessage("Cancelling…");
            emit taskFinished(false, "Operation cancelled.");
            return;
        }
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
                emit logMessage("INFO:Copied " + subfolder + " for " + lang + " to Output.");
            }
        }
    }

    // Copy static localisation files if present
    emit statusMessage("Copying static localisation files");
    emit logMessage("INFO: Copying files from 'static_localisation' into language subfolders in Output...");
    QDir staticLocalisationBaseDir("static_localisation");
    QStringList staticLangFolders;
    if (staticLocalisationBaseDir.exists()) {
        staticLangFolders = staticLocalisationBaseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& langFolder : staticLangFolders) {
            if (m_cancelRequested.load()) {
                emit statusMessage("Cancelling…");
                emit taskFinished(false, "Operation cancelled.");
                return;
            }
            QString sourceLangPath = staticLocalisationBaseDir.filePath(langFolder);
            QDir sourceDir(sourceLangPath);
            if (!sourceDir.exists()) continue;
            QDir destDir(outputPath + "/" + langFolder);
            if (!destDir.exists()) destDir.mkpath(".");
            QStringList filesToCopy = sourceDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
            for (const QString& file : filesToCopy) {
                if (m_cancelRequested.load()) {
                    emit statusMessage("Cancelling…");
                    emit taskFinished(false, "Operation cancelled.");
                    return;
                }
                QString sourceFilePath = sourceDir.filePath(file);
                QString destFilePath = destDir.filePath(file);
                if (!QFile::copy(sourceFilePath, destFilePath)) {
                    emit logMessage("WARNING: Failed to copy " + sourceFilePath + " to " + destFilePath + " (Permissions issue).");
                    success = false;
                } else {
                    emit logMessage("Copied " + file + " to " + destDir.path() + ".");
                }
            }
        }
    } else {
        emit logMessage("INFO: 'static_localisation' folder is not found. Skipping copy.");
    }

    emit progressUpdated(100);

    emit logMessage(QString("SUMMARY: Cleanup process duration: %1 ms; files: %2; keys removed: %3")
        .arg(totalTimerCleanup.elapsed()).arg(filesProcessed).arg(totalKeysRemoved));
    if (success) {
        emit taskFinished(true, "Cleanup and update task completed successfully, cleaned vanilla files are in Output!");
    }
    else {
        emit taskFinished(false, "Cleanup and update task finished with some errors.");
    }
}