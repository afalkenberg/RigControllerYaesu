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
//  Band table  (global — visible to everything below)
// =============================================================================
struct BandInfo { long long defaultHz; };

static const std::map<std::string, BandInfo> g_bandMap {
    {"160m", { 1900000LL}},
    {"80m",  { 3700000LL}},
    {"60m",  { 5357000LL}},
    {"40m",  { 7100000LL}},
    {"30m",  {10125000LL}},
    {"20m",  {14250000LL}},
    {"17m",  {18100000LL}},
    {"15m",  {21200000LL}},
    {"12m",  {24900000LL}},
    {"10m",  {28500000LL}},
    {"6m",   {50125000LL}},
    {"2m",   {144200000LL}},
    {"70cm", {432100000LL}}
};

// =============================================================================
//  Rig State
// =============================================================================
struct RigState {
    long long   freqMain  = 14250000;
    long long   freqSub   = 7100000;
    std::string modeMain  = "USB";
    std::string modeSub   = "USB";
    bool tx       = false;
    int  afGain   = 128;
    int  rfGain   = 255;
    int  squelch  = 0;
    int  power    = 100;
    bool nb       = false;
    bool nr       = false;
    bool autoNotch    = false;
    bool manualNotch  = false;
    int  notchFreq    = 1000;
    int  preamp   = 0;
    bool att      = false;
    bool lock     = false;
    bool split    = false;
    bool rit      = false;
    int  ritOffset = 0;
    bool xit      = false;
    std::string band = "20m";
    // Meters (raw 0-255)
    // RM1=S-MAIN RM2=S-SUB RM3=COMP RM4=ALC RM5=PO RM6=SWR RM7=IDD RM8=VDD
    int smeter_main = 0;
    int smeter_sub  = 0;
    int comp  = 0;
    int alc   = 0;
    int po    = 0;
    int swr   = 0;
    int idd   = 0;
    int vdd   = 0;
    bool connected = false;
};

// =============================================================================
//  CAT Controller
// =============================================================================
class FTX1Controller {
public:
    bool connect(const std::string& port, int baud) {
        std::lock_guard<std::mutex> lk(mx_);
        if (!serial_.open(port, baud)) return false;
        state_.connected = true;
        serial_.write(CAT::setAI(true));
        running_ = true;
        pollThread_ = std::thread([this]{ pollLoop(); });
        return true;
    }

    void disconnect() {
        running_ = false;
        if (pollThread_.joinable()) pollThread_.join();
        std::lock_guard<std::mutex> lk(mx_);
        serial_.write(CAT::setAI(false));
        serial_.close();
        state_.connected = false;
    }

    // ── Public API ────────────────────────────────────────────────────────────

    bool setFrequency(const std::string& vfo, long long hz) {
        std::lock_guard<std::mutex> lk(mx_);
        if (vfo == "SUB") state_.freqSub  = hz;
        else              state_.freqMain = hz;
        return send(CAT::setFrequency(vfo, hz));
    }

    bool setMode(const std::string& vfo, const std::string& mode) {
        std::lock_guard<std::mutex> lk(mx_);
        if (vfo == "SUB") state_.modeSub  = mode;
        else              state_.modeMain = mode;
        return send(CAT::setMode(vfo, mode));
    }

    bool setPTT(bool tx) {
        std::lock_guard<std::mutex> lk(mx_);
        state_.tx = tx;
        return send(CAT::setPTT(tx));
    }

    bool setAFGain(int v)  { std::lock_guard<std::mutex> lk(mx_); state_.afGain=v;  return send(CAT::setAFGain(v)); }
    bool setRFGain(int v)  { std::lock_guard<std::mutex> lk(mx_); state_.rfGain=v;  return send(CAT::setRFGain(v)); }
    bool setSquelch(int v) { std::lock_guard<std::mutex> lk(mx_); state_.squelch=v; return send(CAT::setSquelch(v)); }
    bool setPower(int v)   { std::lock_guard<std::mutex> lk(mx_); state_.power=v;   return send(CAT::setPower(v)); }

