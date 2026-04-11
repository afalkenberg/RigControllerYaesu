/**
 * CATCommand.h
 * ============
 * Complete CAT command builder for the Yaesu FTX-1 series transceiver.
 *
 * Covers the FTX-1 CAT Operation Reference Manual in page order:
 *   Pages 5-8  : Core commands (AB, AC, AG, AI, AM, AO, BA, BC, BD, BI,
 *                BM, BP, BS, BU, CF, CH, CN, CO, CS, CT, DA, DN, DT, EX,
 *                FA, FB, FN, FR, FT, GP, GT, ID, IF, IS, KM, KP, KR, KS,
 *                KY, LK, LM, MA, MB, MC, MD, MG, ML, MR, MS, NB, NR, PA,
 *                PC, PR, RA, RC, RD, RM, RT, RU, RG, SC, SD, SF, SH, SM,
 *                SP, SQ, SS, ST, SV, TS, TX, UP, VD, VE, VG, VM, VS, VX, ZI)
 *   Pages 9-27 : EX (Extended Menu) commands — all menu items named from
 *                the advance manual table titles.
 *
 * Usage:
 *   std::string cmd = CATCommand::setMainSideFrequency(14250000LL);
 *   serial.write(cmd);  // sends "FA014250000;"
 *
 * Every function returns a std::string containing the complete CAT command
 * including the semicolon terminator.  Read (get) functions return the
 * query string; the radio's answer must be parsed by the caller.
 *
 * Conventions:
 *   - VFO:   0 = MAIN, 1 = SUB
 *   - Mode:  0=LSB 1=USB 2=CW 3=CW-R 4=AM 5=FM 6=DATA-L 7=DATA-U
 *            8=DATA-FM 9=C4FM
 *   - bool parameters: true = ON/enable
 *   - Frequencies in Hz (long long)
 *   - Levels 0-255 unless otherwise noted
 */

#pragma once
#include <string>
#include <cstdio>
#include <algorithm>

// MSVC-safe clamp
template<typename T>
static inline T cat_clamp(T v, T lo, T hi){ return v<lo?lo:(v>hi?hi:v); }

class CATCommand {
public:
    // ====================================================================
    // SECTION 1 — CORE COMMANDS (pages 5–8, alphabetical by 2-letter code)
    // ====================================================================

    // ── AB: MAIN-side to SUB-side copy ──────────────────────────────────
    static std::string setMainSideToSubSide() { return "AB;"; }

    // ── AC: ANTENNA TUNER CONTROL ───────────────────────────────────────
    // p1: 0=off, 1=on(tune complete), 2=TX+tuning
    static std::string setAntennaTunerControl(int p1) {
        char b[8]; snprintf(b,sizeof(b),"AC0%d0;",cat_clamp(p1,0,2)); return b;
    }
    static std::string getAntennaTunerControl() { return "AC;"; }

    // ── AG: AF GAIN ─────────────────────────────────────────────────────
    // vfo: 0=MAIN, 1=SUB   level: 0-255
    static std::string setAfGain(int vfo, int level) {
        char b[12]; snprintf(b,sizeof(b),"AG%d%03d;",cat_clamp(vfo,0,1),cat_clamp(level,0,255)); return b;
    }
    static std::string getAfGain(int vfo=0) {
        char b[6]; snprintf(b,sizeof(b),"AG%d;",cat_clamp(vfo,0,1)); return b;
    }

    // ── AI: AUTO INFORMATION ────────────────────────────────────────────
    // mode: 0=OFF, 1=AI1(Kenwood compat), 2=AI2(full AI)
    static std::string setAutoInformation(int mode) {
        char b[6]; snprintf(b,sizeof(b),"AI%d;",cat_clamp(mode,0,2)); return b;
    }
    static std::string getAutoInformation() { return "AI;"; }

    // ── AM: MAIN-side to MEMORY CHANNEL copy ───────────────────────────
    static std::string setMainSideToMemoryChannel() { return "AM;"; }

    // ── AO: AMC OUTPUT LEVEL ────────────────────────────────────────────
    // level: 0-100
    static std::string setAmcOutputLevel(int level) {
        char b[8]; snprintf(b,sizeof(b),"AO%03d;",cat_clamp(level,0,100)); return b;
    }
    static std::string getAmcOutputLevel() { return "AO;"; }

    // ── BA: SUB-side to MAIN-side copy ──────────────────────────────────
    static std::string setSubSideToMainSide() { return "BA;"; }

    // ── BC: AUTO NOTCH (DNF) ─────────────────────────────────────────────
    // vfo: 0=MAIN, 1=SUB   on: true=ON
    static std::string setAutoNotch(int vfo, bool on) {
        char b[8]; snprintf(b,sizeof(b),"BC%d%d;",cat_clamp(vfo,0,1),on?1:0); return b;
    }
    static std::string getAutoNotch(int vfo=0) {
        char b[6]; snprintf(b,sizeof(b),"BC%d;",cat_clamp(vfo,0,1)); return b;
    }

    // ── BD: BAND DOWN ───────────────────────────────────────────────────
    static std::string setBandDown() { return "BD0;"; }

    // ── BI: BREAK-IN ────────────────────────────────────────────────────
    // mode: 0=OFF, 1=SEMI, 2=FULL
    static std::string setBreakIn(int mode) {
        char b[6]; snprintf(b,sizeof(b),"BI%d;",cat_clamp(mode,0,2)); return b;
    }
    static std::string getBreakIn() { return "BI;"; }

    // ── BM: SUB-side to MEMORY CHANNEL copy ────────────────────────────
    static std::string setSubSideToMemoryChannel() { return "BM;"; }

    // ── BP: MANUAL NOTCH ────────────────────────────────────────────────
    // on: false=OFF  freq: 0-4000 Hz (only used when on=true)
    static std::string setManualNotch(bool on, int freqHz=1000) {
        if (!on) return "BP00000;";
        char b[12]; snprintf(b,sizeof(b),"BP1%04d;",cat_clamp(freqHz,0,4000)); return b;
    }
    static std::string getManualNotch() { return "BP;"; }

    // ── BS: BAND SELECT ─────────────────────────────────────────────────
    // band: 0=160m 1=80m 2=60m 3=40m 4=30m 5=20m 6=17m 7=15m 8=12m
    //       9=10m 10=6m 11=2m 12=70cm
    static std::string setBandSelect(int band) {
        char b[8]; snprintf(b,sizeof(b),"BS%02d;",cat_clamp(band,0,12)); return b;
    }

    // ── BU: BAND UP ─────────────────────────────────────────────────────
    static std::string setBandUp() { return "BU0;"; }

    // ── CF: CLAR (CLARIFIER) ────────────────────────────────────────────
    // vfo: 0=MAIN, 1=SUB   on: true=ON   offsetHz: -9999..+9999
    static std::string setClarifier(int vfo, bool on, int offsetHz=0) {
        char sign = offsetHz>=0?'+':'-';
        char b[16]; snprintf(b,sizeof(b),"CF%d%d%c%04d;",
            cat_clamp(vfo,0,1),on?1:0,sign,std::abs(offsetHz)%10000);
        return b;
    }
    static std::string getClarifier(int vfo=0) {
        char b[6]; snprintf(b,sizeof(b),"CF%d;",cat_clamp(vfo,0,1)); return b;
    }

    // ── CH: CHANNEL UP/DOWN ─────────────────────────────────────────────
    // dir: 0=DOWN, 1=UP
    static std::string setChannelUpDown(int dir) {
        char b[6]; snprintf(b,sizeof(b),"CH%d;",cat_clamp(dir,0,1)); return b;
    }

    // ── CN: CTCSS NUMBER ────────────────────────────────────────────────
    // vfo: 0=MAIN, 1=SUB   toneNum: 0=67.0Hz .. 49=254.1Hz (CTCSS table)
    static std::string setCtcssNumber(int vfo, int toneNum) {
        char b[10]; snprintf(b,sizeof(b),"CN%d%02d;",cat_clamp(vfo,0,1),cat_clamp(toneNum,0,49)); return b;
    }
    static std::string getCtcssNumber(int vfo=0) {
        char b[6]; snprintf(b,sizeof(b),"CN%d;",cat_clamp(vfo,0,1)); return b;
    }

    // ── CO: CONTOUR / APF ───────────────────────────────────────────────
    // level: -40..+20 dB   width: 01-11 (Q factor index)
    static std::string setContour(int levelDb, int width=5) {
        levelDb = cat_clamp(levelDb,-40,20);
        width   = cat_clamp(width,1,11);
        char sign = levelDb>=0?'+':'-';
        char b[12]; snprintf(b,sizeof(b),"CO%c%02d%02d;",sign,std::abs(levelDb),width); return b;
    }
    static std::string getContour() { return "CO;"; }

    // ── CS: CW SPOT ─────────────────────────────────────────────────────
    static std::string setCwSpot(bool on) { return on ? "CS1;" : "CS0;"; }
    static std::string getCwSpot()        { return "CS;"; }

