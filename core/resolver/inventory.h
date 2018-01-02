#ifndef INVENTORY_H
#define INVENTORY_H

#include <qpid/messaging/Connection.h>
#include <qpid/messaging/Message.h>
#include <qpid/messaging/Receiver.h>
#include <qpid/messaging/Sender.h>
#include <qpid/messaging/Session.h>
#include <qpid/messaging/Address.h>

#include <string>

#include <sqlite3.h>

#include <boost/filesystem.hpp>

using namespace qpid::messaging;
using namespace qpid::types;
namespace fs = ::boost::filesystem;

class Inventory {
public:
    Inventory(const fs::path &dbfile);
    ~Inventory();
    void close();

    bool isDeviceRegistered(std::string uuid);
    void deleteDevice(std::string uuid);

    std::string getDeviceName(std::string uuid);
    std::string getDeviceRoom(std::string uuid);
    bool setDeviceName(std::string uuid, std::string name);
    std::string getRoomName(std::string uuid);
    bool setRoomName(std::string uuid, std::string name);
    bool setDeviceRoom(std::string deviceuuid, std::string roomuuid);
    std::string getDeviceRoomName(std::string uuid);
    Variant::Map getRooms();
    bool deleteRoom(std::string uuid);

    std::string getFloorplanName(std::string uuid);
    bool setFloorplanName(std::string uuid, std::string name);
    bool setDeviceFloorplan(std::string deviceuuid, std::string floorplanuuid, int x, int y);
    void delDeviceFloorplan(std::string deviceuuid, std::string floorplanuuid);
    bool deleteFloorplan(std::string uuid);
    Variant::Map getFloorplans();

    std::string getLocationName(std::string uuid);
    std::string getRoomLocation(std::string uuid);
    bool setLocationName(std::string uuid, std::string name);
    bool setRoomLocation(std::string roomuuid, std::string locationuuid);
    bool deleteLocation(std::string uuid);
    Variant::Map getLocations();

    bool createUser(std::string uuid, std::string username, std::string password, std::string pin, std::string description);
    bool deleteUser(std::string uuid);
    bool authUser(std::string uuid);
    bool setPassword(std::string uuid);
    bool setPin(std::string uuid);
    bool setPermission(std::string uuid, std::string permission);
    bool deletePermission(std::string uuid, std::string permission);
    Variant::Map getPermissions(std::string uuid);


private:
    sqlite3 *db;
    std::string getFirst(const char *query);
    std::string getFirst(const char *query, int n, ...);
    std::string getFirstFound(const char *query, bool &found, int n, ...);
    std::string getFirstArgs(const char *query, bool &found, int n, va_list args);
    bool createTableIfNotExist(std::string tablename, std::string createquery);
};

#endif
