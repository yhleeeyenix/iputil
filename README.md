- To set up the environment, ensure you have the following:
  1. Install Python 3.8.xx (download from https://www.python.org/).
  2. Install dependencies:
     - pip install pyinstaller==4.10
     - pip install PyQt5==5.15.10

<br>

- To create a standalone executable, use the PyInstaller command:
  - pyinstaller --onefile --noconsole .\IPC_Search_Tool.py

<br>

- Once the executable is built, you can run it directly:
  1. Navigate to the dist directory.
  2. Double-click the IPC_Search_Tool.exe file to start the application.