    bool vfoAction(const std::string& a) {
        std::lock_guard<std::mutex> lk(mx_);
        return send(CAT::vfoAction(a));
    }

    bool setBand(const std::string& b) {
        std::lock_guard<std::mutex> lk(mx_);
        state_.band = b;
        auto it = g_bandMap.find(b);
        if (it != g_bandMap.end()) state_.freqMain = it->second.defaultHz;
        return send(CAT::setBand(b));
    }

    bool setFilterWidth(int hz) { std::lock_guard<std::mutex> lk(mx_); return send(CAT::setFilterWidth(hz)); }
    bool setNB(bool on)  { std::lock_guard<std::mutex> lk(mx_); state_.nb=on;           return send(CAT::setNB(on)); }
    bool setNR(bool on)  { std::lock_guard<std::mutex> lk(mx_); state_.nr=on;           return send(CAT::setNR(on)); }
    bool setAutoNotch(bool on)  { std::lock_guard<std::mutex> lk(mx_); state_.autoNotch=on;   return send(CAT::setAutoNotch(on)); }
    bool setManualNotch(bool on, int f=1000) {
        std::lock_guard<std::mutex> lk(mx_);
        state_.manualNotch=on; state_.notchFreq=f;
        return send(CAT::setManualNotch(on,f));
    }
    bool setPreamp(int l) { std::lock_guard<std::mutex> lk(mx_); state_.preamp=l;  return send(CAT::setPreamp(l)); }
    bool setATT(bool on)  { std::lock_guard<std::mutex> lk(mx_); state_.att=on;    return send(CAT::setATT(on)); }
    bool setLock(bool on) { std::lock_guard<std::mutex> lk(mx_); state_.lock=on;   return send(CAT::setLock(on)); }
    bool setSplit(bool on){ std::lock_guard<std::mutex> lk(mx_); state_.split=on;  return send(CAT::setSplit(on)); }

    bool setRIT(bool on, int offset=0) {
        std::lock_guard<std::mutex> lk(mx_);
        state_.rit=on; state_.ritOffset=offset;
        return send(CAT::setRIT(on)) && send(CAT::setRITOffset(offset));
    }
    bool setXIT(bool on) { std::lock_guard<std::mutex> lk(mx_); state_.xit=on; return send(CAT::setXIT(on)); }
    bool tuner(const std::string& a) { std::lock_guard<std::mutex> lk(mx_); return send(CAT::tuner(a)); }

    bool sendRaw(const std::string& cmd) {
        std::lock_guard<std::mutex> lk(mx_);
        send(cmd);
        lastRaw_ = serial_.readResponse(500);
        return true;
    }
    std::string getLastRaw() { std::lock_guard<std::mutex> lk(mx_); return lastRaw_; }

    json getStatus() {
        std::lock_guard<std::mutex> lk(mx_);
        return json{
            {"connected",    state_.connected},
            {"freq_main",    state_.freqMain},
            {"freq_sub",     state_.freqSub},
            {"mode_main",    state_.modeMain},
            {"mode_sub",     state_.modeSub},
            {"tx",           state_.tx},
            {"af_gain",      state_.afGain},
            {"rf_gain",      state_.rfGain},
            {"squelch",      state_.squelch},
            {"power",        state_.power},
            {"nb",           state_.nb},
            {"nr",           state_.nr},
            {"auto_notch",   state_.autoNotch},
            {"manual_notch", state_.manualNotch},
            {"notch_freq",   state_.notchFreq},
            {"preamp",       state_.preamp},
            {"att",          state_.att},
            {"lock",         state_.lock},
            {"split",        state_.split},
            {"rit",          state_.rit},
            {"rit_offset",   state_.ritOffset},
            {"xit",          state_.xit},
            {"band",         state_.band},
            {"smeter_main",  state_.smeter_main},
            {"smeter_sub",   state_.smeter_sub},
            {"comp",         state_.comp},
            {"alc",          state_.alc},
            {"po",           state_.po},
            {"swr",          state_.swr},
            {"idd",          state_.idd},
            {"vdd",          state_.vdd}
        };
    }

