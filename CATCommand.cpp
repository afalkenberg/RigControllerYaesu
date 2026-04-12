/**
 * CATCommand.cpp
 * ==============
 * Implementation of all CATCommand static methods.
 *
 * Parameter ranges and command formats match the
 * FTX-1 CAT Operation Reference Manual (2025).
 */

#include "CATCommand.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>

// ============================================================================
//  SECTION 1 — CORE COMMANDS
// ============================================================================

std::string CATCommand::setMainSideToSubSide()          { return "AB;"; }

std::string CATCommand::setAntennaTunerControl(int p1)  { char b[8];  snprintf(b,sizeof(b),"AC0%d0;",cat_clamp(p1,0,2)); return b; }
std::string CATCommand::getAntennaTunerControl()        { return "AC;"; }

std::string CATCommand::setAfGain(int vfo, int level)   { char b[12]; snprintf(b,sizeof(b),"AG%d%03d;",cat_clamp(vfo,0,1),cat_clamp(level,0,255)); return b; }
std::string CATCommand::getAfGain(int vfo)              { char b[6];  snprintf(b,sizeof(b),"AG%d;",cat_clamp(vfo,0,1)); return b; }

std::string CATCommand::setAutoInformation(int mode)    { char b[6];  snprintf(b,sizeof(b),"AI%d;",cat_clamp(mode,0,2)); return b; }
std::string CATCommand::getAutoInformation()            { return "AI;"; }

std::string CATCommand::setMainSideToMemoryChannel()    { return "AM;"; }

std::string CATCommand::setAmcOutputLevel(int level)    { char b[8];  snprintf(b,sizeof(b),"AO%03d;",cat_clamp(level,0,100)); return b; }
std::string CATCommand::getAmcOutputLevel()             { return "AO;"; }

std::string CATCommand::setSubSideToMainSide()          { return "BA;"; }

std::string CATCommand::setAutoNotch(int vfo, bool on)  { char b[8];  snprintf(b,sizeof(b),"BC%d%d;",cat_clamp(vfo,0,1),on?1:0); return b; }
std::string CATCommand::getAutoNotch(int vfo)           { char b[6];  snprintf(b,sizeof(b),"BC%d;",cat_clamp(vfo,0,1)); return b; }

std::string CATCommand::setBandDown(int vfo)            { char b[6];  snprintf(b, sizeof(b), "BD%d;", cat_clamp(vfo, 0, 1)); return b; }

std::string CATCommand::setBreakIn(int mode)            { char b[6];  snprintf(b,sizeof(b),"BI%d;",cat_clamp(mode,0,2)); return b; }
std::string CATCommand::getBreakIn()                    { return "BI;"; }

std::string CATCommand::setSubSideToMemoryChannel()     { return "BM;"; }

std::string CATCommand::setManualNotch(bool on, int freqHz) {
    if (!on) return "BP00000;";
    char b[12]; snprintf(b,sizeof(b),"BP1%04d;",cat_clamp(freqHz,0,4000)); return b;
}
std::string CATCommand::getManualNotch()                { return "BP;"; }

std::string CATCommand::setBandSelect(int band)         { char b[8];  snprintf(b,sizeof(b),"BS%02d;",cat_clamp(band,0,12)); return b; }

std::string CATCommand::setBandUp(int vfo)              { char b[6];  snprintf(b, sizeof(b), "BU%d;", cat_clamp(vfo, 0, 1)); return b; }

std::string CATCommand::setClarifier(int vfo, bool on, int offsetHz) {
    offsetHz = cat_clamp(offsetHz,-9999,9999);
    char sign = offsetHz>=0?'+':'-';
    char b[16]; snprintf(b,sizeof(b),"CF%d%d%c%04d;",cat_clamp(vfo,0,1),on?1:0,sign,std::abs(offsetHz));
    return b;
}
std::string CATCommand::getClarifier(int vfo)           { char b[6];  snprintf(b,sizeof(b),"CF%d;",cat_clamp(vfo,0,1)); return b; }

std::string CATCommand::setChannelUpDown(int dir)       { char b[6];  snprintf(b,sizeof(b),"CH%d;",cat_clamp(dir,0,1)); return b; }

std::string CATCommand::setCtcssNumber(int vfo, int tone){ char b[10]; snprintf(b,sizeof(b),"CN%d%02d;",cat_clamp(vfo,0,1),cat_clamp(tone,0,49)); return b; }
std::string CATCommand::getCtcssNumber(int vfo)         { char b[6];  snprintf(b,sizeof(b),"CN%d;",cat_clamp(vfo,0,1)); return b; }

std::string CATCommand::setContour(int levelDb, int width) {
    levelDb = cat_clamp(levelDb,-40,20); width = cat_clamp(width,1,11);
    char sign = levelDb>=0?'+':'-';
    char b[12]; snprintf(b,sizeof(b),"CO%c%02d%02d;",sign,std::abs(levelDb),width); return b;
}
std::string CATCommand::getContour()                    { return "CO;"; }

std::string CATCommand::setCwSpot(bool on)              { return on ? "CS1;" : "CS0;"; }
std::string CATCommand::getCwSpot()                     { return "CS;"; }

std::string CATCommand::setCtcss(int vfo, int mode)     { char b[8];  snprintf(b,sizeof(b),"CT%d%d;",cat_clamp(vfo,0,1),cat_clamp(mode,0,2)); return b; }
std::string CATCommand::getCtcss(int vfo)               { char b[6];  snprintf(b,sizeof(b),"CT%d;",cat_clamp(vfo,0,1)); return b; }

std::string CATCommand::setLcdDimmer(int type, int level){ char b[10]; snprintf(b,sizeof(b),"DA%d%03d;",cat_clamp(type,0,1),cat_clamp(level,0,100)); return b; }
std::string CATCommand::getLcdDimmer(int type)          { char b[6];  snprintf(b,sizeof(b),"DA%d;",cat_clamp(type,0,1)); return b; }

std::string CATCommand::setDownKey()                    { return "DN;"; }

std::string CATCommand::setDateTime(const std::string& dt14) { return "DT" + dt14.substr(0,14) + ";"; }
std::string CATCommand::getDateTime()                   { return "DT;"; }

std::string CATCommand::setMenuRaw(int menuNum, const std::string& value) {
    char b[16]; snprintf(b,sizeof(b),"EX%04d",menuNum);
    return std::string(b) + value + ";";
}
std::string CATCommand::getMenuRaw(int menuNum) {
    char b[12]; snprintf(b,sizeof(b),"EX%04d;",menuNum); return b;
}

std::string CATCommand::setMainSideFrequency(long long freqHz) { char b[16]; snprintf(b,sizeof(b),"FA%09lld;",freqHz); return b; }
std::string CATCommand::getMainSideFrequency()          { return "FA;"; }