    // ── CT: CTCSS ───────────────────────────────────────────────────────
    // vfo: 0=MAIN, 1=SUB   mode: 0=OFF 1=ENC 2=ENC+DEC
    static std::string setCtcss(int vfo, int mode) {
        char b[8]; snprintf(b,sizeof(b),"CT%d%d;",cat_clamp(vfo,0,1),cat_clamp(mode,0,2)); return b;
    }
    static std::string getCtcss(int vfo=0) {
        char b[6]; snprintf(b,sizeof(b),"CT%d;",cat_clamp(vfo,0,1)); return b;
    }

    // ── DA: LCD CONTRAST / DIMMER ───────────────────────────────────────
    // type: 0=contrast, 1=dimmer   level: 0-100
    static std::string setLcdDimmer(int type, int level) {
        char b[10]; snprintf(b,sizeof(b),"DA%d%03d;",cat_clamp(type,0,1),cat_clamp(level,0,100)); return b;
    }
    static std::string getLcdDimmer(int type=1) {
        char b[6]; snprintf(b,sizeof(b),"DA%d;",cat_clamp(type,0,1)); return b;
    }

    // ── DN: DOWN KEY ────────────────────────────────────────────────────
    static std::string setDownKey() { return "DN;"; }

    // ── DT: DATE AND TIME ───────────────────────────────────────────────
    // YYYYMMDDHHmmSS (14 digits)
    static std::string setDateTime(const std::string& dt14) {
        return "DT" + dt14.substr(0,14) + ";";
    }
    static std::string getDateTime() { return "DT;"; }

    // ── EX: MENU (extended) — see Section 2 below ───────────────────────
    // Raw EX command builder used by named wrappers
    static std::string setMenuRaw(int menuNum, const std::string& value) {
        char b[16]; snprintf(b,sizeof(b),"EX%04d",menuNum);
        return std::string(b) + value + ";";
    }
    static std::string getMenuRaw(int menuNum) {
        char b[12]; snprintf(b,sizeof(b),"EX%04d;",menuNum); return b;
    }

    // ── FA: FREQUENCY MAIN-side ─────────────────────────────────────────
    // freqHz: 1800000 .. 470000000
    static std::string setMainSideFrequency(long long freqHz) {
        char b[16]; snprintf(b,sizeof(b),"FA%09lld;",freqHz); return b;
    }
    static std::string getMainSideFrequency() { return "FA;"; }

    // ── FB: FREQUENCY SUB-side ──────────────────────────────────────────
    static std::string setSubSideFrequency(long long freqHz) {
        char b[16]; snprintf(b,sizeof(b),"FB%09lld;",freqHz); return b;
    }
    static std::string getSubSideFrequency() { return "FB;"; }

    // ── FN: FINE TUNING ─────────────────────────────────────────────────
    // vfo: 0=MAIN, 1=SUB
    static std::string setFineTuning(int vfo, bool on) {
        char b[8]; snprintf(b,sizeof(b),"FN%d%d;",cat_clamp(vfo,0,1),on?1:0); return b;
    }
    static std::string getFineTuning(int vfo=0) {
        char b[6]; snprintf(b,sizeof(b),"FN%d;",cat_clamp(vfo,0,1)); return b;
    }

    // ── FR: FUNCTION RX (single/dual receive) ───────────────────────────
    // mode: 0=single, 1=dual
    static std::string setFunctionRx(int mode) {
        char b[6]; snprintf(b,sizeof(b),"FR%d;",cat_clamp(mode,0,1)); return b;
    }
    static std::string getFunctionRx() { return "FR;"; }

    // ── FT: FUNCTION TX (transmit VFO) ──────────────────────────────────
    // vfo: 0=MAIN, 1=SUB
    static std::string setFunctionTx(int vfo) {
        char b[6]; snprintf(b,sizeof(b),"FT%d;",cat_clamp(vfo,0,1)); return b;
    }
    static std::string getFunctionTx() { return "FT;"; }

    // ── GP: GP OUT A/B/C/D ──────────────────────────────────────────────
    // port: 0=A,1=B,2=C,3=D   level: 0=LOW, 1=HIGH
    static std::string setGpOut(int port, int level) {
        char b[8]; snprintf(b,sizeof(b),"GP%d%d;",cat_clamp(port,0,3),cat_clamp(level,0,1)); return b;
    }
    static std::string getGpOut(int port=0) {
        char b[6]; snprintf(b,sizeof(b),"GP%d;",cat_clamp(port,0,3)); return b;
    }

    // ── GT: AGC FUNCTION ────────────────────────────────────────────────
    // vfo: 0=MAIN, 1=SUB   mode: 0=OFF 1=FAST 2=MID 3=SLOW 4=AUTO
    static std::string setAgcFunction(int vfo, int mode) {
        char b[10]; snprintf(b,sizeof(b),"GT0%d%d;",cat_clamp(vfo,0,1),cat_clamp(mode,0,4)); return b;
    }
    static std::string getAgcFunction(int vfo=0) {
        char b[8]; snprintf(b,sizeof(b),"GT0%d;",cat_clamp(vfo,0,1)); return b;
    }

    // ── ID: RADIO ID ────────────────────────────────────────────────────
    static std::string getRadioId() { return "ID;"; }

    // ── IF: INFORMATION (MAIN-side) ─────────────────────────────────────
    static std::string getInformation() { return "IF;"; }

    // ── IS: IF SHIFT ────────────────────────────────────────────────────
    // vfo: 0=MAIN, 1=SUB   offsetHz: -1000..+1000
    static std::string setIfShift(int vfo, int offsetHz) {
        offsetHz = cat_clamp(offsetHz,-1000,1000);
        char sign = offsetHz>=0?'+':'-';
        char b[14]; snprintf(b,sizeof(b),"IS%d%c%04d;",cat_clamp(vfo,0,1),sign,std::abs(offsetHz));
        return b;
    }
    static std::string getIfShift(int vfo=0) {
        char b[6]; snprintf(b,sizeof(b),"IS%d;",cat_clamp(vfo,0,1)); return b;
    }

    // ── KM: KEYER MEMORY ────────────────────────────────────────────────
    // ch: 1-5   text: CW message text (A-Z 0-9 space / = + ? @ etc.)
    static std::string setKeyerMemory(int ch, const std::string& text) {
        char b[8]; snprintf(b,sizeof(b),"KM%d",cat_clamp(ch,1,5));
        return std::string(b) + text + ";";
    }
    static std::string getKeyerMemory(int ch) {
        char b[6]; snprintf(b,sizeof(b),"KM%d;",cat_clamp(ch,1,5)); return b;
    }

    // ── KP: KEY PITCH ───────────────────────────────────────────────────
    // pitch: 300-1050 Hz (10 Hz steps)
    static std::string setKeyPitch(int pitchHz) {
        pitchHz = cat_clamp(pitchHz,300,1050);
        char b[10]; snprintf(b,sizeof(b),"KP%04d;",pitchHz); return b;
    }
    static std::string getKeyPitch() { return "KP;"; }

    // ── KR: KEYER ───────────────────────────────────────────────────────
    // mode: 0=OFF 1=MODE-A 2=MODE-B
    static std::string setKeyer(int mode) {
        char b[6]; snprintf(b,sizeof(b),"KR%d;",cat_clamp(mode,0,2)); return b;
    }
    static std::string getKeyer() { return "KR;"; }

    // ── KS: KEY SPEED ───────────────────────────────────────────────────
    // wpm: 4-60
    static std::string setKeySpeed(int wpm) {
        char b[8]; snprintf(b,sizeof(b),"KS%03d;",cat_clamp(wpm,4,60)); return b;
    }
    static std::string getKeySpeed() { return "KS;"; }

    // ── KY: CW KEYING MEMORY PLAY ───────────────────────────────────────
    // ch: 0=stop, 1-5=play channel, or direct text send
    static std::string setCwKeyingMemoryPlay(int ch) {
        char b[6]; snprintf(b,sizeof(b),"KY%d;",cat_clamp(ch,0,5)); return b;
    }
    // Send text directly as CW
    static std::string setCwKeyingText(const std::string& text) {
        return "KY " + text + ";";
    }

    // ── LK: LOCK ────────────────────────────────────────────────────────
    // type: 0=MAIN lock, 1=MAIN+SUB lock
    static std::string setLock(bool on, int type=0) {
        char b[8]; snprintf(b,sizeof(b),"LK%d%d;",on?1:0,cat_clamp(type,0,1)); return b;
    }
    static std::string getLock() { return "LK;"; }

    // ── LM: LOAD MESSAGE (voice/received sound recording) ───────────────
    // type: 0=RX, 1=TX   ch: 1-5
    static std::string setLoadMessage(int type, int ch) {
        char b[8]; snprintf(b,sizeof(b),"LM%d%d;",cat_clamp(type,0,1),cat_clamp(ch,1,5)); return b;
    }

    // ── MA: MEMORY CHANNEL to MAIN-side transfer ────────────────────────
    static std::string setMemoryChannelToMainSide() { return "MA;"; }

    // ── MB: MEMORY CHANNEL to SUB-side transfer ─────────────────────────
    static std::string setMemoryChannelToSubSide() { return "MB;"; }

