/**
 * FTX1HttpServer.cpp
 * ==================
 * Complete HTTP REST API for the FTX-1 CAT controller.
 */

// httplib must come before windows.h
#include "httplib.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "FTX1HttpServer.h"
#include "CATCommand.h"
#include <iostream>

using json = nlohmann::json;

// ============================================================================
//  File-scope helper — must appear before setupRoutes() so lambdas can see it
// ============================================================================
static json queryResult(FTX1Controller& rig, const std::string& cmd) {
    std::string raw = rig.query(cmd);
    json j;
    j["ok"]  = !raw.empty();
    j["raw"] = raw;
    j["cmd"] = cmd;
    // Strip the 2-char command prefix and trailing ';' to expose the value
    if (raw.size() > 3 && raw.back() == ';')
        j["value"] = raw.substr(2, raw.size() - 3);
    else
        j["value"] = "";
    return j;
}

// ============================================================================
//  Constructor / Destructor
// ============================================================================
FTX1HttpServer::FTX1HttpServer(FTX1Controller& rig) : rig_(rig) {
    setupRoutes();
}

FTX1HttpServer::~FTX1HttpServer() { stop(); }

// ============================================================================
//  Listen / Stop / Static root
// ============================================================================
bool FTX1HttpServer::listen(const std::string& host, int port) {
    svr_.set_mount_point("/", staticRoot_);
    std::cout << "[HTTP] Listening on http://" << host << ":" << port << "\n";
    return svr_.listen(host.c_str(), port);
}

void FTX1HttpServer::stop()                              { svr_.stop(); }
void FTX1HttpServer::setStaticRoot(const std::string& p) { staticRoot_ = p; }