std::string CATCommand::setSubSideFrequency(long long freqHz)  { char b[16]; snprintf(b,sizeof(b),"FB%09lld;",freqHz); return b; }
std::string CATCommand::getSubSideFrequency()           { return "FB;"; }

std::string CATCommand::setFineTuning(int mode) { char b[8];  snprintf(b,sizeof(b),"FN%d;", cat_clamp(mode,0,2)); return b; }
std::string CATCommand::getFineTuning()        { return "FN;"; }

std::string CATCommand::setFunctionRx(int mode)         { char b[6];  snprintf(b,sizeof(b),"FR%d;",cat_clamp(mode,0,1)); return b; }
std::string CATCommand::getFunctionRx()                 { return "FR;"; }

std::string CATCommand::setFunctionTx(int vfo)          { char b[6];  snprintf(b,sizeof(b),"FT%d;",cat_clamp(vfo,0,1)); return b; }
std::string CATCommand::getFunctionTx()                 { return "FT;"; }

std::string CATCommand::setGpOut(int port, int level)   { char b[8];  snprintf(b,sizeof(b),"GP%d%d;",cat_clamp(port,0,3),cat_clamp(level,0,1)); return b; }
std::string CATCommand::getGpOut(int port)              { char b[6];  snprintf(b,sizeof(b),"GP%d;",cat_clamp(port,0,3)); return b; }

std::string CATCommand::setAgcFunction(int vfo, int mode){ char b[10]; snprintf(b,sizeof(b),"GT0%d%d;",cat_clamp(vfo,0,1),cat_clamp(mode,0,4)); return b; }
std::string CATCommand::getAgcFunction(int vfo)         { char b[8];  snprintf(b,sizeof(b),"GT0%d;",cat_clamp(vfo,0,1)); return b; }

std::string CATCommand::getRadioId()                    { return "ID;"; }
std::string CATCommand::getInformation()                { return "IF;"; }

std::string CATCommand::setIfShift(int vfo, int offsetHz) {
    offsetHz = cat_clamp(offsetHz,-1000,1000);
    char sign = offsetHz>=0?'+':'-';
    char b[14]; snprintf(b,sizeof(b),"IS%d%c%04d;",cat_clamp(vfo,0,1),sign,std::abs(offsetHz));
    return b;
}
std::string CATCommand::getIfShift(int vfo)             { char b[6];  snprintf(b,sizeof(b),"IS%d;",cat_clamp(vfo,0,1)); return b; }

std::string CATCommand::setKeyerMemory(int ch, const std::string& text) {
    char b[8]; snprintf(b,sizeof(b),"KM%d",cat_clamp(ch,1,5));
    return std::string(b) + text + ";";
}
std::string CATCommand::getKeyerMemory(int ch)          { char b[6];  snprintf(b,sizeof(b),"KM%d;",cat_clamp(ch,1,5)); return b; }

std::string CATCommand::setKeyPitch(int pitchHz)        { char b[10]; snprintf(b,sizeof(b),"KP%04d;",cat_clamp(pitchHz,300,1050)); return b; }
std::string CATCommand::getKeyPitch()                   { return "KP;"; }

std::string CATCommand::setKeyer(int mode)              { char b[6];  snprintf(b,sizeof(b),"KR%d;",cat_clamp(mode,0,2)); return b; }
std::string CATCommand::getKeyer()                      { return "KR;"; }

std::string CATCommand::setKeySpeed(int wpm)            { char b[8];  snprintf(b,sizeof(b),"KS%03d;",cat_clamp(wpm,4,60)); return b; }
std::string CATCommand::getKeySpeed()                   { return "KS;"; }

std::string CATCommand::setCwKeyingMemoryPlay(int ch)   { char b[6];  snprintf(b,sizeof(b),"KY%d;",cat_clamp(ch,0,5)); return b; }
std::string CATCommand::setCwKeyingText(const std::string& text) { return "KY " + text + ";"; }

std::string CATCommand::setLock(bool on)                { char b[8];  snprintf(b,sizeof(b),"LK%d;",on?1:0); return b; }
std::string CATCommand::getLock()                       { return "LK;"; }

std::string CATCommand::setLoadMessage(int type, int ch){ char b[8];  snprintf(b,sizeof(b),"LM%d%d;",cat_clamp(type,0,1),cat_clamp(ch,1,5)); return b; }

std::string CATCommand::setMemoryChannelToMainSide()    { return "MA;"; }
std::string CATCommand::setMemoryChannelToSubSide()     { return "MB;"; }

std::string CATCommand::setMemoryChannel(int vfo, int ch){ char b[10]; snprintf(b,sizeof(b),"MC%d%03d;",cat_clamp(vfo,0,1),cat_clamp(ch,0,117)); return b; }
std::string CATCommand::getMemoryChannel(int vfo)       { char b[6];  snprintf(b,sizeof(b),"MC%d;",cat_clamp(vfo,0,1)); return b; }

// Mode code table — FTX-1 uses alphanumeric single-char codes
// 1=LSB 2=USB 3=CW-U 4=FM 5=AM 6=RTTY-L 7=CW-L 8=DATA-L
// 9=RTTY-U A=DATA-FM B=FM-N C=DATA-U D=AM-N E=PSK F=DATA-FM-N H=C4FM-DN I=C4FM-VW
static const struct { const char* name; const char* code; } MODE_TABLE[] = {
    {"LSB",      "1"}, {"USB",      "2"}, {"CW-U",     "3"}, {"CW",       "3"},
    {"FM",       "4"}, {"AM",       "5"}, {"RTTY-L",   "6"}, {"CW-L",     "7"},
    {"CWR",      "7"}, {"DATA-L",   "8"}, {"RTTY-U",   "9"}, {"DATA-FM",  "A"},
    {"FM-N",     "B"}, {"DATA-U",   "C"}, {"AM-N",     "D"}, {"PSK",      "E"},
    {"DATA-FM-N","F"}, {"C4FM-DN",  "H"}, {"C4FM-VW",  "I"}, {"C4FM",     "H"},
    {nullptr, nullptr}
};

std::string CATCommand::modeNameToCode(const std::string& name) {
    for (int i = 0; MODE_TABLE[i].name; i++)
        if (name == MODE_TABLE[i].name) return MODE_TABLE[i].code;
    return "2"; // default USB
}

std::string CATCommand::modeCodeToName(const std::string& code) {
    for (int i = 0; MODE_TABLE[i].name; i++)
        if (code == MODE_TABLE[i].code) return MODE_TABLE[i].name;
    return "?";
}

std::string CATCommand::setOperatingMode(int vfo, const std::string& modeCode) {
    char b[8]; snprintf(b,sizeof(b),"MD%d%s;",cat_clamp(vfo,0,1),modeCode.c_str());
    return b;
}
std::string CATCommand::setOperatingModeByName(int vfo, const std::string& name) {
    return setOperatingMode(vfo, modeNameToCode(name));
}
std::string CATCommand::getOperatingMode(int vfo) {
    char b[6]; snprintf(b,sizeof(b),"MD%d;",cat_clamp(vfo,0,1)); return b;
}