    // ── MC: MEMORY CHANNEL ──────────────────────────────────────────────
    // vfo: 0=MAIN, 1=SUB   ch: 0-117
    static std::string setMemoryChannel(int vfo, int ch) {
        char b[10]; snprintf(b,sizeof(b),"MC%d%03d;",cat_clamp(vfo,0,1),cat_clamp(ch,0,117)); return b;
    }
    static std::string getMemoryChannel(int vfo=0) {
        char b[6]; snprintf(b,sizeof(b),"MC%d;",cat_clamp(vfo,0,1)); return b;
    }

    // ── MD: OPERATING MODE ──────────────────────────────────────────────
    // vfo: 0=MAIN, 1=SUB
    // mode: 0=LSB 1=USB 2=CW 3=CW-R 4=AM 5=FM 6=DATA-L 7=DATA-U 8=DATA-FM 9=C4FM
    static std::string setOperatingMode(int vfo, int mode) {
        char b[8]; snprintf(b,sizeof(b),"MD%d%d;",cat_clamp(vfo,0,1),cat_clamp(mode,0,9)); return b;
    }
    static std::string getOperatingMode(int vfo=0) {
        char b[6]; snprintf(b,sizeof(b),"MD%d;",cat_clamp(vfo,0,1)); return b;
    }

    // ── MG: MIC GAIN ────────────────────────────────────────────────────
    // level: 0-100
    static std::string setMicGain(int level) {
        char b[8]; snprintf(b,sizeof(b),"MG%03d;",cat_clamp(level,0,100)); return b;
    }
    static std::string getMicGain() { return "MG;"; }

    // ── ML: MONITOR LEVEL ───────────────────────────────────────────────
    // level: 0-100
    static std::string setMonitorLevel(int level) {
        char b[8]; snprintf(b,sizeof(b),"ML%03d;",cat_clamp(level,0,100)); return b;
    }
    static std::string getMonitorLevel() { return "ML;"; }

    // ── MR: MEMORY CHANNEL READ ─────────────────────────────────────────
    // vfo: 0=MAIN, 1=SUB   ch: 0-117
    static std::string getMemoryChannelRead(int vfo, int ch) {
        char b[10]; snprintf(b,sizeof(b),"MR%d%03d;",cat_clamp(vfo,0,1),cat_clamp(ch,0,117)); return b;
    }

    // ── MS: METER SW ────────────────────────────────────────────────────
    // meter: 1=S-MAIN 2=S-SUB 3=COMP 4=ALC 5=PO 6=SWR 7=IDD 8=VDD
    static std::string setMeterSw(int meter) {
        char b[6]; snprintf(b,sizeof(b),"MS%d;",cat_clamp(meter,1,8)); return b;
    }
    static std::string getMeterSw() { return "MS;"; }

    // ── NB: NOISE BLANKER ───────────────────────────────────────────────
    // vfo: 0=MAIN, 1=SUB
    static std::string setNoiseBlanker(int vfo, bool on) {
        char b[8]; snprintf(b,sizeof(b),"NB%d%d;",cat_clamp(vfo,0,1),on?1:0); return b;
    }
    static std::string getNoiseBlanker(int vfo=0) {
        char b[6]; snprintf(b,sizeof(b),"NB%d;",cat_clamp(vfo,0,1)); return b;
    }

    // ── NR: NOISE REDUCTION (DNR) ───────────────────────────────────────
    // vfo: 0=MAIN, 1=SUB
    static std::string setNoiseReduction(int vfo, bool on) {
        char b[8]; snprintf(b,sizeof(b),"NR%d%d;",cat_clamp(vfo,0,1),on?1:0); return b;
    }
    static std::string getNoiseReduction(int vfo=0) {
        char b[6]; snprintf(b,sizeof(b),"NR%d;",cat_clamp(vfo,0,1)); return b;
    }

    // ── PA: PREAMP / IPO ────────────────────────────────────────────────
    // band: 0=HF/50MHz, 1=VHF, 2=UHF
    // level: 0=IPO(OFF) 1=AMP1 2=AMP2 (HF/50); 0=OFF 1=ON (VHF/UHF)
    static std::string setPreamp(int band, int level) {
        char b[8]; snprintf(b,sizeof(b),"PA%d%d;",cat_clamp(band,0,2),cat_clamp(level,0,2)); return b;
    }
    static std::string getPreamp(int band=0) {
        char b[6]; snprintf(b,sizeof(b),"PA%d;",cat_clamp(band,0,2)); return b;
    }

    // ── PC: POWER CONTROL ───────────────────────────────────────────────
    // watts: 0-100
    static std::string setPowerControl(int watts) {
        char b[8]; snprintf(b,sizeof(b),"PC%03d;",cat_clamp(watts,0,100)); return b;
    }
    static std::string getPowerControl() { return "PC;"; }

    // ── PR: SPEECH PROCESSOR ────────────────────────────────────────────
    static std::string setSpeechProcessor(bool on) { return on ? "PR1;" : "PR0;"; }
    static std::string getSpeechProcessor()        { return "PR;"; }

    // ── RA: ATTENUATOR ──────────────────────────────────────────────────
    // on: true=ATT ON   level: 0=6dB, 1=12dB, 2=18dB (or 0=off when on=false)
    static std::string setAttenuator(bool on, int level=0) {
        if (!on) return "RA00;";
        char b[8]; snprintf(b,sizeof(b),"RA%02d;",cat_clamp(level+1,1,3)); return b;
    }
    static std::string getAttenuator() { return "RA;"; }

    // ── RC: CLAR/RIT CLEAR ──────────────────────────────────────────────
    static std::string setClarClear() { return "RC;"; }

    // ── RD: RIT/XIT DOWN ────────────────────────────────────────────────
    // steps: number of 10-Hz steps down
    static std::string setRitDown(int steps=1) {
        char b[8]; snprintf(b,sizeof(b),"RD%04d;",cat_clamp(steps,0,9999)); return b;
    }

    // ── RM: READ METER ──────────────────────────────────────────────────
    // meter: 1=S-MAIN 2=S-SUB 3=COMP 4=ALC 5=PO 6=SWR 7=IDD 8=VDD
    // Response: RM[meter][6-digit raw value 000000-000255];
    static std::string getReadMeter(int meter) {
        char b[6]; snprintf(b,sizeof(b),"RM%d;",cat_clamp(meter,1,8)); return b;
    }
    // Convenience helpers
    static std::string getSmeterMain()  { return "RM1;"; }
    static std::string getSmeterSub()   { return "RM2;"; }
    static std::string getComp()        { return "RM3;"; }
    static std::string getAlc()         { return "RM4;"; }
    static std::string getPowerOutput() { return "RM5;"; }
    static std::string getSwr()         { return "RM6;"; }
    static std::string getIdd()         { return "RM7;"; }
    static std::string getVdd()         { return "RM8;"; }

    // ── RT: RIT ─────────────────────────────────────────────────────────
    static std::string setRit(bool on) { return on ? "RT1;" : "RT0;"; }
    static std::string getRit()        { return "RT;"; }

    // ── RU: RIT/XIT UP ──────────────────────────────────────────────────
    // steps: number of 10-Hz steps up
    static std::string setRitUp(int steps=1) {
        char b[8]; snprintf(b,sizeof(b),"RU%04d;",cat_clamp(steps,0,9999)); return b;
    }

    // ── RG: RF GAIN ─────────────────────────────────────────────────────
    // level: 0-255
    static std::string setRfGain(int level) {
        char b[8]; snprintf(b,sizeof(b),"RG%03d;",cat_clamp(level,0,255)); return b;
    }
    static std::string getRfGain() { return "RG;"; }

    // ── SC: SCAN ────────────────────────────────────────────────────────
    // mode: 0=stop 1=UP 2=DOWN 3=MEM 4=MODE 5=GRP
    static std::string setScan(int mode) {
        char b[6]; snprintf(b,sizeof(b),"SC%d;",cat_clamp(mode,0,5)); return b;
    }
    static std::string getScan() { return "SC;"; }

    // ── SD: CW BREAK-IN DELAY TIME ──────────────────────────────────────
    // ms: 30-3000 (10ms steps)
    static std::string setCwBreakInDelay(int ms) {
        char b[8]; snprintf(b,sizeof(b),"SD%04d;",cat_clamp(ms,30,3000)); return b;
    }
    static std::string getCwBreakInDelay() { return "SD;"; }

    // ── SF: FUNC-KNOB FUNCTION ──────────────────────────────────────────
    // func: 0=MULTI 1=AF 2=RF/SQL etc. (radio-dependent)
    static std::string setFuncKnobFunction(int func) {
        char b[6]; snprintf(b,sizeof(b),"SF%d;",cat_clamp(func,0,9)); return b;
    }
    static std::string getFuncKnobFunction() { return "SF;"; }

    // ── SH: WIDTH ───────────────────────────────────────────────────────
    // vfo: 0=MAIN, 1=SUB
    // widthIdx: 0-99  (0=narrowest, maps to 50Hz steps from 50Hz upward)
    static std::string setWidth(int vfo, int widthIdx) {
        char b[10]; snprintf(b,sizeof(b),"SH%d%02d;",cat_clamp(vfo,0,1),cat_clamp(widthIdx,0,99)); return b;
    }
    static std::string getWidth(int vfo=0) {
        char b[6]; snprintf(b,sizeof(b),"SH%d;",cat_clamp(vfo,0,1)); return b;
    }

