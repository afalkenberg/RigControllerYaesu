/**
 * FTX1Controller.cpp
 * ==================
 * Implementation of FTX1Controller.
 */

#include "FTX1Controller.h"
#include <algorithm>
#include <cstdio>
#include <iostream>

// ============================================================================
//  Constructor / Destructor
// ============================================================================
FTX1Controller::FTX1Controller()  {}
FTX1Controller::~FTX1Controller() { disconnect(); }

// ============================================================================
//  Serial Port (Windows Win32)
// ============================================================================
bool FTX1Controller::serialOpen(const std::string& port, int baud) {
    std::string p = "\\\\.\\" + port;
    hSerial_ = CreateFileA(p.c_str(),
        GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hSerial_ == INVALID_HANDLE_VALUE) return false;

    DCB dcb{}; dcb.DCBlength = sizeof(dcb);
    GetCommState(hSerial_, &dcb);
    dcb.BaudRate    = (DWORD)baud;
    dcb.ByteSize    = 8;
    dcb.Parity      = NOPARITY;
    dcb.StopBits    = ONESTOPBIT;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    SetCommState(hSerial_, &dcb);

    COMMTIMEOUTS to{50,0,50,0,50};
    SetCommTimeouts(hSerial_, &to);
    serialOpen_ = true;
    return true;
}

void FTX1Controller::serialClose() {
    if (!serialOpen_) return;
    CloseHandle(hSerial_);
    hSerial_    = INVALID_HANDLE_VALUE;
    serialOpen_ = false;
}

bool FTX1Controller::serialIsOpen() const { return serialOpen_; }

bool FTX1Controller::serialWrite(const std::string& cmd) {
    if (!serialOpen_ || cmd.empty()) return false;
    DWORD written;
    return (WriteFile(hSerial_, cmd.data(), (DWORD)cmd.size(), &written, nullptr) && written == (DWORD)cmd.size());
}

std::string FTX1Controller::serialReadResponse(int timeoutMs) {
    if (!serialOpen_) return "";
    std::string buf;
    auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        char c; DWORD rd;
        ReadFile(hSerial_, &c, 1, &rd, nullptr);
        if (rd == 0) { Sleep(5); continue; }
        buf += c;
        if (c == ';') break;
    }
    return buf;
}

// ============================================================================
//  Internal helpers
// ============================================================================
bool FTX1Controller::send(const std::string& cmd) {
    return serialWrite(cmd);
}

bool FTX1Controller::sendAndForget(const std::string& cmd) {
    return serialWrite(cmd);
}

std::string FTX1Controller::modeNameToIndex(const std::string& name) {
    if (name=="LSB")     return "1"; if (name=="USB")     return "2";
    if (name=="CW-U")    return "3"; if (name=="CW-L")     return "7";
    if (name=="AM")      return "5"; if (name=="FM")       return "4";
    if (name=="DATA-L")  return "8"; if (name=="DATA-U")  return "C";
    if (name=="DATA-FM") return "A"; if (name=="C4FM-DN")    return "H";
    return "0";
}

int FTX1Controller::hzToWidthIndex(int hz) {
    // SH command: index 0 = 50Hz, each step = 50Hz → index = (hz-50)/50
    return std::max(0, std::min(99, (hz - 50) / 50));
}

// ============================================================================
//  Connection
// ============================================================================
bool FTX1Controller::connect(const std::string& port, int baud) {
    std::lock_guard<std::mutex> lk(mx_);
    if (!serialOpen(port, baud)) return false;
    state_.connected = true;
    // Enable full AI mode so radio pushes state changes automatically
    serialWrite(CATCommand::setAutoInformation(2));
    running_     = true;
    pollThread_  = std::thread([this]{ pollLoop(); });
    return true;
}

void FTX1Controller::disconnect() {
    running_ = false;
    if (pollThread_.joinable()) pollThread_.join();
    std::lock_guard<std::mutex> lk(mx_);
    if (serialOpen_) {
        serialWrite(CATCommand::setAutoInformation(0));
        serialClose();
    }
    state_.connected = false;
}

bool FTX1Controller::isConnected() const {
    std::lock_guard<std::mutex> lk(mx_);
    return state_.connected;
}

void FTX1Controller::onStatusUpdate(std::function<void(json)> cb) {
    statusCb_ = cb;
}

// ============================================================================
//  Status / State
// ============================================================================
RigState FTX1Controller::getState() const {
    std::lock_guard<std::mutex> lk(mx_);
    return state_;
}

json FTX1Controller::getStatus() const {
    std::lock_guard<std::mutex> lk(mx_);
    return json {
        {"connected",       state_.connected},
        {"freq_main",       state_.freqMain},
        {"freq_sub",        state_.freqSub},
        {"mode_main",       state_.modeMain},
        {"mode_sub",        state_.modeSub},
        {"band_main",       state_.bandMain},
        {"band_sub",        state_.bandSub},
        {"tx",              state_.tx},
        {"split",           state_.split},
        {"power",           state_.powerWatts},
        {"rit",             state_.rit},
        {"rit_offset",      state_.ritOffset},
        {"xit",             state_.xit},
        {"vfo_select",      state_.vfoSelect},
        {"af_gain_main",    state_.afGainMain},
        {"af_gain_sub",     state_.afGainSub},
        {"rf_gain",         state_.rfGain},
        {"squelch_main",    state_.squelchMain},
        {"squelch_sub",     state_.squelchSub},
        {"mic_gain",        state_.micGain},
        {"monitor_level",   state_.monitorLevel},
        {"preamp_main",     state_.preampMain},
        {"att_main",        state_.attMain},
        {"agc_main",        state_.agcMain},
        {"nb_main",         state_.nbMain},
        {"nr_main",         state_.nrMain},
        {"dnf_main",        state_.dnfMain},
        {"man_notch_main",  state_.manNotchMain},
        {"man_notch_freq_main", state_.manNotchFreqMain},
        {"lock_main",       state_.lockMain},
        {"preamp_sub",      state_.preampSub},
        {"att_sub",         state_.attSub},
        {"agc_sub",         state_.agcSub},
        {"nb_sub",          state_.nbSub},
        {"nr_sub",          state_.nrSub},
        {"dnf_sub",         state_.dnfSub},
        {"man_notch_sub",   state_.manNotchSub},
        {"man_notch_freq_sub",  state_.manNotchFreqSub},
        {"lock_sub",        state_.lockSub},
        {"if_shift_main",   state_.ifShiftMain},
        {"if_width_main",   state_.ifWidthMain},
        {"if_shift_sub",    state_.ifShiftSub},
        {"if_width_sub",    state_.ifWidthSub},
        {"contour_lvl_main",state_.contourLvlMain},
        {"contour_wid_main",state_.contourWidMain},
        {"speech_proc",     state_.speechProc},
        {"amc_output",      state_.amcOutput},
        {"vox_on",          state_.voxOn},
        {"vox_gain",        state_.voxGain},
        {"vox_delay",       state_.voxDelay},
        {"nb_level",        state_.nbLevel},
        {"nb_width",        state_.nbWidth},
        {"nr_level",        state_.nrLevel},
        {"smeter_main",     state_.smeterMain},
        {"smeter_sub",      state_.smeterSub},
        {"comp",            state_.comp},
        {"alc",             state_.alc},
        {"po",              state_.po},
        {"swr",             state_.swr},
        {"idd",             state_.idd},
        {"vdd",             state_.vdd}
    };
}

