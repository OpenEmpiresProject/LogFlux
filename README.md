# LogFlux
Network and File log viewer

## Overview
LogFlux is a lightweight desktop application for viewing and following log data from local files and network sources in real time. It is implemented in modern C++ with a Qt-based user interface and focuses on fast navigation, searching with jump-to-result, auto-follow (tail) mode, and a compact keyboard-friendly UI.

![Screenshot](doc/screenshot_1.png)
![Screenshot](doc/screenshot_2.png)

## Technology & Dependencies
LogFlux is built using the following technologies:
- **Programming Language**: C++17
- **UI / Framework**: Qt 6 (QtCore, QtGui, QtWidgets, QtNetwork)
- **Build System**: CMake (minimum required: 3.16; tested with 3.31.6-msvc6)
- **Generator**: Ninja (recommended)
- **Toolchain**: Microsoft Visual Studio 2022 (MSVC)
- **Optional**: Python 3 (only for the test helper script `tests/send_logs.py`)

### Build Instructions (Windows, MSVC + Ninja)
Once the dependencies are installed and discoverable, follow  cmake steps to build the project. Easiest alternative is to open the project in Visual Studio with Qt VS Tool addon installed.

## Running the Test Sender
A small Python script exists at `tests/send_logs.py` to push sample log lines to the network source for manual testing. Python is not required for the application itself.

python tests/send_logs.py

## Current Features
- Open and follow local log files.
- Hosting TCP server for accepting newline delimited log lines.
- Searching with next/previous result jumping and display of total hits.
- Tailing logs (for both file and network modes).
- Log level based text coloring.
- Jumping to log start and end with `less` sort of shortcuts
- Keyboard navigations (navigating between lines, jumping to start/end of the log)
- Filters.
- Bookmark support.
- Stats; total lines, errors, and warnings.
- Minimap based scrolling.
- Shortcuts for jumping between errors and warnings.
- 
## Planned Features
- Ability to select case sensitivity, whole word and regex for searching.
- Support drag and drop of files.

## Usage
To use LogFlux, simply launch the application and navigate to the desired log file or use network (TCP) server. Utilize the search and filter functionalities to find specific entries quickly.

## Contributing
Contributions are welcome.