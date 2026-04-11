/**
 * FTX1Controller.h
 * ================
 * Thread-safe controller for the Yaesu FTX-1 transceiver.
 * Wraps serial communication, state management, and the full
 * CATCommand API into a clean C++ interface.
 *
 * Depends on: CATCommand.h, Windows serial (Win32 API)
 */

#pragma once

#include "httplib.h"
#include "json.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <string>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include <vector>
#include "CATCommand.h"

using json = nlohmann::json;

// ============================================================================
//  Band table
// ============================================================================
struct BandInfo { long long defaultHz; };
static const std::map<std::string, BandInfo> g_bandMap {
    {"160m",{ 1900000LL}}, {"80m", { 3700000LL}}, {"60m", { 5357000LL}},
    {"40m", { 7100000LL}}, {"30m", {10125000LL}}, {"20m", {14250000LL}},
    {"17m", {18100000LL}}, {"15m", {21200000LL}}, {"12m", {24900000LL}},
    {"10m", {28500000LL}}, {"6m",  {50125000LL}},
    {"2m",  {144200000LL}},{"70cm",{432100000LL}}
};

// ============================================================================
//  Rig State — mirrors all controllable/readable radio parameters
// ============================================================================
struct RigState {
    // VFO / frequency
    long long   freqMain    = 14250000LL;
    long long   freqSub     = 7100000LL;
    std::string modeMain    = "USB";
    std::string modeSub     = "USB";
    std::string bandMain    = "20m";
    std::string bandSub     = "40m";

    // TX
    bool tx         = false;
    bool split      = false;
    int  powerWatts = 100;

    // VFO ops
    bool rit        = false;
    int  ritOffset  = 0;      // Hz
    bool xit        = false;
    bool fineTuning = false;
    int  vfoSelect  = 0;      // 0=VFO-A, 1=VFO-B

    // Gains
    int afGainMain  = 128;
    int afGainSub   = 128;
    int rfGain      = 255;
    int squelchMain = 0;
    int squelchSub  = 0;
    int micGain     = 50;
    int monitorLevel= 0;

    // RF path — MAIN
    int  preampMain = 0;      // 0=IPO, 1=AMP1, 2=AMP2
    bool attMain    = false;
    int  agcMain    = 3;      // 0=OFF 1=FAST 2=MID 3=SLOW 4=AUTO
    bool nbMain     = false;
    bool nrMain     = false;
    bool dnfMain    = false;
    bool manNotchMain = false;
    int  manNotchFreqMain = 1000;
    bool lockMain   = false;

    // RF path — SUB
    int  preampSub  = 0;
    bool attSub     = false;
    int  agcSub     = 3;
    bool nbSub      = false;
    bool nrSub      = false;
    bool dnfSub     = false;
    bool manNotchSub = false;
    int  manNotchFreqSub = 1000;
    bool lockSub    = false;

    // IF filter
    int  ifShiftMain   = 0;
    int  ifWidthMain   = 2400;
    int  contourLvlMain= 0;
    int  contourWidMain= 5;
    int  ifShiftSub    = 0;
    int  ifWidthSub    = 2400;
    int  contourLvlSub = 0;
    int  contourWidSub = 5;

    // Audio filter (LCUT/HCUT indices as per EX menu)
    int  lcutFreqMain  = 0;   // 0=OFF
    int  lcutSlopeMain = 0;   // 0=6dB/oct, 1=18dB/oct
    int  hcutFreqMain  = 60;
    int  hcutSlopeMain = 0;
    int  lcutFreqSub   = 0;
    int  lcutSlopeSub  = 0;
    int  hcutFreqSub   = 60;
    int  hcutSlopeSub  = 0;

    // DSP levels
    int  nbLevel    = 5;
    int  nbWidth    = 5;
    int  nrLevel    = 8;

    // TX audio
    bool speechProc = false;
    int  amcOutput  = 50;
    int  voxGain    = 50;
    int  voxDelay   = 5;
    bool voxOn      = false;

    // Tuner
    int  tunerState = 0;      // 0=off, 1=on, 2=tuning

    // Scan
    int  scanMode   = 0;

