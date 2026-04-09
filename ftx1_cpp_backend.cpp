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
//  CAT Command Builder
// =============================================================================
namespace CAT {

    std::string setFrequency(const std::string& vfo, long long hz) {
        std::string cmd = (vfo == "SUB") ? "FB" : "FA";
        char buf[16]; snprintf(buf, sizeof(buf), "%09lld", hz);
        return cmd + buf + ";";
    }
    std::string getFrequency(const std::string& vfo) {
        return (vfo == "SUB") ? "FB;" : "FA;";
    }

    // Mode map
    static const std::map<std::string,std::string> modeMap {
        {"LSB","0"},{"USB","1"},{"CW","2"},{"CWR","3"},
        {"AM","4"},{"FM","5"},{"DATA-L","6"},{"DATA-U","7"},
        {"DATA-FM","8"},{"C4FM","9"}
    };
    std::string setMode(const std::string& vfo, const std::string& mode) {
        auto it = modeMap.find(mode);
        std::string m   = (it != modeMap.end()) ? it->second : "1";
        std::string sub = (vfo == "SUB") ? "1" : "0";
        return "MD" + sub + m + ";";  // FTX-1: MD[vfo][mode]  e.g. MD01=MAIN USB
    }
    std::string getMode() { return "MD0;"; }

    // PTT
    std::string setPTT(bool tx) { return tx ? "TX1;" : "TX0;"; }

    // Gains
    std::string setAFGain(int v, bool sub=false) {
        char buf[16]; snprintf(buf,sizeof(buf),"AG%d%03d;",sub?1:0,CLAMP(v,0,255));
        return buf;
    }
    std::string getAFGain(bool sub=false) { return sub ? "AG1;" : "AG0;"; }
    std::string setRFGain(int v) {
        char buf[16]; snprintf(buf,sizeof(buf),"RG%03d;",CLAMP(v,0,255));
        return buf;
    }
    std::string setSquelch(int v, bool sub=false) {
        char buf[16]; snprintf(buf,sizeof(buf),"SQ%d%03d;",sub?1:0,CLAMP(v,0,255));
        return buf;
    }
    std::string setPower(int v) {
        char buf[16]; snprintf(buf,sizeof(buf),"PC%03d;",CLAMP(v,0,100));
        return buf;
    }

    // VFO actions
    std::string vfoAction(const std::string& action) {
        if (action == "swap")          return "SV;";
        if (action == "copy_main_sub") return "AM;";
        if (action == "copy_sub_main") return "BA;";
        if (action == "toggle_ab")     return "FT;";
        return "";
    }

    // Band — use FA directly since BS is unreliable on FTX-1
    std::string setBand(const std::string& band) {
        auto it = g_bandMap.find(band);
        if (it == g_bandMap.end()) return "";
        char fa[32];
        snprintf(fa, sizeof(fa), "FA%09lld;", it->second.defaultHz);
        return fa;
    }

    // Filter
    std::string setFilterWidth(int hz) {
        char buf[16]; snprintf(buf,sizeof(buf),"FW%04d;",CLAMP(hz,0,4000));
        return buf;
    }

    // DSP
    std::string setNB(bool on)         { return on ? "NB1;" : "NB0;"; }
    std::string setNR(bool on)         { return on ? "NR1;" : "NR0;"; }
    std::string setAutoNotch(bool on)  { return on ? "BC1;" : "BC0;"; }
    std::string setManualNotch(bool on, int freq=1000) {
        if (!on) return "BP00000;";
        char buf[16]; snprintf(buf,sizeof(buf),"BP1%04d;",CLAMP(freq,0,4000));
        return buf;
    }

    // RF path
    std::string setPreamp(int level) {
        char buf[16]; snprintf(buf,sizeof(buf),"PA0%d;",CLAMP(level,0,2));
        return buf;
    }
    std::string setATT(bool on)  { return on ? "RA01;" : "RA00;"; }
    std::string setLock(bool on) { return on ? "LK1;"  : "LK0;";  }
    std::string setSplit(bool on){ return on ? "SP1;"  : "SP0;";  }

