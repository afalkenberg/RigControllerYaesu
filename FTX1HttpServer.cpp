/**
 * FTX1HttpServer.cpp
 * ==================
 * Complete HTTP REST API for the FTX-1 CAT controller.
 *
 * Endpoint conventions:
 *   POST /api/<resource>          — set / action
 *   GET  /api/<resource>          — get current state or query radio
 *   GET  /api/status              — full rig state JSON
 *
 * Every POST body is JSON.  Missing optional fields fall back to
 * sensible defaults.  Every response is JSON with at least {"ok": true/false}.
 */

#include "FTX1HttpServer.h"
#include <iostream>

// ============================================================================
//  Constructor / Destructor
// ============================================================================
FTX1HttpServer::FTX1HttpServer(FTX1Controller& rig) : rig_(rig) {
    setupRoutes();
}

FTX1HttpServer::~FTX1HttpServer() { stop(); }

// ============================================================================
//  Listen / Stop
// ============================================================================
bool FTX1HttpServer::listen(const std::string& host, int port) {
    svr_.set_mount_point("/", staticRoot_);
    std::cout << "[HTTP] Listening on http://" << host << ":" << port << "\n";
    return svr_.listen(host.c_str(), port);
}

void FTX1HttpServer::stop()               { svr_.stop(); }
void FTX1HttpServer::setStaticRoot(const std::string& p) { staticRoot_ = p; }

// ============================================================================
//  Helpers
// ============================================================================
json FTX1HttpServer::parseBody(const httplib::Request& req) {
    return json::parse(req.body, nullptr, false);
}

void FTX1HttpServer::cors(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin",  "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
}

void FTX1HttpServer::ok(httplib::Response& res, bool success) {
    cors(res);
    res.set_content(json{{"ok", success}}.dump(), "application/json");
}

void FTX1HttpServer::okJson(httplib::Response& res, const json& j) {
    cors(res);
    res.set_content(j.dump(), "application/json");
}

void FTX1HttpServer::err(httplib::Response& res, const std::string& msg) {
    cors(res);
    res.status = 400;
    res.set_content(json{{"ok", false}, {"error", msg}}.dump(), "application/json");
}