    // Meters (raw 0-255)
    int  smeterMain = 0;
    int  smeterSub  = 0;
    int  comp       = 0;
    int  alc        = 0;
    int  po         = 0;
    int  swr        = 0;
    int  idd        = 0;
    int  vdd        = 0;

    // Connection
    bool connected  = false;
};

// ============================================================================
//  FTX1Controller
// ============================================================================
class FTX1Controller {
public:
    FTX1Controller();
    ~FTX1Controller();

    // ── Connection ──────────────────────────────────────────────────────
    bool        connect(const std::string& port, int baud = 38400);
    void        disconnect();
    bool        isConnected() const;

    // ── Status ──────────────────────────────────────────────────────────
    json        getStatus() const;
    RigState    getState()  const;

    // Register callback invoked every poll cycle with updated state JSON
    void        onStatusUpdate(std::function<void(json)> cb);

    // ── Frequency ───────────────────────────────────────────────────────
    bool setMainSideFrequency(long long hz);
    bool setSubSideFrequency(long long hz);
    bool getMainSideFrequency();   // sends query; answer arrives via AI/poll
    bool getSubSideFrequency();

    // ── Band (sets frequency to band default) ───────────────────────────
    bool setMainBand(const std::string& band);
    bool setSubBand(const std::string& band);

    // ── Mode ────────────────────────────────────────────────────────────
    bool setMainOperatingMode(const std::string& modeName);
    bool setSubOperatingMode(const std::string& modeName);
    bool getOperatingMode(int vfo = 0);

    // ── TX / PTT ────────────────────────────────────────────────────────
    bool setTx(bool transmit);
    bool getTx();

    // ── Split ───────────────────────────────────────────────────────────
    bool setSplit(bool on);
    bool getSplit();

    // ── VFO operations ──────────────────────────────────────────────────
    bool setSwapVfo();
    bool setMainSideToSubSide();   // AB
    bool setSubSideToMainSide();   // BA
    bool setMainSideToMemoryChannel();
    bool setSubSideToMemoryChannel();
    bool setMemoryChannelToMainSide();
    bool setMemoryChannelToSubSide();
    bool setVfoSelect(int vfo);
    bool getVfoSelect();
    bool setFunctionTx(int vfo);
    bool getFunctionTx();
    bool setFunctionRx(int mode);
    bool getFunctionRx();
    bool setVfoOrMemoryChannel(int vfo, int mode);
    bool getVfoOrMemoryChannel(int vfo = 0);
    bool setBandUp();
    bool setBandDown();

    // ── RIT / XIT ───────────────────────────────────────────────────────
    bool setRit(bool on);
    bool setRitOffset(int hz);   // clears then steps up/down in 10Hz increments
    bool getRit();
    bool setXit(bool on);
    bool clearRitXit();

    // ── Fine tuning ─────────────────────────────────────────────────────
    bool setFineTuning(int vfo, bool on);
    bool getFineTuning(int vfo = 0);

    // ── AF Gain ─────────────────────────────────────────────────────────
    bool setAfGainMain(int level);
    bool setAfGainSub(int level);
    bool getAfGain(int vfo = 0);

    // ── RF Gain ─────────────────────────────────────────────────────────
    bool setRfGain(int level);
    bool getRfGain();

    // ── Squelch ─────────────────────────────────────────────────────────
    bool setSquelchMain(int level);
    bool setSquelchSub(int level);
    bool getSquelchLevel(int vfo = 0);

    // ── Mic Gain ────────────────────────────────────────────────────────
    bool setMicGain(int level);
    bool getMicGain();

    // ── Monitor Level ───────────────────────────────────────────────────
    bool setMonitorLevel(int level);
    bool getMonitorLevel();

    // ── Power ───────────────────────────────────────────────────────────
    bool setPowerControl(int watts);
    bool getPowerControl();

    // ── Preamp / IPO ────────────────────────────────────────────────────
    bool setPreampMain(int level);   // 0=IPO, 1=AMP1, 2=AMP2
    bool setPreampSub(int level);
    bool getPreamp(int band = 0);

    // ── Attenuator ──────────────────────────────────────────────────────
    bool setAttenuatorMain(bool on, int level = 0);
    bool setAttenuatorSub(bool on, int level = 0);
    bool getAttenuator();

