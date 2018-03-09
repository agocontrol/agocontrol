#ifndef INVENTORY_H
#define INVENTORY_H

#include <string>
#include <json/json.h>
#include <sqlite3.h>
#include <boost/filesystem.hpp>

namespace fs = ::boost::filesystem;

class Inventory {
public:
    Inventory(const fs::path &dbfile);
    ~Inventory();

    bool isDeviceRegistered(const std::string& uuid);
    void deleteDevice(const std::string& uuid);

    std::string getDeviceName(const std::string& uuid);
    std::string getDeviceRoom(const std::string& uuid);
    bool setDeviceName(const std::string& uuid, const std::string& name);
    std::string getRoomName(const std::string& uuid);
    bool setRoomName(const std::string& uuid, const std::string& name);
    bool setDeviceRoom(const std::string& deviceuuid, const std::string& roomuuid);
    std::string getDeviceRoomName(const std::string& uuid);
    Json::Value getRooms();
    bool deleteRoom(const std::string& uuid);

    std::string getFloorplanName(const std::string& uuid);
    bool setFloorplanName(const std::string& uuid, const std::string& name);
    bool setDeviceFloorplan(const std::string& deviceuuid, const std::string& floorplanuuid, int x, int y);
    void delDeviceFloorplan(const std::string& deviceuuid, const std::string& floorplanuuid);
    bool deleteFloorplan(const std::string& uuid);
    Json::Value getFloorplans();

    std::string getLocationName(const std::string& uuid);
    std::string getRoomLocation(const std::string& uuid);
    bool setLocationName(const std::string& uuid, const std::string& name);
    bool setRoomLocation(const std::string& roomuuid, const std::string& locationuuid);
    bool deleteLocation(const std::string& uuid);
    Json::Value getLocations();

    bool createUser(const std::string& uuid, const std::string& username, const std::string& password, const std::string& pin, const std::string& description);
    bool deleteUser(const std::string& uuid);
    bool authUser(const std::string& uuid);
    bool setPassword(const std::string& uuid);
    bool setPin(const std::string& uuid);
    bool setPermission(const std::string& uuid, const std::string& permission);
    bool deletePermission(const std::string& uuid, const std::string& permission);
    Json::Value getPermissions(const std::string& uuid);


private:
    sqlite3 *db;
    std::string getFirst(const char *query);
    std::string getFirst(const char *query, int n, ...);
    std::string getFirstFound(const char *query, bool &found, int n, ...);
    std::string getFirstArgs(const char *query, bool &found, int n, va_list args);
    bool createTableIfNotExist(const std::string &tablename, const std::string &createquery);
};

#endif