std::string CATCommand::setMicGain(int level)           { char b[8];  snprintf(b,sizeof(b),"MG%03d;",cat_clamp(level,0,100)); return b; }
std::string CATCommand::getMicGain()                    { return "MG;"; }

std::string CATCommand::setMonitorLevel(int level)      { char b[8];  snprintf(b,sizeof(b),"ML%03d;",cat_clamp(level,0,100)); return b; }
std::string CATCommand::getMonitorLevel()               { return "ML;"; }

std::string CATCommand::getMemoryChannelRead(int vfo, int ch){ char b[10]; snprintf(b,sizeof(b),"MR%d%03d;",cat_clamp(vfo,0,1),cat_clamp(ch,0,117)); return b; }

std::string CATCommand::setMeterSw(int meter)           { char b[6];  snprintf(b,sizeof(b),"MS%d;",cat_clamp(meter,1,8)); return b; }
std::string CATCommand::getMeterSw()                    { return "MS;"; }

std::string CATCommand::setNoiseBlanker(int vfo, bool on){ char b[8]; snprintf(b,sizeof(b),"NB%d%d;",cat_clamp(vfo,0,1),on?1:0); return b; }
std::string CATCommand::getNoiseBlanker(int vfo)        { char b[6];  snprintf(b,sizeof(b),"NB%d;",cat_clamp(vfo,0,1)); return b; }

std::string CATCommand::setNoiseReduction(int vfo, bool on){ char b[8]; snprintf(b,sizeof(b),"NR%d%d;",cat_clamp(vfo,0,1),on?1:0); return b; }
std::string CATCommand::getNoiseReduction(int vfo)      { char b[6];  snprintf(b,sizeof(b),"NR%d;",cat_clamp(vfo,0,1)); return b; }

std::string CATCommand::setPreamp(int band, int level)  { char b[8];  snprintf(b,sizeof(b),"PA%d%d;",cat_clamp(band,0,2),cat_clamp(level,0,2)); return b; }
std::string CATCommand::getPreamp(int band)             { char b[6];  snprintf(b,sizeof(b),"PA%d;",cat_clamp(band,0,2)); return b; }

std::string CATCommand::setPowerControl(int watts)      { char b[8];  snprintf(b,sizeof(b),"PC%03d;",cat_clamp(watts,0,100)); return b; }
std::string CATCommand::getPowerControl()               { return "PC;"; }

std::string CATCommand::setSpeechProcessor(bool on)     { return on ? "PR1;" : "PR0;"; }
std::string CATCommand::getSpeechProcessor()            { return "PR;"; }

std::string CATCommand::setAttenuator(bool on, int level) {
    if (!on) return "RA00;";
    char b[8]; snprintf(b,sizeof(b),"RA%02d;",cat_clamp(level+1,1,3)); return b;
}
std::string CATCommand::getAttenuator()                 { return "RA;"; }

std::string CATCommand::setClarClear()                  { return "RC;"; }
std::string CATCommand::clearRitXit()                   { return "RC;"; }

std::string CATCommand::setRitDown(int steps)           { char b[8];  snprintf(b,sizeof(b),"RD%04d;",cat_clamp(steps,0,9999)); return b; }

std::string CATCommand::getReadMeter(int meter)         { char b[6];  snprintf(b,sizeof(b),"RM%d;",cat_clamp(meter,1,8)); return b; }
std::string CATCommand::getSmeterMain()                 { return "RM1;"; }
std::string CATCommand::getSmeterSub()                  { return "RM2;"; }
std::string CATCommand::getComp()                       { return "RM3;"; }
std::string CATCommand::getAlc()                        { return "RM4;"; }
std::string CATCommand::getPowerOutput()                { return "RM5;"; }
std::string CATCommand::getSwr()                        { return "RM6;"; }
std::string CATCommand::getIdd()                        { return "RM7;"; }
std::string CATCommand::getVdd()                        { return "RM8;"; }

std::string CATCommand::setRit(bool on)                 { return on ? "RT1;" : "RT0;"; }
std::string CATCommand::getRit()                        { return "RT;"; }

std::string CATCommand::setRitUp(int steps)             { char b[8];  snprintf(b,sizeof(b),"RU%04d;",cat_clamp(steps,0,9999)); return b; }

std::string CATCommand::setRfGain(int level)            { char b[8];  snprintf(b,sizeof(b),"RG%03d;",cat_clamp(level,0,255)); return b; }
std::string CATCommand::getRfGain()                     { return "RG;"; }

std::string CATCommand::setScan(int mode)               { char b[6];  snprintf(b,sizeof(b),"SC%d;",cat_clamp(mode,0,5)); return b; }
std::string CATCommand::getScan()                       { return "SC;"; }

std::string CATCommand::setCwBreakInDelay(int ms)       { char b[8];  snprintf(b,sizeof(b),"SD%04d;",cat_clamp(ms,30,3000)); return b; }
std::string CATCommand::getCwBreakInDelay()             { return "SD;"; }

std::string CATCommand::setFuncKnobFunction(int func)   { char b[6];  snprintf(b,sizeof(b),"SF%d;",cat_clamp(func,0,9)); return b; }
std::string CATCommand::getFuncKnobFunction()           { return "SF;"; }

std::string CATCommand::setWidth(int vfo, int widthIdx) { char b[10]; snprintf(b,sizeof(b),"SH%d%02d;",cat_clamp(vfo,0,1),cat_clamp(widthIdx,0,99)); return b; }
std::string CATCommand::getWidth(int vfo)               { char b[6];  snprintf(b,sizeof(b),"SH%d;",cat_clamp(vfo,0,1)); return b; }

std::string CATCommand::getSmeterReading(int vfo)       { char b[6];  snprintf(b,sizeof(b),"SM%d;",cat_clamp(vfo,0,1)); return b; }

std::string CATCommand::setSplit(bool on)               { return on ? "SP1;" : "SP0;"; }
std::string CATCommand::getSplit()                      { return "SP;"; }

std::string CATCommand::setSquelchLevel(int vfo, int level){ char b[10]; snprintf(b,sizeof(b),"SQ%d%03d;",cat_clamp(vfo,0,1),cat_clamp(level,0,255)); return b; }
std::string CATCommand::getSquelchLevel(int vfo)        { char b[6];  snprintf(b,sizeof(b),"SQ%d;",cat_clamp(vfo,0,1)); return b; }

std::string CATCommand::setSpectrumScope(int mode)      { char b[6];  snprintf(b,sizeof(b),"SS%d;",cat_clamp(mode,0,4)); return b; }
std::string CATCommand::getSpectrumScope()              { return "SS;"; }