// ============================================================================
//  Poll loop — pure listener + active meter poll
// ============================================================================
void FTX1Controller::pollLoop() {
    while (running_) {
        {
            std::lock_guard<std::mutex> lk(mx_);
            if (serialOpen_) {
                // Drain any AI-pushed packets from the radio
                std::string buf;
                for (int i = 0; i < 32; i++) {
                    char c; DWORD rd;
                    ReadFile(hSerial_, &c, 1, &rd, nullptr);
                    if (rd == 0) break;
                    buf += c;
                    if (c == ';') { parsePacket(buf); buf.clear(); }
                }
                // Actively poll all 8 meters
                for (int m = 1; m <= METER_COUNT; m++) {
                    serialWrite(CATCommand::getReadMeter(m));
                    std::string r = serialReadResponse(150);
                    if (r.size() > 2 && r.substr(0,2) == "RM") parseRM(r);
                }
            }
        }
        if (statusCb_) statusCb_(getStatus());
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
}

// ============================================================================
//  Packet parsers
// ============================================================================
void FTX1Controller::parsePacket(const std::string& r) {
    if (r.size() < 3) return;
    const std::string cmd = r.substr(0, 2);

    if (cmd == "FA" && r.size() >= 11) {
        try { state_.freqMain = std::stoll(r.substr(2, 9)); } catch(...) {}
    }
    else if (cmd == "FB" && r.size() >= 11) {
        try { state_.freqSub = std::stoll(r.substr(2, 9)); } catch(...) {}
    }
    else if (cmd == "MD" && r.size() >= 4) {
        int vfo  = r[2] - '0';
        int midx = r[3] - '0';
        if (midx >= 0 && midx <= 9) {
            const std::string name = CATCommand::modeName(midx);
            if (vfo == 0) state_.modeMain = name;
            else          state_.modeSub  = name;
        }
    }
    else if (cmd == "TX" && r.size() >= 3) {
        state_.tx = (r[2] != '0');
    }
    else if (cmd == "SP" && r.size() >= 3) {
        state_.split = (r[2] == '1');
    }
    else if (cmd == "RT" && r.size() >= 3) {
        state_.rit = (r[2] == '1');
    }
    else if (cmd == "XT" && r.size() >= 3) {
        state_.xit = (r[2] == '1');
    }
    else if (cmd == "PC" && r.size() >= 5) {
        try { state_.powerWatts = std::stoi(r.substr(2, 3)); } catch(...) {}
    }
    else if (cmd == "AG" && r.size() >= 6) {
        int vfo = r[2] - '0';
        try {
            int v = std::stoi(r.substr(3, 3));
            if (vfo == 0) state_.afGainMain = v;
            else          state_.afGainSub  = v;
        } catch(...) {}
    }
    else if (cmd == "RG" && r.size() >= 5) {
        try { state_.rfGain = std::stoi(r.substr(2, 3)); } catch(...) {}
    }
    else if (cmd == "SQ" && r.size() >= 6) {
        int vfo = r[2] - '0';
        try {
            int v = std::stoi(r.substr(3, 3));
            if (vfo == 0) state_.squelchMain = v;
            else          state_.squelchSub  = v;
        } catch(...) {}
    }
    else if (cmd == "MG" && r.size() >= 5) {
        try { state_.micGain = std::stoi(r.substr(2, 3)); } catch(...) {}
    }
    else if (cmd == "ML" && r.size() >= 5) {
        try { state_.monitorLevel = std::stoi(r.substr(2, 3)); } catch(...) {}
    }
    else if (cmd == "NB" && r.size() >= 4) {
        int vfo = r[2] - '0';
        bool on = (r[3] == '1');
        if (vfo == 0) state_.nbMain = on;
        else          state_.nbSub  = on;
    }
    else if (cmd == "NR" && r.size() >= 4) {
        int vfo = r[2] - '0';
        bool on = (r[3] == '1');
        if (vfo == 0) state_.nrMain = on;
        else          state_.nrSub  = on;
    }
    else if (cmd == "BC" && r.size() >= 4) {
        int vfo = r[2] - '0';
        bool on = (r[3] == '1');
        if (vfo == 0) state_.dnfMain = on;
        else          state_.dnfSub  = on;
    }
    else if (cmd == "BP" && r.size() >= 7) {
        bool on = (r[2] == '1');
        try {
            int f = std::stoi(r.substr(3, 4));
            state_.manNotchMain    = on;
            state_.manNotchFreqMain = f;
        } catch(...) {}
    }
    else if (cmd == "PR" && r.size() >= 3) {
        state_.speechProc = (r[2] == '1');
    }
    else if (cmd == "AO" && r.size() >= 5) {
        try { state_.amcOutput = std::stoi(r.substr(2, 3)); } catch(...) {}
    }
    else if (cmd == "VX" && r.size() >= 3) {
        state_.voxOn = (r[2] == '1');
    }
    else if (cmd == "VG" && r.size() >= 5) {
        try { state_.voxGain = std::stoi(r.substr(2, 3)); } catch(...) {}
    }
    else if (cmd == "PA" && r.size() >= 4) {
        int band  = r[2] - '0';
        int level = r[3] - '0';
        if (band == 0) state_.preampMain = level;
        else           state_.preampSub  = level;
    }
    else if (cmd == "RM") {
        parseRM(r);
    }
}

void FTX1Controller::parseRM(const std::string& r) {
    if (r.size() < 9) return;
    try {
        int id  = r[2] - '0';
        int val = std::min(255, std::stoi(r.substr(3, 6)));
        switch (id) {
            case 1: state_.smeterMain = val; break;
            case 2: state_.smeterSub  = val; break;
            case 3: state_.comp       = val; break;
            case 4: state_.alc        = val; break;
            case 5: state_.po         = val; break;
            case 6: state_.swr        = val; break;
            case 7: state_.idd        = val; break;
            case 8: state_.vdd        = val; break;
        }
    } catch(...) {}
}

// ============================================================================
//  Raw passthrough
// ============================================================================
bool FTX1Controller::sendRaw(const std::string& cmd) {
    std::lock_guard<std::mutex> lk(mx_);
    serialWrite(cmd);
    lastRaw_ = serialReadResponse(500);
    return true;
}

std::string FTX1Controller::getLastRawResponse() const {
    std::lock_guard<std::mutex> lk(mx_);
    return lastRaw_;
}

// ============================================================================
//  Frequency
// ============================================================================
bool FTX1Controller::setMainSideFrequency(long long hz) {
    std::lock_guard<std::mutex> lk(mx_);
    state_.freqMain = hz;
    return send(CATCommand::setMainSideFrequency(hz));
}
bool FTX1Controller::setSubSideFrequency(long long hz) {
    std::lock_guard<std::mutex> lk(mx_);
    state_.freqSub = hz;
    return send(CATCommand::setSubSideFrequency(hz));
}
bool FTX1Controller::getMainSideFrequency() {
    std::lock_guard<std::mutex> lk(mx_);
    return send(CATCommand::getMainSideFrequency());
}
bool FTX1Controller::getSubSideFrequency() {
    std::lock_guard<std::mutex> lk(mx_);
    return send(CATCommand::getSubSideFrequency());
}

// ============================================================================
//  Band
// ============================================================================
bool FTX1Controller::setMainBand(const std::string& band) {
    std::lock_guard<std::mutex> lk(mx_);
    auto it = g_bandMap.find(band);
    if (it == g_bandMap.end()) return false;
    state_.bandMain = band;
    state_.freqMain = it->second.defaultHz;
    return send(CATCommand::setMainSideFrequency(it->second.defaultHz));
}
bool FTX1Controller::setSubBand(const std::string& band) {
    std::lock_guard<std::mutex> lk(mx_);
    auto it = g_bandMap.find(band);
    if (it == g_bandMap.end()) return false;
    state_.bandSub = band;
    state_.freqSub = it->second.defaultHz;
    return send(CATCommand::setSubSideFrequency(it->second.defaultHz));
}

// ============================================================================
//  Mode
// ============================================================================
bool FTX1Controller::setMainOperatingMode(const std::string& modeName) {
    std::lock_guard<std::mutex> lk(mx_);
    state_.modeMain = modeName;
    return send(CATCommand::setOperatingMode(0, modeNameToIndex(modeName)));
}
bool FTX1Controller::setSubOperatingMode(const std::string& modeName) {
    std::lock_guard<std::mutex> lk(mx_);
    state_.modeSub = modeName;
    return send(CATCommand::setOperatingMode(1, modeNameToIndex(modeName)));
}
bool FTX1Controller::getOperatingMode(int vfo) {
    std::lock_guard<std::mutex> lk(mx_);
    return send(CATCommand::getOperatingMode(vfo));
}

// ============================================================================
//  TX / PTT
// ============================================================================
bool FTX1Controller::setTx(bool transmit) {
    std::lock_guard<std::mutex> lk(mx_);
    state_.tx = transmit;
    return send(transmit ? CATCommand::setTx(1) : CATCommand::setRx());
}
bool FTX1Controller::getTx() {
    std::lock_guard<std::mutex> lk(mx_);
    return send(CATCommand::getTx());
}

// ============================================================================
//  Split
// ============================================================================
bool FTX1Controller::setSplit(bool on) {
    std::lock_guard<std::mutex> lk(mx_);
    state_.split = on;
    return send(CATCommand::setSplit(on));
}
bool FTX1Controller::getSplit() {
    std::lock_guard<std::mutex> lk(mx_);
    return send(CATCommand::getSplit());
}

// ============================================================================
//  VFO operations
// ============================================================================
bool FTX1Controller::setSwapVfo() {
    std::lock_guard<std::mutex> lk(mx_);
    std::swap(state_.freqMain, state_.freqSub);
    std::swap(state_.modeMain, state_.modeSub);
    return send(CATCommand::setSwapVfo());
}
bool FTX1Controller::setMainSideToSubSide() {
    std::lock_guard<std::mutex> lk(mx_);
    state_.freqSub = state_.freqMain;
    state_.modeSub = state_.modeMain;
    return send(CATCommand::setMainSideToSubSide());
}
bool FTX1Controller::setSubSideToMainSide() {
    std::lock_guard<std::mutex> lk(mx_);
    state_.freqMain = state_.freqSub;
    state_.modeMain = state_.modeSub;
    return send(CATCommand::setSubSideToMainSide());
}
bool FTX1Controller::setMainSideToMemoryChannel()   { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setMainSideToMemoryChannel()); }
bool FTX1Controller::setSubSideToMemoryChannel()    { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setSubSideToMemoryChannel()); }
bool FTX1Controller::setMemoryChannelToMainSide()   { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setMemoryChannelToMainSide()); }
bool FTX1Controller::setMemoryChannelToSubSide()    { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setMemoryChannelToSubSide()); }
bool FTX1Controller::setVfoSelect(int v)            { std::lock_guard<std::mutex> lk(mx_); state_.vfoSelect=v; return send(CATCommand::setVfoSelect(v)); }
bool FTX1Controller::getVfoSelect()                 { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getVfoSelect()); }
bool FTX1Controller::setFunctionTx(int v)           { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setFunctionTx(v)); }
bool FTX1Controller::getFunctionTx()                { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getFunctionTx()); }
bool FTX1Controller::setFunctionRx(int m)           { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setFunctionRx(m)); }
bool FTX1Controller::getFunctionRx()                { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getFunctionRx()); }
bool FTX1Controller::setVfoOrMemoryChannel(int v,int m){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setVfoOrMemoryChannel(v,m)); }
bool FTX1Controller::getVfoOrMemoryChannel(int v)   { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getVfoOrMemoryChannel(v)); }
bool FTX1Controller::setBandUp(int v)                    { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setBandUp(v)); }
bool FTX1Controller::setBandDown(int v)                  { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setBandDown(v)); }