    // RIT / XIT
    std::string setRIT(bool on) { return on ? "RT1;" : "RT0;"; }
    std::string setRITOffset(int hz) {
        std::string cmds = "RC;";
        if (hz > 0) { char buf[16]; snprintf(buf,sizeof(buf),"RU%04d;", hz/10); cmds+=buf; }
        else if (hz < 0) { char buf[16]; snprintf(buf,sizeof(buf),"RD%04d;",-hz/10); cmds+=buf; }
        return cmds;
    }
    std::string setXIT(bool on) { return on ? "XT1;" : "XT0;"; }

    // Tuner
    std::string tuner(const std::string& action) {
        if (action == "start") return "AC010;";
        if (action == "stop")  return "AC000;";
        return "";
    }

    // Meters  — FTX-1 returns 6-digit value e.g. RM1000042;
    std::string readMeter(int type) {
        char buf[8]; snprintf(buf,sizeof(buf),"RM%d;",type);
        return buf;
    }

    // Auto-information
    std::string setAI(bool on) { return on ? "AI2;" : "AI0;"; }

    // Full status packet
    std::string getIF() { return "IF;"; }

} // namespace CAT

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
    int smeter = 0, po = 0, alc = 0, swr = 0, idd = 0, vdd = 0;
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
        suppressUntil_ = std::chrono::steady_clock::now()
                       + std::chrono::milliseconds(1500);
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
        suppressUntil_ = std::chrono::steady_clock::now()
                       + std::chrono::milliseconds(1500);
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
            {"smeter",       state_.smeter},
            {"po",           state_.po},
            {"alc",          state_.alc},
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

    // Parse IF; response
    void parseIF(const std::string& r) {
        if (r.size() < 30) return;
        try { state_.freqMain = std::stoll(r.substr(2,9)); } catch(...) {}
        // FTX-1 IF response: IF[9 freq][5 rit offset][+/-][rit][xit][mem ch][tx][mode]
        // mode is at position 21, tx at position 28
        static const char* modes[] = {
            "LSB","USB","CW","CWR","AM","FM","DATA-L","DATA-U","DATA-FM","C4FM"
        };
        if (r.size() > 21) {
            int mi = r[21] - '0';
            if (mi >= 0 && mi <= 9) state_.modeMain = modes[mi];
        }
        if (r.size() > 28) state_.tx = (r[28] == '1');
    }

    // Parse RM; — FTX-1 returns 6-digit value e.g. RM1000042;
    void parseRM(const std::string& r) {
        if (r.size() < 9) return;
        try {
            int type = r[2] - '0';
            int val  = std::stoi(r.substr(3,6));
            val = CLAMP(val, 0, 255);
            switch(type) {
                case 1: state_.smeter = val; break;
                case 2: state_.po     = val; break;
                case 3: state_.alc    = val; break;
                case 7: state_.swr    = val; break;
                case 6: state_.idd    = val; break;
                case 5: state_.vdd    = val; break;
            }
        } catch(...) {}
    }

    void pollLoop() {
        while (running_) {
            {
                std::lock_guard<std::mutex> lk(mx_);
                bool suppressed = std::chrono::steady_clock::now() < suppressUntil_;
                if (serial_.isOpen()) {
                    if (!suppressed) {
                        // Full status
                        serial_.write(CAT::getIF());
                        std::string r = serial_.readResponse(400);
                        if (r.size() > 2 && r.substr(0,2) == "IF") parseIF(r);

                        // Sub VFO freq
                        serial_.write(CAT::getFrequency("SUB"));
                        r = serial_.readResponse(300);
                        if (r.size() >= 11 && r.substr(0,2) == "FB") {
                            try { state_.freqSub = std::stoll(r.substr(2,9)); } catch(...) {}
                        }
                    }

                    // S-meter always
                    serial_.write(CAT::readMeter(1));
                    std::string r = serial_.readResponse(200);
                    if (r.size() > 2 && r.substr(0,2) == "RM") parseRM(r);

                    // TX meters only when transmitting
                    if (state_.tx) {
                        for (int m : {2,3,7}) {
                            serial_.write(CAT::readMeter(m));
                            r = serial_.readResponse(200);
                            if (r.size() > 2 && r.substr(0,2) == "RM") parseRM(r);
                        }
                    }
                }
            }
            if (statusCb_) statusCb_(getStatus());
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
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
