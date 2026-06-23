#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <string>
#include <thread>
#include "httplib.h"
#include "simulation.h"

std::mutex simMutex;
std::unique_ptr<SimulationEngine> engine;

// Helper function to read static file content
std::string getFileContent(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "File Not Found: " + path;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

int main() {
    std::cout << "[SERVER] Starting C++ backend server..." << std::endl;

    // Initialize simulation engine
    try {
        engine = std::make_unique<SimulationEngine>();
        engine->loadNetworkConfig("data/network_config.txt");
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Initialization failed: " << e.what() << std::endl;
        return 1;
    }

    httplib::Server svr;

    // 1. Static Asset Handlers
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(getFileContent("frontend/index.html"), "text/html");
    });
    svr.Get("/index.html", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(getFileContent("frontend/index.html"), "text/html");
    });
    svr.Get("/index.css", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(getFileContent("frontend/index.css"), "text/css");
    });
    svr.Get("/app.js", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(getFileContent("frontend/app.js"), "application/javascript");
    });

    // 2. REST API Handlers (Thread-safe via simMutex)
    svr.Get("/api/status", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(simMutex);
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(engine->getFullStateJSON(), "application/json");
    });

    svr.Get("/api/spawn", [](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(simMutex);
        res.set_header("Access-Control-Allow-Origin", "*");
        
        std::string type = req.get_param_value("type");
        std::string id = req.get_param_value("id");
        std::string routeId = req.get_param_value("route");

        if (type.empty() || id.empty() || routeId.empty()) {
            res.status = 400;
            res.set_content("{\"status\":\"error\",\"message\":\"Missing parameters type, id, or route\"}", "application/json");
            return;
        }

        try {
            engine->spawnVehicle(type, id, routeId);
            res.set_content("{\"status\":\"success\",\"message\":\"Vehicle spawned\"}", "application/json");
        } catch (const SmartCityException& e) {
            res.set_content(std::string("{\"status\":\"error\",\"message\":\"") + e.what() + "\"}", "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(std::string("{\"status\":\"error\",\"message\":\"") + e.what() + "\"}", "application/json");
        }
    });

    svr.Get("/api/step", [](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(simMutex);
        res.set_header("Access-Control-Allow-Origin", "*");

        double dt = 1.0;
        std::string dtParam = req.get_param_value("dt");
        if (!dtParam.empty()) {
            try {
                dt = std::stod(dtParam);
            } catch (...) {
                dt = 1.0;
            }
        }

        try {
            engine->step(dt);
            res.set_content("{\"status\":\"success\"}", "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(std::string("{\"status\":\"error\",\"message\":\"") + e.what() + "\"}", "application/json");
        }
    });

    svr.Get("/api/override-signal", [](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(simMutex);
        res.set_header("Access-Control-Allow-Origin", "*");

        std::string sigId = req.get_param_value("id");
        std::string state = req.get_param_value("state"); // "Red", "Yellow", "Green"

        if (sigId.empty() || state.empty()) {
            res.status = 400;
            res.set_content("{\"status\":\"error\",\"message\":\"Missing parameters id or state\"}", "application/json");
            return;
        }

        try {
            engine->overrideSignal(sigId, state);
            res.set_content("{\"status\":\"success\"}", "application/json");
        } catch (const std::exception& e) {
            res.set_content(std::string("{\"status\":\"error\",\"message\":\"") + e.what() + "\"}", "application/json");
        }
    });

    svr.Get("/api/block-road", [](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(simMutex);
        res.set_header("Access-Control-Allow-Origin", "*");

        std::string from = req.get_param_value("from");
        std::string to = req.get_param_value("to");
        std::string blockedParam = req.get_param_value("blocked");

        if (from.empty() || to.empty() || blockedParam.empty()) {
            res.status = 400;
            res.set_content("{\"status\":\"error\",\"message\":\"Missing parameters from, to, or blocked\"}", "application/json");
            return;
        }

        bool blocked = (blockedParam == "1" || blockedParam == "true");

        try {
            engine->setRoadBlocked(from, to, blocked);
            res.set_content("{\"status\":\"success\"}", "application/json");
        } catch (const std::exception& e) {
            res.set_content(std::string("{\"status\":\"error\",\"message\":\"") + e.what() + "\"}", "application/json");
        }
    });

    // Start server on localhost:8080
    std::cout << "[SERVER] Localhost server listening on http://localhost:8080" << std::endl;
    svr.listen("127.0.0.1", 8080);

    return 0;
}
