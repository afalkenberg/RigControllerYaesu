/**
 * CATCommand.h
 * ============
 * CAT command builder for the Yaesu FTX-1 series transceiver.
 * All methods are static — no instance needed.
 *
 * See CATCommand.cpp for full implementation and parameter docs.
 */

#pragma once
#include <string>
#include <cmath>

// MSVC-safe clamp (no dependency on std::clamp / <algorithm>)
template<typename T>
static inline T cat_clamp(T v, T lo, T hi) { return v<lo?lo:(v>hi?hi:v); }

class CATCommand {
public:

    // ====================================================================
    //  SECTION 1 — CORE COMMANDS (pages 5–8)
    // ====================================================================

    // AB — Main-side to Sub-side copy
    static std::string setMainSideToSubSide();

    // AC — Antenna Tuner Control  p1: 0=off 1=on 2=TX+tuning
    static std::string setAntennaTunerControl(int p1);
    static std::string getAntennaTunerControl();

    // AG — AF Gain  vfo: 0=MAIN 1=SUB  level: 0-255
    static std::string setAfGain(int vfo, int level);
    static std::string getAfGain(int vfo = 0);

    // AI — Auto Information  mode: 0=OFF 1=AI1 2=AI2(full)
    static std::string setAutoInformation(int mode);
    static std::string getAutoInformation();

    // AM — Main-side to Memory Channel copy
    static std::string setMainSideToMemoryChannel();

    // AO — AMC Output Level  level: 0-100
    static std::string setAmcOutputLevel(int level);
    static std::string getAmcOutputLevel();

    // BA — Sub-side to Main-side copy
    static std::string setSubSideToMainSide();

    // BC — Auto Notch (DNF)  vfo: 0=MAIN 1=SUB
    static std::string setAutoNotch(int vfo, bool on);
    static std::string getAutoNotch(int vfo = 0);

    // BD — Band Down  vfo: 0=MAIN 1=SUB
    static std::string setBandDown(int vfo = 0);

    // BI — Break-In  mode: 0=OFF 1=SEMI 2=FULL
    static std::string setBreakIn(int mode);
    static std::string getBreakIn();

    // BM — Sub-side to Memory Channel copy
    static std::string setSubSideToMemoryChannel();

    // BP — Manual Notch  freq: 0-4000 Hz
    static std::string setManualNotch(bool on, int freqHz = 1000);
    static std::string getManualNotch();

    // BS — Band Select  band: 0=160m .. 12=70cm
    static std::string setBandSelect(int band);

    // BU — Band Up  vfo: 0=MAIN 1=SUB
    static std::string setBandUp(int vfo = 0);

    // CF — Clarifier  vfo: 0=MAIN 1=SUB  offsetHz: -9999..+9999
    static std::string setClarifier(int vfo, bool on, int offsetHz = 0);
    static std::string getClarifier(int vfo = 0);

    // CH — Channel Up/Down  dir: 0=DOWN 1=UP
    static std::string setChannelUpDown(int dir);

    // CN — CTCSS Number  toneNum: 0-49
    static std::string setCtcssNumber(int vfo, int toneNum);
    static std::string getCtcssNumber(int vfo = 0);

    // CO — Contour/APF  levelDb: -40..+20  width: 1-11
    static std::string setContour(int levelDb, int width = 5);
    static std::string getContour();

    // CS — CW Spot
    static std::string setCwSpot(bool on);
    static std::string getCwSpot();

    // CT — CTCSS  mode: 0=OFF 1=ENC 2=ENC+DEC
    static std::string setCtcss(int vfo, int mode);
    static std::string getCtcss(int vfo = 0);

    // DA — LCD Contrast/Dimmer  type: 0=contrast 1=dimmer  level: 0-100
    static std::string setLcdDimmer(int type, int level);
    static std::string getLcdDimmer(int type = 1);

    // DN — Down Key
    static std::string setDownKey();

    // DT — Date and Time  dt14: YYYYMMDDHHmmSS
    static std::string setDateTime(const std::string& dt14);
    static std::string getDateTime();

    // EX — Menu (raw builder used by named wrappers)
    static std::string setMenuRaw(int menuNum, const std::string& value);
    static std::string getMenuRaw(int menuNum);

    // FA — Frequency Main-side  freqHz: 1800000..470000000
    static std::string setMainSideFrequency(long long freqHz);
    static std::string getMainSideFrequency();

