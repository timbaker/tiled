call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86_amd64
mkdir C:\Programming\TileZed\dist6.11.1
cd C:\Programming\TileZed\dist6.11.1
"C:\Programming\QtSDK2015\6.11.1\msvc2022_64\bin\qmake.exe" C:\Programming\TileZed\tiled\tiled.pro -r -spec win32-msvc "CONFIG+=release"
"C:\Programming\QtSDK2015\Tools\QtCreator\bin\jom\jom.exe"
"C:\Programming\TclTk\8.5.x\32bit-mingw\bin\tclsh85.exe" C:\Programming\TileZed\tiled\dist\make-dist6.11.1.tcl 64bit
PAUSE
