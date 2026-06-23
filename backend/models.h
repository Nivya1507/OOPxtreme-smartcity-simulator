#ifndef MODELS_H
#define MODELS_H

#include <string>
#include <vector>
#include <ostream>
#include <sstream>
#include <iomanip>
#include <memory>
#include <algorithm>
#include "exceptions.h"

// Forward declarations
class Intersection;
class RoadSegment;

// Struct to represent a standard Route consisting of Intersection ID names
class Route {
private:
    std::string routeId;
    std::vector<std::string> stopIds;
public:
    Route() = default;
    Route(const std::string& id, const std::vector<std::string>& stops) 
        : routeId(id), stopIds(stops) {}

    std::string getId() const { return routeId; }
    size_t getStopCount() const { return stopIds.size(); }
    const std::vector<std::string>& getStops() const { return stopIds; }

    // Operator Overloading: operator[] to access stops
    std::string operator[](size_t index) const {
        if (index >= stopIds.size()) {
            throw InvalidSensorDataException("Route stop index out of bounds");
        }
        return stopIds[index];
    }
};

// Abstract Base Class: SmartCityComponent
class SmartCityComponent {
protected:
    std::string id;
public:
    explicit SmartCityComponent(const std::string& compId) : id(compId) {}
    virtual ~SmartCityComponent() = default;

    std::string getId() const { return id; }

    // Pure virtual functions
    virtual std::string serialize() const = 0;
    virtual void update(double dt) = 0;
};

// Interface: ElectricConsumer (for battery powered vehicles or elements)
class ElectricConsumer {
public:
    virtual ~ElectricConsumer() = default;
    virtual double getBatteryLevel() const = 0;
    virtual void consumeEnergy(double amount) = 0;
    virtual void charge(double amount) = 0;
};

// Base Class: TrafficSignal
class TrafficSignal : public SmartCityComponent {
protected:
    std::string currentLight; // "Red", "Yellow", "Green"
    double timer;
    double cycleDuration;
    std::string intersectionId;
public:
    TrafficSignal(const std::string& signalId, const std::string& interId, double cycleTime = 10.0)
        : SmartCityComponent(signalId), currentLight("Red"), timer(0.0), 
          cycleDuration(cycleTime), intersectionId(interId) {}

    virtual ~TrafficSignal() override = default;

    std::string getLightState() const { return currentLight; }
    void setLightState(const std::string& light) { currentLight = light; }
    std::string getIntersectionId() const { return intersectionId; }

    virtual void update(double dt) override {
        timer += dt;
        if (timer >= cycleDuration) {
            timer = 0.0;
            if (currentLight == "Red") {
                currentLight = "Green";
            } else if (currentLight == "Green") {
                currentLight = "Yellow";
            } else {
                currentLight = "Red";
            }
        }
    }

    virtual std::string serialize() const override {
        std::stringstream ss;
        ss << "{\"id\":\"" << id 
           << "\",\"type\":\"TrafficSignal"
           << "\",\"intersectionId\":\"" << intersectionId 
           << "\",\"light\":\"" << currentLight 
           << "\",\"timer\":" << std::fixed << std::setprecision(1) << timer << "}";
        return ss.str();
    }
};

// Derived Class: AdaptiveSignal (polymorphic overrides based on density)
class AdaptiveSignal : public TrafficSignal {
private:
    int vehicleQueueSize;
public:
    AdaptiveSignal(const std::string& signalId, const std::string& interId, double baseCycle = 10.0)
        : TrafficSignal(signalId, interId, baseCycle), vehicleQueueSize(0) {}

    void setQueueSize(int size) {
        if (size < 0) throw InvalidSensorDataException("Queue size cannot be negative");
        vehicleQueueSize = size;
    }

    // Overloaded timing adjustments based on congestion density
    void optimizeTiming() {
        if (vehicleQueueSize > 5) {
            // High queue size on this node: extend green light or shorten red
            cycleDuration = 6.0; // Faster cycle or extended green window
        } else {
            cycleDuration = 12.0;
        }
    }

    virtual void update(double dt) override {
        optimizeTiming();
        TrafficSignal::update(dt);
    }