    // FB — Frequency Sub-side
    static std::string setSubSideFrequency(long long freqHz);
    static std::string getSubSideFrequency();

    // FN — Fine Tuning  (toggles MAIN VFO fine step — no VFO selector)
    static std::string setFineTuning(bool on);
    static std::string getFineTuning();

    // FR — Function RX  mode: 0=single 1=dual
    static std::string setFunctionRx(int mode);
    static std::string getFunctionRx();

    // FT — Function TX  vfo: 0=MAIN 1=SUB
    static std::string setFunctionTx(int vfo);
    static std::string getFunctionTx();

    // GP — GP Out  port: 0-3  level: 0=LOW 1=HIGH
    static std::string setGpOut(int port, int level);
    static std::string getGpOut(int port = 0);

    // GT — AGC Function  vfo: 0=MAIN 1=SUB  mode: 0=OFF 1=FAST 2=MID 3=SLOW 4=AUTO
    static std::string setAgcFunction(int vfo, int mode);
    static std::string getAgcFunction(int vfo = 0);

    // ID — Radio ID
    static std::string getRadioId();

    // IF — Information (Main-side)
    static std::string getInformation();

    // IS — IF Shift  vfo: 0=MAIN 1=SUB  offsetHz: -1000..+1000
    static std::string setIfShift(int vfo, int offsetHz);
    static std::string getIfShift(int vfo = 0);

    // KM — Keyer Memory  ch: 1-5
    static std::string setKeyerMemory(int ch, const std::string& text);
    static std::string getKeyerMemory(int ch);

    // KP — Key Pitch  pitchHz: 300-1050
    static std::string setKeyPitch(int pitchHz);
    static std::string getKeyPitch();

    // KR — Keyer  mode: 0=OFF 1=MODE-A 2=MODE-B
    static std::string setKeyer(int mode);
    static std::string getKeyer();

    // KS — Key Speed  wpm: 4-60
    static std::string setKeySpeed(int wpm);
    static std::string getKeySpeed();

    // KY — CW Keying Memory Play  ch: 0=stop 1-5=play
    static std::string setCwKeyingMemoryPlay(int ch);
    static std::string setCwKeyingText(const std::string& text);

    // LK — Lock  (locks MAIN dial)
    static std::string setLock(bool on);
    static std::string getLock();

    // LM — Load Message  type: 0=RX 1=TX  ch: 1-5
    static std::string setLoadMessage(int type, int ch);

    // MA — Memory Channel to Main-side
    static std::string setMemoryChannelToMainSide();

    // MB — Memory Channel to Sub-side
    static std::string setMemoryChannelToSubSide();

    // MC — Memory Channel  vfo: 0=MAIN 1=SUB  ch: 0-117
    static std::string setMemoryChannel(int vfo, int ch);
    static std::string getMemoryChannel(int vfo = 0);

    // MD — Operating Mode  vfo: 0=MAIN 1=SUB
    // mode codes: 1=LSB 2=USB 3=CW-U 4=FM 5=AM 6=RTTY-L 7=CW-L
    //             8=DATA-L 9=RTTY-U A=DATA-FM B=FM-N C=DATA-U
    //             D=AM-N E=PSK F=DATA-FM-N H=C4FM-DN I=C4FM-VW
    static std::string setOperatingMode(int vfo, const std::string& modeCode);
    static std::string getOperatingMode(int vfo = 0);

    // Convenience: set by human-readable name
    static std::string setOperatingModeByName(int vfo, const std::string& name);

    // Map mode name → CAT code character  e.g. "USB" → "2"
    static std::string modeNameToCode(const std::string& name);
    // Map CAT code → mode name  e.g. "2" → "USB"
    static std::string modeCodeToName(const std::string& code);

    // MG — Mic Gain  level: 0-100
    static std::string setMicGain(int level);
    static std::string getMicGain();

    // ML — Monitor Level  level: 0-100
    static std::string setMonitorLevel(int level);
    static std::string getMonitorLevel();

    // MR — Memory Channel Read  vfo: 0=MAIN 1=SUB  ch: 0-117
    static std::string getMemoryChannelRead(int vfo, int ch);

    // MS — Meter SW  meter: 1-8
    static std::string setMeterSw(int meter);
    static std::string getMeterSw();

    // NB — Noise Blanker  vfo: 0=MAIN 1=SUB
    static std::string setNoiseBlanker(int vfo, bool on);
    static std::string getNoiseBlanker(int vfo = 0);

