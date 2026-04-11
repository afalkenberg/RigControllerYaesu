/**
 * FTX1HttpServer.h
 * ================
 * HTTP REST API server for the FTX-1 CAT controller.
 * Wraps cpp-httplib and exposes every FTX1Controller method
 * as a JSON endpoint.
 *
 * All POST endpoints accept and return JSON.
 * All GET endpoints return JSON.
 * CORS is enabled for all origins (browser GUI access).
 *
 * Depends on: httplib.h, json.hpp, FTX1Controller.h
 */

#pragma once
#include "httplib.h"
#include "json.hpp"
#include "FTX1Controller.h"
#include <string>
#include <thread>
#include <atomic>

using json = nlohmann::json;

class FTX1HttpServer {
public:
    explicit FTX1HttpServer(FTX1Controller& rig);
    ~FTX1HttpServer();

    // Start listening — blocks until stop() is called from another thread
    bool listen(const std::string& host = "0.0.0.0", int port = 8080);

    // Stop the server
    void stop();

    // Set path to serve static files from (GUI html/js/css)
    void setStaticRoot(const std::string& path);

private:
    void setupRoutes();

    // ── Route helpers ────────────────────────────────────────────────────
    // Parse JSON body, return empty object on failure
    static json parseBody(const httplib::Request& req);

    // Build a standard ok/error response
    static void ok(httplib::Response& res, bool success = true);
    static void okJson(httplib::Response& res, const json& j);
    static void err(httplib::Response& res, const std::string& msg);

    // Apply CORS headers to every response
    static void cors(httplib::Response& res);

    FTX1Controller& rig_;
    httplib::Server svr_;
    std::string     staticRoot_ = ".";
};
