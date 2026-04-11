/**
 * ftx1_server.cpp
 * ===============
 * Entry point for the FTX-1 CAT controller HTTP server.
 *
 * All logic lives in the separate class files:
 *   CATCommand.h            — static CAT command string builder
 *   FTX1Controller.h/.cpp   — serial port, rig state, all set/get methods
 *   FTX1HttpServer.h/.cpp   — HTTP REST API (all routes)
 *
 * Dependencies (drop next to this file):
 *   httplib.h   https://github.com/yhirose/cpp-httplib
 *   json.hpp    https://github.com/nlohmann/json
 *
 * Build (MSVC — Developer Command Prompt):
 *   cl /std:c++17 /O2 /EHsc ^
 *      ftx1_server.cpp FTX1Controller.cpp FTX1HttpServer.cpp ^
 *      /link ws2_32.lib
 *
 * Run:
 *   ftx1_server.exe --port COM4 --baud 38400 --http 8080 --auto
 *
 * Command line options:
 *   --port  <COM>    Serial port (default: COM4)
 *   --baud  <rate>   Baud rate   (default: 38400)
 *   --http  <port>   HTTP port   (default: 8080)
 *   --host  <addr>   Bind address (default: 0.0.0.0)
 *   --root  <path>   Static file root for GUI (default: .)
 *   --auto           Auto-connect to serial on startup
 */

// httplib must be included before windows.h to avoid winsock redefinition
#include "httplib.h"
#include "json.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "CATCommand.h"
#include "FTX1Controller.h"
#include "FTX1HttpServer.h"

#include <iostream>
#include <string>

// ============================================================================
//  main
// ============================================================================
int main(int argc, char** argv) {

    // ── Defaults ────────────────────────────────────────────────────────
    std::string port       = "COM4";
    int         baud       = 38400;
    int         httpPort   = 8080;
    std::string httpHost   = "0.0.0.0";
    std::string staticRoot = ".";
    bool        autoConn   = false;

    // ── Parse command line ───────────────────────────────────────────────
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if      ((a == "--port" || a == "-p") && i+1 < argc) port       = argv[++i];
        else if ((a == "--baud" || a == "-b") && i+1 < argc) baud       = std::stoi(argv[++i]);
        else if ( a == "--http"               && i+1 < argc) httpPort   = std::stoi(argv[++i]);
        else if ( a == "--host"               && i+1 < argc) httpHost   = argv[++i];
        else if ( a == "--root"               && i+1 < argc) staticRoot = argv[++i];
        else if ( a == "--auto")                              autoConn   = true;
        else if ( a == "--help" || a == "-h") {
            std::cout
                << "Usage: ftx1_server.exe [options]\n"
                << "  --port <COM>   Serial port         (default: COM4)\n"
                << "  --baud <rate>  Baud rate            (default: 38400)\n"
                << "  --http <port>  HTTP listen port     (default: 8080)\n"
                << "  --host <addr>  HTTP bind address    (default: 0.0.0.0)\n"
                << "  --root <path>  Static file path     (default: .)\n"
                << "  --auto         Auto-connect serial on startup\n";
            return 0;
        }
    }

    // ── Print banner ─────────────────────────────────────────────────────
    std::cout
        << "\n"
        << "  ╔═══════════════════════════════════╗\n"
        << "  ║   Yaesu FTX-1 CAT Controller      ║\n"
        << "  ║   HTTP API + Browser GUI           ║\n"
        << "  ╚═══════════════════════════════════╝\n\n"
        << "  Serial : " << port << " @ " << baud << " baud\n"
        << "  HTTP   : http://" << httpHost << ":" << httpPort << "\n"
        << "  GUI    : http://localhost:" << httpPort << "/ftx1_gui.html\n"
        << "  Static : " << staticRoot << "\n\n";

    // ── Create rig controller ────────────────────────────────────────────
    FTX1Controller rig;

    // ── Auto-connect if requested ────────────────────────────────────────
    if (autoConn) {
        std::cout << "[FTX-1] Connecting to " << port << "...\n";
        if (rig.connect(port, baud)) {
            std::cout << "[FTX-1] Connected OK.\n\n";
        } else {
            std::cerr << "[FTX-1] WARNING: Could not open " << port << ".\n"
                      << "         Use the GUI Connect button to retry.\n\n";
        }
    } else {
        std::cout << "[FTX-1] Not auto-connecting. Use GUI to connect.\n\n";
    }

    // ── Create and start HTTP server ─────────────────────────────────────
    FTX1HttpServer server(rig);
    server.setStaticRoot(staticRoot);

    // Push rig state to any polling clients whenever it changes
    rig.onStatusUpdate([](const nlohmann::json&) {
        // Future: broadcast via WebSocket here if needed
    });

    std::cout << "[FTX-1] Server running. Press Ctrl+C to stop.\n\n";

    // Blocks here until Ctrl+C or server.stop() is called
    server.listen(httpHost, httpPort);

    // ── Clean shutdown ───────────────────────────────────────────────────
    std::cout << "\n[FTX-1] Shutting down...\n";
    rig.disconnect();
    std::cout << "[FTX-1] Bye.\n";

    return 0;
}