    void onStatusUpdate(std::function<void(json)> cb) { statusCb_ = cb; }

private:
    bool send(const std::string& cmd) {
        if (cmd.empty()) return false;
        return serial_.write(cmd);
    }

    // TX status poll via TX; command
    void parseTX(const std::string& r) {
        if (r.size() >= 3 && r.substr(0,2) == "TX")
            state_.tx = (r[2] == '1');
    }

    // Parse RM; — FTX-1: RM[id][6-digit raw]; e.g. RM1000128;
    // IDs: 1=S-MAIN 2=S-SUB 3=COMP 4=ALC 5=PO 6=SWR 7=IDD 8=VDD
    void parseRM(const std::string& r) {
        if (r.size() < 9) return;
        try {
            int id  = r[2] - '0';
            int val = std::stoi(r.substr(3, 6));
            val = CLAMP(val, 0, 255);
            switch(id) {
                case 1: state_.smeter_main = val; break;
                case 2: state_.smeter_sub  = val; break;
                case 3: state_.comp        = val; break;
                case 4: state_.alc         = val; break;
                case 5: state_.po          = val; break;
                case 6: state_.swr         = val; break;
                case 7: state_.idd         = val; break;
                case 8: state_.vdd         = val; break;
            }
        } catch(...) {}
    }

    void pollLoop() {
        while (running_) {
            {
                std::lock_guard<std::mutex> lk(mx_);
                if (serial_.isOpen()) {
                    // Pure listener — never send read commands
                    // Just drain whatever the radio pushes via AI mode
                    // and parse any complete packets we find
                    std::string buf;
                    // Read all available bytes into buffer
                    for (int i = 0; i < 20; i++) {
                        char c; DWORD rd;
                        ReadFile(serial_.handle(), &c, 1, &rd, nullptr);
                        if (rd == 0) break;
                        buf += c;
                        if (c == ';') {
                            parsePacket(buf);
                            buf.clear();
                        }
                    }
                    // Only poll S-meter actively — it is not pushed by AI
                    serial_.write("RM1;");
                    std::string r = serial_.readResponse(200);
                    if (r.size() > 2 && r.substr(0,2) == "RM") parseRM(r);
                }
            }
            if (statusCb_) statusCb_(getStatus());
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    }

    // Parse any packet the radio pushes
    void parsePacket(const std::string& r) {
        if (r.size() < 3) return;
        std::string cmd = r.substr(0,2);

        if (cmd == "FA" && r.size() >= 11) {
            try { state_.freqMain = std::stoll(r.substr(2,9)); } catch(...) {}
        }
        else if (cmd == "FB" && r.size() >= 11) {
            try { state_.freqSub = std::stoll(r.substr(2,9)); } catch(...) {}
        }
        else if (cmd == "MD" && r.size() >= 4) {
            static const char* modes[] = {
                "LSB","USB","CW","CWR","AM","FM",
                "DATA-L","DATA-U","DATA-FM","C4FM"
            };
            // MD[vfo][mode];  vfo=r[2], mode=r[3]
            int mi = r[3] - '0';
            bool isSub = (r[2] == '1');
            if (mi >= 0 && mi <= 9) {
                if (isSub) state_.modeSub  = modes[mi];
                else        state_.modeMain = modes[mi];
            }
        }
        else if (cmd == "TX" && r.size() >= 3) {
            state_.tx = (r[2] == '1');
        }
        else if (cmd == "RM") {
            parseRM(r);
        }
    }

    SerialPort   serial_;
    RigState     state_;
    std::mutex   mx_;
    std::atomic<bool> running_{false};
    std::thread  pollThread_;
    std::string  lastRaw_;
    std::function<void(json)> statusCb_;
    std::chrono::steady_clock::time_point suppressUntil_;
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