std::string CATCommand::setSplitSt(bool on)             { return on ? "ST1;" : "ST0;"; }
std::string CATCommand::getSplitSt()                    { return "ST;"; }

std::string CATCommand::setSwapVfo()                    { return "SV;"; }

std::string CATCommand::setTxw(bool on)                 { return on ? "TS1;" : "TS0;"; }
std::string CATCommand::getTxw()                        { return "TS;"; }

std::string CATCommand::setTx(int mode)                 { char b[6];  snprintf(b,sizeof(b),"TX%d;",cat_clamp(mode,0,2)); return b; }
std::string CATCommand::setRx()                         { return "TX0;"; }
std::string CATCommand::getTx()                         { return "TX;"; }

std::string CATCommand::setMicUp()                      { return "UP;"; }

std::string CATCommand::setVoxDelayTime(int delayCode)  { char b[8];  snprintf(b,sizeof(b),"VD%04d;",cat_clamp(delayCode,0,30)); return b; }
std::string CATCommand::getVoxDelayTime()               { return "VD;"; }

std::string CATCommand::getFirmwareVersion()            { return "VE;"; }

std::string CATCommand::setVoxGain(int gain)            { char b[8];  snprintf(b,sizeof(b),"VG%03d;",cat_clamp(gain,0,100)); return b; }
std::string CATCommand::getVoxGain()                    { return "VG;"; }

std::string CATCommand::setVfoOrMemoryChannel(int vfo, int mode){ char b[8]; snprintf(b,sizeof(b),"VM%d%d;",cat_clamp(vfo,0,1),cat_clamp(mode,0,1)); return b; }
std::string CATCommand::getVfoOrMemoryChannel(int vfo)  { char b[6];  snprintf(b,sizeof(b),"VM%d;",cat_clamp(vfo,0,1)); return b; }

std::string CATCommand::setVfoSelect(int vfo)           { char b[6];  snprintf(b,sizeof(b),"VS%d;",cat_clamp(vfo,0,1)); return b; }
std::string CATCommand::getVfoSelect()                  { return "VS;"; }

std::string CATCommand::setVoxStatus(bool on)           { return on ? "VX1;" : "VX0;"; }
std::string CATCommand::getVoxStatus()                  { return "VX;"; }

std::string CATCommand::setZeroIn()                     { return "ZI;"; }

// ============================================================================
//  SECTION 2 — EX EXTENDED MENU
//  Helper macros to reduce repetition
// ============================================================================

// signed dB value → "+03" / "-10" style 3-char string
static std::string dbStr(int db) {
    char v[6]; snprintf(v,sizeof(v),"%+03d",db); return v;
}
static std::string idxStr2(int i) {
    char v[4]; snprintf(v,sizeof(v),"%02d",i); return v;
}
static std::string idxStr3(int i) {
    char v[4]; snprintf(v,sizeof(v),"%03d",i); return v;
}
static std::string idxStr4(int i) {
    char v[8]; snprintf(v,sizeof(v),"%04d",i); return v;
}
static std::string signedHz(int hz) {
    char sign = hz>=0?'+':'-';
    char v[8]; snprintf(v,sizeof(v),"%c%04d",sign,std::abs(hz)); return v;
}

// ── MODE SSB ─────────────────────────────────────────────────────────────────
std::string CATCommand::setAfTrebleGain(int db)     { return setMenuRaw(101, dbStr(cat_clamp(db,-20,10))); }
std::string CATCommand::getAfTrebleGain()           { return getMenuRaw(101); }
std::string CATCommand::setAfMiddleToneGain(int db) { return setMenuRaw(102, dbStr(cat_clamp(db,-20,10))); }
std::string CATCommand::getAfMiddleToneGain()       { return getMenuRaw(102); }
std::string CATCommand::setAfBassGain(int db)       { return setMenuRaw(103, dbStr(cat_clamp(db,-20,10))); }
std::string CATCommand::getAfBassGain()             { return getMenuRaw(103); }
std::string CATCommand::setAgcFastDelay(int ms)     { return setMenuRaw(104, idxStr4(cat_clamp(ms,20,4000))); }
std::string CATCommand::getAgcFastDelay()           { return getMenuRaw(104); }
std::string CATCommand::setAgcMidDelay(int ms)      { return setMenuRaw(105, idxStr4(cat_clamp(ms,20,4000))); }
std::string CATCommand::getAgcMidDelay()            { return getMenuRaw(105); }
std::string CATCommand::setAgcSlowDelay(int ms)     { return setMenuRaw(106, idxStr4(cat_clamp(ms,20,4000))); }
std::string CATCommand::getAgcSlowDelay()           { return getMenuRaw(106); }
std::string CATCommand::setLcutFreq(int idx)        { return setMenuRaw(107, idxStr2(cat_clamp(idx,0,20))); }
std::string CATCommand::getLcutFreq()               { return getMenuRaw(107); }
std::string CATCommand::setLcutSlope(int slope)     { return setMenuRaw(108, std::string(1,'0'+cat_clamp(slope,0,1))); }
std::string CATCommand::getLcutSlope()              { return getMenuRaw(108); }
std::string CATCommand::setHcutFreq(int idx)        { return setMenuRaw(109, idxStr2(cat_clamp(idx,0,67))); }
std::string CATCommand::getHcutFreq()               { return getMenuRaw(109); }
std::string CATCommand::setHcutSlope(int slope)     { return setMenuRaw(110, std::string(1,'0'+cat_clamp(slope,0,1))); }
std::string CATCommand::getHcutSlope()              { return getMenuRaw(110); }
std::string CATCommand::setUsbOutLevel(int level)   { return setMenuRaw(111, idxStr3(cat_clamp(level,0,100))); }
std::string CATCommand::getUsbOutLevel()            { return getMenuRaw(111); }
std::string CATCommand::setTxBpfSel(int sel)        { return setMenuRaw(112, std::string(1,'0'+cat_clamp(sel,0,4))); }
std::string CATCommand::getTxBpfSel()               { return getMenuRaw(112); }
std::string CATCommand::setModSource(int src)        { return setMenuRaw(113, std::string(1,'0'+cat_clamp(src,0,2))); }
std::string CATCommand::getModSource()              { return getMenuRaw(113); }
std::string CATCommand::setUsbModGain(int gain)     { return setMenuRaw(114, idxStr3(cat_clamp(gain,0,100))); }
std::string CATCommand::getUsbModGain()             { return getMenuRaw(114); }
std::string CATCommand::setRpttSelect(int sel)      { return setMenuRaw(115, std::string(1,'0'+cat_clamp(sel,0,2))); }
std::string CATCommand::getRpttSelect()             { return getMenuRaw(115); }