    // ── AGC ─────────────────────────────────────────────────────────────
    bool setAgcMain(int mode);  // 0=OFF 1=FAST 2=MID 3=SLOW 4=AUTO
    bool setAgcSub(int mode);
    bool getAgcFunction(int vfo = 0);

    // ── Noise Blanker ───────────────────────────────────────────────────
    bool setNoiseBlankerMain(bool on);
    bool setNoiseBlankerSub(bool on);
    bool getNoiseBlanker(int vfo = 0);
    bool setNbLevel(int level);        // EX menu
    bool setNbWidth(int width);        // EX menu
    bool getNbLevel();
    bool getNbWidth();

    // ── Noise Reduction ─────────────────────────────────────────────────
    bool setNoiseReductionMain(bool on);
    bool setNoiseReductionSub(bool on);
    bool getNoiseReduction(int vfo = 0);
    bool setNrLevel(int level);        // EX menu
    bool getNrLevel();

    // ── Auto Notch (DNF) ────────────────────────────────────────────────
    bool setAutoNotchMain(bool on);
    bool setAutoNotchSub(bool on);
    bool getAutoNotch(int vfo = 0);

    // ── Manual Notch ────────────────────────────────────────────────────
    bool setManualNotchMain(bool on, int freqHz = 1000);
    bool setManualNotchSub(bool on, int freqHz = 1000);
    bool getManualNotch();

    // ── Lock ────────────────────────────────────────────────────────────
    bool setLockMain(bool on);
    bool getLock();

    // ── IF Shift ────────────────────────────────────────────────────────
    bool setIfShiftMain(int offsetHz);
    bool setIfShiftSub(int offsetHz);
    bool getIfShift(int vfo = 0);

    // ── Width ───────────────────────────────────────────────────────────
    bool setWidthMain(int hz);
    bool setWidthSub(int hz);
    bool getWidth(int vfo = 0);

    // ── Contour ─────────────────────────────────────────────────────────
    bool setContour(int levelDb, int width = 5);
    bool getContour();
    bool setContourLevel(int db);   // via EX menu
    bool setContourWidth(int q);    // via EX menu
    bool getContourLevel();
    bool getContourWidth();

    // ── IF Notch Width ──────────────────────────────────────────────────
    bool setIfNotchWidth(int w);    // 0=NARROW 1=WIDE
    bool getIfNotchWidth();

    // ── APF Width ───────────────────────────────────────────────────────
    bool setApfWidth(int w);        // 0=NARROW 1=MEDIUM 2=WIDE
    bool getApfWidth();

    // ── LCUT / HCUT (audio filter) ──────────────────────────────────────
    // These apply to the currently active mode's RX audio chain
    bool setLcutFreq(int idx);      // 0=OFF 1-20=100-1050Hz
    bool setLcutSlope(int s);       // 0=6dB/oct 1=18dB/oct
    bool setHcutFreq(int idx);      // 0=OFF 1-67=700-4000Hz
    bool setHcutSlope(int s);
    bool getLcutFreq();
    bool getLcutSlope();
    bool getHcutFreq();
    bool getHcutSlope();
    // Mode-specific variants (SSB/CW/AM/FM/DATA/RTTY)
    bool setLcutFreqForMode(const std::string& mode, int idx);
    bool setLcutSlopeForMode(const std::string& mode, int s);
    bool setHcutFreqForMode(const std::string& mode, int idx);
    bool setHcutSlopeForMode(const std::string& mode, int s);

    // ── AGC delays (per mode) ───────────────────────────────────────────
    bool setAgcFastDelay(int ms);
    bool setAgcMidDelay(int ms);
    bool setAgcSlowDelay(int ms);
    bool getAgcFastDelay();
    bool getAgcMidDelay();
    bool getAgcSlowDelay();

    // ── AF tone controls ────────────────────────────────────────────────
    bool setAfTrebleGain(int db);
    bool setAfMiddleToneGain(int db);
    bool setAfBassGain(int db);
    bool getAfTrebleGain();
    bool getAfMiddleToneGain();
    bool getAfBassGain();

