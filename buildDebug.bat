cd buildD/
cmake -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc -static" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DwxWidgets_ROOT_DIR:PATH="C:\Cpp\wxWidgets" ../
mingw32-make -j4
PAUSE