    virtual std::string serialize() const override {
        std::stringstream ss;
        ss << "{\"id\":\"" << id 
           << "\",\"type\":\"AdaptiveSignal"
           << "\",\"intersectionId\":\"" << intersectionId 
           << "\",\"light\":\"" << currentLight 
           << "\",\"timer\":" << std::fixed << std::setprecision(1) << timer 
           << ",\"queueSize\":" << vehicleQueueSize 
           << ",\"cycle\":" << cycleDuration << "}";
        return ss.str();
    }
};

// Base Class: Vehicle
class Vehicle : public SmartCityComponent {
protected:
    double x, y;
    double speed;
    double maxSpeed;
    Route route;
    size_t currentStopIndex;
    bool isMoving;
    double progress; // Distance along current segment [0, 1]
    std::string currentSegmentStart;
    std::string currentSegmentEnd;

    // Help coordinate interpolation
    double startX, startY;
    double endX, endY;

public:
    Vehicle(const std::string& vId, double maxSpd, const Route& r)
        : SmartCityComponent(vId), x(0.0), y(0.0), speed(0.0), maxSpeed(maxSpd), 
          route(r), currentStopIndex(0), isMoving(false), progress(0.0) {
        if (route.getStopCount() < 2) {
            throw InvalidSensorDataException("Vehicle route must have at least 2 stops");
        }
    }

    virtual ~Vehicle() override = default;

    void setPositions(double sx, double sy, double ex, double ey) {
        startX = sx; startY = sy;
        endX = ex; endY = ey;
        x = startX; y = startY;
    }

    void setSegment(const std::string& from, const std::string& to) {
        currentSegmentStart = from;
        currentSegmentEnd = to;
    }

    std::string getSegmentStart() const { return currentSegmentStart; }
    std::string getSegmentEnd() const { return currentSegmentEnd; }
    double getProgress() const { return progress; }
    void setProgress(double p) { progress = p; }
    double getX() const { return x; }
    double getY() const { return y; }
    bool getIsMoving() const { return isMoving; }
    void setIsMoving(bool moving) { isMoving = moving; }
    double getSpeed() const { return speed; }
    Route getRoute() const { return route; }
    size_t getCurrentStopIndex() const { return currentStopIndex; }
    void setCurrentStopIndex(size_t index) { currentStopIndex = index; }

    virtual double getPriorityLevel() const {
        return 1.0; // Default priority
    }

    // Operator Overloading: Compare priorities for sorting / ordering
    bool operator<(const Vehicle& other) const {
        return this->getPriorityLevel() < other.getPriorityLevel();
    }

    virtual void update(double dt) override {
        if (!isMoving) return;

        // Move vehicle forward along segment
        double segmentDistance = 300.0; // Distance placeholder for grid steps
        double speedDelta = maxSpeed * dt * 0.15; // travel speed scale
        progress += speedDelta / segmentDistance;
        
        if (progress >= 1.0) {
            progress = 1.0;
            isMoving = false;
        }

        // Interpolate current positions
        x = startX + (endX - startX) * progress;
        y = startY + (endY - startY) * progress;
    }

    virtual std::string serialize() const override {
        std::stringstream ss;
        ss << "{\"id\":\"" << id 
           << "\",\"type\":\"Vehicle"
           << "\",\"x\":" << std::fixed << std::setprecision(1) << x 
           << ",\"y\":" << std::fixed << std::setprecision(1) << y 
           << ",\"speed\":" << speed 
           << ",\"progress\":" << progress 
           << ",\"route\":\"" << route.getId() 
           << "\",\"from\":\"" << currentSegmentStart 
           << "\",\"to\":\"" << currentSegmentEnd 
           << "\",\"isMoving\":" << (isMoving ? "true" : "false") << "}";
        return ss.str();
    }

    // Friend Operator Overloading for ostream
    friend std::ostream& operator<<(std::ostream& os, const Vehicle& vehicle) {
        os << "Vehicle[ID=" << vehicle.id 
           << ", Speed=" << vehicle.speed 
           << ", Pos=(" << vehicle.x << "," << vehicle.y << ")"
           << ", Segment=" << vehicle.currentSegmentStart << "->" << vehicle.currentSegmentEnd 
           << "]";
        return os;
    }
};

