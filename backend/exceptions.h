#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <exception>
#include <string>

// Base Exception for Smart City Simulator
class SmartCityException : public std::exception {
protected:
    std::string message;
public:
    explicit SmartCityException(const std::string& msg) : message(msg) {}
    virtual const char* what() const noexcept override {
        return message.c_str();
    }
};

// Exception triggered when an Electric Vehicle's battery is empty
class BatteryDepletedException : public SmartCityException {
private:
    std::string vehicleId;
public:
    explicit BatteryDepletedException(const std::string& vId) 
        : SmartCityException("BatteryDepletedException: Vehicle " + vId + " battery depleted to 0%! Emergency backup power engaged."),
          vehicleId(vId) {}
    
    std::string getVehicleId() const { return vehicleId; }
};

// Exception triggered when vehicle density on a road segment exceeds capacity
class GridlockException : public SmartCityException {
private:
    std::string roadSegment;
public:
    explicit GridlockException(const std::string& segment) 
        : SmartCityException("GridlockException: Road segment '" + segment + "' is gridlocked! Capacity exceeded."),
          roadSegment(segment) {}
    
    std::string getRoadSegment() const { return roadSegment; }
};

// Exception triggered when a vehicle tries to navigate a blocked route
class RouteBlockedException : public SmartCityException {
private:
    std::string vehicleId;
    std::string blockedSegment;
public:
    RouteBlockedException(const std::string& vId, const std::string& segment) 
        : SmartCityException("RouteBlockedException: Vehicle " + vId + " path blocked at road segment '" + segment + "'! Rerouting required."),
          vehicleId(vId), blockedSegment(segment) {}
    
    std::string getVehicleId() const { return vehicleId; }
    std::string getBlockedSegment() const { return blockedSegment; }
};

// Exception triggered if sensor/telemetry inputs are corrupted
class InvalidSensorDataException : public SmartCityException {
public:
    explicit InvalidSensorDataException(const std::string& reason) 
        : SmartCityException("InvalidSensorDataException: " + reason) {}
};

#endif // EXCEPTIONS_H
