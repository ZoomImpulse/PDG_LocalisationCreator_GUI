# PDG Localisation Creator GUI

A **Qt-based desktop application** designed to automate the creation and cleanup of localisation files for Stellaris mods made by PDG. This tool streamlines the translation workflow by fetching the latest data directly from Google Sheets via an API, converting it into a game-compatible YML format, and cleaning up vanilla localisation files based on existing mod entries.

<img width="492" height="292" alt="grafik" src="https://github.com/user-attachments/assets/1acb89e6-9531-4dca-8e99-864bbfac558d" />


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
  Fetches localisation data directly from Google Sheets via a [Google Apps Script API](https://github.com/ZoomImpulse/PDG_ExportSheetData), eliminating the need for manual JSON exports.

- **Parallel Asynchronous Processing**  
  All API requests are launched concurrently, significantly speeding up the data fetching process.

- **Robust API Error Handling**  
  Features an automatic retry mechanism with exponential backoff. If a network request fails, the tool will try again several times with increasing delays, making it resilient to temporary server issues.

- **Advanced String Normalization**  
  Automatically cleans and normalizes strings from the API, removing unwanted line breaks while correctly preserving intentional newline characters (`\n`).

- **Vanilla File Cleanup**  
  Scans vanilla game localisation files and removes any entries that are already defined in your mod's files, preventing overrides and reducing file size.

- **Multi-threaded & Responsive UI**  
  All file and network operations run on a separate worker thread, ensuring the user interface remains responsive at all times.

- **Detailed Progress Tracking**  
  A dynamic status bar shows the real-time status of operations (Fetching, Processing, Completed) and a progress bar tracks overall completion.

- **Comprehensive Logging**  
  Generates detailed, timestamped log files for easy monitoring and debugging of all operations.

---

## How it Works

The application operates in two main phases:

### 1. Localisation Creation

- Connects to a Google Apps Script web app URL.
- Sends API requests for all defined localisation categories (Main, Ships, Tech, etc.) **in parallel**.
- If a request fails due to a network error (e.g., "Internal Server Error"), it will **automatically retry** up to 3 times with an increasing delay.
- Parses the JSON response from the API.
- Cleans and normalizes each translation string to handle whitespace and newlines correctly.
- Generates sorted, game-ready `.yml` files (e.g., `STH_main_l_english.yml`) organized into language-specific folders.
- The output directory is cleared before each run to ensure a clean build.

### 2. Localisation Cleanup & Update

- First, it scans all the mod's `.yml` files in the **Output Path** to create a master list of existing localisation tags for each language.
- It then processes all vanilla game `.yml` files from the specified **Vanilla Files Path**.
- It removes any lines from the vanilla files that contain a tag already present in the mod's files or in a predefined removal list.
- The cleaned vanilla files are written to the **Output Path**, organized by language.
- Finally, it copies essential vanilla folders (`name_lists`, `random_names`) and any custom files from a local `static_localisation` folder (located in the executables path) into the output directory.

---

## Getting Started

### Prerequisites

- Qt 5 or newer (Qt 6 is also compatible).
    - Required modules: `QtWidgets`, `QtCore`, `QtNetwork`.
- A C++17 compatible compiler (MSVC, MinGW, or GCC).

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

#### OR Building with Visual Studio

1. Open the `.sln` file (or generate one using `qmake -tp vc`)
2. Choose configuration (`Debug`/`Release`) and platform (e.g., `x64`)
3. Build the solution

#### 3. Deploying Necessary DLLs (only needed initially)
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
2. **Set Paths:**
   - **Output Path:** Choose where the generated YML files and cleaned vanilla files will be saved.
   - **Vanilla Files Path:** Specify the root directory of the vanilla game's localisation files (e.g., `C:/Program Files (x86)/Steam/steamapps/common/Stellaris/localisation`).
3. **Run:** Click the "ENGAGE" button.
4. **Monitor Progress:** The status label provides a detailed breakdown of the current operations, and the progress bar tracks the overall task. Detailed logs are written to the `logs` directory.

## Logging

The application generates timestamped log files in a `logs directory` (e.g., `logs/log_2023-10-27_14-30-00.txt`). These logs provide detailed information about the processing steps, warnings, and errors. Old log files (from previous days) are automatically cleaned up on startup.

## Credits
This project is an evolution of the work originally done by Possseidon and Oninoni in the [PDG_Utilities](https://github.com/oninoni/PDG_Utilities) repository. 

**Many thanks to Oninoni for the foundation!**

## Contact

For any questions, issues, or feedback, please open an issue on the GitHub Issues page.