// ── MODE CW ──────────────────────────────────────────────────────────────────
std::string CATCommand::setCwAfTrebleGain(int db)     { return setMenuRaw(201, dbStr(cat_clamp(db,-20,10))); }
std::string CATCommand::getCwAfTrebleGain()           { return getMenuRaw(201); }
std::string CATCommand::setCwAfMiddleToneGain(int db) { return setMenuRaw(202, dbStr(cat_clamp(db,-20,10))); }
std::string CATCommand::getCwAfMiddleToneGain()       { return getMenuRaw(202); }
std::string CATCommand::setCwAfBassGain(int db)       { return setMenuRaw(203, dbStr(cat_clamp(db,-20,10))); }
std::string CATCommand::getCwAfBassGain()             { return getMenuRaw(203); }
std::string CATCommand::setCwAgcFastDelay(int ms)     { return setMenuRaw(204, idxStr4(cat_clamp(ms,20,4000))); }
std::string CATCommand::getCwAgcFastDelay()           { return getMenuRaw(204); }
std::string CATCommand::setCwAgcMidDelay(int ms)      { return setMenuRaw(205, idxStr4(cat_clamp(ms,20,4000))); }
std::string CATCommand::getCwAgcMidDelay()            { return getMenuRaw(205); }
std::string CATCommand::setCwAgcSlowDelay(int ms)     { return setMenuRaw(206, idxStr4(cat_clamp(ms,20,4000))); }
std::string CATCommand::getCwAgcSlowDelay()           { return getMenuRaw(206); }
std::string CATCommand::setCwLcutFreq(int idx)        { return setMenuRaw(207, idxStr2(cat_clamp(idx,0,20))); }
std::string CATCommand::getCwLcutFreq()               { return getMenuRaw(207); }
std::string CATCommand::setCwLcutSlope(int s)         { return setMenuRaw(208, std::string(1,'0'+cat_clamp(s,0,1))); }
std::string CATCommand::getCwLcutSlope()              { return getMenuRaw(208); }
std::string CATCommand::setCwHcutFreq(int idx)        { return setMenuRaw(209, idxStr2(cat_clamp(idx,0,67))); }
std::string CATCommand::getCwHcutFreq()               { return getMenuRaw(209); }
std::string CATCommand::setCwHcutSlope(int s)         { return setMenuRaw(210, std::string(1,'0'+cat_clamp(s,0,1))); }
std::string CATCommand::getCwHcutSlope()              { return getMenuRaw(210); }
std::string CATCommand::setCwOutLevel(int level)      { return setMenuRaw(211, idxStr3(cat_clamp(level,0,100))); }
std::string CATCommand::getCwOutLevel()               { return getMenuRaw(211); }
std::string CATCommand::setCwAutoMode(int mode)       { return setMenuRaw(212, std::string(1,'0'+cat_clamp(mode,0,2))); }
std::string CATCommand::getCwAutoMode()               { return getMenuRaw(212); }
std::string CATCommand::setCwNarWidth(int w)          { return setMenuRaw(213, std::string(1,'0'+cat_clamp(w,0,2))); }
std::string CATCommand::getCwNarWidth()               { return getMenuRaw(213); }
std::string CATCommand::setCwPcKeying(int sel)        { return setMenuRaw(214, std::string(1,'0'+cat_clamp(sel,0,2))); }
std::string CATCommand::getCwPcKeying()               { return getMenuRaw(214); }
std::string CATCommand::setCwIndicator(bool on)       { return setMenuRaw(215, on?"1":"0"); }
std::string CATCommand::getCwIndicator()              { return getMenuRaw(215); }

// ── MODE AM ──────────────────────────────────────────────────────────────────
std::string CATCommand::setAmAfTrebleGain(int db)     { return setMenuRaw(301, dbStr(cat_clamp(db,-20,10))); }
std::string CATCommand::getAmAfTrebleGain()           { return getMenuRaw(301); }
std::string CATCommand::setAmAfMiddleToneGain(int db) { return setMenuRaw(302, dbStr(cat_clamp(db,-20,10))); }
std::string CATCommand::getAmAfMiddleToneGain()       { return getMenuRaw(302); }
std::string CATCommand::setAmAfBassGain(int db)       { return setMenuRaw(303, dbStr(cat_clamp(db,-20,10))); }
std::string CATCommand::getAmAfBassGain()             { return getMenuRaw(303); }
std::string CATCommand::setAmAgcFastDelay(int ms)     { return setMenuRaw(304, idxStr4(cat_clamp(ms,20,4000))); }
std::string CATCommand::getAmAgcFastDelay()           { return getMenuRaw(304); }
std::string CATCommand::setAmAgcMidDelay(int ms)      { return setMenuRaw(305, idxStr4(cat_clamp(ms,20,4000))); }
std::string CATCommand::getAmAgcMidDelay()            { return getMenuRaw(305); }
std::string CATCommand::setAmAgcSlowDelay(int ms)     { return setMenuRaw(306, idxStr4(cat_clamp(ms,20,4000))); }
std::string CATCommand::getAmAgcSlowDelay()           { return getMenuRaw(306); }
std::string CATCommand::setAmLcutFreq(int idx)        { return setMenuRaw(307, idxStr2(cat_clamp(idx,0,20))); }
std::string CATCommand::getAmLcutFreq()               { return getMenuRaw(307); }
std::string CATCommand::setAmLcutSlope(int s)         { return setMenuRaw(308, std::string(1,'0'+cat_clamp(s,0,1))); }
std::string CATCommand::getAmLcutSlope()              { return getMenuRaw(308); }
std::string CATCommand::setAmHcutFreq(int idx)        { return setMenuRaw(309, idxStr2(cat_clamp(idx,0,67))); }
std::string CATCommand::getAmHcutFreq()               { return getMenuRaw(309); }
std::string CATCommand::setAmHcutSlope(int s)         { return setMenuRaw(310, std::string(1,'0'+cat_clamp(s,0,1))); }
std::string CATCommand::getAmHcutSlope()              { return getMenuRaw(310); }
std::string CATCommand::setAmModSource(int src)       { return setMenuRaw(311, std::string(1,'0'+cat_clamp(src,0,2))); }
std::string CATCommand::getAmModSource()              { return getMenuRaw(311); }
std::string CATCommand::setAmUsbModGain(int gain)     { return setMenuRaw(312, idxStr3(cat_clamp(gain,0,100))); }
std::string CATCommand::getAmUsbModGain()             { return getMenuRaw(312); }
std::string CATCommand::setAmRpttSelect(int sel)      { return setMenuRaw(313, std::string(1,'0'+cat_clamp(sel,0,2))); }
std::string CATCommand::getAmRpttSelect()             { return getMenuRaw(313); }

