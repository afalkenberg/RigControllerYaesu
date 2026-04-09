/**
 * FTX-1 CAT Controller — C++ Backend
 * ====================================
 * Provides a REST + WebSocket API over HTTP so the browser GUI can
 * control the Yaesu FTX-1 via its USB virtual COM port (CAT-1 / CAT-2).
 *
 * Dependencies (header-only / single-file):
 *   - cpp-httplib  https://github.com/yhirose/cpp-httplib  (httplib.h)
 *   - nlohmann/json https://github.com/nlohmann/json       (json.hpp)
 *   - Platform serial: POSIX termios (Linux/macOS) or Win32 (Windows)
 *
 * Build (Linux/macOS):
 *   g++ -std=c++17 -O2 -pthread ftx1_server.cpp -o ftx1_server
 *
 * Build (Windows with MSVC):
 *   cl /std:c++17 /O2 /EHsc ftx1_server.cpp /link ws2_32.lib
 *
 * Run:
 *   ./ftx1_server --port /dev/ttyUSB0 --baud 38400 --http 8080
 *   On Windows: ftx1_server.exe --port COM5 --baud 38400 --http 8080
 *
 * API Endpoints:
 *   GET  /api/status           → full rig status JSON
 *   POST /api/frequency        → { "vfo":"MAIN|SUB", "hz": 14250000 }
 *   POST /api/mode             → { "vfo":"MAIN", "mode":"USB|LSB|CW|CWR|AM|FM|DATA|C4FM" }
 *   POST /api/ptt              → { "tx": true|false }
 *   POST /api/af_gain          → { "level": 0-255 }
 *   POST /api/rf_gain          → { "level": 0-255 }
 *   POST /api/squelch          → { "level": 0-255 }
 *   POST /api/power            → { "level": 0-100 }
 *   POST /api/vfo              → { "action":"swap|copy_main_sub|copy_sub_main|toggle_ab" }
 *   POST /api/band             → { "band": "160m"|"80m"|"40m"|"30m"|"20m"|"17m"|"15m"|"12m"|"10m"|"6m"|"2m"|"70cm" }
 *   POST /api/filter           → { "width": 0-4 }   (0=wide … 4=narrow)
 *   POST /api/nb               → { "on": true|false }
 *   POST /api/nr               → { "on": true|false }
 *   POST /api/notch            → { "on": true|false, "freq": 0-4000 }
 *   POST /api/preamp           → { "level": 0|1|2 }   (0=OFF,1=IPO,2=AMP)
 *   POST /api/att              → { "on": true|false }
 *   POST /api/lock             → { "on": true|false }
 *   POST /api/split            → { "on": true|false }
 *   POST /api/rit              → { "on": true|false, "offset": -9999..+9999 }
 *   POST /api/xit              → { "on": true|false, "offset": -9999..+9999 }
 *   POST /api/tuner            → { "action":"start|stop" }
 *   POST /api/raw              → { "cmd": "FA00014250000;" }  raw CAT passthrough
 *   WS   /ws                   → bidirectional; server pushes status every 250ms
 */

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