// ============================================================================
//  Route setup
// ============================================================================
void FTX1HttpServer::setupRoutes() {

    // ── CORS preflight ───────────────────────────────────────────────────
    svr_.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        cors(res);
    });

    // ====================================================================
    //  CONNECTION
    // ====================================================================

    // POST /api/connect   { "port": "COM4", "baud": 38400 }
    svr_.Post("/api/connect", [this](const httplib::Request& req, httplib::Response& res) {
        auto j    = parseBody(req);
        auto port = j.value("port", "COM4");
        auto baud = j.value("baud", 38400);
        ok(res, rig_.connect(port, baud));
    });

    // POST /api/disconnect
    svr_.Post("/api/disconnect", [this](const httplib::Request&, httplib::Response& res) {
        rig_.disconnect();
        ok(res);
    });

    // GET /api/status  →  full RigState JSON
    svr_.Get("/api/status", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, rig_.getStatus());
    });

    // ====================================================================
    //  FREQUENCY
    // ====================================================================

    // POST /api/frequency/main   { "hz": 14250000 }
    svr_.Post("/api/frequency/main", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setMainSideFrequency(j.value("hz", 14250000LL)));
    });

    // POST /api/frequency/sub    { "hz": 7100000 }
    svr_.Post("/api/frequency/sub", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setSubSideFrequency(j.value("hz", 7100000LL)));
    });

    // GET /api/frequency/main
    svr_.Get("/api/frequency/main", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getMainSideFrequency()));
    });

    // GET /api/frequency/sub
    svr_.Get("/api/frequency/sub", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getSubSideFrequency()));
    });

    // ====================================================================
    //  BAND
    // ====================================================================

    // POST /api/band/main   { "band": "20m" }
    svr_.Post("/api/band/main", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setMainBand(j.value("band", "20m")));
    });

    // POST /api/band/sub    { "band": "40m" }
    svr_.Post("/api/band/sub", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setSubBand(j.value("band", "40m")));
    });

    svr_.Post("/api/band/up",   [this](const httplib::Request&, httplib::Response& res){ ok(res, rig_.setBandUp()); });
    svr_.Post("/api/band/down", [this](const httplib::Request&, httplib::Response& res){ ok(res, rig_.setBandDown()); });

    // ====================================================================
    //  MODE
    // ====================================================================

    // POST /api/mode/main   { "mode": "USB" }
    svr_.Post("/api/mode/main", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setMainOperatingMode(j.value("mode", "USB")));
    });

    // POST /api/mode/sub    { "mode": "FM" }
    svr_.Post("/api/mode/sub", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setSubOperatingMode(j.value("mode", "USB")));
    });

    // GET /api/mode?vfo=0
    svr_.Get("/api/mode", [this](const httplib::Request& req, httplib::Response& res) {
        int vfo = req.has_param("vfo") ? std::stoi(req.get_param_value("vfo")) : 0;
        ok(res, rig_.getOperatingMode(vfo));
    });

    // ====================================================================
    //  TX / PTT
    // ====================================================================

    // POST /api/tx   { "on": true }
    svr_.Post("/api/tx", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setTx(j.value("on", false)));
    });

    // GET /api/tx
    svr_.Get("/api/tx", [this](const httplib::Request&, httplib::Response& res) {
        ok(res, rig_.getTx());
    });

    // ====================================================================
    //  SPLIT / RIT / XIT
    // ====================================================================

    // POST /api/split   { "on": true }
    svr_.Post("/api/split", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setSplit(j.value("on", false)));
    });

    // POST /api/rit   { "on": true, "offset": 500 }
    svr_.Post("/api/rit", [this](const httplib::Request& req, httplib::Response& res) {
        auto j  = parseBody(req);
        bool on = j.value("on", false);
        int  hz = j.value("offset", 0);
        bool r  = rig_.setRit(on);
        if (on) r &= rig_.setRitOffset(hz);
        ok(res, r);
    });

    // POST /api/rit/clear
    svr_.Post("/api/rit/clear", [this](const httplib::Request&, httplib::Response& res) {
        ok(res, rig_.clearRitXit());
    });

    // POST /api/xit   { "on": true }
    svr_.Post("/api/xit", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setXit(j.value("on", false)));
    });

    // ====================================================================
    //  VFO OPERATIONS
    // ====================================================================

    // POST /api/vfo/swap
    svr_.Post("/api/vfo/swap", [this](const httplib::Request&, httplib::Response& res) {
        ok(res, rig_.setSwapVfo());
    });

    // POST /api/vfo/main_to_sub
    svr_.Post("/api/vfo/main_to_sub", [this](const httplib::Request&, httplib::Response& res) {
        ok(res, rig_.setMainSideToSubSide());
    });

    // POST /api/vfo/sub_to_main
    svr_.Post("/api/vfo/sub_to_main", [this](const httplib::Request&, httplib::Response& res) {
        ok(res, rig_.setSubSideToMainSide());
    });

    // POST /api/vfo/main_to_mem
    svr_.Post("/api/vfo/main_to_mem", [this](const httplib::Request&, httplib::Response& res) {
        ok(res, rig_.setMainSideToMemoryChannel());
    });

    // POST /api/vfo/sub_to_mem
    svr_.Post("/api/vfo/sub_to_mem", [this](const httplib::Request&, httplib::Response& res) {
        ok(res, rig_.setSubSideToMemoryChannel());
    });

    // POST /api/vfo/mem_to_main
    svr_.Post("/api/vfo/mem_to_main", [this](const httplib::Request&, httplib::Response& res) {
        ok(res, rig_.setMemoryChannelToMainSide());
    });

    // POST /api/vfo/mem_to_sub
    svr_.Post("/api/vfo/mem_to_sub", [this](const httplib::Request&, httplib::Response& res) {
        ok(res, rig_.setMemoryChannelToSubSide());
    });

    // POST /api/vfo/select   { "vfo": 0 }
    svr_.Post("/api/vfo/select", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setVfoSelect(j.value("vfo", 0)));
    });

    // POST /api/vfo/function_tx   { "vfo": 0 }
    svr_.Post("/api/vfo/function_tx", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setFunctionTx(j.value("vfo", 0)));
    });

    // POST /api/vfo/function_rx   { "mode": 0 }
    svr_.Post("/api/vfo/function_rx", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setFunctionRx(j.value("mode", 0)));
    });

    // POST /api/fine_tuning   { "vfo": 0, "on": true }
    svr_.Post("/api/fine_tuning", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setFineTuning(j.value("vfo", 0), j.value("on", false)));
    });

    // ====================================================================
    //  GAINS
    // ====================================================================

    // POST /api/af_gain   { "vfo": 0, "level": 128 }
    svr_.Post("/api/af_gain", [this](const httplib::Request& req, httplib::Response& res) {
        auto j   = parseBody(req);
        int  vfo = j.value("vfo", 0);
        int  lv  = j.value("level", 128);
        ok(res, vfo == 0 ? rig_.setAfGainMain(lv) : rig_.setAfGainSub(lv));
    });

    // GET /api/af_gain?vfo=0
    svr_.Get("/api/af_gain", [this](const httplib::Request& req, httplib::Response& res) {
        int vfo = req.has_param("vfo") ? std::stoi(req.get_param_value("vfo")) : 0;
        ok(res, rig_.getAfGain(vfo));
    });

    // POST /api/rf_gain   { "level": 255 }
    svr_.Post("/api/rf_gain", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setRfGain(j.value("level", 255)));
    });

    // POST /api/squelch   { "vfo": 0, "level": 0 }
    svr_.Post("/api/squelch", [this](const httplib::Request& req, httplib::Response& res) {
        auto j   = parseBody(req);
        int  vfo = j.value("vfo", 0);
        int  lv  = j.value("level", 0);
        ok(res, vfo == 0 ? rig_.setSquelchMain(lv) : rig_.setSquelchSub(lv));
    });

    // POST /api/mic_gain   { "level": 50 }
    svr_.Post("/api/mic_gain", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setMicGain(j.value("level", 50)));
    });

    // POST /api/monitor_level   { "level": 0 }
    svr_.Post("/api/monitor_level", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setMonitorLevel(j.value("level", 0)));
    });

    // POST /api/power   { "watts": 100 }
    svr_.Post("/api/power", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setPowerControl(j.value("watts", 100)));
    });

    // ====================================================================
    //  RF PATH — PREAMP / ATT / AGC
    // ====================================================================

    // POST /api/preamp   { "vfo": 0, "level": 0 }   0=IPO 1=AMP1 2=AMP2
    svr_.Post("/api/preamp", [this](const httplib::Request& req, httplib::Response& res) {
        auto j   = parseBody(req);
        int  vfo = j.value("vfo", 0);
        int  lv  = j.value("level", 0);
        ok(res, vfo == 0 ? rig_.setPreampMain(lv) : rig_.setPreampSub(lv));
    });

    // POST /api/att   { "vfo": 0, "on": false, "level": 0 }
    svr_.Post("/api/att", [this](const httplib::Request& req, httplib::Response& res) {
        auto j   = parseBody(req);
        int  vfo = j.value("vfo", 0);
        bool on  = j.value("on", false);
        int  lv  = j.value("level", 0);
        ok(res, vfo == 0 ? rig_.setAttenuatorMain(on, lv) : rig_.setAttenuatorSub(on, lv));
    });

    // POST /api/agc   { "vfo": 0, "mode": 3 }  0=OFF 1=FAST 2=MID 3=SLOW 4=AUTO
    svr_.Post("/api/agc", [this](const httplib::Request& req, httplib::Response& res) {
        auto j   = parseBody(req);
        int  vfo = j.value("vfo", 0);
        int  m   = j.value("mode", 3);
        ok(res, vfo == 0 ? rig_.setAgcMain(m) : rig_.setAgcSub(m));
    });

    // GET /api/agc?vfo=0
    svr_.Get("/api/agc", [this](const httplib::Request& req, httplib::Response& res) {
        int vfo = req.has_param("vfo") ? std::stoi(req.get_param_value("vfo")) : 0;
        ok(res, rig_.getAgcFunction(vfo));
    });

    // ====================================================================
    //  NOISE BLANKER
    // ====================================================================

    // POST /api/nb   { "vfo": 0, "on": false }
    svr_.Post("/api/nb", [this](const httplib::Request& req, httplib::Response& res) {
        auto j   = parseBody(req);
        int  vfo = j.value("vfo", 0);
        bool on  = j.value("on", false);
        ok(res, vfo == 0 ? rig_.setNoiseBlankerMain(on) : rig_.setNoiseBlankerSub(on));
    });

    // POST /api/nb/level   { "level": 5 }
    svr_.Post("/api/nb/level", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setNbLevel(j.value("level", 5)));
    });

    // POST /api/nb/width   { "width": 5 }
    svr_.Post("/api/nb/width", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setNbWidth(j.value("width", 5)));
    });

    // GET /api/nb/level
    svr_.Get("/api/nb/level", [this](const httplib::Request&, httplib::Response& res) {
        ok(res, rig_.getNbLevel());
    });

    // GET /api/nb/width
    svr_.Get("/api/nb/width", [this](const httplib::Request&, httplib::Response& res) {
        ok(res, rig_.getNbWidth());
    });

    // ====================================================================
    //  NOISE REDUCTION
    // ====================================================================

    // POST /api/nr   { "vfo": 0, "on": false }
    svr_.Post("/api/nr", [this](const httplib::Request& req, httplib::Response& res) {
        auto j   = parseBody(req);
        int  vfo = j.value("vfo", 0);
        bool on  = j.value("on", false);
        ok(res, vfo == 0 ? rig_.setNoiseReductionMain(on) : rig_.setNoiseReductionSub(on));
    });

    // POST /api/nr/level   { "level": 8 }
    svr_.Post("/api/nr/level", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setNrLevel(j.value("level", 8)));
    });

    // GET /api/nr/level
    svr_.Get("/api/nr/level", [this](const httplib::Request&, httplib::Response& res) {
        ok(res, rig_.getNrLevel());
    });

    // ====================================================================
    //  AUTO NOTCH (DNF)
    // ====================================================================

    // POST /api/dnf   { "vfo": 0, "on": false }
    svr_.Post("/api/dnf", [this](const httplib::Request& req, httplib::Response& res) {
        auto j   = parseBody(req);
        int  vfo = j.value("vfo", 0);
        bool on  = j.value("on", false);
        ok(res, vfo == 0 ? rig_.setAutoNotchMain(on) : rig_.setAutoNotchSub(on));
    });

    // ====================================================================
    //  MANUAL NOTCH
    // ====================================================================

    // POST /api/notch   { "vfo": 0, "on": true, "freq": 1000 }
    svr_.Post("/api/notch", [this](const httplib::Request& req, httplib::Response& res) {
        auto j   = parseBody(req);
        int  vfo = j.value("vfo", 0);
        bool on  = j.value("on", false);
        int  hz  = j.value("freq", 1000);
        ok(res, vfo == 0 ? rig_.setManualNotchMain(on, hz) : rig_.setManualNotchSub(on, hz));
    });

    // ====================================================================
    //  LOCK
    // ====================================================================

    // POST /api/lock   { "on": false }
    svr_.Post("/api/lock", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setLockMain(j.value("on", false)));
    });

    // ====================================================================
    //  IF FILTER — SHIFT / WIDTH / CONTOUR
    // ====================================================================

    // POST /api/if/shift   { "vfo": 0, "offset": 0 }
    svr_.Post("/api/if/shift", [this](const httplib::Request& req, httplib::Response& res) {
        auto j   = parseBody(req);
        int  vfo = j.value("vfo", 0);
        int  hz  = j.value("offset", 0);
        ok(res, vfo == 0 ? rig_.setIfShiftMain(hz) : rig_.setIfShiftSub(hz));
    });

    // GET /api/if/shift?vfo=0
    svr_.Get("/api/if/shift", [this](const httplib::Request& req, httplib::Response& res) {
        int vfo = req.has_param("vfo") ? std::stoi(req.get_param_value("vfo")) : 0;
        ok(res, rig_.getIfShift(vfo));
    });

    // POST /api/if/width   { "vfo": 0, "hz": 2400 }
    svr_.Post("/api/if/width", [this](const httplib::Request& req, httplib::Response& res) {
        auto j   = parseBody(req);
        int  vfo = j.value("vfo", 0);
        int  hz  = j.value("hz", 2400);
        ok(res, vfo == 0 ? rig_.setWidthMain(hz) : rig_.setWidthSub(hz));
    });

    // GET /api/if/width?vfo=0
    svr_.Get("/api/if/width", [this](const httplib::Request& req, httplib::Response& res) {
        int vfo = req.has_param("vfo") ? std::stoi(req.get_param_value("vfo")) : 0;
        ok(res, rig_.getWidth(vfo));
    });

    // POST /api/if/contour   { "level": 0, "width": 5 }
    svr_.Post("/api/if/contour", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setContour(j.value("level", 0), j.value("width", 5)));
    });

    // POST /api/if/contour_level   { "db": 0 }
    svr_.Post("/api/if/contour_level", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setContourLevel(j.value("db", 0)));
    });

    // POST /api/if/contour_width   { "q": 5 }
    svr_.Post("/api/if/contour_width", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setContourWidth(j.value("q", 5)));
    });

    // POST /api/if/notch_width   { "width": 0 }   0=NARROW 1=WIDE
    svr_.Post("/api/if/notch_width", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setIfNotchWidth(j.value("width", 0)));
    });

    // POST /api/if/apf_width   { "width": 1 }   0=NARROW 1=MEDIUM 2=WIDE
    svr_.Post("/api/if/apf_width", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setApfWidth(j.value("width", 1)));
    });

    // ====================================================================
    //  AUDIO FILTER — LCUT / HCUT (current mode)
    // ====================================================================

    // POST /api/audio/lcut   { "index": 0 }   0=OFF 1-20=100-1050Hz
    svr_.Post("/api/audio/lcut", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setLcutFreq(j.value("index", 0)));
    });

    // POST /api/audio/lcut_slope   { "slope": 0 }   0=6dB/oct 1=18dB/oct
    svr_.Post("/api/audio/lcut_slope", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setLcutSlope(j.value("slope", 0)));
    });

    // POST /api/audio/hcut   { "index": 60 }   0=OFF 1-67=700-4000Hz
    svr_.Post("/api/audio/hcut", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setHcutFreq(j.value("index", 0)));
    });

    // POST /api/audio/hcut_slope   { "slope": 0 }
    svr_.Post("/api/audio/hcut_slope", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setHcutSlope(j.value("slope", 0)));
    });

    // POST /api/audio/lcut_for_mode   { "mode": "USB", "index": 0, "slope": 0 }
    svr_.Post("/api/audio/lcut_for_mode", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        auto m = j.value("mode", "USB");
        bool r = rig_.setLcutFreqForMode(m, j.value("index", 0));
        r     &= rig_.setLcutSlopeForMode(m, j.value("slope", 0));
        ok(res, r);
    });

    // POST /api/audio/hcut_for_mode   { "mode": "USB", "index": 60, "slope": 0 }
    svr_.Post("/api/audio/hcut_for_mode", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        auto m = j.value("mode", "USB");
        bool r = rig_.setHcutFreqForMode(m, j.value("index", 60));
        r     &= rig_.setHcutSlopeForMode(m, j.value("slope", 0));
        ok(res, r);
    });

    // ====================================================================
    //  AGC DELAYS
    // ====================================================================

    // POST /api/agc/fast_delay   { "ms": 300 }
    svr_.Post("/api/agc/fast_delay", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setAgcFastDelay(j.value("ms", 300)));
    });

    // POST /api/agc/mid_delay    { "ms": 800 }
    svr_.Post("/api/agc/mid_delay", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setAgcMidDelay(j.value("ms", 800)));
    });

    // POST /api/agc/slow_delay   { "ms": 1500 }
    svr_.Post("/api/agc/slow_delay", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setAgcSlowDelay(j.value("ms", 1500)));
    });

    // GET /api/agc/delays
    svr_.Get("/api/agc/delays", [this](const httplib::Request&, httplib::Response& res) {
        rig_.getAgcFastDelay();
        rig_.getAgcMidDelay();
        rig_.getAgcSlowDelay();
        ok(res);
    });

    // ====================================================================
    //  AF TONE (TREBLE / MID / BASS)
    // ====================================================================

    // POST /api/af_tone   { "treble": 0, "mid": 0, "bass": 0 }
    svr_.Post("/api/af_tone", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        bool r = rig_.setAfTrebleGain(j.value("treble", 0));
        r     &= rig_.setAfMiddleToneGain(j.value("mid", 0));
        r     &= rig_.setAfBassGain(j.value("bass", 0));
        ok(res, r);
    });

    // POST /api/af_tone/treble   { "db": 0 }
    svr_.Post("/api/af_tone/treble", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setAfTrebleGain(j.value("db", 0)));
    });

    // POST /api/af_tone/mid   { "db": 0 }
    svr_.Post("/api/af_tone/mid", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setAfMiddleToneGain(j.value("db", 0)));
    });

    // POST /api/af_tone/bass   { "db": 0 }
    svr_.Post("/api/af_tone/bass", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setAfBassGain(j.value("db", 0)));
    });

    // ====================================================================
    //  TX AUDIO
    // ====================================================================

    // POST /api/tx_audio/speech_proc   { "on": false }
    svr_.Post("/api/tx_audio/speech_proc", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setSpeechProcessor(j.value("on", false)));
    });

    // POST /api/tx_audio/amc_output   { "level": 50 }
    svr_.Post("/api/tx_audio/amc_output", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setAmcOutputLevel(j.value("level", 50)));
    });

    // POST /api/tx_audio/mod_source   { "src": 0 }   0=MIC 1=REAR 2=USB
    svr_.Post("/api/tx_audio/mod_source", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setModSource(j.value("src", 0)));
    });

    // POST /api/tx_audio/usb_mod_gain   { "gain": 50 }
    svr_.Post("/api/tx_audio/usb_mod_gain", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setUsbModGain(j.value("gain", 50)));
    });

    // POST /api/tx_audio/usb_out_level   { "level": 50 }
    svr_.Post("/api/tx_audio/usb_out_level", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setUsbOutLevel(j.value("level", 50)));
    });

    // POST /api/tx_audio/tx_bpf   { "sel": 0 }
    svr_.Post("/api/tx_audio/tx_bpf", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setTxBpfSel(j.value("sel", 0)));
    });

    // ====================================================================
    //  PARAMETRIC EQ — PROCESSOR OFF
    // ====================================================================

    // POST /api/eq/off   { "band": 1, "freq": 4, "level": 0, "bwth": 5 }
    svr_.Post("/api/eq/off", [this](const httplib::Request& req, httplib::Response& res) {
        auto j    = parseBody(req);
        int  band = j.value("band", 1);
        int  freq = j.value("freq", 4);
        int  lv   = j.value("level", 0);
        int  bw   = j.value("bwth", 5);
        bool r = false;
        if      (band == 1) { r  = rig_.setPrmtrcEq1Freq(freq); r &= rig_.setPrmtrcEq1Level(lv); r &= rig_.setPrmtrcEq1Bwth(bw); }
        else if (band == 2) { r  = rig_.setPrmtrcEq2Freq(freq); r &= rig_.setPrmtrcEq2Level(lv); r &= rig_.setPrmtrcEq2Bwth(bw); }
        else if (band == 3) { r  = rig_.setPrmtrcEq3Freq(freq); r &= rig_.setPrmtrcEq3Level(lv); r &= rig_.setPrmtrcEq3Bwth(bw); }
        ok(res, r);
    });

    // POST /api/eq/off/1/freq    { "index": 4 }
    svr_.Post("/api/eq/off/1/freq",  [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPrmtrcEq1Freq(j.value("index",4))); });
    svr_.Post("/api/eq/off/1/level", [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPrmtrcEq1Level(j.value("db",0))); });
    svr_.Post("/api/eq/off/1/bwth",  [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPrmtrcEq1Bwth(j.value("bw",5))); });
    svr_.Post("/api/eq/off/2/freq",  [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPrmtrcEq2Freq(j.value("index",10))); });
    svr_.Post("/api/eq/off/2/level", [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPrmtrcEq2Level(j.value("db",0))); });
    svr_.Post("/api/eq/off/2/bwth",  [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPrmtrcEq2Bwth(j.value("bw",5))); });
    svr_.Post("/api/eq/off/3/freq",  [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPrmtrcEq3Freq(j.value("index",17))); });
    svr_.Post("/api/eq/off/3/level", [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPrmtrcEq3Level(j.value("db",0))); });
    svr_.Post("/api/eq/off/3/bwth",  [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPrmtrcEq3Bwth(j.value("bw",5))); });

    // GET /api/eq/off
    svr_.Get("/api/eq/off", [this](const httplib::Request&, httplib::Response& res) {
        rig_.getPrmtrcEq1Freq(); rig_.getPrmtrcEq1Level(); rig_.getPrmtrcEq1Bwth();
        rig_.getPrmtrcEq2Freq(); rig_.getPrmtrcEq2Level(); rig_.getPrmtrcEq2Bwth();
        rig_.getPrmtrcEq3Freq(); rig_.getPrmtrcEq3Level(); rig_.getPrmtrcEq3Bwth();
        ok(res);
    });

    // ====================================================================
    //  PARAMETRIC EQ — PROCESSOR ON
    // ====================================================================

    // POST /api/eq/on   { "band": 1, "freq": 4, "level": 0, "bwth": 5 }
    svr_.Post("/api/eq/on", [this](const httplib::Request& req, httplib::Response& res) {
        auto j    = parseBody(req);
        int  band = j.value("band", 1);
        int  freq = j.value("freq", 4);
        int  lv   = j.value("level", 0);
        int  bw   = j.value("bwth", 5);
        bool r = false;
        if      (band == 1) { r  = rig_.setPPrmtrcEq1Freq(freq); r &= rig_.setPPrmtrcEq1Level(lv); r &= rig_.setPPrmtrcEq1Bwth(bw); }
        else if (band == 2) { r  = rig_.setPPrmtrcEq2Freq(freq); r &= rig_.setPPrmtrcEq2Level(lv); r &= rig_.setPPrmtrcEq2Bwth(bw); }
        else if (band == 3) { r  = rig_.setPPrmtrcEq3Freq(freq); r &= rig_.setPPrmtrcEq3Level(lv); r &= rig_.setPPrmtrcEq3Bwth(bw); }
        ok(res, r);
    });

    svr_.Post("/api/eq/on/1/freq",  [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPPrmtrcEq1Freq(j.value("index",4))); });
    svr_.Post("/api/eq/on/1/level", [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPPrmtrcEq1Level(j.value("db",0))); });
    svr_.Post("/api/eq/on/1/bwth",  [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPPrmtrcEq1Bwth(j.value("bw",5))); });
    svr_.Post("/api/eq/on/2/freq",  [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPPrmtrcEq2Freq(j.value("index",10))); });
    svr_.Post("/api/eq/on/2/level", [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPPrmtrcEq2Level(j.value("db",0))); });
    svr_.Post("/api/eq/on/2/bwth",  [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPPrmtrcEq2Bwth(j.value("bw",5))); });
    svr_.Post("/api/eq/on/3/freq",  [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPPrmtrcEq3Freq(j.value("index",17))); });
    svr_.Post("/api/eq/on/3/level", [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPPrmtrcEq3Level(j.value("db",0))); });
    svr_.Post("/api/eq/on/3/bwth",  [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPPrmtrcEq3Bwth(j.value("bw",5))); });

    // GET /api/eq/on
    svr_.Get("/api/eq/on", [this](const httplib::Request&, httplib::Response& res) {
        rig_.getPPrmtrcEq1Freq(); rig_.getPPrmtrcEq1Level(); rig_.getPPrmtrcEq1Bwth();
        rig_.getPPrmtrcEq2Freq(); rig_.getPPrmtrcEq2Level(); rig_.getPPrmtrcEq2Bwth();
        rig_.getPPrmtrcEq3Freq(); rig_.getPPrmtrcEq3Level(); rig_.getPPrmtrcEq3Bwth();
        ok(res);
    });

    // ====================================================================
    //  VOX
    // ====================================================================

    // POST /api/vox   { "on": false, "gain": 50, "delay": 5 }
    svr_.Post("/api/vox", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        bool r = rig_.setVoxStatus(j.value("on", false));
        if (j.contains("gain"))  r &= rig_.setVoxGain(j["gain"]);
        if (j.contains("delay")) r &= rig_.setVoxDelayTime(j["delay"]);
        ok(res, r);
    });

    // POST /api/vox/gain    { "gain": 50 }
    svr_.Post("/api/vox/gain", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setVoxGain(j.value("gain", 50)));
    });

    // POST /api/vox/delay   { "code": 5 }
    svr_.Post("/api/vox/delay", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setVoxDelayTime(j.value("code", 5)));
    });

    // POST /api/vox/select   { "sel": 0 }   0=MIC 1=DATA
    svr_.Post("/api/vox/select", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setVoxSelect(j.value("sel", 0)));
    });

    // ====================================================================
    //  ANTENNA TUNER
    // ====================================================================

    // POST /api/tuner/start
    svr_.Post("/api/tuner/start", [this](const httplib::Request&, httplib::Response& res) {
        ok(res, rig_.setAntennaTunerStart());
    });

    // POST /api/tuner/stop
    svr_.Post("/api/tuner/stop", [this](const httplib::Request&, httplib::Response& res) {
        ok(res, rig_.setAntennaTunerStop());
    });

    // GET /api/tuner
    svr_.Get("/api/tuner", [this](const httplib::Request&, httplib::Response& res) {
        ok(res, rig_.getAntennaTunerControl());
    });

    // ====================================================================
    //  SCAN
    // ====================================================================

    // POST /api/scan   { "mode": 0 }   0=stop 1=UP 2=DOWN 3=MEM 4=MODE 5=GRP
    svr_.Post("/api/scan", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setScan(j.value("mode", 0)));
    });

    // ====================================================================
    //  MEMORY
    // ====================================================================

    // POST /api/memory   { "vfo": 0, "ch": 0 }
    svr_.Post("/api/memory", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setMemoryChannel(j.value("vfo", 0), j.value("ch", 0)));
    });

    // GET /api/memory?vfo=0&ch=0
    svr_.Get("/api/memory", [this](const httplib::Request& req, httplib::Response& res) {
        int vfo = req.has_param("vfo") ? std::stoi(req.get_param_value("vfo")) : 0;
        int ch  = req.has_param("ch")  ? std::stoi(req.get_param_value("ch"))  : 0;
        ok(res, rig_.getMemoryChannelRead(vfo, ch));
    });

    // ====================================================================
    //  CW
    // ====================================================================

    // POST /api/cw/speed   { "wpm": 20 }
    svr_.Post("/api/cw/speed", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setKeySpeed(j.value("wpm", 20)));
    });

    // POST /api/cw/pitch   { "hz": 600 }
    svr_.Post("/api/cw/pitch", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setKeyPitch(j.value("hz", 600)));
    });

    // POST /api/cw/keyer   { "mode": 1 }   0=OFF 1=MODE-A 2=MODE-B
    svr_.Post("/api/cw/keyer", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setKeyer(j.value("mode", 1)));
    });

    // POST /api/cw/break_in   { "mode": 1 }   0=OFF 1=SEMI 2=FULL
    svr_.Post("/api/cw/break_in", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setBreakIn(j.value("mode", 1)));
    });

    // POST /api/cw/break_in_delay   { "ms": 200 }
    svr_.Post("/api/cw/break_in_delay", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setCwBreakInDelay(j.value("ms", 200)));
    });

    // POST /api/cw/spot   { "on": false }
    svr_.Post("/api/cw/spot", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setCwSpot(j.value("on", false)));
    });

    // POST /api/cw/zero_in
    svr_.Post("/api/cw/zero_in", [this](const httplib::Request&, httplib::Response& res) {
        ok(res, rig_.setZeroIn());
    });

    // POST /api/cw/auto_mode   { "mode": 0 }
    svr_.Post("/api/cw/auto_mode", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setCwAutoMode(j.value("mode", 0)));
    });

    // POST /api/cw/memory   { "ch": 1, "text": "CQ CQ DE TEST" }
    svr_.Post("/api/cw/memory", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setKeyerMemory(j.value("ch", 1), j.value("text", "")));
    });

    // POST /api/cw/play   { "ch": 1 }   0=stop
    svr_.Post("/api/cw/play", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setCwKeyingMemoryPlay(j.value("ch", 0)));
    });

    // POST /api/cw/send   { "text": "CQ" }
    svr_.Post("/api/cw/send", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setCwKeyingText(j.value("text", "")));
    });

    // ====================================================================
    //  CTCSS / DCS
    // ====================================================================

    // POST /api/ctcss   { "vfo": 0, "mode": 1, "tone": 8 }
    svr_.Post("/api/ctcss", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        bool r = rig_.setCtcss(j.value("vfo", 0), j.value("mode", 0));
        if (j.contains("tone")) r &= rig_.setCtcssNumber(j.value("vfo", 0), j["tone"]);
        ok(res, r);
    });

    // ====================================================================
    //  CLARIFIER
    // ====================================================================

    // POST /api/clarifier   { "vfo": 0, "on": false, "offset": 0 }
    svr_.Post("/api/clarifier", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setClarifier(j.value("vfo",0), j.value("on",false), j.value("offset",0)));
    });

    // ====================================================================
    //  FM REPEATER
    // ====================================================================

    // POST /api/fm/rpt_shift   { "dir": 0 }   0=simplex 1=minus 2=plus
    svr_.Post("/api/fm/rpt_shift", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setFmRptShift(j.value("dir", 0)));
    });

    // POST /api/fm/rpt_freq   { "band": 144, "hz": 600000 }
    svr_.Post("/api/fm/rpt_freq", [this](const httplib::Request& req, httplib::Response& res) {
        auto j    = parseBody(req);
        int  band = j.value("band", 144);
        int  hz   = j.value("hz", 600000);
        ok(res, band == 430 ? rig_.setFmRptShiftFreq430(hz) : rig_.setFmRptShiftFreq144(hz));
    });

    // ====================================================================
    //  SPECTRUM SCOPE
    // ====================================================================

    // POST /api/scope   { "mode": 0 }
    svr_.Post("/api/scope", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setSpectrumScope(j.value("mode", 0)));
    });

    // ====================================================================
    //  METERS
    // ====================================================================

    // GET /api/meters   →  all 8 meter values, synchronously read
    svr_.Get("/api/meters", [this](const httplib::Request&, httplib::Response& res) {
        json j; j["ok"] = true;
        const char* names[] = {"smeter_main","smeter_sub","comp","alc","po","swr","idd","vdd"};
        for (int m = 1; m <= 8; m++) {
            auto r = queryResult(rig_, CATCommand::getReadMeter(m));
            j[names[m-1]] = r;
        }
        okJson(res, j);
    });

    // POST /api/meter_sw   { "meter": 1 }
    svr_.Post("/api/meter_sw", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setMeterSw(j.value("meter", 1)));
    });

    // ====================================================================
    //  AUTO INFORMATION
    // ====================================================================

    // POST /api/ai   { "mode": 2 }   0=OFF 1=AI1 2=AI2(full)
    svr_.Post("/api/ai", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setAutoInformation(j.value("mode", 2)));
    });

    // ====================================================================
    //  MISC
    // ====================================================================

    // POST /api/lcd   { "type": 1, "level": 80 }   type: 0=contrast 1=dimmer
    svr_.Post("/api/lcd", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setLcdDimmer(j.value("type", 1), j.value("level", 80)));
    });

    // POST /api/datetime   { "dt": "20250410120000" }
    svr_.Post("/api/datetime", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setDateTime(j.value("dt", "")));
    });

    // GET /api/datetime
    svr_.Get("/api/datetime", [this](const httplib::Request&, httplib::Response& res) {
        ok(res, rig_.getDateTime());
    });

    // GET /api/version
    svr_.Get("/api/version", [this](const httplib::Request&, httplib::Response& res) {
        rig_.getFirmwareVersion();
        ok(res);
    });

    // GET /api/id
    svr_.Get("/api/id", [this](const httplib::Request&, httplib::Response& res) {
        rig_.getRadioId();
        ok(res);
    });

    // POST /api/dial_step   { "step": 1 }   0=1Hz 1=10Hz 2=20Hz 3=100Hz 4=500Hz 5=1kHz
    svr_.Post("/api/dial_step", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setDialStep(j.value("step", 1)));
    });

    // POST /api/gp_out   { "port": 0, "level": 1 }
    svr_.Post("/api/gp_out", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setGpOut(j.value("port", 0), j.value("level", 0)));
    });

    // POST /api/tx_power_battery   { "step": 12 }   1-12 = 0.5-6.0W
    svr_.Post("/api/tx_power_battery", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setTxGnrlMaxPowerBattery(j.value("step", 12)));
    });

    // POST /api/rptt_select   { "sel": 0 }   0=DAKY 1=RTS 2=DTR
    svr_.Post("/api/rptt_select", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setRpttSelect(j.value("sel", 0)));
    });

    // POST /api/mic_up
    svr_.Post("/api/mic_up",   [this](const httplib::Request&, httplib::Response& res){ ok(res, rig_.setMicUp()); });

    // POST /api/key_down
    svr_.Post("/api/key_down", [this](const httplib::Request&, httplib::Response& res){ ok(res, rig_.setDownKey()); });

    // POST /api/channel   { "dir": 1 }   0=down 1=up
    svr_.Post("/api/channel", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setChannelUpDown(j.value("dir", 1)));
    });

    // ====================================================================
    //  RAW CAT PASSTHROUGH
    // ====================================================================

    // POST /api/raw   { "cmd": "FA014250000;" }
    svr_.Post("/api/raw", [this](const httplib::Request& req, httplib::Response& res) {
        auto j   = parseBody(req);
        auto cmd = j.value("cmd", "");
        if (cmd.empty()) { err(res, "cmd is required"); return; }
        rig_.sendRaw(cmd);
        okJson(res, json{{"ok", true}, {"response", rig_.getLastRawResponse()}});
    });

    // GET /api/raw?cmd=VE%3B
    svr_.Get("/api/raw", [this](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("cmd")) { err(res, "cmd param required"); return; }
        auto cmd = req.get_param_value("cmd");
        okJson(res, queryResult(rig_, cmd));
    });
}
