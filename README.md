# PDG Localisation Creator GUI

A **Qt-based desktop application** designed to streamline the creation and cleanup of localisation files for Stellaris mods made by PDG.  
This tool helps PDG members manage translation files by converting JSON input into a game-compatible YML format and cleaning up vanilla localisation files based on existing mod entries.

---

## Table of Contents

- [Features](#features)  
- [How it Works](#how-it-works)  
- [Getting Started](#getting-started)  
  - [Prerequisites](#prerequisites)  
  - [Building the Project](#building-the-project)  
  - [Usage](#usage) 
- [Logging](#logging)
- [Contact](#contact)  

---

## Features

- **Mod-Specific Localisation Generation**  
  Supports creating localisation files for:
  - STNH (Star Trek: New Horizons)
  - SWFR (Star Wars: Fallen Republic)

- **JSON to YML Conversion**  
  Converts `.json` input files into `.yml` format, suitable for game localisation.

- **Vanilla File Cleanup**  
  Cleans vanilla game localisation files by removing entries already present in your mod's files.

- **Multi-threaded Operations**  
  Uses a separate worker thread for file processing to keep the UI responsive.

- **Progress Tracking & Logging**  
  Progress bar and detailed timestamped logs for monitoring and debugging.

- **Intuitive GUI**  
  Simple interface for selecting mod types, paths, and initiating processes.

---

## How it Works

The application operates in two main phases:

### 1. Localisation Creation

- Reads JSON input files (e.g., `ST New Horizons - Main Localisation.json`)
- Parses language-specific translation entries
- Generates `.yml` files (e.g., `STH_main_l_english.yml`) organized by language
- Clears the output directory before each run

### 2. Localisation Cleanup & Update

- Triggered automatically after successful creation
- Loads all tags from newly created mod `.yml` files
- Processes vanilla game `.yml` localisation files
- Removes vanilla entries that duplicate mod entries
- Copies essential vanilla folders (e.g., `name_lists`, `random_names`) into the output directory

---

## Getting Started

### Prerequisites

- Qt 5 or newer (with `QtWidgets`, `QtCore`, `QSettings`, etc.)
- A C++ compiler (MSVC, MinGW, or GCC)

---

### Building the Project

#### 1. Clone the Repository

```bash
git clone https://github.com/ZoomImpulse/PDG_LocalisationCreator_GUI.git
cd PDG-Localisation-Creator
```

#### 2. Building with Qt Creator

1. Open the .pro file in Qt Creator
2. Select a suitable kit (e.g., Desktop Qt 5.x.x MinGW 64-bit)
3. Run qmake
4. Build the project

#### 3. Building with Visual Studio

1. Open the `.sln` file (or generate one using `qmake -tp vc`)
2. Choose configuration (`Debug`/`Release`) and platform (e.g., `x64`)
3. Build the solution

#### 4. Deploying Necessary DLLs (only needed initially)
After building, your executable (`.exe`) requires Qt's DLLs to run on other machines. Use Qt's `windeployqt` tool for this:

1. **Navigate to your build output directory** in the command prompt or terminal. This is usually where your `.exe` file is located.
2. **Run** `windeploygt`:
   
   ```bash
   # Example (adjust path to your Qt installation)
   C:\Qt\5.15.2\msvc2019_64\bin\windeployqt.exe PDG_LocalisationCreator_GUI.exe
   ```
   This command will copy all necessary Qt DLLs and other dependencies (like plugins) into your application's directory, making it self-contained for distribution.
   
### Usage

1. **Run the application.**
2. **Select Mod Type: Choose between "STNH" or "SWFR" using the radio buttons.**
3. **Set Paths:**
   - **Input Path:** Select the directory containing your mod's JSON localisation files.
   - **Output Path:** Choose where the generated YML files and cleaned vanilla files will be saved.
   - **Vanilla Files Path:** Specify the root directory of the vanilla game's localisation files (e.g., `C:/Program Files (x86)/Steam/steamapps/common/Stellaris/localisation`).
4. **Run:** Click the "ENGAGE" button.
5. **Monitor Progress:** The status label and progress bar will update as the application processes files. Detailed logs will be written to a logs directory within the application's executable path.

## Logging

The application generates timestamped log files in a `logs directory (e.g., `logs/log_2023-10-27_14-30-00.txt`). These logs provide detailed information about the processing steps, warnings, and errors. Old log files (from previous days) are automatically cleaned up on startup.

## Contact

For any questions, issues, or feedback, please open an issue on the GitHub Issues page.
