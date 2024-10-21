To install the necessary dependencies, use the following commands:

pip install pyinstaller==4.10
pip install PyQt5==5.15.10



To create a standalone executable, use the PyInstaller command:

pyinstaller --onefile --noconsole .\IPC_Search_Tool.py



Once the executable is built, you can run it directly:

1. Navigate to the dist directory.
2. Double-click the IPC_Search_Tool.exe file to start the application.