// Derived Class: TransitVehicle (Polymorphic details, tracks passengers)
class TransitVehicle : public Vehicle {
protected:
    int passengerCapacity;
    int currentPassengers;
    double fareRate;
    double totalFareCollected;
public:
    TransitVehicle(const std::string& vId, double maxSpd, const Route& r, int capacity, double fare)
        : Vehicle(vId, maxSpd, r), passengerCapacity(capacity), currentPassengers(0), 
          fareRate(fare), totalFareCollected(0.0) {}

    virtual ~TransitVehicle() override = default;

    int getPassengerCount() const { return currentPassengers; }
    double getTotalFareCollected() const { return totalFareCollected; }

    void boardPassengers(int count) {
        if (currentPassengers + count > passengerCapacity) {
            int boarded = passengerCapacity - currentPassengers;
            totalFareCollected += boarded * fareRate;
            currentPassengers = passengerCapacity;
        } else {
            currentPassengers += count;
            totalFareCollected += count * fareRate;
        }
    }

    void alightPassengers(int count) {
        currentPassengers = std::max(0, currentPassengers - count);
    }

    virtual double getPriorityLevel() const override {
        return 2.0; // Transit vehicles have higher priority than private cars
    }

    virtual std::string serialize() const override {
        std::stringstream ss;
        ss << "{\"id\":\"" << id 
           << "\",\"type\":\"TransitVehicle"
           << "\",\"x\":" << std::fixed << std::setprecision(1) << x 
           << ",\"y\":" << std::fixed << std::setprecision(1) << y 
           << ",\"speed\":" << speed 
           << ",\"progress\":" << progress 
           << ",\"route\":\"" << route.getId() 
           << "\",\"from\":\"" << currentSegmentStart 
           << "\",\"to\":\"" << currentSegmentEnd 
           << "\",\"isMoving\":" << (isMoving ? "true" : "false")
           << ",\"passengers\":" << currentPassengers 
           << ",\"capacity\":" << passengerCapacity 
           << ",\"fares\":" << totalFareCollected << "}";
        return ss.str();
    }
};

// Concrete Class: Bus (Single inheritance from TransitVehicle)
class Bus : public TransitVehicle {
public:
    Bus(const std::string& vId, double maxSpd, const Route& r)
        : TransitVehicle(vId, maxSpd, r, 40, 2.50) {} // Capacity 40, $2.50 fare

    virtual void update(double dt) override {
        // Basic movement logic
        TransitVehicle::update(dt);
        if (isMoving) {
            speed = maxSpeed * 0.8; // Heavy transit speed dampening
        } else {
            speed = 0.0;
        }
    }

    virtual std::string serialize() const override {
        std::stringstream ss;
        ss << "{\"id\":\"" << id 
           << "\",\"type\":\"Bus"
           << "\",\"x\":" << std::fixed << std::setprecision(1) << x 
           << ",\"y\":" << std::fixed << std::setprecision(1) << y 
           << ",\"speed\":" << std::fixed << std::setprecision(1) << speed 
           << ",\"progress\":" << progress 
           << ",\"route\":\"" << route.getId() 
           << "\",\"from\":\"" << currentSegmentStart 
           << "\",\"to\":\"" << currentSegmentEnd 
           << "\",\"isMoving\":" << (isMoving ? "true" : "false")
           << ",\"passengers\":" << currentPassengers 
           << ",\"capacity\":" << passengerCapacity 
           << ",\"fares\":" << totalFareCollected << "}";
        return ss.str();
    }
};

// Concrete Class: AutonomousElectricBus (Multiple Inheritance: TransitVehicle & ElectricConsumer)
class AutonomousElectricBus : public TransitVehicle, public ElectricConsumer {
private:
    double batteryCapacity; // in kWh
    double currentBattery;   // in %
    double dischargeRate;   // % discharge per simulation update
public:
    AutonomousElectricBus(const std::string& vId, double maxSpd, const Route& r)
        : TransitVehicle(vId, maxSpd, r, 50, 3.00), // Capacity 50, $3.00 fare
          batteryCapacity(150.0), currentBattery(100.0), dischargeRate(1.5) {}

