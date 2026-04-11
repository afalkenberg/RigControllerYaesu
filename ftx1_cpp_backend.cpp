/**
 * FTX-1 CAT Controller — C++ Backend
 * ====================================
 * Dependencies (header-only):
 *   httplib.h  — https://github.com/yhirose/cpp-httplib
 *   json.hpp   — https://github.com/nlohmann/json
 *
 * Build (MSVC):
 *   cl /std:c++17 /O2 /EHsc ftx1_server.cpp /link ws2_32.lib
 *
 * Run:
 *   ftx1_server.exe --port COM4 --baud 38400 --http 8080 --auto
 */

#include "httplib.h"   // must come FIRST — includes winsock2.h before windows.h
#include "json.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>   // after httplib to avoid sockaddr redefinition

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>
#include <functional>
#include <cstring>
#include <algorithm>
#include "CATCommand.h"
#include "FTX1Controller.h"


using json = nlohmann::json;

// ── MSVC-safe clamp ───────────────────────────────────────────────────────────
template<typename T>
static T clamp_(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
#define CLAMP(v,lo,hi) clamp_((v),(lo),(hi))

// =============================================================================
//  Serial Port (Windows only)
// =============================================================================
class SerialPort {
public:
    bool open(const std::string& port, int baud) {
        std::string p = "\\\\.\\" + port;
        h_ = CreateFileA(p.c_str(), GENERIC_READ|GENERIC_WRITE, 0, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h_ == INVALID_HANDLE_VALUE) return false;
        DCB dcb{}; dcb.DCBlength = sizeof(dcb);
        GetCommState(h_, &dcb);
        dcb.BaudRate = (DWORD)baud;
        dcb.ByteSize = 8;
        dcb.Parity   = NOPARITY;
        dcb.StopBits = ONESTOPBIT;
        dcb.fRtsControl = RTS_CONTROL_DISABLE;
        dcb.fDtrControl = DTR_CONTROL_DISABLE;
        SetCommState(h_, &dcb);
        COMMTIMEOUTS to{50,0,50,0,50};
        SetCommTimeouts(h_, &to);
        open_ = true;
        return true;
    }

    void close() {
        if (!open_) return;
        CloseHandle(h_);
        h_    = INVALID_HANDLE_VALUE;
        open_ = false;
    }

    bool isOpen() const { return open_; }
    HANDLE handle()     { return h_; }  // for direct reads in poll loop

    bool write(const std::string& s) {
        if (!open_) return false;
        DWORD w;
        return WriteFile(h_, s.data(), (DWORD)s.size(), &w, nullptr) && w == s.size();
    }

    // Read until ';' or timeout
    std::string readResponse(int timeoutMs = 400) {
        if (!open_) return "";
        std::string buf;
        auto deadline = std::chrono::steady_clock::now()
                      + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            char c; DWORD rd;
            ReadFile(h_, &c, 1, &rd, nullptr);
            if (rd == 0) { Sleep(5); continue; }
            buf += c;
            if (c == ';') break;
        }
        return buf;
    }

private:
    HANDLE h_    = INVALID_HANDLE_VALUE;
    bool   open_ = false;
};