    // NR — Noise Reduction  vfo: 0=MAIN 1=SUB
    static std::string setNoiseReduction(int vfo, bool on);
    static std::string getNoiseReduction(int vfo = 0);

    // PA — Preamp/IPO  band: 0=HF/50 1=VHF 2=UHF  level: 0=IPO 1=AMP1 2=AMP2
    static std::string setPreamp(int band, int level);
    static std::string getPreamp(int band = 0);

    // PC — Power Control  watts: 0-100
    static std::string setPowerControl(int watts);
    static std::string getPowerControl();

    // PR — Speech Processor
    static std::string setSpeechProcessor(bool on);
    static std::string getSpeechProcessor();

    // RA — Attenuator  level: 0=6dB 1=12dB 2=18dB (ignored when on=false)
    static std::string setAttenuator(bool on, int level = 0);
    static std::string getAttenuator();

    // RC — RIT/XIT Clear
    static std::string setClarClear();
    static std::string clearRitXit();

    // RD — RIT/XIT Down  steps: 10-Hz steps
    static std::string setRitDown(int steps = 1);

    // RM — Read Meter  meter: 1=S-MAIN 2=S-SUB 3=COMP 4=ALC 5=PO 6=SWR 7=IDD 8=VDD
    static std::string getReadMeter(int meter);
    static std::string getSmeterMain();
    static std::string getSmeterSub();
    static std::string getComp();
    static std::string getAlc();
    static std::string getPowerOutput();
    static std::string getSwr();
    static std::string getIdd();
    static std::string getVdd();

    // RT — RIT
    static std::string setRit(bool on);
    static std::string getRit();

    // RU — RIT/XIT Up  steps: 10-Hz steps
    static std::string setRitUp(int steps = 1);

    // RG — RF Gain  level: 0-255
    static std::string setRfGain(int level);
    static std::string getRfGain();

    // SC — Scan  mode: 0=stop 1=UP 2=DOWN 3=MEM 4=MODE 5=GRP
    static std::string setScan(int mode);
    static std::string getScan();

    // SD — CW Break-In Delay  ms: 30-3000
    static std::string setCwBreakInDelay(int ms);
    static std::string getCwBreakInDelay();

    // SF — Func-Knob Function
    static std::string setFuncKnobFunction(int func);
    static std::string getFuncKnobFunction();

    // SH — Width  vfo: 0=MAIN 1=SUB  widthIdx: 0-99
    static std::string setWidth(int vfo, int widthIdx);
    static std::string getWidth(int vfo = 0);

    // SM — S-Meter Reading  vfo: 0=MAIN 1=SUB
    static std::string getSmeterReading(int vfo = 0);

    // SP — Split
    static std::string setSplit(bool on);
    static std::string getSplit();

    // SQ — Squelch Level  vfo: 0=MAIN 1=SUB  level: 0-255
    static std::string setSquelchLevel(int vfo, int level);
    static std::string getSquelchLevel(int vfo = 0);

    // SS — Spectrum Scope  mode: 0=OFF 1=CENTER 2=FIXED 3=SCROLL-C 4=SCROLL-F
    static std::string setSpectrumScope(int mode);
    static std::string getSpectrumScope();

    // ST — Split (alias)
    static std::string setSplitSt(bool on);
    static std::string getSplitSt();

    // SV — Swap VFO
    static std::string setSwapVfo();

    // TS — TXW
    static std::string setTxw(bool on);
    static std::string getTxw();

    // TX — TX Set  mode: 0=RX 1=TX(CAT-1) 2=TX(CAT-2)
    static std::string setTx(int mode = 1);
    static std::string setRx();
    static std::string getTx();

    // UP — Mic Up
    static std::string setMicUp();

    // VD — VOX Delay  delayCode: 0-30
    static std::string setVoxDelayTime(int delayCode);
    static std::string getVoxDelayTime();

    // VE — Firmware Version
    static std::string getFirmwareVersion();

    // VG — VOX Gain  gain: 0-100
    static std::string setVoxGain(int gain);
    static std::string getVoxGain();

    // VM — VFO / Memory Channel  vfo: 0=MAIN 1=SUB  mode: 0=VFO 1=MEM
    static std::string setVfoOrMemoryChannel(int vfo, int mode);
    static std::string getVfoOrMemoryChannel(int vfo = 0);