    virtual ~AutonomousElectricBus() override = default;

    // Interface overrides
    virtual double getBatteryLevel() const override { return currentBattery; }
    virtual void consumeEnergy(double amount) override {
        currentBattery = std::max(0.0, currentBattery - amount);
        if (currentBattery <= 0.0) {
            throw BatteryDepletedException(getId());
        }
    }
    virtual void charge(double amount) override {
        currentBattery = std::min(100.0, currentBattery + amount);
    }

    virtual double getPriorityLevel() const override {
        return 3.0; // High eco priority
    }

    virtual void update(double dt) override {
        TransitVehicle::update(dt);
        if (isMoving) {
            speed = maxSpeed * 0.95; // Smooth electric torque
            // Consume battery based on motion
            try {
                consumeEnergy(dischargeRate * dt * 2.0);
            } catch (const BatteryDepletedException& e) {
                // Let simulator catch this, stop the vehicle immediately
                speed = 0.0;
                isMoving = false;
                throw; // Rethrow to propagate to the simulator log
            }
        } else {
            speed = 0.0;
            // Charge slightly at stations
            charge(dt * 4.0);
        }
    }

    virtual std::string serialize() const override {
        std::stringstream ss;
        ss << "{\"id\":\"" << id 
           << "\",\"type\":\"AutonomousElectricBus"
           << "\",\"x\":" << std::fixed << std::setprecision(1) << x 
           << ",\"y\":" << std::fixed << std::setprecision(1) << y 
           << ",\"speed\":" << std::fixed << std::setprecision(1) << speed 
           << ",\"progress\":" << progress 
           << ",\"route\":\"" << route.getId() 
           << "\",\"from\":\"" << currentSegmentStart 
           << "\",\"to\":\"" << currentSegmentEnd 
           << "\",\"isMoving\":" << (isMoving ? "true" : "false")
           << ",\"passengers\":" << currentPassengers 
           << ",\"capacity\":" << passengerCapacity 
           << ",\"fares\":" << totalFareCollected 
           << ",\"battery\":" << std::fixed << std::setprecision(1) << currentBattery << "}";
        return ss.str();
    }
};

// Concrete Class: EmergencyVehicle (Polymorphic routing, triggers signal overrides)
class EmergencyVehicle : public Vehicle {
private:
    std::string sirenMode; // "Siren_Active", "Siren_Off"
public:
    EmergencyVehicle(const std::string& vId, double maxSpd, const Route& r)
        : Vehicle(vId, maxSpd, r), sirenMode("Siren_Active") {}

    virtual double getPriorityLevel() const override {
        return 10.0; // Emergency vehicle priority is highest
    }

    void toggleSiren() {
        sirenMode = (sirenMode == "Siren_Active") ? "Siren_Off" : "Siren_Active";
    }

    virtual void update(double dt) override {
        Vehicle::update(dt);
        if (isMoving) {
            speed = maxSpeed * 1.5; // Exceed normal limits safely
        } else {
            speed = 0.0;
        }
    }

    virtual std::string serialize() const override {
        std::stringstream ss;
        ss << "{\"id\":\"" << id 
           << "\",\"type\":\"EmergencyVehicle"
           << "\",\"x\":" << std::fixed << std::setprecision(1) << x 
           << ",\"y\":" << std::fixed << std::setprecision(1) << y 
           << ",\"speed\":" << std::fixed << std::setprecision(1) << speed 
           << ",\"progress\":" << progress 
           << ",\"route\":\"" << route.getId() 
           << "\",\"from\":\"" << currentSegmentStart 
           << "\",\"to\":\"" << currentSegmentEnd 
           << "\",\"isMoving\":" << (isMoving ? "true" : "false")
           << ",\"siren\":\"" << sirenMode << "\"}";
        return ss.str();
    }
};

#endif // MODELS_H
