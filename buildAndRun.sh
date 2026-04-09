# Get deps (header-only, drop next to .cpp)
wget https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h
wget https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp

# Linux/macOS
g++ -std=c++17 -O2 -pthread ftx1_server.cpp -o ftx1_server
./ftx1_server --port /dev/ttyUSB0 --baud 38400 --http 8080

# Windows (MSVC)
cl /std:c++17 /O2 /EHsc ftx1_server.cpp /link ws2_32.lib
ftx1_server.exe --port COM5 --baud 38400 --http 8080

# Then open ftx1_gui.html in your browser
# Type the COM port in the connect bar and click CONNECT