// ============================================================================
//  RIT / XIT
// ============================================================================
bool FTX1Controller::setRit(bool on) {
    std::lock_guard<std::mutex> lk(mx_);
    state_.rit = on;
    return send(CATCommand::setRit(on));
}
bool FTX1Controller::setRitOffset(int hz) {
    std::lock_guard<std::mutex> lk(mx_);
    state_.ritOffset = hz;
    // Clear first, then step in 10Hz increments
    std::string cmd = CATCommand::clearRitXit();
    if      (hz > 0) cmd += CATCommand::setRitUp(hz / 10);
    else if (hz < 0) cmd += CATCommand::setRitDown(-hz / 10);
    return send(cmd);
}
bool FTX1Controller::getRit()             { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getRit()); }
bool FTX1Controller::setXit(bool on)     { std::lock_guard<std::mutex> lk(mx_); state_.xit=on; return send(on?"XT1;":"XT0;"); }
bool FTX1Controller::clearRitXit()       { std::lock_guard<std::mutex> lk(mx_); state_.ritOffset=0; return send(CATCommand::clearRitXit()); }

// ============================================================================
//  Fine tuning
// ============================================================================
bool FTX1Controller::setFineTuning(int mode) {
    std::lock_guard<std::mutex> lk(mx_);
    state_.fineTuning = mode;
    return send(CATCommand::setFineTuning(mode));
}
bool FTX1Controller::getFineTuning() { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getFineTuning()); }