    // ── TX audio ────────────────────────────────────────────────────────
    bool setSpeechProcessor(bool on);
    bool getSpeechProcessor();
    bool setAmcOutputLevel(int level);
    bool getAmcOutputLevel();
    bool setModSource(int src);     // 0=MIC 1=REAR 2=USB
    bool getModSource();
    bool setUsbModGain(int gain);
    bool getUsbModGain();
    bool setUsbOutLevel(int level);
    bool getUsbOutLevel();
    bool setTxBpfSel(int sel);      // 0=100-3000 .. 4=400-2600
    bool getTxBpfSel();

    // ── Parametric EQ — processor OFF (3 bands) ─────────────────────────
    bool setPrmtrcEq1Freq(int idx);
    bool setPrmtrcEq1Level(int db);
    bool setPrmtrcEq1Bwth(int bw);
    bool setPrmtrcEq2Freq(int idx);
    bool setPrmtrcEq2Level(int db);
    bool setPrmtrcEq2Bwth(int bw);
    bool setPrmtrcEq3Freq(int idx);
    bool setPrmtrcEq3Level(int db);
    bool setPrmtrcEq3Bwth(int bw);
    bool getPrmtrcEq1Freq();
    bool getPrmtrcEq1Level();
    bool getPrmtrcEq1Bwth();
    bool getPrmtrcEq2Freq();
    bool getPrmtrcEq2Level();
    bool getPrmtrcEq2Bwth();
    bool getPrmtrcEq3Freq();
    bool getPrmtrcEq3Level();
    bool getPrmtrcEq3Bwth();

    // ── Parametric EQ — processor ON (3 bands) ──────────────────────────
    bool setPPrmtrcEq1Freq(int idx);
    bool setPPrmtrcEq1Level(int db);
    bool setPPrmtrcEq1Bwth(int bw);
    bool setPPrmtrcEq2Freq(int idx);
    bool setPPrmtrcEq2Level(int db);
    bool setPPrmtrcEq2Bwth(int bw);
    bool setPPrmtrcEq3Freq(int idx);
    bool setPPrmtrcEq3Level(int db);
    bool setPPrmtrcEq3Bwth(int bw);
    bool getPPrmtrcEq1Freq();
    bool getPPrmtrcEq1Level();
    bool getPPrmtrcEq1Bwth();
    bool getPPrmtrcEq2Freq();
    bool getPPrmtrcEq2Level();
    bool getPPrmtrcEq2Bwth();
    bool getPPrmtrcEq3Freq();
    bool getPPrmtrcEq3Level();
    bool getPPrmtrcEq3Bwth();

    // ── VOX ─────────────────────────────────────────────────────────────
    bool setVoxStatus(bool on);
    bool getVoxStatus();
    bool setVoxGain(int gain);
    bool getVoxGain();
    bool setVoxDelayTime(int code);
    bool getVoxDelayTime();
    bool setVoxSelect(int sel);     // 0=MIC 1=DATA
    bool getVoxSelect();

    // ── Antenna tuner ───────────────────────────────────────────────────
    bool setAntennaTunerStart();
    bool setAntennaTunerStop();
    bool getAntennaTunerControl();

    // ── Scan ────────────────────────────────────────────────────────────
    bool setScan(int mode);         // 0=stop 1=UP 2=DOWN 3=MEM 4=MODE 5=GRP
    bool getScan();

    // ── Memory ──────────────────────────────────────────────────────────
    bool setMemoryChannel(int vfo, int ch);
    bool getMemoryChannel(int vfo = 0);
    bool getMemoryChannelRead(int vfo, int ch);

    // ── CW ──────────────────────────────────────────────────────────────
    bool setKeySpeed(int wpm);
    bool getKeySpeed();
    bool setKeyPitch(int hz);
    bool getKeyPitch();
    bool setKeyer(int mode);
    bool getKeyer();
    bool setCwBreakInDelay(int ms);
    bool getCwBreakInDelay();
    bool setBreakIn(int mode);
    bool getBreakIn();
    bool setCwSpot(bool on);
    bool getCwSpot();
    bool setZeroIn();
    bool setCwAutoMode(int mode);
    bool getCwAutoMode();
    bool setKeyerMemory(int ch, const std::string& text);
    bool getKeyerMemory(int ch);
    bool setCwKeyingMemoryPlay(int ch);
    bool setCwKeyingText(const std::string& text);