    // ── SM: S-METER READING ─────────────────────────────────────────────
    // vfo: 0=MAIN, 1=SUB
    // Response: SM[vfo][4-digit 0-0030];  (0=S0 .. 15=S9 .. 30=+60dB approx)
    static std::string getSmeterReading(int vfo=0) {
        char b[6]; snprintf(b,sizeof(b),"SM%d;",cat_clamp(vfo,0,1)); return b;
    }

    // ── SP: SPLIT ───────────────────────────────────────────────────────
    static std::string setSplit(bool on) { return on ? "SP1;" : "SP0;"; }
    static std::string getSplit()        { return "SP;"; }

    // ── SQ: SQUELCH LEVEL ───────────────────────────────────────────────
    // vfo: 0=MAIN, 1=SUB   level: 0-255
    static std::string setSquelchLevel(int vfo, int level) {
        char b[10]; snprintf(b,sizeof(b),"SQ%d%03d;",cat_clamp(vfo,0,1),cat_clamp(level,0,255)); return b;
    }
    static std::string getSquelchLevel(int vfo=0) {
        char b[6]; snprintf(b,sizeof(b),"SQ%d;",cat_clamp(vfo,0,1)); return b;
    }

    // ── SS: SPECTRUM SCOPE ──────────────────────────────────────────────
    // mode: 0=OFF 1=CENTER 2=FIXED 3=SCROLL-C 4=SCROLL-F
    static std::string setSpectrumScope(int mode) {
        char b[6]; snprintf(b,sizeof(b),"SS%d;",cat_clamp(mode,0,4)); return b;
    }
    static std::string getSpectrumScope() { return "SS;"; }

    // ── ST: SPLIT ───────────────────────────────────────────────────────
    // (alias of SP on some Yaesu models; on FTX-1 same as SP)
    static std::string setSplitSt(bool on) { return on ? "ST1;" : "ST0;"; }
    static std::string getSplitSt()        { return "ST;"; }

    // ── SV: SWAP VFO ────────────────────────────────────────────────────
    static std::string setSwapVfo() { return "SV;"; }

    // ── TS: TXW ─────────────────────────────────────────────────────────
    // Transmit Watch — listen on TX frequency during split
    static std::string setTxw(bool on) { return on ? "TS1;" : "TS0;"; }
    static std::string getTxw()        { return "TS;"; }

    // ── TX: TX SET ──────────────────────────────────────────────────────
    // mode: 0=RX, 1=TX (CAT-1), 2=TX (CAT-2)
    static std::string setTx(int mode=1) {
        char b[6]; snprintf(b,sizeof(b),"TX%d;",cat_clamp(mode,0,2)); return b;
    }
    static std::string setRx() { return "TX0;"; }
    static std::string getTx() { return "TX;"; }

    // ── UP: MIC UP ──────────────────────────────────────────────────────
    static std::string setMicUp() { return "UP;"; }

    // ── VD: VOX DELAY TIME / DATA DELAY TIME ────────────────────────────
    // delayCode: 0-30  (maps to: 50,100,150,...,3000ms)
    static std::string setVoxDelayTime(int delayCode) {
        char b[8]; snprintf(b,sizeof(b),"VD%04d;",cat_clamp(delayCode,0,30)); return b;
    }
    static std::string getVoxDelayTime() { return "VD;"; }

    // ── VE: FIRMWARE VERSION READ ────────────────────────────────────────
    static std::string getFirmwareVersion() { return "VE;"; }

    // ── VG: VOX GAIN ────────────────────────────────────────────────────
    // gain: 0-100
    static std::string setVoxGain(int gain) {
        char b[8]; snprintf(b,sizeof(b),"VG%03d;",cat_clamp(gain,0,100)); return b;
    }
    static std::string getVoxGain() { return "VG;"; }

    // ── VM: VFO / MEMORY CHANNEL ────────────────────────────────────────
    // vfo: 0=MAIN, 1=SUB   mode: 0=VFO, 1=MEM
    static std::string setVfoOrMemoryChannel(int vfo, int mode) {
        char b[8]; snprintf(b,sizeof(b),"VM%d%d;",cat_clamp(vfo,0,1),cat_clamp(mode,0,1)); return b;
    }
    static std::string getVfoOrMemoryChannel(int vfo=0) {
        char b[6]; snprintf(b,sizeof(b),"VM%d;",cat_clamp(vfo,0,1)); return b;
    }

    // ── VS: VFO SELECT ──────────────────────────────────────────────────
    // vfo: 0=VFO-A, 1=VFO-B
    static std::string setVfoSelect(int vfo) {
        char b[6]; snprintf(b,sizeof(b),"VS%d;",cat_clamp(vfo,0,1)); return b;
    }
    static std::string getVfoSelect() { return "VS;"; }

    // ── VX: VOX STATUS ──────────────────────────────────────────────────
    static std::string setVoxStatus(bool on) { return on ? "VX1;" : "VX0;"; }
    static std::string getVoxStatus()        { return "VX;"; }

    // ── ZI: ZERO IN ─────────────────────────────────────────────────────
    static std::string setZeroIn() { return "ZI;"; }

    // ====================================================================
    // SECTION 2 — EX EXTENDED MENU COMMANDS (pages 9–27)
    // Named from the advance manual table section titles.
    // Format: EX[4-digit menu number][value digits];
    // Menu numbers are from the FTX-1 advance manual tables.
    // ====================================================================

    // ─── MODE SSB (pages 9-10) ──────────────────────────────────────────

    // AF Treble Gain: -20..+10 dB  (EX0101)
    static std::string setAfTrebleGain(int db) {
        db=cat_clamp(db,-20,10);
        char v[4]; snprintf(v,sizeof(v),"%+03d",db);
        return setMenuRaw(101, v);
    }
    static std::string getAfTrebleGain() { return getMenuRaw(101); }

    // AF Middle Tone Gain: -20..+10 dB  (EX0102)
    static std::string setAfMiddleToneGain(int db) {
        db=cat_clamp(db,-20,10);
        char v[4]; snprintf(v,sizeof(v),"%+03d",db);
        return setMenuRaw(102, v);
    }
    static std::string getAfMiddleToneGain() { return getMenuRaw(102); }

    // AF Bass Gain: -20..+10 dB  (EX0103)
    static std::string setAfBassGain(int db) {
        db=cat_clamp(db,-20,10);
        char v[4]; snprintf(v,sizeof(v),"%+03d",db);
        return setMenuRaw(103, v);
    }
    static std::string getAfBassGain() { return getMenuRaw(103); }

    // AGC Fast Delay: 20-4000 ms in 20ms steps  (EX0104)
    static std::string setAgcFastDelay(int ms) {
        ms=cat_clamp(ms,20,4000);
        char v[8]; snprintf(v,sizeof(v),"%04d",ms);
        return setMenuRaw(104, v);
    }
    static std::string getAgcFastDelay() { return getMenuRaw(104); }

    // AGC Mid Delay: 20-4000 ms  (EX0105)
    static std::string setAgcMidDelay(int ms) {
        ms=cat_clamp(ms,20,4000);
        char v[8]; snprintf(v,sizeof(v),"%04d",ms);
        return setMenuRaw(105, v);
    }
    static std::string getAgcMidDelay() { return getMenuRaw(105); }

    // AGC Slow Delay: 20-4000 ms  (EX0106)
    static std::string setAgcSlowDelay(int ms) {
        ms=cat_clamp(ms,20,4000);
        char v[8]; snprintf(v,sizeof(v),"%04d",ms);
        return setMenuRaw(106, v);
    }
    static std::string getAgcSlowDelay() { return getMenuRaw(106); }

    // LCUT Freq: 0=OFF, 1-20=100-1050Hz (50Hz steps)  (EX0107)
    static std::string setLcutFreq(int idx) {
        idx=cat_clamp(idx,0,20);
        char v[4]; snprintf(v,sizeof(v),"%02d",idx);
        return setMenuRaw(107, v);
    }
    static std::string getLcutFreq() { return getMenuRaw(107); }