// ============================================================================
//  Gains
// ============================================================================
bool FTX1Controller::setAfGainMain(int v)  { std::lock_guard<std::mutex> lk(mx_); state_.afGainMain=v;  return send(CATCommand::setAfGain(0,v)); }
bool FTX1Controller::setAfGainSub(int v)   { std::lock_guard<std::mutex> lk(mx_); state_.afGainSub=v;   return send(CATCommand::setAfGain(1,v)); }
bool FTX1Controller::getAfGain(int vfo)    { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getAfGain(vfo)); }
bool FTX1Controller::setRfGain(int v)      { std::lock_guard<std::mutex> lk(mx_); state_.rfGain=v;      return send(CATCommand::setRfGain(v)); }
bool FTX1Controller::getRfGain()           { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getRfGain()); }
bool FTX1Controller::setSquelchMain(int v) { std::lock_guard<std::mutex> lk(mx_); state_.squelchMain=v; return send(CATCommand::setSquelchLevel(0,v)); }
bool FTX1Controller::setSquelchSub(int v)  { std::lock_guard<std::mutex> lk(mx_); state_.squelchSub=v;  return send(CATCommand::setSquelchLevel(1,v)); }
bool FTX1Controller::getSquelchLevel(int v){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getSquelchLevel(v)); }
bool FTX1Controller::setMicGain(int v)     { std::lock_guard<std::mutex> lk(mx_); state_.micGain=v;     return send(CATCommand::setMicGain(v)); }
bool FTX1Controller::getMicGain()          { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getMicGain()); }
bool FTX1Controller::setMonitorLevel(int v){ std::lock_guard<std::mutex> lk(mx_); state_.monitorLevel=v;return send(CATCommand::setMonitorLevel(v)); }
bool FTX1Controller::getMonitorLevel()     { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getMonitorLevel()); }
bool FTX1Controller::setPowerControl(int v){ std::lock_guard<std::mutex> lk(mx_); state_.powerWatts=v;  return send(CATCommand::setPowerControl(v)); }
bool FTX1Controller::getPowerControl()     { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getPowerControl()); }

// ============================================================================
//  Preamp / ATT / AGC
// ============================================================================
bool FTX1Controller::setPreampMain(int l) { std::lock_guard<std::mutex> lk(mx_); state_.preampMain=l; return send(CATCommand::setPreamp(0,l)); }
bool FTX1Controller::setPreampSub(int l)  { std::lock_guard<std::mutex> lk(mx_); state_.preampSub=l;  return send(CATCommand::setPreamp(1,l)); }
bool FTX1Controller::getPreamp(int band)  { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getPreamp(band)); }
bool FTX1Controller::setAttenuatorMain(bool on, int level){ std::lock_guard<std::mutex> lk(mx_); state_.attMain=on; return send(CATCommand::setAttenuator(on,level)); }
bool FTX1Controller::setAttenuatorSub(bool on, int level) { std::lock_guard<std::mutex> lk(mx_); state_.attSub=on;  return send(CATCommand::setAttenuator(on,level)); }
bool FTX1Controller::getAttenuator()     { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getAttenuator()); }
bool FTX1Controller::setAgcMain(int m)   { std::lock_guard<std::mutex> lk(mx_); state_.agcMain=m; return send(CATCommand::setAgcFunction(0,m)); }
bool FTX1Controller::setAgcSub(int m)    { std::lock_guard<std::mutex> lk(mx_); state_.agcSub=m;  return send(CATCommand::setAgcFunction(1,m)); }
bool FTX1Controller::getAgcFunction(int v){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getAgcFunction(v)); }