    // ── CTCSS / DCS ─────────────────────────────────────────────────────
    bool setCtcss(int vfo, int mode);
    bool getCtcss(int vfo = 0);
    bool setCtcssNumber(int vfo, int toneNum);
    bool getCtcssNumber(int vfo = 0);

    // ── Clarifier ───────────────────────────────────────────────────────
    bool setClarifier(int vfo, bool on, int offsetHz = 0);
    bool getClarifier(int vfo = 0);

    // ── FM repeater ─────────────────────────────────────────────────────
    bool setFmRptShift(int dir);            // 0=simplex 1=minus 2=plus
    bool getFmRptShift();
    bool setFmRptShiftFreq144(int hz);
    bool setFmRptShiftFreq430(int hz);
    bool getFmRptShiftFreq144();
    bool getFmRptShiftFreq430();

    // ── Spectrum scope ──────────────────────────────────────────────────
    bool setSpectrumScope(int mode);
    bool getSpectrumScope();

    // ── Meters ──────────────────────────────────────────────────────────
    bool readAllMeters();
    bool getSmeterMain();
    bool getSmeterSub();
    bool getComp();
    bool getAlc();
    bool getPowerOutput();
    bool getSwr();
    bool getIdd();
    bool getVdd();
    bool getMeterSw(int meter);
    bool setMeterSw(int meter);

    // ── Auto information ────────────────────────────────────────────────
    bool setAutoInformation(int mode);  // 0=OFF 1=AI1 2=AI2(full)
    bool getAutoInformation();

    // ── Misc ────────────────────────────────────────────────────────────
    bool setLcdDimmer(int type, int level);
    bool getLcdDimmer(int type = 1);
    bool setGpOut(int port, int level);
    bool getGpOut(int port = 0);
    bool setDateTime(const std::string& dt14);
    bool getDateTime();
    bool getFirmwareVersion();
    bool getRadioId();
    bool getInformation();
    bool setMicUp();
    bool setDownKey();
    bool setChannelUpDown(int dir);
    bool setRpttSelect(int sel);
    bool getRpttSelect();
    bool setDialStep(int step);
    bool getDialStep();
    bool setTxGnrlMaxPowerBattery(int step);
    bool getTxGnrlMaxPowerBattery();

    // ── Raw CAT passthrough ─────────────────────────────────────────────
    bool        sendRaw(const std::string& cmd);
    std::string getLastRawResponse() const;

private:
    // ── Serial port ─────────────────────────────────────────────────────
    bool        serialOpen(const std::string& port, int baud);
    void        serialClose();
    bool        serialWrite(const std::string& cmd);
    std::string serialReadResponse(int timeoutMs = 400);
    bool        serialIsOpen() const;

    // ── Internal helpers ────────────────────────────────────────────────
    bool        send(const std::string& cmd);
    bool        sendAndForget(const std::string& cmd); // fire & forget, no state update
    static int  modeNameToIndex(const std::string& name);
    static int  hzToWidthIndex(int hz);     // 50-4000Hz → 0-79 index for SH command

    // ── Packet parser (AI mode push responses) ──────────────────────────
    void        parsePacket(const std::string& pkt);
    void        parseRM(const std::string& pkt);

    // ── Poll loop ───────────────────────────────────────────────────────
    void        pollLoop();

    // ── Members ─────────────────────────────────────────────────────────
    HANDLE              hSerial_    = INVALID_HANDLE_VALUE;
    bool                serialOpen_ = false;
    RigState            state_;
    mutable std::mutex  mx_;
    std::atomic<bool>   running_    {false};
    std::thread         pollThread_;
    std::string         lastRaw_;
    std::function<void(json)> statusCb_;

    // meter IDs: 1=S-MAIN 2=S-SUB 3=COMP 4=ALC 5=PO 6=SWR 7=IDD 8=VDD
    static constexpr int METER_COUNT = 8;
};