    // VS — VFO Select  vfo: 0=VFO-A 1=VFO-B
    static std::string setVfoSelect(int vfo);
    static std::string getVfoSelect();

    // VX — VOX Status
    static std::string setVoxStatus(bool on);
    static std::string getVoxStatus();

    // ZI — Zero In
    static std::string setZeroIn();

    // ====================================================================
    //  SECTION 2 — EX EXTENDED MENU (pages 9–27)
    // ====================================================================

    // ── MODE SSB ──────────────────────────────────────────────────────────
    static std::string setAfTrebleGain(int db);       static std::string getAfTrebleGain();
    static std::string setAfMiddleToneGain(int db);   static std::string getAfMiddleToneGain();
    static std::string setAfBassGain(int db);         static std::string getAfBassGain();
    static std::string setAgcFastDelay(int ms);       static std::string getAgcFastDelay();
    static std::string setAgcMidDelay(int ms);        static std::string getAgcMidDelay();
    static std::string setAgcSlowDelay(int ms);       static std::string getAgcSlowDelay();
    static std::string setLcutFreq(int idx);          static std::string getLcutFreq();
    static std::string setLcutSlope(int slope);       static std::string getLcutSlope();
    static std::string setHcutFreq(int idx);          static std::string getHcutFreq();
    static std::string setHcutSlope(int slope);       static std::string getHcutSlope();
    static std::string setUsbOutLevel(int level);     static std::string getUsbOutLevel();
    static std::string setTxBpfSel(int sel);          static std::string getTxBpfSel();
    static std::string setModSource(int src);         static std::string getModSource();
    static std::string setUsbModGain(int gain);       static std::string getUsbModGain();
    static std::string setRpttSelect(int sel);        static std::string getRpttSelect();

    // ── MODE CW ───────────────────────────────────────────────────────────
    static std::string setCwAfTrebleGain(int db);     static std::string getCwAfTrebleGain();
    static std::string setCwAfMiddleToneGain(int db); static std::string getCwAfMiddleToneGain();
    static std::string setCwAfBassGain(int db);       static std::string getCwAfBassGain();
    static std::string setCwAgcFastDelay(int ms);     static std::string getCwAgcFastDelay();
    static std::string setCwAgcMidDelay(int ms);      static std::string getCwAgcMidDelay();
    static std::string setCwAgcSlowDelay(int ms);     static std::string getCwAgcSlowDelay();
    static std::string setCwLcutFreq(int idx);        static std::string getCwLcutFreq();
    static std::string setCwLcutSlope(int s);         static std::string getCwLcutSlope();
    static std::string setCwHcutFreq(int idx);        static std::string getCwHcutFreq();
    static std::string setCwHcutSlope(int s);         static std::string getCwHcutSlope();
    static std::string setCwOutLevel(int level);      static std::string getCwOutLevel();
    static std::string setCwAutoMode(int mode);       static std::string getCwAutoMode();
    static std::string setCwNarWidth(int w);          static std::string getCwNarWidth();
    static std::string setCwPcKeying(int sel);        static std::string getCwPcKeying();
    static std::string setCwIndicator(bool on);       static std::string getCwIndicator();

    // ── MODE AM ───────────────────────────────────────────────────────────
    static std::string setAmAfTrebleGain(int db);     static std::string getAmAfTrebleGain();
    static std::string setAmAfMiddleToneGain(int db); static std::string getAmAfMiddleToneGain();
    static std::string setAmAfBassGain(int db);       static std::string getAmAfBassGain();
    static std::string setAmAgcFastDelay(int ms);     static std::string getAmAgcFastDelay();
    static std::string setAmAgcMidDelay(int ms);      static std::string getAmAgcMidDelay();
    static std::string setAmAgcSlowDelay(int ms);     static std::string getAmAgcSlowDelay();
    static std::string setAmLcutFreq(int idx);        static std::string getAmLcutFreq();
    static std::string setAmLcutSlope(int s);         static std::string getAmLcutSlope();
    static std::string setAmHcutFreq(int idx);        static std::string getAmHcutFreq();
    static std::string setAmHcutSlope(int s);         static std::string getAmHcutSlope();
    static std::string setAmModSource(int src);       static std::string getAmModSource();
    static std::string setAmUsbModGain(int gain);     static std::string getAmUsbModGain();
    static std::string setAmRpttSelect(int sel);      static std::string getAmRpttSelect();

