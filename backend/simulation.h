#ifndef SIMULATION_H
#define SIMULATION_H

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include "models.h"
#include "exceptions.h"

// Structure to represent Intersections (Graph Nodes)
struct Intersection {
    std::string id;
    double x, y;
    std::string name;

    Intersection(const std::string& nodeId, double nx, double ny, const std::string& nodeName)
        : id(nodeId), x(nx), y(ny), name(nodeName) {}
};

// Structure to represent Road Segments (Graph Edges)
struct RoadSegment {
    std::string id; // Format: "from_to"
    std::string fromNode;
    std::string toNode;
    double maxSpeed;
    int capacity;
    int currentVehicles;
    bool isBlocked;

    RoadSegment(const std::string& from, const std::string& to, double speedLimit, int cap)
        : id(from + "_" + to), fromNode(from), toNode(to), maxSpeed(speedLimit), 
          capacity(cap), currentVehicles(0), isBlocked(false) {}
};

class SimulationEngine {
private:
    std::vector<std::unique_ptr<Intersection>> intersections;
    std::vector<std::unique_ptr<RoadSegment>> roadSegments;
    std::vector<std::unique_ptr<Vehicle>> vehicles;
    std::vector<std::unique_ptr<TrafficSignal>> signals;
    std::vector<Route> routes;

    std::vector<std::string> logEvents; // Memory log of events to send to the UI
    std::string logFilePath;

    // Helper: Write event to file and UI log
    void logEvent(const std::string& event) {
        logEvents.push_back(event);
        if (logEvents.size() > 50) {
            logEvents.erase(logEvents.begin());
        }

        std::ofstream logFile(logFilePath, std::ios::app);
        if (logFile.is_open()) {
            logFile << "[SIM LOG] " << event << std::endl;
        }
    }

public:
    explicit SimulationEngine(const std::string& logPath = "data/simulation_log.txt") 
        : logFilePath(logPath) {
        // Clear log file on start
        std::ofstream logFile(logFilePath, std::ios::trunc);
        if (logFile.is_open()) {
            logFile << "=== Smart City Simulation Initialized ===" << std::endl;
        }
    }

    ~SimulationEngine() {
        // Explicitly clear pointer arrays to trigger destructors
        vehicles.clear();
        signals.clear();
        intersections.clear();
        roadSegments.clear();
    }

    // Load layout config
    void loadNetworkConfig(const std::string& configPath) {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            throw InvalidSensorDataException("Failed to open configuration file at: " + configPath);
        }