// ── MODE FM ──────────────────────────────────────────────────────────────────
std::string CATCommand::setFmAfTrebleGain(int db)     { return setMenuRaw(401, dbStr(cat_clamp(db,-20,10))); }
std::string CATCommand::getFmAfTrebleGain()           { return getMenuRaw(401); }
std::string CATCommand::setFmAfMiddleToneGain(int db) { return setMenuRaw(402, dbStr(cat_clamp(db,-20,10))); }
std::string CATCommand::getFmAfMiddleToneGain()       { return getMenuRaw(402); }
std::string CATCommand::setFmAfBassGain(int db)       { return setMenuRaw(403, dbStr(cat_clamp(db,-20,10))); }
std::string CATCommand::getFmAfBassGain()             { return getMenuRaw(403); }
std::string CATCommand::setFmAgcFastDelay(int ms)     { return setMenuRaw(404, idxStr4(cat_clamp(ms,20,4000))); }
std::string CATCommand::getFmAgcFastDelay()           { return getMenuRaw(404); }
std::string CATCommand::setFmAgcMidDelay(int ms)      { return setMenuRaw(405, idxStr4(cat_clamp(ms,20,4000))); }
std::string CATCommand::getFmAgcMidDelay()            { return getMenuRaw(405); }
std::string CATCommand::setFmAgcSlowDelay(int ms)     { return setMenuRaw(406, idxStr4(cat_clamp(ms,20,4000))); }
std::string CATCommand::getFmAgcSlowDelay()           { return getMenuRaw(406); }
std::string CATCommand::setFmLcutFreq(int idx)        { return setMenuRaw(407, idxStr2(cat_clamp(idx,0,20))); }
std::string CATCommand::getFmLcutFreq()               { return getMenuRaw(407); }
std::string CATCommand::setFmLcutSlope(int s)         { return setMenuRaw(408, std::string(1,'0'+cat_clamp(s,0,1))); }
std::string CATCommand::getFmLcutSlope()              { return getMenuRaw(408); }
std::string CATCommand::setFmHcutFreq(int idx)        { return setMenuRaw(409, idxStr2(cat_clamp(idx,0,67))); }
std::string CATCommand::getFmHcutFreq()               { return getMenuRaw(409); }
std::string CATCommand::setFmHcutSlope(int s)         { return setMenuRaw(410, std::string(1,'0'+cat_clamp(s,0,1))); }
std::string CATCommand::getFmHcutSlope()              { return getMenuRaw(410); }
std::string CATCommand::setFmModSource(int src)       { return setMenuRaw(411, std::string(1,'0'+cat_clamp(src,0,2))); }
std::string CATCommand::getFmModSource()              { return getMenuRaw(411); }
std::string CATCommand::setFmUsbModGain(int gain)     { return setMenuRaw(412, idxStr3(cat_clamp(gain,0,100))); }
std::string CATCommand::getFmUsbModGain()             { return getMenuRaw(412); }
std::string CATCommand::setFmRpttSelect(int sel)      { return setMenuRaw(413, std::string(1,'0'+cat_clamp(sel,0,2))); }
std::string CATCommand::getFmRpttSelect()             { return getMenuRaw(413); }
std::string CATCommand::setFmRptShift(int dir)        { return setMenuRaw(414, std::string(1,'0'+cat_clamp(dir,0,2))); }
std::string CATCommand::getFmRptShift()               { return getMenuRaw(414); }
std::string CATCommand::setFmRptShiftFreq144(int hz)  { return setMenuRaw(415, idxStr4(cat_clamp(hz,0,9999999)).substr(0,7)); }
std::string CATCommand::getFmRptShiftFreq144()        { return getMenuRaw(415); }
std::string CATCommand::setFmRptShiftFreq430(int hz)  { return setMenuRaw(416, idxStr4(cat_clamp(hz,0,9999999)).substr(0,7)); }
std::string CATCommand::getFmRptShiftFreq430()        { return getMenuRaw(416); }

// ── MODE DATA ────────────────────────────────────────────────────────────────
std::string CATCommand::setDataAfTrebleGain(int db)     { return setMenuRaw(501, dbStr(cat_clamp(db,-20,10))); }
std::string CATCommand::getDataAfTrebleGain()           { return getMenuRaw(501); }
std::string CATCommand::setDataAfMiddleToneGain(int db) { return setMenuRaw(502, dbStr(cat_clamp(db,-20,10))); }
std::string CATCommand::getDataAfMiddleToneGain()       { return getMenuRaw(502); }
std::string CATCommand::setDataAfBassGain(int db)       { return setMenuRaw(503, dbStr(cat_clamp(db,-20,10))); }
std::string CATCommand::getDataAfBassGain()             { return getMenuRaw(503); }
std::string CATCommand::setDataAgcFastDelay(int ms)     { return setMenuRaw(504, idxStr4(cat_clamp(ms,20,4000))); }
std::string CATCommand::getDataAgcFastDelay()           { return getMenuRaw(504); }
std::string CATCommand::setDataAgcMidDelay(int ms)      { return setMenuRaw(505, idxStr4(cat_clamp(ms,20,4000))); }
std::string CATCommand::getDataAgcMidDelay()            { return getMenuRaw(505); }
std::string CATCommand::setDataAgcSlowDelay(int ms)     { return setMenuRaw(506, idxStr4(cat_clamp(ms,20,4000))); }
std::string CATCommand::getDataAgcSlowDelay()           { return getMenuRaw(506); }
std::string CATCommand::setDataLcutFreq(int idx)        { return setMenuRaw(507, idxStr2(cat_clamp(idx,0,20))); }
std::string CATCommand::getDataLcutFreq()               { return getMenuRaw(507); }
std::string CATCommand::setDataLcutSlope(int s)         { return setMenuRaw(508, std::string(1,'0'+cat_clamp(s,0,1))); }
std::string CATCommand::getDataLcutSlope()              { return getMenuRaw(508); }
std::string CATCommand::setDataHcutFreq(int idx)        { return setMenuRaw(509, idxStr2(cat_clamp(idx,0,67))); }
std::string CATCommand::getDataHcutFreq()               { return getMenuRaw(509); }
std::string CATCommand::setDataHcutSlope(int s)         { return setMenuRaw(510, std::string(1,'0'+cat_clamp(s,0,1))); }
std::string CATCommand::getDataHcutSlope()              { return getMenuRaw(510); }
std::string CATCommand::setDataUsbOutLevel(int level)   { return setMenuRaw(511, idxStr3(cat_clamp(level,0,100))); }
std::string CATCommand::getDataUsbOutLevel()            { return getMenuRaw(511); }
std::string CATCommand::setDataTxBpfSel(int sel)        { return setMenuRaw(512, std::string(1,'0'+cat_clamp(sel,0,4))); }
std::string CATCommand::getDataTxBpfSel()               { return getMenuRaw(512); }
std::string CATCommand::setDataModSource(int src)        { return setMenuRaw(513, std::string(1,'0'+cat_clamp(src,0,2))); }
std::string CATCommand::getDataModSource()              { return getMenuRaw(513); }
std::string CATCommand::setDataUsbModGain(int gain)     { return setMenuRaw(514, idxStr3(cat_clamp(gain,0,100))); }
std::string CATCommand::getDataUsbModGain()             { return getMenuRaw(514); }
std::string CATCommand::setDataRpttSelect(int sel)      { return setMenuRaw(515, std::string(1,'0'+cat_clamp(sel,0,2))); }
std::string CATCommand::getDataRpttSelect()             { return getMenuRaw(515); }
std::string CATCommand::setDataNarWidth(int w)          { return setMenuRaw(516, std::string(1,'0'+cat_clamp(w,0,2))); }
std::string CATCommand::getDataNarWidth()               { return getMenuRaw(516); }
std::string CATCommand::setDataPskTone(int tone)        { return setMenuRaw(517, idxStr3(cat_clamp(tone,0,255))); }
std::string CATCommand::getDataPskTone()                { return getMenuRaw(517); }
std::string CATCommand::setDataShiftSsb(int hz)         { return setMenuRaw(518, signedHz(cat_clamp(hz,-3000,3000))); }
std::string CATCommand::getDataShiftSsb()               { return getMenuRaw(518); }