    // ── MODE FM ───────────────────────────────────────────────────────────
    static std::string setFmAfTrebleGain(int db);     static std::string getFmAfTrebleGain();
    static std::string setFmAfMiddleToneGain(int db); static std::string getFmAfMiddleToneGain();
    static std::string setFmAfBassGain(int db);       static std::string getFmAfBassGain();
    static std::string setFmAgcFastDelay(int ms);     static std::string getFmAgcFastDelay();
    static std::string setFmAgcMidDelay(int ms);      static std::string getFmAgcMidDelay();
    static std::string setFmAgcSlowDelay(int ms);     static std::string getFmAgcSlowDelay();
    static std::string setFmLcutFreq(int idx);        static std::string getFmLcutFreq();
    static std::string setFmLcutSlope(int s);         static std::string getFmLcutSlope();
    static std::string setFmHcutFreq(int idx);        static std::string getFmHcutFreq();
    static std::string setFmHcutSlope(int s);         static std::string getFmHcutSlope();
    static std::string setFmModSource(int src);       static std::string getFmModSource();
    static std::string setFmUsbModGain(int gain);     static std::string getFmUsbModGain();
    static std::string setFmRpttSelect(int sel);      static std::string getFmRpttSelect();
    static std::string setFmRptShift(int dir);        static std::string getFmRptShift();
    static std::string setFmRptShiftFreq144(int hz);  static std::string getFmRptShiftFreq144();
    static std::string setFmRptShiftFreq430(int hz);  static std::string getFmRptShiftFreq430();

    // ── MODE DATA ─────────────────────────────────────────────────────────
    static std::string setDataAfTrebleGain(int db);     static std::string getDataAfTrebleGain();
    static std::string setDataAfMiddleToneGain(int db); static std::string getDataAfMiddleToneGain();
    static std::string setDataAfBassGain(int db);       static std::string getDataAfBassGain();
    static std::string setDataAgcFastDelay(int ms);     static std::string getDataAgcFastDelay();
    static std::string setDataAgcMidDelay(int ms);      static std::string getDataAgcMidDelay();
    static std::string setDataAgcSlowDelay(int ms);     static std::string getDataAgcSlowDelay();
    static std::string setDataLcutFreq(int idx);        static std::string getDataLcutFreq();
    static std::string setDataLcutSlope(int s);         static std::string getDataLcutSlope();
    static std::string setDataHcutFreq(int idx);        static std::string getDataHcutFreq();
    static std::string setDataHcutSlope(int s);         static std::string getDataHcutSlope();
    static std::string setDataUsbOutLevel(int level);   static std::string getDataUsbOutLevel();
    static std::string setDataTxBpfSel(int sel);        static std::string getDataTxBpfSel();
    static std::string setDataModSource(int src);       static std::string getDataModSource();
    static std::string setDataUsbModGain(int gain);     static std::string getDataUsbModGain();
    static std::string setDataRpttSelect(int sel);      static std::string getDataRpttSelect();
    static std::string setDataNarWidth(int w);          static std::string getDataNarWidth();
    static std::string setDataPskTone(int tone);        static std::string getDataPskTone();
    static std::string setDataShiftSsb(int hz);         static std::string getDataShiftSsb();

    // ── MODE RTTY ─────────────────────────────────────────────────────────
    static std::string setRttyAfTrebleGain(int db);     static std::string getRttyAfTrebleGain();
    static std::string setRttyAfMiddleToneGain(int db); static std::string getRttyAfMiddleToneGain();
    static std::string setRttyAfBassGain(int db);       static std::string getRttyAfBassGain();
    static std::string setRttyAgcFastDelay(int ms);     static std::string getRttyAgcFastDelay();
    static std::string setRttyAgcMidDelay(int ms);      static std::string getRttyAgcMidDelay();
    static std::string setRttyAgcSlowDelay(int ms);     static std::string getRttyAgcSlowDelay();
    static std::string setRttyLcutFreq(int idx);        static std::string getRttyLcutFreq();
    static std::string setRttyLcutSlope(int s);         static std::string getRttyLcutSlope();
    static std::string setRttyHcutFreq(int idx);        static std::string getRttyHcutFreq();
    static std::string setRttyHcutSlope(int s);         static std::string getRttyHcutSlope();
    static std::string setRttyRpttSelect(int sel);      static std::string getRttyRpttSelect();
    static std::string setRttyMarkFrequency(int f);     static std::string getRttyMarkFrequency();
    static std::string setRttyShiftFrequency(int idx);  static std::string getRttyShiftFrequency();
    static std::string setRttyPolarityTx(int pol);      static std::string getRttyPolarityTx();
    static std::string setRttyPolarityRx(int pol);      static std::string getRttyPolarityRx();