// ============================================================================
//  Noise Blanker
// ============================================================================
bool FTX1Controller::setNoiseBlankerMain(bool on){ std::lock_guard<std::mutex> lk(mx_); state_.nbMain=on; return send(CATCommand::setNoiseBlanker(0,on)); }
bool FTX1Controller::setNoiseBlankerSub(bool on) { std::lock_guard<std::mutex> lk(mx_); state_.nbSub=on;  return send(CATCommand::setNoiseBlanker(1,on)); }
bool FTX1Controller::getNoiseBlanker(int v)       { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getNoiseBlanker(v)); }
bool FTX1Controller::setNbLevel(int l) { std::lock_guard<std::mutex> lk(mx_); state_.nbLevel=l; return send(CATCommand::setNbLevel(l)); }
bool FTX1Controller::setNbWidth(int w) { std::lock_guard<std::mutex> lk(mx_); state_.nbWidth=w; return send(CATCommand::setNbWidth(w)); }
bool FTX1Controller::getNbLevel()      { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getNbLevel()); }
bool FTX1Controller::getNbWidth()      { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getNbWidth()); }

// ============================================================================
//  Noise Reduction
// ============================================================================
bool FTX1Controller::setNoiseReductionMain(bool on){ std::lock_guard<std::mutex> lk(mx_); state_.nrMain=on; return send(CATCommand::setNoiseReduction(0,on)); }
bool FTX1Controller::setNoiseReductionSub(bool on) { std::lock_guard<std::mutex> lk(mx_); state_.nrSub=on;  return send(CATCommand::setNoiseReduction(1,on)); }
bool FTX1Controller::getNoiseReduction(int v)       { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getNoiseReduction(v)); }
bool FTX1Controller::setNrLevel(int l) { std::lock_guard<std::mutex> lk(mx_); state_.nrLevel=l; return send(CATCommand::setNrLevel(l)); }
bool FTX1Controller::getNrLevel()      { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getNrLevel()); }

// ============================================================================
//  Auto Notch (DNF)
// ============================================================================
bool FTX1Controller::setAutoNotchMain(bool on){ std::lock_guard<std::mutex> lk(mx_); state_.dnfMain=on; return send(CATCommand::setAutoNotch(0,on)); }
bool FTX1Controller::setAutoNotchSub(bool on) { std::lock_guard<std::mutex> lk(mx_); state_.dnfSub=on;  return send(CATCommand::setAutoNotch(1,on)); }
bool FTX1Controller::getAutoNotch(int v)       { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getAutoNotch(v)); }

// ============================================================================
//  Manual Notch
// ============================================================================
bool FTX1Controller::setManualNotchMain(bool on, int hz) {
    std::lock_guard<std::mutex> lk(mx_);
    state_.manNotchMain=on; state_.manNotchFreqMain=hz;
    return send(CATCommand::setManualNotch(on,hz));
}
bool FTX1Controller::setManualNotchSub(bool on, int hz) {
    std::lock_guard<std::mutex> lk(mx_);
    state_.manNotchSub=on; state_.manNotchFreqSub=hz;
    return send(CATCommand::setManualNotch(on,hz));
}
bool FTX1Controller::getManualNotch() { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getManualNotch()); }

// ============================================================================
//  Lock
// ============================================================================
bool FTX1Controller::setLockMain(bool on){ std::lock_guard<std::mutex> lk(mx_); state_.lockMain=on; return send(CATCommand::setLock(on)); }
bool FTX1Controller::getLock()           { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getLock()); }

// ============================================================================
//  IF Shift
// ============================================================================
bool FTX1Controller::setIfShiftMain(int hz){ std::lock_guard<std::mutex> lk(mx_); state_.ifShiftMain=hz; return send(CATCommand::setIfShift(0,hz)); }
bool FTX1Controller::setIfShiftSub(int hz) { std::lock_guard<std::mutex> lk(mx_); state_.ifShiftSub=hz;  return send(CATCommand::setIfShift(1,hz)); }
bool FTX1Controller::getIfShift(int v)     { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getIfShift(v)); }

// ============================================================================
//  Width
// ============================================================================
bool FTX1Controller::setWidthMain(int hz){ std::lock_guard<std::mutex> lk(mx_); state_.ifWidthMain=hz; return send(CATCommand::setWidth(0,hzToWidthIndex(hz))); }
bool FTX1Controller::setWidthSub(int hz) { std::lock_guard<std::mutex> lk(mx_); state_.ifWidthSub=hz;  return send(CATCommand::setWidth(1,hzToWidthIndex(hz))); }
bool FTX1Controller::getWidth(int v)     { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getWidth(v)); }

// ============================================================================
//  Contour
// ============================================================================
bool FTX1Controller::setContour(int db, int w)    { std::lock_guard<std::mutex> lk(mx_); state_.contourLvlMain=db; state_.contourWidMain=w; return send(CATCommand::setContour(db,w)); }
bool FTX1Controller::getContour()                  { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getContour()); }
bool FTX1Controller::setContourLevel(int db)       { std::lock_guard<std::mutex> lk(mx_); state_.contourLvlMain=db; return send(CATCommand::setContourLevel(db)); }
bool FTX1Controller::setContourWidth(int q)        { std::lock_guard<std::mutex> lk(mx_); state_.contourWidMain=q;  return send(CATCommand::setContourWidth(q)); }
bool FTX1Controller::getContourLevel()             { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getContourLevel()); }
bool FTX1Controller::getContourWidth()             { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getContourWidth()); }
bool FTX1Controller::setIfNotchWidth(int w)        { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setIfNotchWidth(w)); }
bool FTX1Controller::getIfNotchWidth()             { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getIfNotchWidth()); }
bool FTX1Controller::setApfWidth(int w)            { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setApfWidth(w)); }
bool FTX1Controller::getApfWidth()                 { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getApfWidth()); }

// ============================================================================
//  LCUT / HCUT
// ============================================================================
bool FTX1Controller::setLcutFreq(int i)  { std::lock_guard<std::mutex> lk(mx_); state_.lcutFreqMain=i;  return send(CATCommand::setLcutFreq(i)); }
bool FTX1Controller::setLcutSlope(int s) { std::lock_guard<std::mutex> lk(mx_); state_.lcutSlopeMain=s; return send(CATCommand::setLcutSlope(s)); }
bool FTX1Controller::setHcutFreq(int i)  { std::lock_guard<std::mutex> lk(mx_); state_.hcutFreqMain=i;  return send(CATCommand::setHcutFreq(i)); }
bool FTX1Controller::setHcutSlope(int s) { std::lock_guard<std::mutex> lk(mx_); state_.hcutSlopeMain=s; return send(CATCommand::setHcutSlope(s)); }
bool FTX1Controller::getLcutFreq()       { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getLcutFreq()); }
bool FTX1Controller::getLcutSlope()      { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getLcutSlope()); }
bool FTX1Controller::getHcutFreq()       { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getHcutFreq()); }
bool FTX1Controller::getHcutSlope()      { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getHcutSlope()); }

