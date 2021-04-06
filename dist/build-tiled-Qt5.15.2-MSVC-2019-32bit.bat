call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x86
mkdir C:\Programming\TileZed\dist32
cd C:\Programming\TileZed\dist32
"C:\Programming\QtSDK2015\5.15.2\msvc2019\bin\qmake.exe" C:\Programming\TileZed\tiled\tiled.pro -r -spec win32-msvc "CONFIG+=release"
"C:\Programming\QtSDK2015\Tools\QtCreator\bin\jom.exe"
"C:\Programming\TclTk\8.5.x\32bit-mingw\bin\tclsh85.exe" C:\Programming\TileZed\tiled\dist\make-dist.tcl 32bit
PAUSE