    // ── TX PARAMETRIC EQ — PROCESSOR OFF ─────────────────────────────────
    static std::string setPrmtrcEq1Freq(int idx);   static std::string getPrmtrcEq1Freq();
    static std::string setPrmtrcEq1Level(int db);   static std::string getPrmtrcEq1Level();
    static std::string setPrmtrcEq1Bwth(int bw);    static std::string getPrmtrcEq1Bwth();
    static std::string setPrmtrcEq2Freq(int idx);   static std::string getPrmtrcEq2Freq();
    static std::string setPrmtrcEq2Level(int db);   static std::string getPrmtrcEq2Level();
    static std::string setPrmtrcEq2Bwth(int bw);    static std::string getPrmtrcEq2Bwth();
    static std::string setPrmtrcEq3Freq(int idx);   static std::string getPrmtrcEq3Freq();
    static std::string setPrmtrcEq3Level(int db);   static std::string getPrmtrcEq3Level();
    static std::string setPrmtrcEq3Bwth(int bw);    static std::string getPrmtrcEq3Bwth();

    // ── TX PARAMETRIC EQ — PROCESSOR ON ──────────────────────────────────
    static std::string setPPrmtrcEq1Freq(int idx);  static std::string getPPrmtrcEq1Freq();
    static std::string setPPrmtrcEq1Level(int db);  static std::string getPPrmtrcEq1Level();
    static std::string setPPrmtrcEq1Bwth(int bw);   static std::string getPPrmtrcEq1Bwth();
    static std::string setPPrmtrcEq2Freq(int idx);  static std::string getPPrmtrcEq2Freq();
    static std::string setPPrmtrcEq2Level(int db);  static std::string getPPrmtrcEq2Level();
    static std::string setPPrmtrcEq2Bwth(int bw);   static std::string getPPrmtrcEq2Bwth();
    static std::string setPPrmtrcEq3Freq(int idx);  static std::string getPPrmtrcEq3Freq();
    static std::string setPPrmtrcEq3Level(int db);  static std::string getPPrmtrcEq3Level();
    static std::string setPPrmtrcEq3Bwth(int bw);   static std::string getPPrmtrcEq3Bwth();

    // ── TX GENERAL ────────────────────────────────────────────────────────
    static std::string setTxGnrlMaxPowerBattery(int step);
    static std::string getTxGnrlMaxPowerBattery();

    // ── RX DSP ────────────────────────────────────────────────────────────
    static std::string setNbLevel(int level);       static std::string getNbLevel();
    static std::string setNbWidth(int width);       static std::string getNbWidth();
    static std::string setNrLevel(int level);       static std::string getNrLevel();
    static std::string setContourLevel(int db);     static std::string getContourLevel();
    static std::string setContourWidth(int q);      static std::string getContourWidth();
    static std::string setIfNotchWidth(int w);      static std::string getIfNotchWidth();
    static std::string setApfWidth(int w);          static std::string getApfWidth();

    // ── VOX / KEY / DIAL ─────────────────────────────────────────────────
    static std::string setVoxSelect(int sel);       static std::string getVoxSelect();
    static std::string setEmergencyFreqTx(bool on); static std::string getEmergencyFreqTx();
    static std::string setMicUpAssign(int dir);     static std::string getMicUpAssign();
    static std::string setDialStep(int step);       static std::string getDialStep();

    // ====================================================================
    //  SECTION 3 — RESPONSE PARSING HELPERS
    // ====================================================================

    // Parse RM response → raw 0-255; sets meterId
    static int  parseReadMeter(const std::string& r, int& meterId);

    // Parse FA/FB response → Hz
    static long long parseFrequency(const std::string& r);

    // Parse MD response → mode index 0-9; sets vfo
    static int  parseMode(const std::string& r, int& vfo);

    // Parse TX response → true if transmitting
    static bool parseTx(const std::string& r);

    // Map mode index to name string (legacy integer index, kept for compatibility)
    static const char* modeName(int idx);
};