// Mode-specific LCUT/HCUT — dispatches to the correct EX menu based on mode name
bool FTX1Controller::setLcutFreqForMode(const std::string& m, int i) {
    std::lock_guard<std::mutex> lk(mx_);
    if (m=="CW") return send(CATCommand::setCwLcutFreq(i));
    if (m=="AM") return send(CATCommand::setAmLcutFreq(i));
    if (m=="FM") return send(CATCommand::setFmLcutFreq(i));
    if (m=="DATA-L"||m=="DATA-U"||m=="DATA-FM") return send(CATCommand::setDataLcutFreq(i));
    if (m=="RTTY") return send(CATCommand::setRttyLcutFreq(i));
    return send(CATCommand::setLcutFreq(i)); // default = SSB
}
bool FTX1Controller::setLcutSlopeForMode(const std::string& m, int s) {
    std::lock_guard<std::mutex> lk(mx_);
    if (m=="CW") return send(CATCommand::setCwLcutSlope(s));
    if (m=="AM") return send(CATCommand::setAmLcutSlope(s));
    if (m=="FM") return send(CATCommand::setFmLcutSlope(s));
    if (m=="DATA-L"||m=="DATA-U"||m=="DATA-FM") return send(CATCommand::setDataLcutSlope(s));
    if (m=="RTTY") return send(CATCommand::setRttyLcutSlope(s));
    return send(CATCommand::setLcutSlope(s));
}
bool FTX1Controller::setHcutFreqForMode(const std::string& m, int i) {
    std::lock_guard<std::mutex> lk(mx_);
    if (m=="CW") return send(CATCommand::setCwHcutFreq(i));
    if (m=="AM") return send(CATCommand::setAmHcutFreq(i));
    if (m=="FM") return send(CATCommand::setFmHcutFreq(i));
    if (m=="DATA-L"||m=="DATA-U"||m=="DATA-FM") return send(CATCommand::setDataHcutFreq(i));
    if (m=="RTTY") return send(CATCommand::setRttyHcutFreq(i));
    return send(CATCommand::setHcutFreq(i));
}
bool FTX1Controller::setHcutSlopeForMode(const std::string& m, int s) {
    std::lock_guard<std::mutex> lk(mx_);
    if (m=="CW") return send(CATCommand::setCwHcutSlope(s));
    if (m=="AM") return send(CATCommand::setAmHcutSlope(s));
    if (m=="FM") return send(CATCommand::setFmHcutSlope(s));
    if (m=="DATA-L"||m=="DATA-U"||m=="DATA-FM") return send(CATCommand::setDataHcutSlope(s));
    if (m=="RTTY") return send(CATCommand::setRttyHcutSlope(s));
    return send(CATCommand::setHcutSlope(s));
}

// ============================================================================
//  AGC delays
// ============================================================================
bool FTX1Controller::setAgcFastDelay(int ms){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setAgcFastDelay(ms)); }
bool FTX1Controller::setAgcMidDelay(int ms) { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setAgcMidDelay(ms)); }
bool FTX1Controller::setAgcSlowDelay(int ms){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setAgcSlowDelay(ms)); }
bool FTX1Controller::getAgcFastDelay()      { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getAgcFastDelay()); }
bool FTX1Controller::getAgcMidDelay()       { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getAgcMidDelay()); }
bool FTX1Controller::getAgcSlowDelay()      { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getAgcSlowDelay()); }

// ============================================================================
//  AF tone controls
// ============================================================================
bool FTX1Controller::setAfTrebleGain(int db)    { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setAfTrebleGain(db)); }
bool FTX1Controller::setAfMiddleToneGain(int db){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setAfMiddleToneGain(db)); }
bool FTX1Controller::setAfBassGain(int db)      { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setAfBassGain(db)); }
bool FTX1Controller::getAfTrebleGain()          { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getAfTrebleGain()); }
bool FTX1Controller::getAfMiddleToneGain()      { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getAfMiddleToneGain()); }
bool FTX1Controller::getAfBassGain()            { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getAfBassGain()); }

// ============================================================================
//  TX audio
// ============================================================================
bool FTX1Controller::setSpeechProcessor(bool on){ std::lock_guard<std::mutex> lk(mx_); state_.speechProc=on; return send(CATCommand::setSpeechProcessor(on)); }
bool FTX1Controller::getSpeechProcessor()       { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getSpeechProcessor()); }
bool FTX1Controller::setAmcOutputLevel(int v)   { std::lock_guard<std::mutex> lk(mx_); state_.amcOutput=v; return send(CATCommand::setAmcOutputLevel(v)); }
bool FTX1Controller::getAmcOutputLevel()        { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getAmcOutputLevel()); }
bool FTX1Controller::setModSource(int s)        { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setModSource(s)); }
bool FTX1Controller::getModSource()             { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getModSource()); }
bool FTX1Controller::setUsbModGain(int v)       { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setUsbModGain(v)); }
bool FTX1Controller::getUsbModGain()            { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getUsbModGain()); }
bool FTX1Controller::setUsbOutLevel(int v)      { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setUsbOutLevel(v)); }
bool FTX1Controller::getUsbOutLevel()           { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getUsbOutLevel()); }
bool FTX1Controller::setTxBpfSel(int s)         { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setTxBpfSel(s)); }
bool FTX1Controller::getTxBpfSel()              { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getTxBpfSel()); }

