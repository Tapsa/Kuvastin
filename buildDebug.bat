cd Debug/
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DwxWidgets_ROOT_DIR:PATH="C:\Cpp\wxWidgets" ../
mingw32-make -j4
PAUSE