// ── MODE RTTY ────────────────────────────────────────────────────────────────
std::string CATCommand::setRttyAfTrebleGain(int db)     { return setMenuRaw(601, dbStr(cat_clamp(db,-20,10))); }
std::string CATCommand::getRttyAfTrebleGain()           { return getMenuRaw(601); }
std::string CATCommand::setRttyAfMiddleToneGain(int db) { return setMenuRaw(602, dbStr(cat_clamp(db,-20,10))); }
std::string CATCommand::getRttyAfMiddleToneGain()       { return getMenuRaw(602); }
std::string CATCommand::setRttyAfBassGain(int db)       { return setMenuRaw(603, dbStr(cat_clamp(db,-20,10))); }
std::string CATCommand::getRttyAfBassGain()             { return getMenuRaw(603); }
std::string CATCommand::setRttyAgcFastDelay(int ms)     { return setMenuRaw(604, idxStr4(cat_clamp(ms,20,4000))); }
std::string CATCommand::getRttyAgcFastDelay()           { return getMenuRaw(604); }
std::string CATCommand::setRttyAgcMidDelay(int ms)      { return setMenuRaw(605, idxStr4(cat_clamp(ms,20,4000))); }
std::string CATCommand::getRttyAgcMidDelay()            { return getMenuRaw(605); }
std::string CATCommand::setRttyAgcSlowDelay(int ms)     { return setMenuRaw(606, idxStr4(cat_clamp(ms,20,4000))); }
std::string CATCommand::getRttyAgcSlowDelay()           { return getMenuRaw(606); }
std::string CATCommand::setRttyLcutFreq(int idx)        { return setMenuRaw(607, idxStr2(cat_clamp(idx,0,20))); }
std::string CATCommand::getRttyLcutFreq()               { return getMenuRaw(607); }
std::string CATCommand::setRttyLcutSlope(int s)         { return setMenuRaw(608, std::string(1,'0'+cat_clamp(s,0,1))); }
std::string CATCommand::getRttyLcutSlope()              { return getMenuRaw(608); }
std::string CATCommand::setRttyHcutFreq(int idx)        { return setMenuRaw(609, idxStr2(cat_clamp(idx,0,67))); }
std::string CATCommand::getRttyHcutFreq()               { return getMenuRaw(609); }
std::string CATCommand::setRttyHcutSlope(int s)         { return setMenuRaw(610, std::string(1,'0'+cat_clamp(s,0,1))); }
std::string CATCommand::getRttyHcutSlope()              { return getMenuRaw(610); }
std::string CATCommand::setRttyRpttSelect(int sel)      { return setMenuRaw(611, std::string(1,'0'+cat_clamp(sel,0,2))); }
std::string CATCommand::getRttyRpttSelect()             { return getMenuRaw(611); }
std::string CATCommand::setRttyMarkFrequency(int f)     { return setMenuRaw(612, std::string(1,'0'+cat_clamp(f,0,1))); }
std::string CATCommand::getRttyMarkFrequency()          { return getMenuRaw(612); }
std::string CATCommand::setRttyShiftFrequency(int idx)  { return setMenuRaw(613, std::string(1,'0'+cat_clamp(idx,0,3))); }
std::string CATCommand::getRttyShiftFrequency()         { return getMenuRaw(613); }
std::string CATCommand::setRttyPolarityTx(int pol)      { return setMenuRaw(614, std::string(1,'0'+cat_clamp(pol,0,1))); }
std::string CATCommand::getRttyPolarityTx()             { return getMenuRaw(614); }
std::string CATCommand::setRttyPolarityRx(int pol)      { return setMenuRaw(615, std::string(1,'0'+cat_clamp(pol,0,1))); }
std::string CATCommand::getRttyPolarityRx()             { return getMenuRaw(615); }

// ── TX PARAMETRIC EQ — PROCESSOR OFF ─────────────────────────────────────────
std::string CATCommand::setPrmtrcEq1Freq(int idx)   { return setMenuRaw(701, idxStr2(cat_clamp(idx,0,20))); }
std::string CATCommand::getPrmtrcEq1Freq()          { return getMenuRaw(701); }
std::string CATCommand::setPrmtrcEq1Level(int db)   { return setMenuRaw(702, dbStr(cat_clamp(db,-20,10))); }
std::string CATCommand::getPrmtrcEq1Level()         { return getMenuRaw(702); }
std::string CATCommand::setPrmtrcEq1Bwth(int bw)    { return setMenuRaw(703, idxStr2(cat_clamp(bw,1,10))); }
std::string CATCommand::getPrmtrcEq1Bwth()          { return getMenuRaw(703); }
std::string CATCommand::setPrmtrcEq2Freq(int idx)   { return setMenuRaw(704, idxStr2(cat_clamp(idx,0,20))); }
std::string CATCommand::getPrmtrcEq2Freq()          { return getMenuRaw(704); }
std::string CATCommand::setPrmtrcEq2Level(int db)   { return setMenuRaw(705, dbStr(cat_clamp(db,-20,10))); }
std::string CATCommand::getPrmtrcEq2Level()         { return getMenuRaw(705); }
std::string CATCommand::setPrmtrcEq2Bwth(int bw)    { return setMenuRaw(706, idxStr2(cat_clamp(bw,1,10))); }
std::string CATCommand::getPrmtrcEq2Bwth()          { return getMenuRaw(706); }
std::string CATCommand::setPrmtrcEq3Freq(int idx)   { return setMenuRaw(707, idxStr2(cat_clamp(idx,0,20))); }
std::string CATCommand::getPrmtrcEq3Freq()          { return getMenuRaw(707); }
std::string CATCommand::setPrmtrcEq3Level(int db)   { return setMenuRaw(708, dbStr(cat_clamp(db,-20,10))); }
std::string CATCommand::getPrmtrcEq3Level()         { return getMenuRaw(708); }
std::string CATCommand::setPrmtrcEq3Bwth(int bw)    { return setMenuRaw(709, idxStr2(cat_clamp(bw,1,10))); }
std::string CATCommand::getPrmtrcEq3Bwth()          { return getMenuRaw(709); }