// ============================================================================
//  Parametric EQ — processor OFF
// ============================================================================
bool FTX1Controller::setPrmtrcEq1Freq(int i)  { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setPrmtrcEq1Freq(i)); }
bool FTX1Controller::setPrmtrcEq1Level(int db){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setPrmtrcEq1Level(db)); }
bool FTX1Controller::setPrmtrcEq1Bwth(int bw) { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setPrmtrcEq1Bwth(bw)); }
bool FTX1Controller::setPrmtrcEq2Freq(int i)  { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setPrmtrcEq2Freq(i)); }
bool FTX1Controller::setPrmtrcEq2Level(int db){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setPrmtrcEq2Level(db)); }
bool FTX1Controller::setPrmtrcEq2Bwth(int bw) { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setPrmtrcEq2Bwth(bw)); }
bool FTX1Controller::setPrmtrcEq3Freq(int i)  { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setPrmtrcEq3Freq(i)); }
bool FTX1Controller::setPrmtrcEq3Level(int db){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setPrmtrcEq3Level(db)); }
bool FTX1Controller::setPrmtrcEq3Bwth(int bw) { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setPrmtrcEq3Bwth(bw)); }
bool FTX1Controller::getPrmtrcEq1Freq()       { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getPrmtrcEq1Freq()); }
bool FTX1Controller::getPrmtrcEq1Level()      { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getPrmtrcEq1Level()); }
bool FTX1Controller::getPrmtrcEq1Bwth()       { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getPrmtrcEq1Bwth()); }
bool FTX1Controller::getPrmtrcEq2Freq()       { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getPrmtrcEq2Freq()); }
bool FTX1Controller::getPrmtrcEq2Level()      { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getPrmtrcEq2Level()); }
bool FTX1Controller::getPrmtrcEq2Bwth()       { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getPrmtrcEq2Bwth()); }
bool FTX1Controller::getPrmtrcEq3Freq()       { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getPrmtrcEq3Freq()); }
bool FTX1Controller::getPrmtrcEq3Level()      { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getPrmtrcEq3Level()); }
bool FTX1Controller::getPrmtrcEq3Bwth()       { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getPrmtrcEq3Bwth()); }

// ============================================================================
//  Parametric EQ — processor ON
// ============================================================================
bool FTX1Controller::setPPrmtrcEq1Freq(int i)  { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setPPrmtrcEq1Freq(i)); }
bool FTX1Controller::setPPrmtrcEq1Level(int db){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setPPrmtrcEq1Level(db)); }
bool FTX1Controller::setPPrmtrcEq1Bwth(int bw) { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setPPrmtrcEq1Bwth(bw)); }
bool FTX1Controller::setPPrmtrcEq2Freq(int i)  { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setPPrmtrcEq2Freq(i)); }
bool FTX1Controller::setPPrmtrcEq2Level(int db){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setPPrmtrcEq2Level(db)); }
bool FTX1Controller::setPPrmtrcEq2Bwth(int bw) { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setPPrmtrcEq2Bwth(bw)); }
bool FTX1Controller::setPPrmtrcEq3Freq(int i)  { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setPPrmtrcEq3Freq(i)); }
bool FTX1Controller::setPPrmtrcEq3Level(int db){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setPPrmtrcEq3Level(db)); }
bool FTX1Controller::setPPrmtrcEq3Bwth(int bw) { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setPPrmtrcEq3Bwth(bw)); }
bool FTX1Controller::getPPrmtrcEq1Freq()       { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getPPrmtrcEq1Freq()); }
bool FTX1Controller::getPPrmtrcEq1Level()      { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getPPrmtrcEq1Level()); }
bool FTX1Controller::getPPrmtrcEq1Bwth()       { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getPPrmtrcEq1Bwth()); }
bool FTX1Controller::getPPrmtrcEq2Freq()       { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getPPrmtrcEq2Freq()); }
bool FTX1Controller::getPPrmtrcEq2Level()      { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getPPrmtrcEq2Level()); }
bool FTX1Controller::getPPrmtrcEq2Bwth()       { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getPPrmtrcEq2Bwth()); }
bool FTX1Controller::getPPrmtrcEq3Freq()       { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getPPrmtrcEq3Freq()); }
bool FTX1Controller::getPPrmtrcEq3Level()      { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getPPrmtrcEq3Level()); }
bool FTX1Controller::getPPrmtrcEq3Bwth()       { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getPPrmtrcEq3Bwth()); }

// ============================================================================
//  VOX
// ============================================================================
bool FTX1Controller::setVoxStatus(bool on)  { std::lock_guard<std::mutex> lk(mx_); state_.voxOn=on;    return send(CATCommand::setVoxStatus(on)); }
bool FTX1Controller::getVoxStatus()         { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getVoxStatus()); }
bool FTX1Controller::setVoxGain(int v)      { std::lock_guard<std::mutex> lk(mx_); state_.voxGain=v;   return send(CATCommand::setVoxGain(v)); }
bool FTX1Controller::getVoxGain()           { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getVoxGain()); }
bool FTX1Controller::setVoxDelayTime(int c) { std::lock_guard<std::mutex> lk(mx_); state_.voxDelay=c;  return send(CATCommand::setVoxDelayTime(c)); }
bool FTX1Controller::getVoxDelayTime()      { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getVoxDelayTime()); }
bool FTX1Controller::setVoxSelect(int s)    { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setVoxSelect(s)); }
bool FTX1Controller::getVoxSelect()         { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getVoxSelect()); }

// ============================================================================
//  Antenna tuner
// ============================================================================
bool FTX1Controller::setAntennaTunerStart()   { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setAntennaTunerControl(2)); }
bool FTX1Controller::setAntennaTunerStop()    { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setAntennaTunerControl(0)); }
bool FTX1Controller::getAntennaTunerControl() { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getAntennaTunerControl()); }

// ============================================================================
//  Scan
// ============================================================================
bool FTX1Controller::setScan(int m){ std::lock_guard<std::mutex> lk(mx_); state_.scanMode=m; return send(CATCommand::setScan(m)); }
bool FTX1Controller::getScan()     { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getScan()); }

// ============================================================================
//  Memory
// ============================================================================
bool FTX1Controller::setMemoryChannel(int v, int ch)   { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setMemoryChannel(v,ch)); }
bool FTX1Controller::getMemoryChannel(int v)           { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getMemoryChannel(v)); }
bool FTX1Controller::getMemoryChannelRead(int v, int ch){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getMemoryChannelRead(v,ch)); }

