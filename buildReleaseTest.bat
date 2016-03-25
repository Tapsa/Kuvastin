cd buildR/
cmake -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc -static" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DwxWidgets_ROOT_DIR:PATH="C:\Cpp\wxWidgets" ../
mingw32-make -j4
strip Kuvastin.exe
PAUSE