// MSVC-safe clamp (std::clamp needs C++17 + <algorithm>, this always works)
template<typename T>
static T clamp_(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
#define CLAMP(v,lo,hi) clamp_((v),(lo),(hi))

// ── Header-only deps (drop these .h files next to this source) ────────────────
#include "httplib.h"   // cpp-httplib
#include "json.hpp"    // nlohmann::json
using json = nlohmann::json;

// ── Platform serial ───────────────────────────────────────────────────────────
#ifdef _WIN32
#  include <windows.h>
#else
#  include <fcntl.h>
#  include <unistd.h>
#  include <termios.h>
#endif

// =============================================================================
//  Serial Port Abstraction
// =============================================================================
class SerialPort {
public:
    SerialPort() {}
    ~SerialPort() { close(); }

    bool open(const std::string& port, int baud) {
#ifdef _WIN32
        std::string p = "\\\\.\\" + port;
        h_ = CreateFileA(p.c_str(), GENERIC_READ|GENERIC_WRITE, 0, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h_ == INVALID_HANDLE_VALUE) return false;
        DCB dcb{}; dcb.DCBlength = sizeof(dcb);
        GetCommState(h_, &dcb);
        dcb.BaudRate = baud; dcb.ByteSize = 8;
        dcb.Parity = NOPARITY; dcb.StopBits = ONESTOPBIT;
        dcb.fRtsControl = RTS_CONTROL_DISABLE;
        dcb.fDtrControl = DTR_CONTROL_DISABLE;
        SetCommState(h_, &dcb);
        COMMTIMEOUTS to{50,0,50,0,50};
        SetCommTimeouts(h_, &to);
        open_ = true;
#else
        fd_ = ::open(port.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
        if (fd_ < 0) return false;
        struct termios tio{};
        tcgetattr(fd_, &tio);
        speed_t spd = B38400;
        if (baud == 4800)  spd = B4800;
        if (baud == 9600)  spd = B9600;
        if (baud == 19200) spd = B19200;
        cfsetispeed(&tio, spd); cfsetospeed(&tio, spd);
        tio.c_cflag = CS8 | CLOCAL | CREAD;
        tio.c_iflag = IGNPAR;
        tio.c_oflag = 0; tio.c_lflag = 0;
        tio.c_cc[VMIN] = 0; tio.c_cc[VTIME] = 5; // 500ms read timeout
        tcflush(fd_, TCIFLUSH);
        tcsetattr(fd_, TCSANOW, &tio);
        open_ = true;
#endif
        return true;
    }

    void close() {
        if (!open_) return;
#ifdef _WIN32
        CloseHandle(h_);
#else
        ::close(fd_);
#endif
        open_ = false;
    }

    bool isOpen() const { return open_; }

    bool write(const std::string& s) {
        if (!open_) return false;
#ifdef _WIN32
        DWORD w; return WriteFile(h_, s.data(), (DWORD)s.size(), &w, nullptr);
#else
        return ::write(fd_, s.data(), s.size()) == (ssize_t)s.size();
#endif
    }

    // Read until ';' terminator or timeout
    std::string readResponse(int timeoutMs = 500) {
        if (!open_) return "";
        std::string buf;
        auto deadline = std::chrono::steady_clock::now()
                      + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            char c;
#ifdef _WIN32
            DWORD rd; ReadFile(h_, &c, 1, &rd, nullptr);
            if (rd == 0) { std::this_thread::sleep_for(std::chrono::milliseconds(5)); continue; }
#else
            int rd = ::read(fd_, &c, 1);
            if (rd <= 0) continue;
#endif
            buf += c;
            if (c == ';') break;
        }
        return buf;
    }

private:
    bool open_ = false;
#ifdef _WIN32
    HANDLE h_ = INVALID_HANDLE_VALUE;
#else
    int fd_ = -1;
#endif
};

// =============================================================================
//  CAT Command Builder
// =============================================================================
namespace CAT {
    // Frequency: 9-digit Hz, zero-padded
    std::string setFrequency(const std::string& vfo, long long hz) {
        // MAIN=FA, SUB=FB
        std::string cmd = (vfo == "SUB") ? "FB" : "FA";
        char buf[16]; snprintf(buf, sizeof(buf), "%09lld", hz);
        return cmd + buf + ";";
    }
    std::string getFrequency(const std::string& vfo) {
        return (vfo == "SUB") ? "FB;" : "FA;";
    }
    // MD: mode  0=LSB 1=USB 2=CW 3=CWR 4=AM 5=FM 6=DATA-L 7=DATA-U 8=DATA-FM 9=C4FM
    static const std::map<std::string,std::string> modeMap;
    std::string setMode(const std::string& vfo, const std::string& mode) {
        auto it = modeMap.find(mode);
        std::string m = (it != modeMap.end()) ? it->second : "1";
        std::string sub = (vfo == "SUB") ? "1" : "0";
        return "MD" + m + sub + ";";
    }
    std::string getMode() { return "MD0;"; }

    std::string setPTT(bool tx) { return tx ? "TX1;" : "TX0;"; }

    std::string setAFGain(int v, bool sub=false) {
        char buf[16]; snprintf(buf,sizeof(buf),"AG%d%03d;", sub?1:0, CLAMP(v,0,255));
        return buf;
    }
    std::string getAFGain(bool sub=false) { return sub ? "AG1;" : "AG0;"; }

    std::string setRFGain(int v) {
        char buf[16]; snprintf(buf,sizeof(buf),"RG%03d;", CLAMP(v,0,255));
        return buf;
    }

    std::string setSquelch(int v, bool sub=false) {
        char buf[16]; snprintf(buf,sizeof(buf),"SQ%d%03d;", sub?1:0, CLAMP(v,0,255));
        return buf;
    }

    std::string setPower(int v) {
        char buf[16]; snprintf(buf,sizeof(buf),"PC%03d;", CLAMP(v,0,100));
        return buf;
    }

    // VFO swap / copy
    std::string vfoAction(const std::string& action) {
        if (action == "swap")           return "SV;";
        if (action == "copy_main_sub")  return "AM;";  // A→M then M→B
        if (action == "copy_sub_main")  return "BA;";
        if (action == "toggle_ab")      return "FT;";  // VFO A/B
        return "";
    }

    // Band select: just set frequency directly — BS command unreliable on FTX-1
    std::string setBand(const std::string& band) {
        auto it = bandMap.find(band);
        if (it == bandMap.end()) return "";
        char fa[32];
        snprintf(fa, sizeof(fa), "FA%09lld;", it->second.defaultHz);
        return fa;  // FA only, no BS
    }

    // Filter: NA0/NA1 (narrow on/off); FW=filter width 0000-4000
    std::string setFilterWidth(int hz) {
        char buf[16]; snprintf(buf,sizeof(buf),"FW%04d;", CLAMP(hz,0,4000));
        return buf;
    }

    // Noise Blanker: NB
    std::string setNB(bool on) { return on ? "NB1;" : "NB0;"; }

    // Noise Reduction: NR
    std::string setNR(bool on) { return on ? "NR1;" : "NR0;"; }

    // Auto Notch (DNF): BC
    std::string setAutoNotch(bool on) { return on ? "BC1;" : "BC0;"; }

    // Manual Notch: BP on/off + freq
    std::string setManualNotch(bool on, int freq=1000) {
        if (!on) return "BP00000;";
        char buf[16]; snprintf(buf,sizeof(buf),"BP1%04d;", CLAMP(freq,0,4000));
        return buf;
    }

    // Preamp / IPO: PA 0=OFF(IPO) 1=AMP1 2=AMP2
    std::string setPreamp(int level) {
        char buf[16]; snprintf(buf,sizeof(buf),"PA0%d;", CLAMP(level,0,2));
        return buf;
    }

    // Attenuator: RA
    std::string setATT(bool on) { return on ? "RA01;" : "RA00;"; }

    // Lock: LK
    std::string setLock(bool on) { return on ? "LK1;" : "LK0;"; }

    // Split: SP
    std::string setSplit(bool on) { return on ? "SP1;" : "SP0;"; }

    // RIT: RT on/off, RU/RD offset
    std::string setRIT(bool on) { return on ? "RT1;" : "RT0;"; }
    std::string setRITOffset(int hz) {
        // RC clears, then RU/RD in 10Hz steps
        std::string cmds = "RC;";
        if (hz > 0) { char buf[16]; snprintf(buf,sizeof(buf),"RU%04d;",hz/10); cmds+=buf; }
        else if (hz < 0) { char buf[16]; snprintf(buf,sizeof(buf),"RD%04d;",-hz/10); cmds+=buf; }
        return cmds;
    }

    // XIT: XT on/off (uses same offset control)
    std::string setXIT(bool on) { return on ? "XT1;" : "XT0;"; }

    // Tuner: AC
    std::string tuner(const std::string& action) {
        if (action == "start") return "AC010;";
        if (action == "stop")  return "AC000;";
        return "";
    }

    // Meter read: RM (S-meter, power, SWR, ALC, IDD)
    std::string readMeter(int type) {
        // RM1=S-meter RM2=PO RM3=ALC RM4=COMP RM5=VDD RM6=IDD RM7=SWR
        char buf[8]; snprintf(buf,sizeof(buf),"RM%d;",type);
        return buf;
    }

    // Auto Information: AI — enable AI mode so rig pushes changes
    std::string setAI(bool on) { return on ? "AI2;" : "AI0;"; }

    // IF: read full IF status packet (includes freq, mode, RIT, etc.)
    std::string getIF() { return "IF;"; }
} // namespace CAT

// =============================================================================
//  Rig State
// =============================================================================
struct RigState {
    long long  freqMain  = 14250000;
    long long  freqSub   = 7100000;
    std::string modeMain = "USB";
    std::string modeSub  = "USB";
    bool tx      = false;
    int  afGain  = 128;
    int  rfGain  = 255;
    int  squelch = 0;
    int  power   = 100;
    bool nb      = false;
    bool nr      = false;
    bool autoNotch = false;
    bool manualNotch = false;
    int  notchFreq = 1000;
    int  preamp  = 0;        // 0=IPO 1=AMP1 2=AMP2
    bool att     = false;
    bool lock    = false;
    bool split   = false;
    bool rit     = false;
    int  ritOffset = 0;
    bool xit     = false;
    int  xitOffset = 0;
    // meters (0-255)
    int smeter = 0;
    int po     = 0;
    int alc    = 0;
    int swr    = 0;
    int idd    = 0;
    int vdd    = 0;
    bool connected = false;
    std::string band = "20m";
};

// =============================================================================
//  CAT Controller
// =============================================================================
class FTX1Controller {
public:
    FTX1Controller() {}

    bool connect(const std::string& port, int baud) {
        std::lock_guard<std::mutex> lk(mx_);
        if (!serial_.open(port, baud)) return false;
        state_.connected = true;
        // Enable AI mode for automatic updates
        serial_.write(CAT::setAI(true));
        // Kick off polling thread
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

    // ── Public API (thread-safe) ──────────────────────────────────────────────

    bool setFrequency(const std::string& vfo, long long hz) {
        std::lock_guard<std::mutex> lk(mx_);
        if (vfo == "SUB") state_.freqSub  = hz;
        else              state_.freqMain = hz;
        // suppress poll overwrite for 1.5 seconds after a set
        suppressPollUntil_ = std::chrono::steady_clock::now()
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
        // update local freq state to the band default too
        auto it = CAT::bandMap.find(b);
        if (it != CAT::bandMap.end()) state_.freqMain = it->second.defaultHz;
        suppressPollUntil_ = std::chrono::steady_clock::now()
                           + std::chrono::milliseconds(1500);
        return send(CAT::setBand(b));
    }

    bool setFilterWidth(int hz) { std::lock_guard<std::mutex> lk(mx_); return send(CAT::setFilterWidth(hz)); }
    bool setNB(bool on)  { std::lock_guard<std::mutex> lk(mx_); state_.nb=on;         return send(CAT::setNB(on)); }
    bool setNR(bool on)  { std::lock_guard<std::mutex> lk(mx_); state_.nr=on;         return send(CAT::setNR(on)); }
    bool setAutoNotch(bool on) { std::lock_guard<std::mutex> lk(mx_); state_.autoNotch=on; return send(CAT::setAutoNotch(on)); }
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
        std::string r = serial_.readResponse();
        lastRaw_ = r;
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

    // Register a callback for push-based status updates (used by WS)
    void onStatusUpdate(std::function<void(json)> cb) { statusCb_ = cb; }

private:
    bool send(const std::string& cmd) {
        if (cmd.empty()) return false;
        return serial_.write(cmd);
    }

    // Parse IF; response: IF[14-digit freq][5-digit rit][+/-][rit on][xit on][mem][tx][mode]...;
    void parseIF(const std::string& r) {
        if (r.size() < 30) return;
        // freq: chars 2-10 (9 digits)
        state_.freqMain = std::stoll(r.substr(2,9));
        // mode: char 21  0=LSB 1=USB 2=CW 3=CWR 4=AM 5=FM 6=DATA-L 7=DATA-U 8=DATA-FM 9=C4FM
        static const char* modes[] = {"LSB","USB","CW","CWR","AM","FM","DATA-L","DATA-U","DATA-FM","C4FM"};
        int modeIdx = r[21] - '0';
        if (modeIdx >= 0 && modeIdx <= 9) state_.modeMain = modes[modeIdx];
        // tx: char 24  0=RX 1=TX
        state_.tx = (r.size() > 24 && r[24] == '1');
    }

    // Parse meter response: RM[type][6-digit value];  e.g. RM1000000;
    void parseRM(const std::string& r) {
        if (r.size() < 9) return;
        int type = r[2] - '0';
        int val  = std::stoi(r.substr(3, 6));   // 6 digits
        // scale to 0-255 (raw value is 0-255 stored in 6 digits)
        val = std::min(val, 255);
        switch(type) {
            case 1: state_.smeter = val; break;
            case 2: state_.po     = val; break;
            case 3: state_.alc    = val; break;
            case 7: state_.swr    = val; break;
            case 6: state_.idd    = val; break;
            case 5: state_.vdd    = val; break;
        }
    }

    void pollLoop() {
        while (running_) {
            {
                std::lock_guard<std::mutex> lk(mx_);
                // skip frequency/mode poll if a set command was just sent
                bool suppressed = std::chrono::steady_clock::now() < suppressPollUntil_;
                if (serial_.isOpen()) {
                    if (!suppressed) {
                        serial_.write(CAT::getIF());
                        std::string r = serial_.readResponse(300);
                        if (r.size() > 2 && r.substr(0,2) == "IF") parseIF(r);

                        serial_.write(CAT::getFrequency("SUB"));
                        r = serial_.readResponse(200);
                        if (r.size() >= 11 && r.substr(0,2) == "FB")
                            state_.freqSub = std::stoll(r.substr(2,9));
                    }
                    // meters always poll (not affected by freq suppress)
                    serial_.write(CAT::readMeter(1));
                    std::string r = serial_.readResponse(150);
                    if (r.size() > 2 && r.substr(0,2) == "RM") parseRM(r);

                    if (state_.tx) {
                        for (int m : {2,3,7}) {
                            serial_.write(CAT::readMeter(m));
                            r = serial_.readResponse(150);
                            if (r.size() > 2 && r.substr(0,2) == "RM") parseRM(r);
                        }
                    }
                }
            }
            if (statusCb_) statusCb_(getStatus());
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }

    SerialPort serial_;
    RigState   state_;
    std::mutex mx_;
    std::atomic<bool> running_{false};
    std::thread pollThread_;
    std::string lastRaw_;
    std::function<void(json)> statusCb_;
    std::chrono::steady_clock::time_point suppressPollUntil_;
};

// =============================================================================
//  HTTP Server
// =============================================================================
void setupRoutes(httplib::Server& svr, FTX1Controller& rig) {
    using namespace httplib;

    auto jsonResp = [](Response& res, const json& j) {
        res.set_content(j.dump(), "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
    };

    svr.Options(".*", [](const Request&, Response& res){
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    });

    // ── Status ────────────────────────────────────────────────────────────────
    svr.Get("/api/status", [&](const Request&, Response& res){
        jsonResp(res, rig.getStatus());
    });

    // ── Connect/Disconnect ────────────────────────────────────────────────────
    svr.Post("/api/connect", [&](const Request& req, Response& res){
        auto j = json::parse(req.body, nullptr, false);
        std::string port = j.value("port", "/dev/ttyUSB0");
        int baud = j.value("baud", 38400);
        bool ok = rig.connect(port, baud);
        jsonResp(res, {{"ok", ok}});
    });
    svr.Post("/api/disconnect", [&](const Request&, Response& res){
        rig.disconnect();
        jsonResp(res, {{"ok", true}});
    });

    // ── Frequency ─────────────────────────────────────────────────────────────
    svr.Post("/api/frequency", [&](const Request& req, Response& res){
        auto j = json::parse(req.body, nullptr, false);
        std::string vfo = j.value("vfo", "MAIN");
        long long hz = j.value("hz", 14250000LL);
        jsonResp(res, {{"ok", rig.setFrequency(vfo, hz)}});
    });

    // ── Mode ──────────────────────────────────────────────────────────────────
    svr.Post("/api/mode", [&](const Request& req, Response& res){
        auto j = json::parse(req.body, nullptr, false);
        jsonResp(res, {{"ok", rig.setMode(j.value("vfo","MAIN"), j.value("mode","USB"))}});
    });

    // ── PTT ───────────────────────────────────────────────────────────────────
    svr.Post("/api/ptt", [&](const Request& req, Response& res){
        auto j = json::parse(req.body, nullptr, false);
        jsonResp(res, {{"ok", rig.setPTT(j.value("tx", false))}});
    });

    // ── Gains / Levels ────────────────────────────────────────────────────────
    svr.Post("/api/af_gain",  [&](const Request& req, Response& res){
        auto j=json::parse(req.body,nullptr,false);
        jsonResp(res,{{"ok",rig.setAFGain(j.value("level",128))}});
    });
    svr.Post("/api/rf_gain",  [&](const Request& req, Response& res){
        auto j=json::parse(req.body,nullptr,false);
        jsonResp(res,{{"ok",rig.setRFGain(j.value("level",255))}});
    });
    svr.Post("/api/squelch",  [&](const Request& req, Response& res){
        auto j=json::parse(req.body,nullptr,false);
        jsonResp(res,{{"ok",rig.setSquelch(j.value("level",0))}});
    });
    svr.Post("/api/power",    [&](const Request& req, Response& res){
        auto j=json::parse(req.body,nullptr,false);
        jsonResp(res,{{"ok",rig.setPower(j.value("level",100))}});
    });

    // ── VFO ───────────────────────────────────────────────────────────────────
    svr.Post("/api/vfo",  [&](const Request& req, Response& res){
        auto j=json::parse(req.body,nullptr,false);
        jsonResp(res,{{"ok",rig.vfoAction(j.value("action","toggle_ab"))}});
    });

    // ── Band ──────────────────────────────────────────────────────────────────
    svr.Post("/api/band", [&](const Request& req, Response& res){
        auto j=json::parse(req.body,nullptr,false);
        jsonResp(res,{{"ok",rig.setBand(j.value("band","20m"))}});
    });

    // ── Filter ────────────────────────────────────────────────────────────────
    svr.Post("/api/filter",[&](const Request& req, Response& res){
        auto j=json::parse(req.body,nullptr,false);
        jsonResp(res,{{"ok",rig.setFilterWidth(j.value("width",2400))}});
    });

    // ── DSP ───────────────────────────────────────────────────────────────────
    svr.Post("/api/nb",   [&](const Request& req, Response& res){
        auto j=json::parse(req.body,nullptr,false);
        jsonResp(res,{{"ok",rig.setNB(j.value("on",false))}});
    });
    svr.Post("/api/nr",   [&](const Request& req, Response& res){
        auto j=json::parse(req.body,nullptr,false);
        jsonResp(res,{{"ok",rig.setNR(j.value("on",false))}});
    });
    svr.Post("/api/notch",[&](const Request& req, Response& res){
        auto j=json::parse(req.body,nullptr,false);
        bool on = j.value("on",false);
        int  f  = j.value("freq",1000);
        bool ok = on ? rig.setManualNotch(on,f) : rig.setAutoNotch(false) && rig.setManualNotch(false);
        jsonResp(res,{{"ok",ok}});
    });

    // ── RF Path ───────────────────────────────────────────────────────────────
    svr.Post("/api/preamp",[&](const Request& req, Response& res){
        auto j=json::parse(req.body,nullptr,false);
        jsonResp(res,{{"ok",rig.setPreamp(j.value("level",0))}});
    });
    svr.Post("/api/att",  [&](const Request& req, Response& res){
        auto j=json::parse(req.body,nullptr,false);
        jsonResp(res,{{"ok",rig.setATT(j.value("on",false))}});
    });

    // ── Ops ───────────────────────────────────────────────────────────────────
    svr.Post("/api/lock", [&](const Request& req, Response& res){
        auto j=json::parse(req.body,nullptr,false);
        jsonResp(res,{{"ok",rig.setLock(j.value("on",false))}});
    });
    svr.Post("/api/split",[&](const Request& req, Response& res){
        auto j=json::parse(req.body,nullptr,false);
        jsonResp(res,{{"ok",rig.setSplit(j.value("on",false))}});
    });
    svr.Post("/api/rit",  [&](const Request& req, Response& res){
        auto j=json::parse(req.body,nullptr,false);
        jsonResp(res,{{"ok",rig.setRIT(j.value("on",false),j.value("offset",0))}});
    });
    svr.Post("/api/xit",  [&](const Request& req, Response& res){
        auto j=json::parse(req.body,nullptr,false);
        jsonResp(res,{{"ok",rig.setXIT(j.value("on",false))}});
    });
    svr.Post("/api/tuner",[&](const Request& req, Response& res){
        auto j=json::parse(req.body,nullptr,false);
        jsonResp(res,{{"ok",rig.tuner(j.value("action","start"))}});
    });

    // ── Raw CAT passthrough ───────────────────────────────────────────────────
    svr.Post("/api/raw",  [&](const Request& req, Response& res){
        auto j=json::parse(req.body,nullptr,false);
        rig.sendRaw(j.value("cmd",""));
        jsonResp(res,{{"ok",true},{"response",rig.getLastRaw()}});
    });
}

// =============================================================================
//  main
// =============================================================================
int main(int argc, char** argv) {
    std::string port   = "/dev/ttyUSB0";
    int         baud   = 38400;
    int         httpPort = 8080;
    bool        autoConnect = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if ((a == "--port" || a == "-p") && i+1 < argc) port = argv[++i];
        else if ((a == "--baud" || a == "-b") && i+1 < argc) baud = std::stoi(argv[++i]);
        else if ((a == "--http") && i+1 < argc) httpPort = std::stoi(argv[++i]);
        else if (a == "--auto") autoConnect = true;
    }

    FTX1Controller rig;

    if (autoConnect) {
        std::cout << "[FTX-1] Auto-connecting to " << port << " @ " << baud << " baud\n";
        if (!rig.connect(port, baud)) {
            std::cerr << "[FTX-1] Failed to open serial port. "
                         "You can still connect via API.\n";
        } else {
            std::cout << "[FTX-1] Connected.\n";
        }
    }

    httplib::Server svr;
    setupRoutes(svr, rig);

    // Serve the GUI HTML directly if it lives next to the binary
    svr.set_mount_point("/", ".");

    std::cout << "[FTX-1] HTTP server on http://localhost:" << httpPort << "\n";
    std::cout << "[FTX-1] Open ftx1_gui.html in your browser.\n";

    svr.listen("0.0.0.0", httpPort);
    rig.disconnect();
    return 0;
}