    // LCUT Slope: 0=6dB/oct, 1=18dB/oct  (EX0108)
    static std::string setLcutSlope(int slope) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(slope,0,1));
        return setMenuRaw(108, v);
    }
    static std::string getLcutSlope() { return getMenuRaw(108); }

    // HCUT Freq: 0=OFF, 1-67=700-4000Hz (50Hz steps)  (EX0109)
    static std::string setHcutFreq(int idx) {
        idx=cat_clamp(idx,0,67);
        char v[4]; snprintf(v,sizeof(v),"%02d",idx);
        return setMenuRaw(109, v);
    }
    static std::string getHcutFreq() { return getMenuRaw(109); }

    // HCUT Slope: 0=6dB/oct, 1=18dB/oct  (EX0110)
    static std::string setHcutSlope(int slope) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(slope,0,1));
        return setMenuRaw(110, v);
    }
    static std::string getHcutSlope() { return getMenuRaw(110); }

    // USB Out Level: 0-100  (EX0111)
    static std::string setUsbOutLevel(int level) {
        char v[4]; snprintf(v,sizeof(v),"%03d",cat_clamp(level,0,100));
        return setMenuRaw(111, v);
    }
    static std::string getUsbOutLevel() { return getMenuRaw(111); }

    // TX BPF Sel: 0=100-3000, 1=100-2900, 2=200-2800, 3=300-2700, 4=400-2600  (EX0112)
    static std::string setTxBpfSel(int sel) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(sel,0,4));
        return setMenuRaw(112, v);
    }
    static std::string getTxBpfSel() { return getMenuRaw(112); }

    // Mod Source: 0=MIC, 1=REAR, 2=USB  (EX0113)
    static std::string setModSource(int src) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(src,0,2));
        return setMenuRaw(113, v);
    }
    static std::string getModSource() { return getMenuRaw(113); }

    // USB Mod Gain: 0-100  (EX0114)
    static std::string setUsbModGain(int gain) {
        char v[4]; snprintf(v,sizeof(v),"%03d",cat_clamp(gain,0,100));
        return setMenuRaw(114, v);
    }
    static std::string getUsbModGain() { return getMenuRaw(114); }

    // RPTT Select: 0=DAKY, 1=RTS, 2=DTR  (EX0115)
    static std::string setRpttSelect(int sel) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(sel,0,2));
        return setMenuRaw(115, v);
    }
    static std::string getRpttSelect() { return getMenuRaw(115); }

    // ─── MODE CW (pages 11-12) ──────────────────────────────────────────

    // CW AF Treble Gain: -20..+10 dB  (EX0201)
    static std::string setCwAfTrebleGain(int db) {
        db=cat_clamp(db,-20,10);
        char v[4]; snprintf(v,sizeof(v),"%+03d",db);
        return setMenuRaw(201, v);
    }
    static std::string getCwAfTrebleGain() { return getMenuRaw(201); }

    // CW AF Middle Tone Gain: -20..+10 dB  (EX0202)
    static std::string setCwAfMiddleToneGain(int db) {
        db=cat_clamp(db,-20,10);
        char v[4]; snprintf(v,sizeof(v),"%+03d",db);
        return setMenuRaw(202, v);
    }
    static std::string getCwAfMiddleToneGain() { return getMenuRaw(202); }

    // CW AF Bass Gain: -20..+10 dB  (EX0203)
    static std::string setCwAfBassGain(int db) {
        db=cat_clamp(db,-20,10);
        char v[4]; snprintf(v,sizeof(v),"%+03d",db);
        return setMenuRaw(203, v);
    }
    static std::string getCwAfBassGain() { return getMenuRaw(203); }

    // CW AGC Fast Delay: 20-4000ms  (EX0204)
    static std::string setCwAgcFastDelay(int ms) {
        char v[8]; snprintf(v,sizeof(v),"%04d",cat_clamp(ms,20,4000));
        return setMenuRaw(204, v);
    }
    static std::string getCwAgcFastDelay() { return getMenuRaw(204); }

    // CW AGC Mid Delay: 20-4000ms  (EX0205)
    static std::string setCwAgcMidDelay(int ms) {
        char v[8]; snprintf(v,sizeof(v),"%04d",cat_clamp(ms,20,4000));
        return setMenuRaw(205, v);
    }
    static std::string getCwAgcMidDelay() { return getMenuRaw(205); }

    // CW AGC Slow Delay: 20-4000ms  (EX0206)
    static std::string setCwAgcSlowDelay(int ms) {
        char v[8]; snprintf(v,sizeof(v),"%04d",cat_clamp(ms,20,4000));
        return setMenuRaw(206, v);
    }
    static std::string getCwAgcSlowDelay() { return getMenuRaw(206); }

    // CW LCUT Freq: 0=OFF, 1-20=100-1050Hz  (EX0207)
    static std::string setCwLcutFreq(int idx) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(idx,0,20));
        return setMenuRaw(207, v);
    }
    static std::string getCwLcutFreq() { return getMenuRaw(207); }

    // CW LCUT Slope: 0=6dB/oct, 1=18dB/oct  (EX0208)
    static std::string setCwLcutSlope(int s) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(s,0,1));
        return setMenuRaw(208, v);
    }
    static std::string getCwLcutSlope() { return getMenuRaw(208); }

    // CW HCUT Freq: 0=OFF, 1-67=700-4000Hz  (EX0209)
    static std::string setCwHcutFreq(int idx) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(idx,0,67));
        return setMenuRaw(209, v);
    }
    static std::string getCwHcutFreq() { return getMenuRaw(209); }

    // CW HCUT Slope: 0=6dB/oct, 1=18dB/oct  (EX0210)
    static std::string setCwHcutSlope(int s) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(s,0,1));
        return setMenuRaw(210, v);
    }
    static std::string getCwHcutSlope() { return getMenuRaw(210); }

    // CW Out Level: 0-100  (EX0211)
    static std::string setCwOutLevel(int level) {
        char v[4]; snprintf(v,sizeof(v),"%03d",cat_clamp(level,0,100));
        return setMenuRaw(211, v);
    }
    static std::string getCwOutLevel() { return getMenuRaw(211); }

    // CW Auto Mode: 0=OFF, 1=50ms, 2=ON  (EX0212)
    static std::string setCwAutoMode(int mode) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(mode,0,2));
        return setMenuRaw(212, v);
    }
    static std::string getCwAutoMode() { return getMenuRaw(212); }

    // NAR Width (CW): 0=NARROW 1=MEDIUM 2=WIDE  (EX0213)
    static std::string setCwNarWidth(int w) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(w,0,2));
        return setMenuRaw(213, v);
    }
    static std::string getCwNarWidth() { return getMenuRaw(213); }

    // CW PC Keying: 0=OFF, 1=RTS, 2=DTR  (EX0214)
    static std::string setCwPcKeying(int sel) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(sel,0,2));
        return setMenuRaw(214, v);
    }
    static std::string getCwPcKeying() { return getMenuRaw(214); }

    // CW Indicator: 0=OFF, 1=ON  (EX0215)
    static std::string setCwIndicator(bool on) {
        return setMenuRaw(215, on?"1":"0");
    }
    static std::string getCwIndicator() { return getMenuRaw(215); }

    // ─── MODE AM (pages 13-14) ──────────────────────────────────────────

    // AM AF Treble Gain: -20..+10 dB  (EX0301)
    static std::string setAmAfTrebleGain(int db) {
        db=cat_clamp(db,-20,10);
        char v[4]; snprintf(v,sizeof(v),"%+03d",db);
        return setMenuRaw(301, v);
    }
    static std::string getAmAfTrebleGain() { return getMenuRaw(301); }

    // AM AF Middle Tone Gain  (EX0302)
    static std::string setAmAfMiddleToneGain(int db) {
        db=cat_clamp(db,-20,10);
        char v[4]; snprintf(v,sizeof(v),"%+03d",db);
        return setMenuRaw(302, v);
    }
    static std::string getAmAfMiddleToneGain() { return getMenuRaw(302); }

    // AM AF Bass Gain  (EX0303)
    static std::string setAmAfBassGain(int db) {
        db=cat_clamp(db,-20,10);
        char v[4]; snprintf(v,sizeof(v),"%+03d",db);
        return setMenuRaw(303, v);
    }
    static std::string getAmAfBassGain() { return getMenuRaw(303); }

    // AM AGC Fast Delay  (EX0304)
    static std::string setAmAgcFastDelay(int ms) {
        char v[8]; snprintf(v,sizeof(v),"%04d",cat_clamp(ms,20,4000));
        return setMenuRaw(304, v);
    }
    static std::string getAmAgcFastDelay() { return getMenuRaw(304); }

    // AM AGC Mid Delay  (EX0305)
    static std::string setAmAgcMidDelay(int ms) {
        char v[8]; snprintf(v,sizeof(v),"%04d",cat_clamp(ms,20,4000));
        return setMenuRaw(305, v);
    }
    static std::string getAmAgcMidDelay() { return getMenuRaw(305); }

    // AM AGC Slow Delay  (EX0306)
    static std::string setAmAgcSlowDelay(int ms) {
        char v[8]; snprintf(v,sizeof(v),"%04d",cat_clamp(ms,20,4000));
        return setMenuRaw(306, v);
    }
    static std::string getAmAgcSlowDelay() { return getMenuRaw(306); }

    // AM LCUT Freq  (EX0307)
    static std::string setAmLcutFreq(int idx) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(idx,0,20));
        return setMenuRaw(307, v);
    }
    static std::string getAmLcutFreq() { return getMenuRaw(307); }

    // AM LCUT Slope  (EX0308)
    static std::string setAmLcutSlope(int s) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(s,0,1));
        return setMenuRaw(308, v);
    }
    static std::string getAmLcutSlope() { return getMenuRaw(308); }

    // AM HCUT Freq  (EX0309)
    static std::string setAmHcutFreq(int idx) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(idx,0,67));
        return setMenuRaw(309, v);
    }
    static std::string getAmHcutFreq() { return getMenuRaw(309); }

    // AM HCUT Slope  (EX0310)
    static std::string setAmHcutSlope(int s) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(s,0,1));
        return setMenuRaw(310, v);
    }
    static std::string getAmHcutSlope() { return getMenuRaw(310); }

    // AM Mod Source  (EX0311)
    static std::string setAmModSource(int src) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(src,0,2));
        return setMenuRaw(311, v);
    }
    static std::string getAmModSource() { return getMenuRaw(311); }

    // AM USB Mod Gain  (EX0312)
    static std::string setAmUsbModGain(int gain) {
        char v[4]; snprintf(v,sizeof(v),"%03d",cat_clamp(gain,0,100));
        return setMenuRaw(312, v);
    }
    static std::string getAmUsbModGain() { return getMenuRaw(312); }

    // AM RPTT Select  (EX0313)
    static std::string setAmRpttSelect(int sel) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(sel,0,2));
        return setMenuRaw(313, v);
    }
    static std::string getAmRpttSelect() { return getMenuRaw(313); }

    // ─── MODE FM (pages 15-16) ──────────────────────────────────────────

    // FM AF Treble Gain  (EX0401)
    static std::string setFmAfTrebleGain(int db) {
        db=cat_clamp(db,-20,10);
        char v[4]; snprintf(v,sizeof(v),"%+03d",db);
        return setMenuRaw(401, v);
    }
    static std::string getFmAfTrebleGain() { return getMenuRaw(401); }

    // FM AF Middle Tone Gain  (EX0402)
    static std::string setFmAfMiddleToneGain(int db) {
        db=cat_clamp(db,-20,10);
        char v[4]; snprintf(v,sizeof(v),"%+03d",db);
        return setMenuRaw(402, v);
    }
    static std::string getFmAfMiddleToneGain() { return getMenuRaw(402); }

    // FM AF Bass Gain  (EX0403)
    static std::string setFmAfBassGain(int db) {
        db=cat_clamp(db,-20,10);
        char v[4]; snprintf(v,sizeof(v),"%+03d",db);
        return setMenuRaw(403, v);
    }
    static std::string getFmAfBassGain() { return getMenuRaw(403); }

    // FM AGC Fast Delay  (EX0404)
    static std::string setFmAgcFastDelay(int ms) {
        char v[8]; snprintf(v,sizeof(v),"%04d",cat_clamp(ms,20,4000));
        return setMenuRaw(404, v);
    }
    static std::string getFmAgcFastDelay() { return getMenuRaw(404); }

    // FM AGC Mid Delay  (EX0405)
    static std::string setFmAgcMidDelay(int ms) {
        char v[8]; snprintf(v,sizeof(v),"%04d",cat_clamp(ms,20,4000));
        return setMenuRaw(405, v);
    }
    static std::string getFmAgcMidDelay() { return getMenuRaw(405); }

    // FM AGC Slow Delay  (EX0406)
    static std::string setFmAgcSlowDelay(int ms) {
        char v[8]; snprintf(v,sizeof(v),"%04d",cat_clamp(ms,20,4000));
        return setMenuRaw(406, v);
    }
    static std::string getFmAgcSlowDelay() { return getMenuRaw(406); }

    // FM LCUT Freq  (EX0407)
    static std::string setFmLcutFreq(int idx) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(idx,0,20));
        return setMenuRaw(407, v);
    }
    static std::string getFmLcutFreq() { return getMenuRaw(407); }

    // FM LCUT Slope  (EX0408)
    static std::string setFmLcutSlope(int s) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(s,0,1));
        return setMenuRaw(408, v);
    }
    static std::string getFmLcutSlope() { return getMenuRaw(408); }

    // FM HCUT Freq  (EX0409)
    static std::string setFmHcutFreq(int idx) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(idx,0,67));
        return setMenuRaw(409, v);
    }
    static std::string getFmHcutFreq() { return getMenuRaw(409); }

    // FM HCUT Slope  (EX0410)
    static std::string setFmHcutSlope(int s) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(s,0,1));
        return setMenuRaw(410, v);
    }
    static std::string getFmHcutSlope() { return getMenuRaw(410); }

    // FM Mod Source  (EX0411)
    static std::string setFmModSource(int src) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(src,0,2));
        return setMenuRaw(411, v);
    }
    static std::string getFmModSource() { return getMenuRaw(411); }

    // FM USB Mod Gain  (EX0412)
    static std::string setFmUsbModGain(int gain) {
        char v[4]; snprintf(v,sizeof(v),"%03d",cat_clamp(gain,0,100));
        return setMenuRaw(412, v);
    }
    static std::string getFmUsbModGain() { return getMenuRaw(412); }

    // FM RPTT Select  (EX0413)
    static std::string setFmRpttSelect(int sel) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(sel,0,2));
        return setMenuRaw(413, v);
    }
    static std::string getFmRpttSelect() { return getMenuRaw(413); }

    // FM RPT Shift (VHF/UHF offset direction): 0=SIMPLEX 1=MINUS 2=PLUS  (EX0414)
    static std::string setFmRptShift(int dir) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(dir,0,2));
        return setMenuRaw(414, v);
    }
    static std::string getFmRptShift() { return getMenuRaw(414); }

    // FM RPT Shift Freq (144MHz): Hz  (EX0415)
    static std::string setFmRptShiftFreq144(int hz) {
        char v[8]; snprintf(v,sizeof(v),"%07d",cat_clamp(hz,0,9999999));
        return setMenuRaw(415, v);
    }
    static std::string getFmRptShiftFreq144() { return getMenuRaw(415); }

    // FM RPT Shift Freq (430MHz): Hz  (EX0416)
    static std::string setFmRptShiftFreq430(int hz) {
        char v[8]; snprintf(v,sizeof(v),"%07d",cat_clamp(hz,0,9999999));
        return setMenuRaw(416, v);
    }
    static std::string getFmRptShiftFreq430() { return getMenuRaw(416); }

    // ─── MODE DATA (pages 17-18) ─────────────────────────────────────────

    // DATA AF Treble Gain  (EX0501)
    static std::string setDataAfTrebleGain(int db) {
        db=cat_clamp(db,-20,10);
        char v[4]; snprintf(v,sizeof(v),"%+03d",db);
        return setMenuRaw(501, v);
    }
    static std::string getDataAfTrebleGain() { return getMenuRaw(501); }

    // DATA AF Middle Tone Gain  (EX0502)
    static std::string setDataAfMiddleToneGain(int db) {
        db=cat_clamp(db,-20,10);
        char v[4]; snprintf(v,sizeof(v),"%+03d",db);
        return setMenuRaw(502, v);
    }
    static std::string getDataAfMiddleToneGain() { return getMenuRaw(502); }

    // DATA AF Bass Gain  (EX0503)
    static std::string setDataAfBassGain(int db) {
        db=cat_clamp(db,-20,10);
        char v[4]; snprintf(v,sizeof(v),"%+03d",db);
        return setMenuRaw(503, v);
    }
    static std::string getDataAfBassGain() { return getMenuRaw(503); }

    // DATA AGC Fast Delay  (EX0504)
    static std::string setDataAgcFastDelay(int ms) {
        char v[8]; snprintf(v,sizeof(v),"%04d",cat_clamp(ms,20,4000));
        return setMenuRaw(504, v);
    }
    static std::string getDataAgcFastDelay() { return getMenuRaw(504); }

    // DATA AGC Mid Delay  (EX0505)
    static std::string setDataAgcMidDelay(int ms) {
        char v[8]; snprintf(v,sizeof(v),"%04d",cat_clamp(ms,20,4000));
        return setMenuRaw(505, v);
    }
    static std::string getDataAgcMidDelay() { return getMenuRaw(505); }

    // DATA AGC Slow Delay  (EX0506)
    static std::string setDataAgcSlowDelay(int ms) {
        char v[8]; snprintf(v,sizeof(v),"%04d",cat_clamp(ms,20,4000));
        return setMenuRaw(506, v);
    }
    static std::string getDataAgcSlowDelay() { return getMenuRaw(506); }

    // DATA LCUT Freq  (EX0507)
    static std::string setDataLcutFreq(int idx) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(idx,0,20));
        return setMenuRaw(507, v);
    }
    static std::string getDataLcutFreq() { return getMenuRaw(507); }

    // DATA LCUT Slope  (EX0508)
    static std::string setDataLcutSlope(int s) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(s,0,1));
        return setMenuRaw(508, v);
    }
    static std::string getDataLcutSlope() { return getMenuRaw(508); }

    // DATA HCUT Freq  (EX0509)
    static std::string setDataHcutFreq(int idx) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(idx,0,67));
        return setMenuRaw(509, v);
    }
    static std::string getDataHcutFreq() { return getMenuRaw(509); }

    // DATA HCUT Slope  (EX0510)
    static std::string setDataHcutSlope(int s) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(s,0,1));
        return setMenuRaw(510, v);
    }
    static std::string getDataHcutSlope() { return getMenuRaw(510); }

    // DATA USB Out Level  (EX0511)
    static std::string setDataUsbOutLevel(int level) {
        char v[4]; snprintf(v,sizeof(v),"%03d",cat_clamp(level,0,100));
        return setMenuRaw(511, v);
    }
    static std::string getDataUsbOutLevel() { return getMenuRaw(511); }

    // DATA TX BPF Sel  (EX0512)
    static std::string setDataTxBpfSel(int sel) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(sel,0,4));
        return setMenuRaw(512, v);
    }
    static std::string getDataTxBpfSel() { return getMenuRaw(512); }

    // DATA Mod Source  (EX0513)
    static std::string setDataModSource(int src) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(src,0,2));
        return setMenuRaw(513, v);
    }
    static std::string getDataModSource() { return getMenuRaw(513); }

    // DATA USB Mod Gain  (EX0514)
    static std::string setDataUsbModGain(int gain) {
        char v[4]; snprintf(v,sizeof(v),"%03d",cat_clamp(gain,0,100));
        return setMenuRaw(514, v);
    }
    static std::string getDataUsbModGain() { return getMenuRaw(514); }

    // DATA RPTT Select  (EX0515)
    static std::string setDataRpttSelect(int sel) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(sel,0,2));
        return setMenuRaw(515, v);
    }
    static std::string getDataRpttSelect() { return getMenuRaw(515); }

    // DATA NAR Width  (EX0516)
    static std::string setDataNarWidth(int w) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(w,0,2));
        return setMenuRaw(516, v);
    }
    static std::string getDataNarWidth() { return getMenuRaw(516); }

    // DATA PSK Tone  (EX0517)
    static std::string setDataPskTone(int tone) {
        char v[4]; snprintf(v,sizeof(v),"%03d",cat_clamp(tone,0,255));
        return setMenuRaw(517, v);
    }
    static std::string getDataPskTone() { return getMenuRaw(517); }

    // DATA Shift (SSB): -3000..+3000 Hz  (EX0518)
    static std::string setDataShiftSsb(int hz) {
        hz=cat_clamp(hz,-3000,3000);
        char sign=hz>=0?'+':'-';
        char v[8]; snprintf(v,sizeof(v),"%c%04d",sign,std::abs(hz));
        return setMenuRaw(518, v);
    }
    static std::string getDataShiftSsb() { return getMenuRaw(518); }

    // ─── MODE RTTY (pages 19-20) ─────────────────────────────────────────

    // RTTY AF Treble Gain  (EX0601)
    static std::string setRttyAfTrebleGain(int db) {
        db=cat_clamp(db,-20,10);
        char v[4]; snprintf(v,sizeof(v),"%+03d",db);
        return setMenuRaw(601, v);
    }
    static std::string getRttyAfTrebleGain() { return getMenuRaw(601); }

    // RTTY AF Middle Tone Gain  (EX0602)
    static std::string setRttyAfMiddleToneGain(int db) {
        db=cat_clamp(db,-20,10);
        char v[4]; snprintf(v,sizeof(v),"%+03d",db);
        return setMenuRaw(602, v);
    }
    static std::string getRttyAfMiddleToneGain() { return getMenuRaw(602); }

    // RTTY AF Bass Gain  (EX0603)
    static std::string setRttyAfBassGain(int db) {
        db=cat_clamp(db,-20,10);
        char v[4]; snprintf(v,sizeof(v),"%+03d",db);
        return setMenuRaw(603, v);
    }
    static std::string getRttyAfBassGain() { return getMenuRaw(603); }

    // RTTY AGC Fast Delay  (EX0604)
    static std::string setRttyAgcFastDelay(int ms) {
        char v[8]; snprintf(v,sizeof(v),"%04d",cat_clamp(ms,20,4000));
        return setMenuRaw(604, v);
    }
    static std::string getRttyAgcFastDelay() { return getMenuRaw(604); }

    // RTTY AGC Mid Delay  (EX0605)
    static std::string setRttyAgcMidDelay(int ms) {
        char v[8]; snprintf(v,sizeof(v),"%04d",cat_clamp(ms,20,4000));
        return setMenuRaw(605, v);
    }
    static std::string getRttyAgcMidDelay() { return getMenuRaw(605); }

    // RTTY AGC Slow Delay  (EX0606)
    static std::string setRttyAgcSlowDelay(int ms) {
        char v[8]; snprintf(v,sizeof(v),"%04d",cat_clamp(ms,20,4000));
        return setMenuRaw(606, v);
    }
    static std::string getRttyAgcSlowDelay() { return getMenuRaw(606); }

    // RTTY LCUT Freq  (EX0607)
    static std::string setRttyLcutFreq(int idx) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(idx,0,20));
        return setMenuRaw(607, v);
    }
    static std::string getRttyLcutFreq() { return getMenuRaw(607); }

    // RTTY LCUT Slope  (EX0608)
    static std::string setRttyLcutSlope(int s) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(s,0,1));
        return setMenuRaw(608, v);
    }
    static std::string getRttyLcutSlope() { return getMenuRaw(608); }

    // RTTY HCUT Freq  (EX0609)
    static std::string setRttyHcutFreq(int idx) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(idx,0,67));
        return setMenuRaw(609, v);
    }
    static std::string getRttyHcutFreq() { return getMenuRaw(609); }

    // RTTY HCUT Slope  (EX0610)
    static std::string setRttyHcutSlope(int s) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(s,0,1));
        return setMenuRaw(610, v);
    }
    static std::string getRttyHcutSlope() { return getMenuRaw(610); }

    // RTTY RPTT Select  (EX0611)
    static std::string setRttyRpttSelect(int sel) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(sel,0,2));
        return setMenuRaw(611, v);
    }
    static std::string getRttyRpttSelect() { return getMenuRaw(611); }

    // RTTY Mark Frequency: 0=1275Hz, 1=2125Hz  (EX0612)
    static std::string setRttyMarkFrequency(int f) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(f,0,1));
        return setMenuRaw(612, v);
    }
    static std::string getRttyMarkFrequency() { return getMenuRaw(612); }

    // RTTY Shift Frequency: 0=170Hz 1=200Hz 2=425Hz 3=850Hz  (EX0613)
    static std::string setRttyShiftFrequency(int idx) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(idx,0,3));
        return setMenuRaw(613, v);
    }
    static std::string getRttyShiftFrequency() { return getMenuRaw(613); }

    // RTTY Polarity TX: 0=NOR, 1=REV  (EX0614)
    static std::string setRttyPolarityTx(int pol) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(pol,0,1));
        return setMenuRaw(614, v);
    }
    static std::string getRttyPolarityTx() { return getMenuRaw(614); }

    // RTTY Polarity RX: 0=NOR, 1=REV  (EX0615)
    static std::string setRttyPolarityRx(int pol) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(pol,0,1));
        return setMenuRaw(615, v);
    }
    static std::string getRttyPolarityRx() { return getMenuRaw(615); }

    // ─── TX AUDIO / PARAMETRIC EQ — PROC OFF (pages 21-22) ──────────────

    // PRMTRC EQ1 FREQ (proc OFF): 0-20 → 100-3500Hz table  (EX0701)
    static std::string setPrmtrcEq1Freq(int idx) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(idx,0,20));
        return setMenuRaw(701, v);
    }
    static std::string getPrmtrcEq1Freq() { return getMenuRaw(701); }

    // PRMTRC EQ1 LEVEL (proc OFF): -20..+10 dB  (EX0702)
    static std::string setPrmtrcEq1Level(int db) {
        db=cat_clamp(db,-20,10);
        char v[4]; snprintf(v,sizeof(v),"%+03d",db);
        return setMenuRaw(702, v);
    }
    static std::string getPrmtrcEq1Level() { return getMenuRaw(702); }

    // PRMTRC EQ1 BWTH (proc OFF): 1-10  (EX0703)
    static std::string setPrmtrcEq1Bwth(int bw) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(bw,1,10));
        return setMenuRaw(703, v);
    }
    static std::string getPrmtrcEq1Bwth() { return getMenuRaw(703); }

    // PRMTRC EQ2 FREQ (proc OFF)  (EX0704)
    static std::string setPrmtrcEq2Freq(int idx) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(idx,0,20));
        return setMenuRaw(704, v);
    }
    static std::string getPrmtrcEq2Freq() { return getMenuRaw(704); }

    // PRMTRC EQ2 LEVEL (proc OFF)  (EX0705)
    static std::string setPrmtrcEq2Level(int db) {
        db=cat_clamp(db,-20,10);
        char v[4]; snprintf(v,sizeof(v),"%+03d",db);
        return setMenuRaw(705, v);
    }
    static std::string getPrmtrcEq2Level() { return getMenuRaw(705); }

    // PRMTRC EQ2 BWTH (proc OFF)  (EX0706)
    static std::string setPrmtrcEq2Bwth(int bw) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(bw,1,10));
        return setMenuRaw(706, v);
    }
    static std::string getPrmtrcEq2Bwth() { return getMenuRaw(706); }

    // PRMTRC EQ3 FREQ (proc OFF)  (EX0707)
    static std::string setPrmtrcEq3Freq(int idx) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(idx,0,20));
        return setMenuRaw(707, v);
    }
    static std::string getPrmtrcEq3Freq() { return getMenuRaw(707); }

    // PRMTRC EQ3 LEVEL (proc OFF)  (EX0708)
    static std::string setPrmtrcEq3Level(int db) {
        db=cat_clamp(db,-20,10);
        char v[4]; snprintf(v,sizeof(v),"%+03d",db);
        return setMenuRaw(708, v);
    }
    static std::string getPrmtrcEq3Level() { return getMenuRaw(708); }

    // PRMTRC EQ3 BWTH (proc OFF)  (EX0709)
    static std::string setPrmtrcEq3Bwth(int bw) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(bw,1,10));
        return setMenuRaw(709, v);
    }
    static std::string getPrmtrcEq3Bwth() { return getMenuRaw(709); }

    // ─── TX AUDIO / PARAMETRIC EQ — PROC ON (pages 23-24) ───────────────

    // P PRMTRC EQ1 FREQ (proc ON)  (EX0801)
    static std::string setPPrmtrcEq1Freq(int idx) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(idx,0,20));
        return setMenuRaw(801, v);
    }
    static std::string getPPrmtrcEq1Freq() { return getMenuRaw(801); }

    // P PRMTRC EQ1 LEVEL (proc ON): -20..+10 dB  (EX0802)
    static std::string setPPrmtrcEq1Level(int db) {
        db=cat_clamp(db,-20,10);
        char v[4]; snprintf(v,sizeof(v),"%+03d",db);
        return setMenuRaw(802, v);
    }
    static std::string getPPrmtrcEq1Level() { return getMenuRaw(802); }

    // P PRMTRC EQ1 BWTH (proc ON): 1-10  (EX0803)
    static std::string setPPrmtrcEq1Bwth(int bw) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(bw,1,10));
        return setMenuRaw(803, v);
    }
    static std::string getPPrmtrcEq1Bwth() { return getMenuRaw(803); }

    // P PRMTRC EQ2 FREQ (proc ON)  (EX0804)
    static std::string setPPrmtrcEq2Freq(int idx) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(idx,0,20));
        return setMenuRaw(804, v);
    }
    static std::string getPPrmtrcEq2Freq() { return getMenuRaw(804); }

    // P PRMTRC EQ2 LEVEL (proc ON)  (EX0805)
    static std::string setPPrmtrcEq2Level(int db) {
        db=cat_clamp(db,-20,10);
        char v[4]; snprintf(v,sizeof(v),"%+03d",db);
        return setMenuRaw(805, v);
    }
    static std::string getPPrmtrcEq2Level() { return getMenuRaw(805); }

    // P PRMTRC EQ2 BWTH (proc ON)  (EX0806)
    static std::string setPPrmtrcEq2Bwth(int bw) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(bw,1,10));
        return setMenuRaw(806, v);
    }
    static std::string getPPrmtrcEq2Bwth() { return getMenuRaw(806); }

    // P PRMTRC EQ3 FREQ (proc ON)  (EX0807)
    static std::string setPPrmtrcEq3Freq(int idx) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(idx,0,20));
        return setMenuRaw(807, v);
    }
    static std::string getPPrmtrcEq3Freq() { return getMenuRaw(807); }

    // P PRMTRC EQ3 LEVEL (proc ON)  (EX0808)
    static std::string setPPrmtrcEq3Level(int db) {
        db=cat_clamp(db,-20,10);
        char v[4]; snprintf(v,sizeof(v),"%+03d",db);
        return setMenuRaw(808, v);
    }
    static std::string getPPrmtrcEq3Level() { return getMenuRaw(808); }

    // P PRMTRC EQ3 BWTH (proc ON)  (EX0809)
    static std::string setPPrmtrcEq3Bwth(int bw) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(bw,1,10));
        return setMenuRaw(809, v);
    }
    static std::string getPPrmtrcEq3Bwth() { return getMenuRaw(809); }

    // ─── TX GENERAL (page 25) ────────────────────────────────────────────

    // TX GNRL Max Power (battery): 0.5-6.0W in 0.5W steps  (EX0901)
    // value: 1-12 where 1=0.5W, 12=6.0W
    static std::string setTxGnrlMaxPowerBattery(int step) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(step,1,12));
        return setMenuRaw(901, v);
    }
    static std::string getTxGnrlMaxPowerBattery() { return getMenuRaw(901); }

    // ─── RX DSP (page 26) ────────────────────────────────────────────────

    // NB Level: 1-10  (EX1001)
    static std::string setNbLevel(int level) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(level,1,10));
        return setMenuRaw(1001, v);
    }
    static std::string getNbLevel() { return getMenuRaw(1001); }

    // NB Width: 1-9  (EX1002)
    static std::string setNbWidth(int width) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(width,1,9));
        return setMenuRaw(1002, v);
    }
    static std::string getNbWidth() { return getMenuRaw(1002); }

    // NR Level: 1-15  (EX1003)
    static std::string setNrLevel(int level) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(level,1,15));
        return setMenuRaw(1003, v);
    }
    static std::string getNrLevel() { return getMenuRaw(1003); }

    // Contour Level: -40..+20 dB  (EX1004)
    static std::string setContourLevel(int db) {
        db=cat_clamp(db,-40,20);
        char v[4]; snprintf(v,sizeof(v),"%+03d",db);
        return setMenuRaw(1004, v);
    }
    static std::string getContourLevel() { return getMenuRaw(1004); }

    // Contour Width (Q): 1-11  (EX1005)
    static std::string setContourWidth(int q) {
        char v[4]; snprintf(v,sizeof(v),"%02d",cat_clamp(q,1,11));
        return setMenuRaw(1005, v);
    }
    static std::string getContourWidth() { return getMenuRaw(1005); }

    // IF Notch Width: 0=NARROW, 1=WIDE  (EX1006)
    static std::string setIfNotchWidth(int w) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(w,0,1));
        return setMenuRaw(1006, v);
    }
    static std::string getIfNotchWidth() { return getMenuRaw(1006); }

    // APF Width: 0=NARROW 1=MEDIUM 2=WIDE  (EX1007)
    static std::string setApfWidth(int w) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(w,0,2));
        return setMenuRaw(1007, v);
    }
    static std::string getApfWidth() { return getMenuRaw(1007); }

    // ─── VOX SELECT / KEY / DIAL (pages 26-27) ───────────────────────────

    // VOX Select: 0=MIC, 1=DATA  (EX1101)
    static std::string setVoxSelect(int sel) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(sel,0,1));
        return setMenuRaw(1101, v);
    }
    static std::string getVoxSelect() { return getMenuRaw(1101); }

    // Emergency Freq TX: 0=OFF, 1=ON  (EX1102)
    static std::string setEmergencyFreqTx(bool on) {
        return setMenuRaw(1102, on?"1":"0");
    }
    static std::string getEmergencyFreqTx() { return getMenuRaw(1102); }

    // Mic Up: 0=UP, 1=DOWN (key assignment)  (EX1201)
    static std::string setMicUpAssign(int dir) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(dir,0,1));
        return setMenuRaw(1201, v);
    }
    static std::string getMicUpAssign() { return getMenuRaw(1201); }

    // Dial Step (VFO): 0=1Hz 1=10Hz 2=20Hz 3=100Hz 4=500Hz 5=1kHz  (EX1301)
    static std::string setDialStep(int step) {
        char v[2]; snprintf(v,sizeof(v),"%d",cat_clamp(step,0,5));
        return setMenuRaw(1301, v);
    }
    static std::string getDialStep() { return getMenuRaw(1301); }

    // ====================================================================
    // SECTION 3 — RESPONSE PARSING HELPERS
    // ====================================================================

    // Parse RM response: "RM[id][6-digit raw];"  →  raw 0-255
    static int parseReadMeter(const std::string& r, int& meterId) {
        if (r.size() < 9 || r.substr(0,2) != "RM") { meterId=-1; return 0; }
        meterId = r[2]-'0';
        try { return std::min(255, std::stoi(r.substr(3,6))); } catch(...) { return 0; }
    }

    // Parse FA/FB frequency response  →  Hz
    static long long parseFrequency(const std::string& r) {
        if (r.size() < 11) return 0;
        try { return std::stoll(r.substr(2,9)); } catch(...) { return 0; }
    }

    // Parse MD mode response: "MD[vfo][mode];"  →  mode 0-9
    static int parseMode(const std::string& r, int& vfo) {
        if (r.size() < 4 || r.substr(0,2) != "MD") { vfo=-1; return -1; }
        vfo  = r[2]-'0';
        return r[3]-'0';
    }

    // Parse TX response: "TX[0|1|2];"
    static bool parseTx(const std::string& r) {
        return r.size() >= 3 && r.substr(0,2) == "TX" && r[2] != '0';
    }

    // Map mode index to name
    static const char* modeName(int idx) {
        static const char* names[] = {
            "LSB","USB","CW","CW-R","AM","FM","DATA-L","DATA-U","DATA-FM","C4FM"
        };
        if (idx < 0 || idx > 9) return "?";
        return names[idx];
    }
};