// ── TX PARAMETRIC EQ — PROCESSOR ON ──────────────────────────────────────────
std::string CATCommand::setPPrmtrcEq1Freq(int idx)  { return setMenuRaw(801, idxStr2(cat_clamp(idx,0,20))); }
std::string CATCommand::getPPrmtrcEq1Freq()         { return getMenuRaw(801); }
std::string CATCommand::setPPrmtrcEq1Level(int db)  { return setMenuRaw(802, dbStr(cat_clamp(db,-20,10))); }
std::string CATCommand::getPPrmtrcEq1Level()        { return getMenuRaw(802); }
std::string CATCommand::setPPrmtrcEq1Bwth(int bw)   { return setMenuRaw(803, idxStr2(cat_clamp(bw,1,10))); }
std::string CATCommand::getPPrmtrcEq1Bwth()         { return getMenuRaw(803); }
std::string CATCommand::setPPrmtrcEq2Freq(int idx)  { return setMenuRaw(804, idxStr2(cat_clamp(idx,0,20))); }
std::string CATCommand::getPPrmtrcEq2Freq()         { return getMenuRaw(804); }
std::string CATCommand::setPPrmtrcEq2Level(int db)  { return setMenuRaw(805, dbStr(cat_clamp(db,-20,10))); }
std::string CATCommand::getPPrmtrcEq2Level()        { return getMenuRaw(805); }
std::string CATCommand::setPPrmtrcEq2Bwth(int bw)   { return setMenuRaw(806, idxStr2(cat_clamp(bw,1,10))); }
std::string CATCommand::getPPrmtrcEq2Bwth()         { return getMenuRaw(806); }
std::string CATCommand::setPPrmtrcEq3Freq(int idx)  { return setMenuRaw(807, idxStr2(cat_clamp(idx,0,20))); }
std::string CATCommand::getPPrmtrcEq3Freq()         { return getMenuRaw(807); }
std::string CATCommand::setPPrmtrcEq3Level(int db)  { return setMenuRaw(808, dbStr(cat_clamp(db,-20,10))); }
std::string CATCommand::getPPrmtrcEq3Level()        { return getMenuRaw(808); }
std::string CATCommand::setPPrmtrcEq3Bwth(int bw)   { return setMenuRaw(809, idxStr2(cat_clamp(bw,1,10))); }
std::string CATCommand::getPPrmtrcEq3Bwth()         { return getMenuRaw(809); }

// ── TX GENERAL ───────────────────────────────────────────────────────────────
std::string CATCommand::setTxGnrlMaxPowerBattery(int step) { return setMenuRaw(901, idxStr2(cat_clamp(step,1,12))); }
std::string CATCommand::getTxGnrlMaxPowerBattery()         { return getMenuRaw(901); }

// ── RX DSP ───────────────────────────────────────────────────────────────────
std::string CATCommand::setNbLevel(int level)   { return setMenuRaw(1001, idxStr2(cat_clamp(level,1,10))); }
std::string CATCommand::getNbLevel()            { return getMenuRaw(1001); }
std::string CATCommand::setNbWidth(int width)   { return setMenuRaw(1002, idxStr2(cat_clamp(width,1,9))); }
std::string CATCommand::getNbWidth()            { return getMenuRaw(1002); }
std::string CATCommand::setNrLevel(int level)   { return setMenuRaw(1003, idxStr2(cat_clamp(level,1,15))); }
std::string CATCommand::getNrLevel()            { return getMenuRaw(1003); }
std::string CATCommand::setContourLevel(int db) { return setMenuRaw(1004, dbStr(cat_clamp(db,-40,20))); }
std::string CATCommand::getContourLevel()       { return getMenuRaw(1004); }
std::string CATCommand::setContourWidth(int q)  { return setMenuRaw(1005, idxStr2(cat_clamp(q,1,11))); }
std::string CATCommand::getContourWidth()       { return getMenuRaw(1005); }
std::string CATCommand::setIfNotchWidth(int w)  { return setMenuRaw(1006, std::string(1,'0'+cat_clamp(w,0,1))); }
std::string CATCommand::getIfNotchWidth()       { return getMenuRaw(1006); }
std::string CATCommand::setApfWidth(int w)      { return setMenuRaw(1007, std::string(1,'0'+cat_clamp(w,0,2))); }
std::string CATCommand::getApfWidth()           { return getMenuRaw(1007); }

// ── VOX / KEY / DIAL ─────────────────────────────────────────────────────────
std::string CATCommand::setVoxSelect(int sel)       { return setMenuRaw(1101, std::string(1,'0'+cat_clamp(sel,0,1))); }
std::string CATCommand::getVoxSelect()              { return getMenuRaw(1101); }
std::string CATCommand::setEmergencyFreqTx(bool on) { return setMenuRaw(1102, on?"1":"0"); }
std::string CATCommand::getEmergencyFreqTx()        { return getMenuRaw(1102); }
std::string CATCommand::setMicUpAssign(int dir)     { return setMenuRaw(1201, std::string(1,'0'+cat_clamp(dir,0,1))); }
std::string CATCommand::getMicUpAssign()            { return getMenuRaw(1201); }
std::string CATCommand::setDialStep(int step)       { return setMenuRaw(1301, std::string(1,'0'+cat_clamp(step,0,5))); }
std::string CATCommand::getDialStep()               { return getMenuRaw(1301); }

// ============================================================================
//  SECTION 3 — RESPONSE PARSING HELPERS
// ============================================================================

int CATCommand::parseReadMeter(const std::string& r, int& meterId) {
    if (r.size() < 9 || r.substr(0,2) != "RM") { meterId = -1; return 0; }
    meterId = r[2] - '0';
    try { return std::min(255, std::stoi(r.substr(3,6))); } catch(...) { return 0; }
}

long long CATCommand::parseFrequency(const std::string& r) {
    if (r.size() < 11) return 0LL;
    try { return std::stoll(r.substr(2,9)); } catch(...) { return 0LL; }
}

int CATCommand::parseMode(const std::string& r, int& vfo) {
    if (r.size() < 4 || r.substr(0,2) != "MD") { vfo = -1; return -1; }
    vfo = r[2] - '0';
    return r[3] - '0';
}

bool CATCommand::parseTx(const std::string& r) {
    return r.size() >= 3 && r.substr(0,2) == "TX" && r[2] != '0';
}

const char* CATCommand::modeName(int idx) {
    static const char* names[] = {
        "LSB","USB","CW","CW-R","AM","FM","DATA-L","DATA-U","DATA-FM","C4FM"
    };
    return (idx >= 0 && idx <= 9) ? names[idx] : "?";
}