        std::string line;
        while (std::getline(file, line)) {
            // Trim whitespace
            if (line.empty() || line[0] == '#') continue;

            std::stringstream ss(line);
            std::string command;
            ss >> command;

            if (command == "NODE") {
                std::string id, name;
                double x, y;
                ss >> id >> x >> y;
                std::getline(ss, name);
                // trim leading space from name
                if (!name.empty() && name[0] == ' ') {
                    name = name.substr(1);
                }
                intersections.push_back(std::make_unique<Intersection>(id, x, y, name));
                
                // Add an adaptive traffic signal at every node
                std::string sigId = "Signal_" + id;
                signals.push_back(std::make_unique<AdaptiveSignal>(sigId, id, 10.0));
            } 
            else if (command == "EDGE") {
                std::string from, to;
                double maxSpeed;
                int capacity;
                ss >> from >> to >> maxSpeed >> capacity;
                roadSegments.push_back(std::make_unique<RoadSegment>(from, to, maxSpeed, capacity));
            } 
            else if (command == "ROUTE") {
                std::string routeId, stop;
                std::vector<std::string> stops;
                ss >> routeId;
                while (ss >> stop) {
                    stops.push_back(stop);
                }
                routes.push_back(Route(routeId, stops));
            }
        }
        logEvent("Loaded " + std::to_string(intersections.size()) + " intersections, " +
                 std::to_string(roadSegments.size()) + " roads, and " +
                 std::to_string(routes.size()) + " transit routes.");
    }

    // Lookup intersection pointer
    Intersection* findIntersection(const std::string& id) {
        for (auto& inter : intersections) {
            if (inter->id == id) return inter.get();
        }
        return nullptr;
    }

    // Lookup road segment pointer
    RoadSegment* findRoadSegment(const std::string& from, const std::string& to) {
        for (auto& road : roadSegments) {
            if (road->fromNode == from && road->toNode == to) return road.get();
        }
        return nullptr;
    }

    // Lookup route structure by ID
    Route findRoute(const std::string& routeId) {
        for (const auto& r : routes) {
            if (r.getId() == routeId) return r;
        }
        throw InvalidSensorDataException("Route " + routeId + " not found!");
    }

    // Block or unblock a road segment
    void setRoadBlocked(const std::string& from, const std::string& to, bool blocked) {
        RoadSegment* road = findRoadSegment(from, to);
        if (road) {
            road->isBlocked = blocked;
            logEvent("Road segment " + from + " -> " + to + " has been " + 
                     (blocked ? "BLOCKED" : "UNBLOCKED") + ".");
        } else {
            throw InvalidSensorDataException("Road segment " + from + " -> " + to + " does not exist.");
        }
    }

    // Override a specific signal to a light color
    void overrideSignal(const std::string& signalId, const std::string& state) {
        for (auto& sig : signals) {
            if (sig->getId() == signalId) {
                sig->setLightState(state);
                logEvent("Signal " + signalId + " overridden to: " + state);
                return;
            }
        }
        throw InvalidSensorDataException("Signal " + signalId + " not found.");
    }

    // Spawn a new vehicle
    void spawnVehicle(const std::string& type, const std::string& vId, const std::string& routeId) {
        // Prevent duplicate IDs
        for (const auto& v : vehicles) {
            if (v->getId() == vId) {
                throw InvalidSensorDataException("Vehicle with ID " + vId + " already exists!");
            }
        }

        Route r = findRoute(routeId);
        std::unique_ptr<Vehicle> newVehicle;

        if (type == "Bus") {
            newVehicle = std::make_unique<Bus>(vId, 45.0, r);
        } else if (type == "ElectricBus") {
            newVehicle = std::make_unique<AutonomousElectricBus>(vId, 50.0, r);
        } else if (type == "Emergency") {
            newVehicle = std::make_unique<EmergencyVehicle>(vId, 70.0, r);
        } else {
            throw InvalidSensorDataException("Unknown vehicle type: " + type);
        }

        // Initialize positions to first segment
        std::string startStop = r[0];
        std::string endStop = r[1];
        Intersection* startInter = findIntersection(startStop);
        Intersection* endInter = findIntersection(endStop);

        if (!startInter || !endInter) {
            throw InvalidSensorDataException("Invalid stop nodes in route: " + routeId);
        }

        RoadSegment* segment = findRoadSegment(startStop, endStop);
        if (!segment) {
            throw InvalidSensorDataException("No road links the nodes: " + startStop + " -> " + endStop);
        }

        if (segment->currentVehicles >= segment->capacity) {
            throw GridlockException(segment->id);
        }

        segment->currentVehicles++;
        newVehicle->setPositions(startInter->x, startInter->y, endInter->x, endInter->y);
        newVehicle->setSegment(startStop, endStop);
        newVehicle->setCurrentStopIndex(0);
        newVehicle->setIsMoving(true);

        vehicles.push_back(std::move(newVehicle));
        logEvent("Spawned " + type + " ID: " + vId + " on Route: " + routeId + ".");
    }

    // Run one step of the simulation
    void step(double dt) {
        // 1. Update Traffic Signals (polymorphic queue adjustments and states)
        for (auto& sig : signals) {
            // Count vehicles waiting at the intersection of this signal
            std::string interId = sig->getIntersectionId();
            int waitingCount = 0;
            for (const auto& v : vehicles) {
                if (v->getSegmentEnd() == interId && !v->getIsMoving()) {
                    waitingCount++;
                }
            }

            // Downcast to AdaptiveSignal if possible to adjust timers
            AdaptiveSignal* adaptSig = dynamic_cast<AdaptiveSignal*>(sig.get());
            if (adaptSig) {
                adaptSig->setQueueSize(waitingCount);
            }

            sig->update(dt);
        }

        // 2. Update Vehicles
        for (size_t i = 0; i < vehicles.size(); i++) {
            auto& vehicle = vehicles[i];

            // If the vehicle was already marked not moving but is at the end of its segment,
            // try to transition it to the next segment!
            if (!vehicle->getIsMoving() && vehicle->getProgress() >= 1.0) {
                // Determine next segment in route
                Route r = vehicle->getRoute();
                size_t currentStopIndex = vehicle->getCurrentStopIndex();
                size_t nextStopIndex = (currentStopIndex + 1) % r.getStopCount();
                size_t targetStopIndex = (nextStopIndex + 1) % r.getStopCount();
                size_t newStopIndex = nextStopIndex;

                if (nextStopIndex == r.getStopCount() - 1) {
                    targetStopIndex = 1;
                    newStopIndex = 0;
                }

                std::string from = r[nextStopIndex];
                std::string to = r[targetStopIndex];

                try {
                    // Check if segment is blocked
                    RoadSegment* currentSegment = findRoadSegment(vehicle->getSegmentStart(), vehicle->getSegmentEnd());
                    RoadSegment* nextSegment = findRoadSegment(from, to);

                    if (!nextSegment) {
                        throw InvalidSensorDataException("Road segment " + from + " -> " + to + " does not exist on route.");
                    }

                    if (nextSegment->isBlocked) {
                        throw RouteBlockedException(vehicle->getId(), nextSegment->id);
                    }

                    // Check if traffic light at "from" node (which is the end of the vehicle's CURRENT segment) is Red
                    bool hasPriorityOverride = false;
                    
                    // Polymorphism check: Check if vehicle is Emergency and has siren mode active
                    EmergencyVehicle* emerg = dynamic_cast<EmergencyVehicle*>(vehicle.get());
                    if (emerg) {
                        hasPriorityOverride = true;
                        // Turn traffic light green to let it pass
                        for (auto& sig : signals) {
                            if (sig->getIntersectionId() == from) {
                                sig->setLightState("Green");
                                logEvent("EmergencyVehicle " + vehicle->getId() + " overrode signal to GREEN at " + from);
                            }
                        }
                    }

                    // Look up current light at the intersection the vehicle is trying to exit
                    std::string lightState = "Green";
                    for (const auto& sig : signals) {
                        if (sig->getIntersectionId() == from) {
                            lightState = sig->getLightState();
                            break;
                        }
                    }

                    if (lightState == "Red" && !hasPriorityOverride) {
                        // Must wait! Leave speed at 0 and do not transition
                        vehicle->setIsMoving(false);
                        continue; 
                    }

                    // Check next segment capacity
                    if (nextSegment->currentVehicles >= nextSegment->capacity) {
                        throw GridlockException(nextSegment->id);
                    }

                    // Transition vehicle
                    if (currentSegment) {
                        currentSegment->currentVehicles = std::max(0, currentSegment->currentVehicles - 1);
                    }

                    nextSegment->currentVehicles++;
                    
                    Intersection* fromInter = findIntersection(from);
                    Intersection* toInter = findIntersection(to);
                    vehicle->setPositions(fromInter->x, fromInter->y, toInter->x, toInter->y);
                    vehicle->setSegment(from, to);
                    vehicle->setProgress(0.0);
                    vehicle->setCurrentStopIndex(newStopIndex);
                    vehicle->setIsMoving(true);

                    // Dynamic board passengers for Transit
                    TransitVehicle* transit = dynamic_cast<TransitVehicle*>(vehicle.get());
                    if (transit) {
                        transit->alightPassengers(rand() % 5);
                        transit->boardPassengers(rand() % 8);
                    }

                } catch (const RouteBlockedException& e) {
                    logEvent(e.what());
                    // In a real system we would reroute, here we skip the blocked segment 
                    // or toggle transit state to waiting
                    vehicle->setIsMoving(false);
                } catch (const GridlockException& e) {
                    logEvent(e.what());
                    vehicle->setIsMoving(false); // Idle in place
                } catch (const SmartCityException& e) {
                    logEvent(std::string("Simulation Error: ") + e.what());
                    vehicle->setIsMoving(false);
                }
            }

            // Normal update loop
            try {
                vehicle->update(dt);
            } 
            catch (const BatteryDepletedException& e) {
                logEvent(e.what());
                // Handle battery depletion: Tow vehicle to the nearest station (first node in route)
                logEvent("Towing vehicle " + vehicle->getId() + " to " + vehicle->getRoute()[0] + " for charging.");
                
                // Reset positions
                RoadSegment* currSeg = findRoadSegment(vehicle->getSegmentStart(), vehicle->getSegmentEnd());
                if (currSeg) currSeg->currentVehicles = std::max(0, currSeg->currentVehicles - 1);

                std::string firstStop = vehicle->getRoute()[0];
                std::string secondStop = vehicle->getRoute()[1];
                Intersection* n1 = findIntersection(firstStop);
                Intersection* n2 = findIntersection(secondStop);
                RoadSegment* nextSeg = findRoadSegment(firstStop, secondStop);

                if (n1 && n2 && nextSeg) {
                    nextSeg->currentVehicles++;
                    vehicle->setPositions(n1->x, n1->y, n2->x, n2->y);
                    vehicle->setSegment(firstStop, secondStop);
                    vehicle->setProgress(0.0);
                    vehicle->setCurrentStopIndex(0);
                    vehicle->setIsMoving(false);
                    
                    // Charge the vehicle to full after towing
                    AutonomousElectricBus* electric = dynamic_cast<AutonomousElectricBus*>(vehicle.get());
                    if (electric) {
                        electric->charge(100.0);
                        logEvent("Vehicle " + vehicle->getId() + " recharged to 100%. Ready to resume.");
                    }
                }
            }
            catch (const std::exception& e) {
                logEvent(std::string("Std Exception: ") + e.what());
            }
        }
    }

    // Serialize full simulation state to JSON
    std::string getFullStateJSON() {
        std::stringstream ss;
        ss << "{";

        // Intersections
        ss << "\"intersections\":[";
        for (size_t i = 0; i < intersections.size(); ++i) {
            ss << "{\"id\":\"" << intersections[i]->id 
               << "\",\"name\":\"" << intersections[i]->name 
               << "\",\"x\":" << intersections[i]->x 
               << ",\"y\":" << intersections[i]->y << "}";
            if (i < intersections.size() - 1) ss << ",";
        }
        ss << "],";

        // Roads
        ss << "\"roads\":[";
        for (size_t i = 0; i < roadSegments.size(); ++i) {
            ss << "{\"id\":\"" << roadSegments[i]->id 
               << "\",\"from\":\"" << roadSegments[i]->fromNode 
               << "\",\"to\":\"" << roadSegments[i]->toNode 
               << "\",\"blocked\":" << (roadSegments[i]->isBlocked ? "true" : "false")
               << ",\"vehicles\":" << roadSegments[i]->currentVehicles 
               << ",\"capacity\":" << roadSegments[i]->capacity << "}";
            if (i < roadSegments.size() - 1) ss << ",";
        }
        ss << "],";

        // Signals
        ss << "\"signals\":[";
        for (size_t i = 0; i < signals.size(); ++i) {
            ss << signals[i]->serialize();
            if (i < signals.size() - 1) ss << ",";
        }
        ss << "],";

        // Vehicles
        ss << "\"vehicles\":[";
        for (size_t i = 0; i < vehicles.size(); ++i) {
            ss << vehicles[i]->serialize();
            if (i < vehicles.size() - 1) ss << ",";
        }
        ss << "],";

        // Logs
        ss << "\"logs\":[";
        for (size_t i = 0; i < logEvents.size(); ++i) {
            // Escape any quotes in logs
            std::string escLog = logEvents[i];
            size_t pos = 0;
            while ((pos = escLog.find("\"", pos)) != std::string::npos) {
                escLog.replace(pos, 1, "\\\"");
                pos += 2;
            }
            ss << "\"" << escLog << "\"";
            if (i < logEvents.size() - 1) ss << ",";
        }
        ss << "]";

        ss << "}";
        return ss.str();
    }
};

#endif // SIMULATION_H
