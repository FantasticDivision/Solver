cmake -B build
cmake --build build --config Release
Copy-Item -Path "build\Release\*.pyd" -Destination "python\fitsolver"