// ============================================================================
//  CW
// ============================================================================
bool FTX1Controller::setKeySpeed(int w)             { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setKeySpeed(w)); }
bool FTX1Controller::getKeySpeed()                  { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getKeySpeed()); }
bool FTX1Controller::setKeyPitch(int hz)            { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setKeyPitch(hz)); }
bool FTX1Controller::getKeyPitch()                  { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getKeyPitch()); }
bool FTX1Controller::setKeyer(int m)                { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setKeyer(m)); }
bool FTX1Controller::getKeyer()                     { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getKeyer()); }
bool FTX1Controller::setCwBreakInDelay(int ms)      { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setCwBreakInDelay(ms)); }
bool FTX1Controller::getCwBreakInDelay()            { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getCwBreakInDelay()); }
bool FTX1Controller::setBreakIn(int m)              { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setBreakIn(m)); }
bool FTX1Controller::getBreakIn()                   { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getBreakIn()); }
bool FTX1Controller::setCwSpot(bool on)             { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setCwSpot(on)); }
bool FTX1Controller::getCwSpot()                    { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getCwSpot()); }
bool FTX1Controller::setZeroIn()                    { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setZeroIn()); }
bool FTX1Controller::setCwAutoMode(int m)           { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setCwAutoMode(m)); }
bool FTX1Controller::getCwAutoMode()                { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getCwAutoMode()); }
bool FTX1Controller::setKeyerMemory(int ch, const std::string& t){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setKeyerMemory(ch,t)); }
bool FTX1Controller::getKeyerMemory(int ch)         { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getKeyerMemory(ch)); }
bool FTX1Controller::setCwKeyingMemoryPlay(int ch)  { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setCwKeyingMemoryPlay(ch)); }
bool FTX1Controller::setCwKeyingText(const std::string& t){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setCwKeyingText(t)); }

// ============================================================================
//  CTCSS / DCS
// ============================================================================
bool FTX1Controller::setCtcss(int v, int m)      { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setCtcss(v,m)); }
bool FTX1Controller::getCtcss(int v)             { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getCtcss(v)); }
bool FTX1Controller::setCtcssNumber(int v, int t){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setCtcssNumber(v,t)); }
bool FTX1Controller::getCtcssNumber(int v)       { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getCtcssNumber(v)); }

// ============================================================================
//  Clarifier
// ============================================================================
bool FTX1Controller::setClarifier(int v, bool on, int hz){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setClarifier(v,on,hz)); }
bool FTX1Controller::getClarifier(int v)                 { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getClarifier(v)); }

// ============================================================================
//  FM repeater
// ============================================================================
bool FTX1Controller::setFmRptShift(int d)        { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setFmRptShift(d)); }
bool FTX1Controller::getFmRptShift()             { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getFmRptShift()); }
bool FTX1Controller::setFmRptShiftFreq144(int hz){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setFmRptShiftFreq144(hz)); }
bool FTX1Controller::setFmRptShiftFreq430(int hz){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setFmRptShiftFreq430(hz)); }
bool FTX1Controller::getFmRptShiftFreq144()      { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getFmRptShiftFreq144()); }
bool FTX1Controller::getFmRptShiftFreq430()      { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getFmRptShiftFreq430()); }

// ============================================================================
//  Spectrum scope
// ============================================================================
bool FTX1Controller::setSpectrumScope(int m){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setSpectrumScope(m)); }
bool FTX1Controller::getSpectrumScope()     { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getSpectrumScope()); }

// ============================================================================
//  Meters
// ============================================================================
bool FTX1Controller::readAllMeters() {
    std::lock_guard<std::mutex> lk(mx_);
    bool ok = true;
    for (int m = 1; m <= METER_COUNT; m++) {
        ok &= serialWrite(CATCommand::getReadMeter(m));
        std::string r = serialReadResponse(150);
        if (r.size() > 2 && r.substr(0,2) == "RM") parseRM(r);
    }
    return ok;
}
bool FTX1Controller::getSmeterMain()  { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getSmeterMain()); }
bool FTX1Controller::getSmeterSub()   { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getSmeterSub()); }
bool FTX1Controller::getComp()        { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getComp()); }
bool FTX1Controller::getAlc()         { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getAlc()); }
bool FTX1Controller::getPowerOutput() { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getPowerOutput()); }
bool FTX1Controller::getSwr()         { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getSwr()); }
bool FTX1Controller::getIdd()         { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getIdd()); }
bool FTX1Controller::getVdd()         { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getVdd()); }
bool FTX1Controller::getMeterSw(int m){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getMeterSw()); }
bool FTX1Controller::setMeterSw(int m){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setMeterSw(m)); }

// ============================================================================
//  Auto information
// ============================================================================
bool FTX1Controller::setAutoInformation(int m){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setAutoInformation(m)); }
bool FTX1Controller::getAutoInformation()     { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getAutoInformation()); }

// ============================================================================
//  Misc
// ============================================================================
bool FTX1Controller::setLcdDimmer(int t, int l){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setLcdDimmer(t,l)); }
bool FTX1Controller::getLcdDimmer(int t)       { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getLcdDimmer(t)); }
bool FTX1Controller::setGpOut(int p, int l)    { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setGpOut(p,l)); }
bool FTX1Controller::getGpOut(int p)           { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getGpOut(p)); }
bool FTX1Controller::setDateTime(const std::string& dt){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setDateTime(dt)); }
bool FTX1Controller::getDateTime()             { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getDateTime()); }
bool FTX1Controller::getFirmwareVersion()      { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getFirmwareVersion()); }
bool FTX1Controller::getRadioId()              { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getRadioId()); }
bool FTX1Controller::getInformation()          { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getInformation()); }
bool FTX1Controller::setMicUp()               { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setMicUp()); }
bool FTX1Controller::setDownKey()             { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setDownKey()); }
bool FTX1Controller::setChannelUpDown(int d)  { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setChannelUpDown(d)); }
bool FTX1Controller::setRpttSelect(int s)     { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setRpttSelect(s)); }
bool FTX1Controller::getRpttSelect()          { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getRpttSelect()); }
bool FTX1Controller::setDialStep(int s)       { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setDialStep(s)); }
bool FTX1Controller::getDialStep()            { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getDialStep()); }
bool FTX1Controller::setTxGnrlMaxPowerBattery(int s){ std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::setTxGnrlMaxPowerBattery(s)); }
bool FTX1Controller::getTxGnrlMaxPowerBattery()    { std::lock_guard<std::mutex> lk(mx_); return send(CATCommand::getTxGnrlMaxPowerBattery()); }
