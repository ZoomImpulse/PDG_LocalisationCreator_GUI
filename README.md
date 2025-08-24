# PDG Localisation Creator GUI

A **Qt-based desktop application** designed to automate the creation and cleanup of localisation files for Stellaris mods made by PDG. This tool streamlines the translation workflow by fetching the latest data directly from Google Sheets via an API, converting it into a game-compatible YML format, and cleaning up vanilla localisation files based on existing mod entries.

<img width="502" height="282" alt="Screenshot 2025-08-24 160254" src="https://github.com/user-attachments/assets/53ce9ca3-6249-4f8b-8662-d6d987da2607" />

---

## Table of Contents

- [Features](#features)  
- [How it Works](#how-it-works)  
- [Getting Started](#getting-started)  
  - [Prerequisites](#prerequisites)  
  - [Building the Project](#building-the-project)  
  - [Usage](#usage) 
- [Logging](#logging)
- [Credits](#credits)
- [Contact](#contact)  

---

## Features

- **Direct Google Sheets Integration**  
  Fetches localisation data directly from Google Sheets via a [Google Apps Script API](https://github.com/ZoomImpulse/PDG_ExportSheetData), eliminating manual JSON exports.

- **Sheet Selection Dialog (per category)**  
  Built-in UI to choose which sheets to export for each category (Main, Ships, Modifiers, Events, Tech, Synced). Selections are saved and restored across runs.

  <img width="802" height="552" alt="Screenshot 2025-08-24 160652" src="https://github.com/user-attachments/assets/e5b867be-0e9e-4509-b063-a5f24b779329" />

- **Parallel Asynchronous Processing**  
  Launches API requests concurrently for faster data fetching.

- **Robust API Error Handling with Retries**  
  Automatic exponential backoff retries on network failures, with clear logs of attempts and totals.

- **Advanced String Normalization**  
  Cleans and normalizes strings from the API, removing unwanted whitespace while preserving intended newlines (`\n`).

- **Vanilla File Cleanup (auto-run after create)**  
  Removes any vanilla entries overridden by the mod and applies a hardcoded removal list. Skips Italian, copies `name_lists` and `random_names`, and merges `static_localisation/` if present.

- **Responsive UI with Progress Overlay**  
  All work runs on a worker thread. An in-window overlay shows overall progress plus fetching/processing indicators.

  <img width="502" height="282" alt="Screenshot 2025-08-24 160318" src="https://github.com/user-attachments/assets/8d02fe8c-3c36-407b-b36b-0618d0a2317b" />

- **Comprehensive Logging with Timing**  
  Timestamped logs with level prefixes (INFO, WARNING, ERROR, DEBUG, SUMMARY). Per-step timings (per request and per language) and total durations using QElapsedTimer.

- **Config Persistence**  
  Remembers Output/Vanilla paths and selected sheet IDs via a local `config.ini`.

---

## How it Works

The application operates in two main phases:

### 1. Localisation Creation

- Requires explicit sheet selections per category via the built-in dialog.
- Connects to a Google Apps Script web app URL.
- Sends API requests for the selected sheets per category **in parallel**.
- Automatic retries with exponential backoff on failures.
- Parses JSON, normalizes strings, and writes sorted YML output per language.
- Clears the Output folder at the start of each run.

### 2. Localisation Cleanup & Update (auto-run)

- Automatically starts after creation succeeds.
- Scans the generated mod YMLs in Output to collect used tags per language.
- Processes vanilla YMLs from the Vanilla path and removes overridden tags and a hardcoded removal list.
- Writes cleaned vanilla YMLs to Output/<lang>/ and copies `name_lists` and `random_names` folders.
- Optionally merges any files from a local `static_localisation/<lang>/` into Output.

---

## Getting Started

### Prerequisites

- Qt 6 (tested). Required modules: `QtWidgets`, `QtCore`, `QtNetwork`.
- A C++17 compatible compiler (MSVC recommended).

---

### Building the Project

#### 1. Clone the Repository

```bash
git clone https://github.com/ZoomImpulse/PDG_LocalisationCreator_GUI.git
cd PDG_LocalisationCreator_GUI
```

#### 2. Building with Visual Studio (recommended)

1. Open `PDG_LocalisationCreator_GUI.sln` in Visual Studio (with Qt VS Tools if installed).
2. Choose configuration (`Debug`/`Release`) and platform (`x64`).
3. Build the solution.

#### Alternative: Qt Creator

- Open the `.vcxproj` or use your preferred CMake/qmake setup that includes Qt 6 and MSVC.

#### 3. Deploying Necessary DLLs (Windows)
After building, your executable (`.exe`) requires Qt's DLLs to run on other machines. Use Qt's `windeployqt` tool for this:

1. **Navigate to your build output directory** in the command prompt or terminal. This is usually where your `.exe` file is located.
2. **Run** `windeployqt`:
   
   ```bash
   # Example (adjust path to your Qt installation)
   C:\Qt\6.x.x\msvc2019_64\bin\windeployqt.exe PDG_LocalisationCreator_GUI.exe
   ```
   This command will copy all necessary Qt DLLs and other dependencies (like plugins) into your application's directory, making it self-contained for distribution.
   
### Usage

1. Run the application.
2. Set Paths:
   - Output Path: destination for generated mod YMLs and cleaned vanilla files.
   - Vanilla Files Path: your Stellaris `localisation/` root.
3. Click "Select Sheets" and choose desired sheets per category. Press OK.
4. Click "ENGAGE" to start. Creation runs first; on success, Cleanup runs automatically.
5. Monitor the in-window progress overlay (Fetching/Processing indicators + overall progress).
6. Check `Output/` for results and `logs/` for detailed logs.

## Logging

- Logs are written to `logs/log_YYYY-MM-DD_hh-mm-ss.txt` per run.
- Messages include level prefixes: `INFO`, `WARNING`, `ERROR`, `DEBUG`, `SUMMARY`.
- Timing details:
  - Create: per API-request durations and a total duration summary.
  - Cleanup: per-language durations, per-file update counts, and total summary (files and keys removed).
- Old logs from previous days are automatically deleted on startup.

## Credits
This project is an evolution of the work originally done by Possseidon and Oninoni in the [PDG_Utilities](https://github.com/oninoni/PDG_Utilities) repository. 

**Many thanks to Oninoni for the foundation!**

## Contact

For any questions, issues, or feedback, please open an issue on the GitHub Issues page.