// =============================================================================
//  HTTP Server
// =============================================================================
void setupRoutes(httplib::Server& svr, FTX1Controller& rig) {
    using namespace httplib;

    auto ok = [](Response& res, bool v) {
        res.set_content(json{{"ok",v}}.dump(), "application/json");
        res.set_header("Access-Control-Allow-Origin","*");
    };
    auto okj = [](Response& res, const json& j) {
        res.set_content(j.dump(), "application/json");
        res.set_header("Access-Control-Allow-Origin","*");
    };

    svr.Options(".*",[](const Request&,Response& res){
        res.set_header("Access-Control-Allow-Origin","*");
        res.set_header("Access-Control-Allow-Methods","GET,POST,OPTIONS");
        res.set_header("Access-Control-Allow-Headers","Content-Type");
    });

    svr.Get("/api/status",[&](const Request&,Response& res){ okj(res,rig.getStatus()); });

    svr.Post("/api/connect",[&](const Request& req,Response& res){
        auto j=json::parse(req.body,nullptr,false);
        ok(res, rig.connect(j.value("port","COM4"), j.value("baud",38400)));
    });
    svr.Post("/api/disconnect",[&](const Request&,Response& res){
        rig.disconnect(); ok(res,true);
    });

    svr.Post("/api/frequency",[&](const Request& req,Response& res){
        auto j=json::parse(req.body,nullptr,false);
        ok(res, rig.setFrequency(j.value("vfo","MAIN"), j.value("hz",14250000LL)));
    });
    svr.Post("/api/mode",[&](const Request& req,Response& res){
        auto j=json::parse(req.body,nullptr,false);
        ok(res, rig.setMode(j.value("vfo","MAIN"), j.value("mode","USB")));
    });
    svr.Post("/api/ptt",[&](const Request& req,Response& res){
        auto j=json::parse(req.body,nullptr,false);
        ok(res, rig.setPTT(j.value("tx",false)));
    });
    svr.Post("/api/af_gain",[&](const Request& req,Response& res){
        auto j=json::parse(req.body,nullptr,false);
        ok(res, rig.setAFGain(j.value("level",128)));
    });
    svr.Post("/api/rf_gain",[&](const Request& req,Response& res){
        auto j=json::parse(req.body,nullptr,false);
        ok(res, rig.setRFGain(j.value("level",255)));
    });
    svr.Post("/api/squelch",[&](const Request& req,Response& res){
        auto j=json::parse(req.body,nullptr,false);
        ok(res, rig.setSquelch(j.value("level",0)));
    });
    svr.Post("/api/power",[&](const Request& req,Response& res){
        auto j=json::parse(req.body,nullptr,false);
        ok(res, rig.setPower(j.value("level",100)));
    });
    svr.Post("/api/vfo",[&](const Request& req,Response& res){
        auto j=json::parse(req.body,nullptr,false);
        ok(res, rig.vfoAction(j.value("action","toggle_ab")));
    });
    svr.Post("/api/band",[&](const Request& req,Response& res){
        auto j=json::parse(req.body,nullptr,false);
        ok(res, rig.setBand(j.value("band","20m")));
    });
    svr.Post("/api/filter",[&](const Request& req,Response& res){
        auto j=json::parse(req.body,nullptr,false);
        ok(res, rig.setFilterWidth(j.value("width",2400)));
    });
    svr.Post("/api/nb",[&](const Request& req,Response& res){
        auto j=json::parse(req.body,nullptr,false);
        ok(res, rig.setNB(j.value("on",false)));
    });
    svr.Post("/api/nr",[&](const Request& req,Response& res){
        auto j=json::parse(req.body,nullptr,false);
        ok(res, rig.setNR(j.value("on",false)));
    });
    svr.Post("/api/notch",[&](const Request& req,Response& res){
        auto j=json::parse(req.body,nullptr,false);
        ok(res, rig.setManualNotch(j.value("on",false), j.value("freq",1000)));
    });
    svr.Post("/api/preamp",[&](const Request& req,Response& res){
        auto j=json::parse(req.body,nullptr,false);
        ok(res, rig.setPreamp(j.value("level",0)));
    });
    svr.Post("/api/att",[&](const Request& req,Response& res){
        auto j=json::parse(req.body,nullptr,false);
        ok(res, rig.setATT(j.value("on",false)));
    });
    svr.Post("/api/lock",[&](const Request& req,Response& res){
        auto j=json::parse(req.body,nullptr,false);
        ok(res, rig.setLock(j.value("on",false)));
    });
    svr.Post("/api/split",[&](const Request& req,Response& res){
        auto j=json::parse(req.body,nullptr,false);
        ok(res, rig.setSplit(j.value("on",false)));
    });
    svr.Post("/api/rit",[&](const Request& req,Response& res){
        auto j=json::parse(req.body,nullptr,false);
        ok(res, rig.setRIT(j.value("on",false), j.value("offset",0)));
    });
    svr.Post("/api/xit",[&](const Request& req,Response& res){
        auto j=json::parse(req.body,nullptr,false);
        ok(res, rig.setXIT(j.value("on",false)));
    });
    svr.Post("/api/tuner",[&](const Request& req,Response& res){
        auto j=json::parse(req.body,nullptr,false);
        ok(res, rig.tuner(j.value("action","start")));
    });
    svr.Post("/api/raw",[&](const Request& req,Response& res){
        auto j=json::parse(req.body,nullptr,false);
        rig.sendRaw(j.value("cmd",""));
        okj(res, {{"ok",true},{"response",rig.getLastRaw()}});
    });

    // Serve GUI files from current directory
    svr.set_mount_point("/",".");
}

// =============================================================================
//  main
// =============================================================================
int main(int argc, char** argv) {
    std::string port     = "COM4";
    int         baud     = 38400;
    int         httpPort = 8080;
    bool        autoConn = false;

    for (int i=1; i<argc; ++i) {
        std::string a = argv[i];
        if ((a=="--port"||a=="-p") && i+1<argc) port = argv[++i];
        else if ((a=="--baud"||a=="-b") && i+1<argc) baud = std::stoi(argv[++i]);
        else if (a=="--http" && i+1<argc) httpPort = std::stoi(argv[++i]);
        else if (a=="--auto") autoConn = true;
    }

    FTX1Controller rig;

    if (autoConn) {
        std::cout << "[FTX-1] Connecting to " << port << " @ " << baud << " baud...\n";
        if (rig.connect(port, baud))
            std::cout << "[FTX-1] Connected OK.\n";
        else
            std::cerr << "[FTX-1] Serial open failed. Connect via GUI.\n";
    }

    httplib::Server svr;
    setupRoutes(svr, rig);

    std::cout << "[FTX-1] HTTP server at http://localhost:" << httpPort << "\n";
    svr.listen("0.0.0.0", httpPort);
    rig.disconnect();
    return 0;
}