// ============================================================================
//  Response helpers
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
        auto j = parseBody(req);
        ok(res, rig_.connect(j.value("port","COM4"), j.value("baud",38400)));
    });

    // POST /api/disconnect
    svr_.Post("/api/disconnect", [this](const httplib::Request&, httplib::Response& res) {
        rig_.disconnect();
        ok(res);
    });

    // GET /api/status
    svr_.Get("/api/status", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, rig_.getStatus());
    });

    // ====================================================================
    //  FREQUENCY
    // ====================================================================

    svr_.Post("/api/frequency/main", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setMainSideFrequency(j.value("hz", 14250000LL)));
    });

    svr_.Post("/api/frequency/sub", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setSubSideFrequency(j.value("hz", 7100000LL)));
    });

    svr_.Get("/api/frequency/main", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getMainSideFrequency()));
    });

    svr_.Get("/api/frequency/sub", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getSubSideFrequency()));
    });

    // ====================================================================
    //  BAND
    // ====================================================================

    svr_.Post("/api/band/main", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setMainBand(j.value("band","20m")));
    });

    svr_.Post("/api/band/sub", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setSubBand(j.value("band","40m")));
    });

    svr_.Post("/api/band/up",   [this](const httplib::Request&, httplib::Response& res){ ok(res, rig_.setBandUp()); });
    svr_.Post("/api/band/down", [this](const httplib::Request&, httplib::Response& res){ ok(res, rig_.setBandDown()); });

    // ====================================================================
    //  MODE
    // ====================================================================

    svr_.Post("/api/mode/main", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setMainOperatingMode(j.value("mode","USB")));
    });

    svr_.Post("/api/mode/sub", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setSubOperatingMode(j.value("mode","USB")));
    });

    svr_.Get("/api/mode", [this](const httplib::Request& req, httplib::Response& res) {
        int vfo = req.has_param("vfo") ? std::stoi(req.get_param_value("vfo")) : 0;
        okJson(res, queryResult(rig_, CATCommand::getOperatingMode(vfo)));
    });

    // ====================================================================
    //  TX / PTT
    // ====================================================================

    svr_.Post("/api/tx", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setTx(j.value("on", false)));
    });

    svr_.Get("/api/tx", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getTx()));
    });

    // ====================================================================
    //  SPLIT / RIT / XIT
    // ====================================================================

    svr_.Post("/api/split", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setSplit(j.value("on", false)));
    });

    svr_.Post("/api/rit", [this](const httplib::Request& req, httplib::Response& res) {
        auto j  = parseBody(req);
        bool on = j.value("on", false);
        bool r  = rig_.setRit(on);
        if (on) r &= rig_.setRitOffset(j.value("offset", 0));
        ok(res, r);
    });

    svr_.Post("/api/rit/clear", [this](const httplib::Request&, httplib::Response& res) {
        ok(res, rig_.clearRitXit());
    });

    svr_.Post("/api/xit", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setXit(j.value("on", false)));
    });

    svr_.Get("/api/rit", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getRit()));
    });

    // ====================================================================
    //  VFO OPERATIONS
    // ====================================================================

    svr_.Post("/api/vfo/swap",        [this](const httplib::Request&, httplib::Response& res){ ok(res, rig_.setSwapVfo()); });
    svr_.Post("/api/vfo/main_to_sub", [this](const httplib::Request&, httplib::Response& res){ ok(res, rig_.setMainSideToSubSide()); });
    svr_.Post("/api/vfo/sub_to_main", [this](const httplib::Request&, httplib::Response& res){ ok(res, rig_.setSubSideToMainSide()); });
    svr_.Post("/api/vfo/main_to_mem", [this](const httplib::Request&, httplib::Response& res){ ok(res, rig_.setMainSideToMemoryChannel()); });
    svr_.Post("/api/vfo/sub_to_mem",  [this](const httplib::Request&, httplib::Response& res){ ok(res, rig_.setSubSideToMemoryChannel()); });
    svr_.Post("/api/vfo/mem_to_main", [this](const httplib::Request&, httplib::Response& res){ ok(res, rig_.setMemoryChannelToMainSide()); });
    svr_.Post("/api/vfo/mem_to_sub",  [this](const httplib::Request&, httplib::Response& res){ ok(res, rig_.setMemoryChannelToSubSide()); });

    svr_.Post("/api/vfo/select", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setVfoSelect(j.value("vfo", 0)));
    });

    svr_.Post("/api/vfo/function_tx", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setFunctionTx(j.value("vfo", 0)));
    });

    svr_.Post("/api/vfo/function_rx", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setFunctionRx(j.value("mode", 0)));
    });

    svr_.Get("/api/vfo/function_tx", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getFunctionTx()));
    });

    svr_.Post("/api/fine_tuning", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setFineTuning(j.value("vfo",0), j.value("on",false)));
    });

    // ====================================================================
    //  GAINS
    // ====================================================================

    svr_.Post("/api/af_gain", [this](const httplib::Request& req, httplib::Response& res) {
        auto j  = parseBody(req);
        int vfo = j.value("vfo", 0);
        int lv  = j.value("level", 128);
        ok(res, vfo == 0 ? rig_.setAfGainMain(lv) : rig_.setAfGainSub(lv));
    });

    svr_.Get("/api/af_gain", [this](const httplib::Request& req, httplib::Response& res) {
        int vfo = req.has_param("vfo") ? std::stoi(req.get_param_value("vfo")) : 0;
        okJson(res, queryResult(rig_, CATCommand::getAfGain(vfo)));
    });

    svr_.Post("/api/rf_gain", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setRfGain(j.value("level", 255)));
    });

    svr_.Get("/api/rf_gain", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getRfGain()));
    });

    svr_.Post("/api/squelch", [this](const httplib::Request& req, httplib::Response& res) {
        auto j  = parseBody(req);
        int vfo = j.value("vfo", 0);
        int lv  = j.value("level", 0);
        ok(res, vfo == 0 ? rig_.setSquelchMain(lv) : rig_.setSquelchSub(lv));
    });

    svr_.Get("/api/squelch", [this](const httplib::Request& req, httplib::Response& res) {
        int vfo = req.has_param("vfo") ? std::stoi(req.get_param_value("vfo")) : 0;
        okJson(res, queryResult(rig_, CATCommand::getSquelchLevel(vfo)));
    });

    svr_.Post("/api/mic_gain", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setMicGain(j.value("level", 50)));
    });

    svr_.Get("/api/mic_gain", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getMicGain()));
    });

    svr_.Post("/api/monitor_level", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setMonitorLevel(j.value("level", 0)));
    });

    svr_.Get("/api/monitor_level", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getMonitorLevel()));
    });

    svr_.Post("/api/power", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setPowerControl(j.value("watts", 100)));
    });

    svr_.Get("/api/power", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getPowerControl()));
    });

    // ====================================================================
    //  RF PATH — PREAMP / ATT / AGC
    // ====================================================================

    svr_.Post("/api/preamp", [this](const httplib::Request& req, httplib::Response& res) {
        auto j  = parseBody(req);
        int vfo = j.value("vfo", 0);
        int lv  = j.value("level", 0);
        ok(res, vfo == 0 ? rig_.setPreampMain(lv) : rig_.setPreampSub(lv));
    });

    svr_.Get("/api/preamp", [this](const httplib::Request& req, httplib::Response& res) {
        int band = req.has_param("band") ? std::stoi(req.get_param_value("band")) : 0;
        okJson(res, queryResult(rig_, CATCommand::getPreamp(band)));
    });

    svr_.Post("/api/att", [this](const httplib::Request& req, httplib::Response& res) {
        auto j  = parseBody(req);
        int vfo = j.value("vfo", 0);
        bool on = j.value("on", false);
        int lv  = j.value("level", 0);
        ok(res, vfo == 0 ? rig_.setAttenuatorMain(on,lv) : rig_.setAttenuatorSub(on,lv));
    });

    svr_.Get("/api/att", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getAttenuator()));
    });

    svr_.Post("/api/agc", [this](const httplib::Request& req, httplib::Response& res) {
        auto j  = parseBody(req);
        int vfo = j.value("vfo", 0);
        int m   = j.value("mode", 3);
        ok(res, vfo == 0 ? rig_.setAgcMain(m) : rig_.setAgcSub(m));
    });

    svr_.Get("/api/agc", [this](const httplib::Request& req, httplib::Response& res) {
        int vfo = req.has_param("vfo") ? std::stoi(req.get_param_value("vfo")) : 0;
        okJson(res, queryResult(rig_, CATCommand::getAgcFunction(vfo)));
    });

    svr_.Post("/api/agc/fast_delay", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setAgcFastDelay(j.value("ms", 300)));
    });

    svr_.Get("/api/agc/fast_delay", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getAgcFastDelay()));
    });

    svr_.Post("/api/agc/mid_delay", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setAgcMidDelay(j.value("ms", 800)));
    });

    svr_.Get("/api/agc/mid_delay", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getAgcMidDelay()));
    });

    svr_.Post("/api/agc/slow_delay", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setAgcSlowDelay(j.value("ms", 1500)));
    });

    svr_.Get("/api/agc/slow_delay", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getAgcSlowDelay()));
    });

    // ====================================================================
    //  NOISE BLANKER
    // ====================================================================

    svr_.Post("/api/nb", [this](const httplib::Request& req, httplib::Response& res) {
        auto j  = parseBody(req);
        int vfo = j.value("vfo", 0);
        bool on = j.value("on", false);
        ok(res, vfo == 0 ? rig_.setNoiseBlankerMain(on) : rig_.setNoiseBlankerSub(on));
    });

    svr_.Get("/api/nb", [this](const httplib::Request& req, httplib::Response& res) {
        int vfo = req.has_param("vfo") ? std::stoi(req.get_param_value("vfo")) : 0;
        okJson(res, queryResult(rig_, CATCommand::getNoiseBlanker(vfo)));
    });

    svr_.Post("/api/nb/level", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setNbLevel(j.value("level", 5)));
    });

    svr_.Get("/api/nb/level", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getNbLevel()));
    });

    svr_.Post("/api/nb/width", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setNbWidth(j.value("width", 5)));
    });

    svr_.Get("/api/nb/width", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getNbWidth()));
    });

    // ====================================================================
    //  NOISE REDUCTION
    // ====================================================================

    svr_.Post("/api/nr", [this](const httplib::Request& req, httplib::Response& res) {
        auto j  = parseBody(req);
        int vfo = j.value("vfo", 0);
        bool on = j.value("on", false);
        ok(res, vfo == 0 ? rig_.setNoiseReductionMain(on) : rig_.setNoiseReductionSub(on));
    });

    svr_.Get("/api/nr", [this](const httplib::Request& req, httplib::Response& res) {
        int vfo = req.has_param("vfo") ? std::stoi(req.get_param_value("vfo")) : 0;
        okJson(res, queryResult(rig_, CATCommand::getNoiseReduction(vfo)));
    });

    svr_.Post("/api/nr/level", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setNrLevel(j.value("level", 8)));
    });

    svr_.Get("/api/nr/level", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getNrLevel()));
    });

    // ====================================================================
    //  AUTO NOTCH (DNF)
    // ====================================================================

    svr_.Post("/api/dnf", [this](const httplib::Request& req, httplib::Response& res) {
        auto j  = parseBody(req);
        int vfo = j.value("vfo", 0);
        bool on = j.value("on", false);
        ok(res, vfo == 0 ? rig_.setAutoNotchMain(on) : rig_.setAutoNotchSub(on));
    });

    svr_.Get("/api/dnf", [this](const httplib::Request& req, httplib::Response& res) {
        int vfo = req.has_param("vfo") ? std::stoi(req.get_param_value("vfo")) : 0;
        okJson(res, queryResult(rig_, CATCommand::getAutoNotch(vfo)));
    });

    // ====================================================================
    //  MANUAL NOTCH
    // ====================================================================

    svr_.Post("/api/notch", [this](const httplib::Request& req, httplib::Response& res) {
        auto j  = parseBody(req);
        int vfo = j.value("vfo", 0);
        bool on = j.value("on", false);
        int hz  = j.value("freq", 1000);
        ok(res, vfo == 0 ? rig_.setManualNotchMain(on,hz) : rig_.setManualNotchSub(on,hz));
    });

    svr_.Get("/api/notch", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getManualNotch()));
    });

    // ====================================================================
    //  LOCK
    // ====================================================================

    svr_.Post("/api/lock", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setLockMain(j.value("on", false)));
    });

    svr_.Get("/api/lock", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getLock()));
    });

    // ====================================================================
    //  IF FILTER — SHIFT / WIDTH / CONTOUR
    // ====================================================================

    svr_.Post("/api/if/shift", [this](const httplib::Request& req, httplib::Response& res) {
        auto j  = parseBody(req);
        int vfo = j.value("vfo", 0);
        int hz  = j.value("offset", 0);
        ok(res, vfo == 0 ? rig_.setIfShiftMain(hz) : rig_.setIfShiftSub(hz));
    });

    svr_.Get("/api/if/shift", [this](const httplib::Request& req, httplib::Response& res) {
        int vfo = req.has_param("vfo") ? std::stoi(req.get_param_value("vfo")) : 0;
        okJson(res, queryResult(rig_, CATCommand::getIfShift(vfo)));
    });

    svr_.Post("/api/if/width", [this](const httplib::Request& req, httplib::Response& res) {
        auto j  = parseBody(req);
        int vfo = j.value("vfo", 0);
        int hz  = j.value("hz", 2400);
        ok(res, vfo == 0 ? rig_.setWidthMain(hz) : rig_.setWidthSub(hz));
    });

    svr_.Get("/api/if/width", [this](const httplib::Request& req, httplib::Response& res) {
        int vfo = req.has_param("vfo") ? std::stoi(req.get_param_value("vfo")) : 0;
        okJson(res, queryResult(rig_, CATCommand::getWidth(vfo)));
    });

    svr_.Post("/api/if/contour", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setContour(j.value("level",0), j.value("width",5)));
    });

    svr_.Get("/api/if/contour", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getContour()));
    });

    svr_.Post("/api/if/contour_level", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setContourLevel(j.value("db", 0)));
    });

    svr_.Get("/api/if/contour_level", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getContourLevel()));
    });

    svr_.Post("/api/if/contour_width", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setContourWidth(j.value("q", 5)));
    });

    svr_.Get("/api/if/contour_width", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getContourWidth()));
    });

    svr_.Post("/api/if/notch_width", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setIfNotchWidth(j.value("width", 0)));
    });

    svr_.Get("/api/if/notch_width", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getIfNotchWidth()));
    });

    svr_.Post("/api/if/apf_width", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setApfWidth(j.value("width", 1)));
    });

    svr_.Get("/api/if/apf_width", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getApfWidth()));
    });

    // ====================================================================
    //  AUDIO FILTER — LCUT / HCUT
    // ====================================================================

    svr_.Post("/api/audio/lcut", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setLcutFreq(j.value("index", 0)));
    });

    svr_.Get("/api/audio/lcut", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getLcutFreq()));
    });

    svr_.Post("/api/audio/lcut_slope", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setLcutSlope(j.value("slope", 0)));
    });

    svr_.Get("/api/audio/lcut_slope", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getLcutSlope()));
    });

    svr_.Post("/api/audio/hcut", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setHcutFreq(j.value("index", 0)));
    });

    svr_.Get("/api/audio/hcut", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getHcutFreq()));
    });

    svr_.Post("/api/audio/hcut_slope", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setHcutSlope(j.value("slope", 0)));
    });

    svr_.Get("/api/audio/hcut_slope", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getHcutSlope()));
    });

    svr_.Post("/api/audio/lcut_for_mode", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        auto m = j.value("mode","USB");
        bool r = rig_.setLcutFreqForMode(m, j.value("index",0));
        r     &= rig_.setLcutSlopeForMode(m, j.value("slope",0));
        ok(res, r);
    });

    svr_.Post("/api/audio/hcut_for_mode", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        auto m = j.value("mode","USB");
        bool r = rig_.setHcutFreqForMode(m, j.value("index",60));
        r     &= rig_.setHcutSlopeForMode(m, j.value("slope",0));
        ok(res, r);
    });

    // ====================================================================
    //  AF TONE
    // ====================================================================

    svr_.Post("/api/af_tone", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        bool r = rig_.setAfTrebleGain(j.value("treble",0));
        r     &= rig_.setAfMiddleToneGain(j.value("mid",0));
        r     &= rig_.setAfBassGain(j.value("bass",0));
        ok(res, r);
    });

    svr_.Post("/api/af_tone/treble", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req); ok(res, rig_.setAfTrebleGain(j.value("db",0)));
    });
    svr_.Get("/api/af_tone/treble", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getAfTrebleGain()));
    });

    svr_.Post("/api/af_tone/mid", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req); ok(res, rig_.setAfMiddleToneGain(j.value("db",0)));
    });
    svr_.Get("/api/af_tone/mid", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getAfMiddleToneGain()));
    });

    svr_.Post("/api/af_tone/bass", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req); ok(res, rig_.setAfBassGain(j.value("db",0)));
    });
    svr_.Get("/api/af_tone/bass", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getAfBassGain()));
    });

    // ====================================================================
    //  TX AUDIO
    // ====================================================================

    svr_.Post("/api/tx_audio/speech_proc", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req); ok(res, rig_.setSpeechProcessor(j.value("on",false)));
    });
    svr_.Get("/api/tx_audio/speech_proc", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getSpeechProcessor()));
    });

    svr_.Post("/api/tx_audio/amc_output", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req); ok(res, rig_.setAmcOutputLevel(j.value("level",50)));
    });
    svr_.Get("/api/tx_audio/amc_output", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getAmcOutputLevel()));
    });

    svr_.Post("/api/tx_audio/mod_source", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req); ok(res, rig_.setModSource(j.value("src",0)));
    });
    svr_.Get("/api/tx_audio/mod_source", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getModSource()));
    });

    svr_.Post("/api/tx_audio/usb_mod_gain", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req); ok(res, rig_.setUsbModGain(j.value("gain",50)));
    });
    svr_.Get("/api/tx_audio/usb_mod_gain", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getUsbModGain()));
    });

    svr_.Post("/api/tx_audio/usb_out_level", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req); ok(res, rig_.setUsbOutLevel(j.value("level",50)));
    });
    svr_.Get("/api/tx_audio/usb_out_level", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getUsbOutLevel()));
    });

    svr_.Post("/api/tx_audio/tx_bpf", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req); ok(res, rig_.setTxBpfSel(j.value("sel",0)));
    });
    svr_.Get("/api/tx_audio/tx_bpf", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getTxBpfSel()));
    });

    // ====================================================================
    //  PARAMETRIC EQ — PROCESSOR OFF
    // ====================================================================

    svr_.Post("/api/eq/off", [this](const httplib::Request& req, httplib::Response& res) {
        auto j    = parseBody(req);
        int  band = j.value("band",1);
        int  freq = j.value("freq",4);
        int  lv   = j.value("level",0);
        int  bw   = j.value("bwth",5);
        bool r = false;
        if      (band==1){ r =rig_.setPrmtrcEq1Freq(freq); r&=rig_.setPrmtrcEq1Level(lv); r&=rig_.setPrmtrcEq1Bwth(bw); }
        else if (band==2){ r =rig_.setPrmtrcEq2Freq(freq); r&=rig_.setPrmtrcEq2Level(lv); r&=rig_.setPrmtrcEq2Bwth(bw); }
        else if (band==3){ r =rig_.setPrmtrcEq3Freq(freq); r&=rig_.setPrmtrcEq3Level(lv); r&=rig_.setPrmtrcEq3Bwth(bw); }
        ok(res, r);
    });

    svr_.Post("/api/eq/off/1/freq",  [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPrmtrcEq1Freq(j.value("index",4))); });
    svr_.Post("/api/eq/off/1/level", [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPrmtrcEq1Level(j.value("db",0))); });
    svr_.Post("/api/eq/off/1/bwth",  [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPrmtrcEq1Bwth(j.value("bw",5))); });
    svr_.Post("/api/eq/off/2/freq",  [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPrmtrcEq2Freq(j.value("index",10))); });
    svr_.Post("/api/eq/off/2/level", [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPrmtrcEq2Level(j.value("db",0))); });
    svr_.Post("/api/eq/off/2/bwth",  [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPrmtrcEq2Bwth(j.value("bw",5))); });
    svr_.Post("/api/eq/off/3/freq",  [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPrmtrcEq3Freq(j.value("index",17))); });
    svr_.Post("/api/eq/off/3/level", [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPrmtrcEq3Level(j.value("db",0))); });
    svr_.Post("/api/eq/off/3/bwth",  [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setPrmtrcEq3Bwth(j.value("bw",5))); });

    svr_.Get("/api/eq/off", [this](const httplib::Request&, httplib::Response& res) {
        json j; j["ok"] = true;
        j["band1"] = {{"freq",  queryResult(rig_, CATCommand::getPrmtrcEq1Freq())},
                      {"level", queryResult(rig_, CATCommand::getPrmtrcEq1Level())},
                      {"bwth",  queryResult(rig_, CATCommand::getPrmtrcEq1Bwth())}};
        j["band2"] = {{"freq",  queryResult(rig_, CATCommand::getPrmtrcEq2Freq())},
                      {"level", queryResult(rig_, CATCommand::getPrmtrcEq2Level())},
                      {"bwth",  queryResult(rig_, CATCommand::getPrmtrcEq2Bwth())}};
        j["band3"] = {{"freq",  queryResult(rig_, CATCommand::getPrmtrcEq3Freq())},
                      {"level", queryResult(rig_, CATCommand::getPrmtrcEq3Level())},
                      {"bwth",  queryResult(rig_, CATCommand::getPrmtrcEq3Bwth())}};
        okJson(res, j);
    });

    // ====================================================================
    //  PARAMETRIC EQ — PROCESSOR ON
    // ====================================================================

    svr_.Post("/api/eq/on", [this](const httplib::Request& req, httplib::Response& res) {
        auto j    = parseBody(req);
        int  band = j.value("band",1);
        int  freq = j.value("freq",4);
        int  lv   = j.value("level",0);
        int  bw   = j.value("bwth",5);
        bool r = false;
        if      (band==1){ r =rig_.setPPrmtrcEq1Freq(freq); r&=rig_.setPPrmtrcEq1Level(lv); r&=rig_.setPPrmtrcEq1Bwth(bw); }
        else if (band==2){ r =rig_.setPPrmtrcEq2Freq(freq); r&=rig_.setPPrmtrcEq2Level(lv); r&=rig_.setPPrmtrcEq2Bwth(bw); }
        else if (band==3){ r =rig_.setPPrmtrcEq3Freq(freq); r&=rig_.setPPrmtrcEq3Level(lv); r&=rig_.setPPrmtrcEq3Bwth(bw); }
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

    svr_.Get("/api/eq/on", [this](const httplib::Request&, httplib::Response& res) {
        json j; j["ok"] = true;
        j["band1"] = {{"freq",  queryResult(rig_, CATCommand::getPPrmtrcEq1Freq())},
                      {"level", queryResult(rig_, CATCommand::getPPrmtrcEq1Level())},
                      {"bwth",  queryResult(rig_, CATCommand::getPPrmtrcEq1Bwth())}};
        j["band2"] = {{"freq",  queryResult(rig_, CATCommand::getPPrmtrcEq2Freq())},
                      {"level", queryResult(rig_, CATCommand::getPPrmtrcEq2Level())},
                      {"bwth",  queryResult(rig_, CATCommand::getPPrmtrcEq2Bwth())}};
        j["band3"] = {{"freq",  queryResult(rig_, CATCommand::getPPrmtrcEq3Freq())},
                      {"level", queryResult(rig_, CATCommand::getPPrmtrcEq3Level())},
                      {"bwth",  queryResult(rig_, CATCommand::getPPrmtrcEq3Bwth())}};
        okJson(res, j);
    });

    // ====================================================================
    //  VOX
    // ====================================================================

    svr_.Post("/api/vox", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        bool r = rig_.setVoxStatus(j.value("on",false));
        if (j.contains("gain"))  r &= rig_.setVoxGain(j["gain"]);
        if (j.contains("delay")) r &= rig_.setVoxDelayTime(j["delay"]);
        ok(res, r);
    });

    svr_.Get("/api/vox", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getVoxStatus()));
    });

    svr_.Post("/api/vox/gain",   [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setVoxGain(j.value("gain",50))); });
    svr_.Get("/api/vox/gain",    [this](const httplib::Request&, httplib::Response& res){ okJson(res, queryResult(rig_, CATCommand::getVoxGain())); });
    svr_.Post("/api/vox/delay",  [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setVoxDelayTime(j.value("code",5))); });
    svr_.Get("/api/vox/delay",   [this](const httplib::Request&, httplib::Response& res){ okJson(res, queryResult(rig_, CATCommand::getVoxDelayTime())); });
    svr_.Post("/api/vox/select", [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setVoxSelect(j.value("sel",0))); });
    svr_.Get("/api/vox/select",  [this](const httplib::Request&, httplib::Response& res){ okJson(res, queryResult(rig_, CATCommand::getVoxSelect())); });

    // ====================================================================
    //  ANTENNA TUNER
    // ====================================================================

    svr_.Post("/api/tuner/start", [this](const httplib::Request&, httplib::Response& res){ ok(res, rig_.setAntennaTunerStart()); });
    svr_.Post("/api/tuner/stop",  [this](const httplib::Request&, httplib::Response& res){ ok(res, rig_.setAntennaTunerStop()); });
    svr_.Get("/api/tuner",        [this](const httplib::Request&, httplib::Response& res){ okJson(res, queryResult(rig_, CATCommand::getAntennaTunerControl())); });

    // ====================================================================
    //  SCAN
    // ====================================================================

    svr_.Post("/api/scan", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setScan(j.value("mode",0)));
    });

    svr_.Get("/api/scan", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getScan()));
    });

    // ====================================================================
    //  MEMORY
    // ====================================================================

    svr_.Post("/api/memory", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setMemoryChannel(j.value("vfo",0), j.value("ch",0)));
    });

    svr_.Get("/api/memory", [this](const httplib::Request& req, httplib::Response& res) {
        int vfo = req.has_param("vfo") ? std::stoi(req.get_param_value("vfo")) : 0;
        int ch  = req.has_param("ch")  ? std::stoi(req.get_param_value("ch"))  : 0;
        okJson(res, queryResult(rig_, CATCommand::getMemoryChannelRead(vfo, ch)));
    });

    // ====================================================================
    //  CW
    // ====================================================================

    svr_.Post("/api/cw/speed",         [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setKeySpeed(j.value("wpm",20))); });
    svr_.Get("/api/cw/speed",          [this](const httplib::Request&, httplib::Response& res){ okJson(res,queryResult(rig_,CATCommand::getKeySpeed())); });
    svr_.Post("/api/cw/pitch",         [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setKeyPitch(j.value("hz",600))); });
    svr_.Get("/api/cw/pitch",          [this](const httplib::Request&, httplib::Response& res){ okJson(res,queryResult(rig_,CATCommand::getKeyPitch())); });
    svr_.Post("/api/cw/keyer",         [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setKeyer(j.value("mode",1))); });
    svr_.Get("/api/cw/keyer",          [this](const httplib::Request&, httplib::Response& res){ okJson(res,queryResult(rig_,CATCommand::getKeyer())); });
    svr_.Post("/api/cw/break_in",      [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setBreakIn(j.value("mode",1))); });
    svr_.Get("/api/cw/break_in",       [this](const httplib::Request&, httplib::Response& res){ okJson(res,queryResult(rig_,CATCommand::getBreakIn())); });
    svr_.Post("/api/cw/break_in_delay",[this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setCwBreakInDelay(j.value("ms",200))); });
    svr_.Get("/api/cw/break_in_delay", [this](const httplib::Request&, httplib::Response& res){ okJson(res,queryResult(rig_,CATCommand::getCwBreakInDelay())); });
    svr_.Post("/api/cw/spot",          [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setCwSpot(j.value("on",false))); });
    svr_.Get("/api/cw/spot",           [this](const httplib::Request&, httplib::Response& res){ okJson(res,queryResult(rig_,CATCommand::getCwSpot())); });
    svr_.Post("/api/cw/zero_in",       [this](const httplib::Request&, httplib::Response& res){ ok(res,rig_.setZeroIn()); });
    svr_.Post("/api/cw/auto_mode",     [this](const httplib::Request& req, httplib::Response& res){ auto j=parseBody(req); ok(res,rig_.setCwAutoMode(j.value("mode",0))); });
    svr_.Get("/api/cw/auto_mode",      [this](const httplib::Request&, httplib::Response& res){ okJson(res,queryResult(rig_,CATCommand::getCwAutoMode())); });

    svr_.Post("/api/cw/memory", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setKeyerMemory(j.value("ch",1), j.value("text","")));
    });
    svr_.Get("/api/cw/memory", [this](const httplib::Request& req, httplib::Response& res) {
        int ch = req.has_param("ch") ? std::stoi(req.get_param_value("ch")) : 1;
        okJson(res, queryResult(rig_, CATCommand::getKeyerMemory(ch)));
    });

    svr_.Post("/api/cw/play", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setCwKeyingMemoryPlay(j.value("ch",0)));
    });

    svr_.Post("/api/cw/send", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setCwKeyingText(j.value("text","")));
    });

    // ====================================================================
    //  CTCSS
    // ====================================================================

    svr_.Post("/api/ctcss", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        bool r = rig_.setCtcss(j.value("vfo",0), j.value("mode",0));
        if (j.contains("tone")) r &= rig_.setCtcssNumber(j.value("vfo",0), j["tone"]);
        ok(res, r);
    });

    svr_.Get("/api/ctcss", [this](const httplib::Request& req, httplib::Response& res) {
        int vfo = req.has_param("vfo") ? std::stoi(req.get_param_value("vfo")) : 0;
        okJson(res, queryResult(rig_, CATCommand::getCtcss(vfo)));
    });

    // ====================================================================
    //  CLARIFIER
    // ====================================================================

    svr_.Post("/api/clarifier", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setClarifier(j.value("vfo",0), j.value("on",false), j.value("offset",0)));
    });

    svr_.Get("/api/clarifier", [this](const httplib::Request& req, httplib::Response& res) {
        int vfo = req.has_param("vfo") ? std::stoi(req.get_param_value("vfo")) : 0;
        okJson(res, queryResult(rig_, CATCommand::getClarifier(vfo)));
    });

    // ====================================================================
    //  FM REPEATER
    // ====================================================================

    svr_.Post("/api/fm/rpt_shift", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setFmRptShift(j.value("dir",0)));
    });

    svr_.Get("/api/fm/rpt_shift", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getFmRptShift()));
    });

    svr_.Post("/api/fm/rpt_freq", [this](const httplib::Request& req, httplib::Response& res) {
        auto j   = parseBody(req);
        int band = j.value("band",144);
        int hz   = j.value("hz",600000);
        ok(res, band==430 ? rig_.setFmRptShiftFreq430(hz) : rig_.setFmRptShiftFreq144(hz));
    });

    svr_.Get("/api/fm/rpt_freq", [this](const httplib::Request& req, httplib::Response& res) {
        int band = req.has_param("band") ? std::stoi(req.get_param_value("band")) : 144;
        okJson(res, queryResult(rig_, band==430
            ? CATCommand::getFmRptShiftFreq430()
            : CATCommand::getFmRptShiftFreq144()));
    });

    // ====================================================================
    //  SPECTRUM SCOPE
    // ====================================================================

    svr_.Post("/api/scope", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setSpectrumScope(j.value("mode",0)));
    });

    svr_.Get("/api/scope", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getSpectrumScope()));
    });

    // ====================================================================
    //  METERS
    // ====================================================================

    svr_.Get("/api/meters", [this](const httplib::Request&, httplib::Response& res) {
        static const char* names[] = {
            "smeter_main","smeter_sub","comp","alc","po","swr","idd","vdd"
        };
        json j; j["ok"] = true;
        for (int m = 1; m <= 8; m++)
            j[names[m-1]] = queryResult(rig_, CATCommand::getReadMeter(m));
        okJson(res, j);
    });

    svr_.Post("/api/meter_sw", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setMeterSw(j.value("meter",1)));
    });

    svr_.Get("/api/meter_sw", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getMeterSw()));
    });

    // ====================================================================
    //  AUTO INFORMATION
    // ====================================================================

    svr_.Post("/api/ai", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setAutoInformation(j.value("mode",2)));
    });

    svr_.Get("/api/ai", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getAutoInformation()));
    });

    // ====================================================================
    //  MISC
    // ====================================================================

    svr_.Post("/api/lcd", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setLcdDimmer(j.value("type",1), j.value("level",80)));
    });

    svr_.Post("/api/datetime", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setDateTime(j.value("dt","")));
    });

    svr_.Get("/api/datetime", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getDateTime()));
    });

    svr_.Get("/api/version", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getFirmwareVersion()));
    });

    svr_.Get("/api/id", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getRadioId()));
    });

    svr_.Post("/api/dial_step", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setDialStep(j.value("step",1)));
    });

    svr_.Get("/api/dial_step", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getDialStep()));
    });

    svr_.Post("/api/gp_out", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setGpOut(j.value("port",0), j.value("level",0)));
    });

    svr_.Post("/api/tx_power_battery", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setTxGnrlMaxPowerBattery(j.value("step",12)));
    });

    svr_.Post("/api/rptt_select", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setRpttSelect(j.value("sel",0)));
    });

    svr_.Get("/api/rptt_select", [this](const httplib::Request&, httplib::Response& res) {
        okJson(res, queryResult(rig_, CATCommand::getRpttSelect()));
    });

    svr_.Post("/api/mic_up",   [this](const httplib::Request&, httplib::Response& res){ ok(res,rig_.setMicUp()); });
    svr_.Post("/api/key_down", [this](const httplib::Request&, httplib::Response& res){ ok(res,rig_.setDownKey()); });

    svr_.Post("/api/channel", [this](const httplib::Request& req, httplib::Response& res) {
        auto j = parseBody(req);
        ok(res, rig_.setChannelUpDown(j.value("dir",1)));
    });

    // ====================================================================
    //  RAW CAT PASSTHROUGH
    // ====================================================================

    svr_.Post("/api/raw", [this](const httplib::Request& req, httplib::Response& res) {
        auto j   = parseBody(req);
        auto cmd = j.value("cmd","");
        if (cmd.empty()) { err(res, "cmd is required"); return; }
        okJson(res, queryResult(rig_, cmd));
    });

    svr_.Get("/api/raw", [this](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("cmd")) { err(res, "cmd param required"); return; }
        okJson(res, queryResult(rig_, req.get_param_value("cmd")));
    });
}
