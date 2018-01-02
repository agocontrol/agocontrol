#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <sstream>
#include <stdlib.h>
#include <stdarg.h>

#include "inventory.h"
#include "agolog.h"

namespace fs = ::boost::filesystem;

bool Inventory::createTableIfNotExist(std::string tablename, std::string createquery) {
    std::string query = "SELECT name FROM sqlite_master WHERE type='table' AND name = ?";
    if (getFirst(query.c_str(), 1, tablename.c_str()) != tablename) {
        AGO_INFO() << "Creating missing table '" << tablename << "'";
        getFirst(createquery.c_str());
        if (getFirst(query.c_str(), 1, tablename.c_str()) != tablename) {
            AGO_ERROR() << "Can't create table '" << tablename << "'";
            return false;
        }
    }
    return true;
}

Inventory::Inventory(const fs::path &dbfile) {
    int rc = sqlite3_open(dbfile.c_str(), &db);
    if( rc != SQLITE_OK ){
        AGO_ERROR() << "Can't open database " << dbfile.string() << ": " << sqlite3_errmsg(db);
        throw std::runtime_error("Cannot open database");
    }
    AGO_DEBUG() << "Succesfully opened database: " << dbfile.c_str();

    createTableIfNotExist("devices","CREATE TABLE devices (uuid text, name text, room text)");
    createTableIfNotExist("rooms", "CREATE TABLE rooms (uuid text, name text, location text)");
    createTableIfNotExist("floorplans", "CREATE TABLE floorplans (uuid text, name text)");
    createTableIfNotExist("devicesfloorplan", "CREATE TABLE devicesfloorplan (floorplan text, device text, x integer, y integer)");
    createTableIfNotExist("locations", "CREATE TABLE locations (uuid text, name text, description text)");
    createTableIfNotExist("users", "CREATE TABLE users (uuid text, username text, password text, pin text, description text)");
}

Inventory::~Inventory() {
    close();
}

void Inventory::close() {
    if(db) {
        sqlite3_close(db);
        db = NULL;
    }
}

std::string Inventory::getDeviceName(std::string uuid) {
    std::string query = "SELECT name FROM devices WHERE uuid = ?";
    return getFirst(query.c_str(), 1, uuid.c_str());
}

std::string Inventory::getDeviceRoom(std::string uuid) {
    std::string query = "SELECT room FROM devices WHERE uuid = ?";
    return getFirst(query.c_str(), 1, uuid.c_str());
}

bool Inventory::isDeviceRegistered(std::string uuid) {
    std::string query = "SELECT name FROM devices WHERE uuid = ?";
    bool found = false;
    getFirstFound(query.c_str(), found, 1, uuid.c_str());
    return found;
}

void Inventory::deleteDevice(std::string uuid) {
    std::string query = "DELETE FROM devices WHERE uuid = ?";
    getFirst(query.c_str(), 1, uuid.c_str());
}

bool Inventory::setDeviceName(std::string uuid, std::string name) {
    if (!isDeviceRegistered(uuid)) {
        std::string query = "INSERT INTO devices (name, uuid) VALUES (?, ?)";
        AGO_DEBUG() << "creating device: " << query.c_str();
        getFirst(query.c_str(), 2, name.c_str(), uuid.c_str());
    } else {
        std::string query = "UPDATE devices SET name = ? WHERE uuid = ?";
        getFirst(query.c_str(), 2, name.c_str(), uuid.c_str());
    }

    return getDeviceName(uuid) == name;
}

std::string Inventory::getRoomName(std::string uuid) {
    std::string query = "SELECT name from rooms WHERE uuid = ?";
    return getFirst(query.c_str(), 1, uuid.c_str());
}

bool Inventory::setRoomName(std::string uuid, std::string name) {
    if (getRoomName(uuid) == "") { // does not exist, create
        std::string query = "INSERT INTO rooms (name, uuid) VALUES (?, ?)";
        AGO_DEBUG() << "creating room: " << query.c_str();
        getFirst(query.c_str(), 2, name.c_str(), uuid.c_str());
    } else {
        std::string query = "UPDATE rooms SET name = ? WHERE uuid = ?";
        getFirst(query.c_str(), 2, name.c_str(), uuid.c_str());
    }
    return getRoomName(uuid) == name;
} 

bool Inventory::setDeviceRoom(std::string deviceuuid, std::string roomuuid) {
    std::string query = "UPDATE devices SET room = ? WHERE uuid = ?";
    getFirst(query.c_str(), 2, roomuuid.c_str(), deviceuuid.c_str());
    return getDeviceRoom(deviceuuid) == roomuuid;
}

std::string Inventory::getDeviceRoomName(std::string uuid) {
    std::string query = "SELECT room FROM devices WHERE uuid = ?";
    return getRoomName(getFirst(query.c_str(), 1, uuid.c_str()));
} 

Variant::Map Inventory::getRooms() {
    Variant::Map result;
    sqlite3_stmt *stmt;
    int rc;

    rc = sqlite3_prepare_v2(db, "SELECT uuid, name, location from rooms", -1, &stmt, NULL);
    if(rc!=SQLITE_OK) {
        AGO_ERROR() << "sql error #" << rc << ": " << sqlite3_errmsg(db);
        return result;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Variant::Map entry;
        const char *roomname = (const char*)sqlite3_column_text(stmt, 1);
        const char *location = (const char*)sqlite3_column_text(stmt, 2);
        const char *uuid = (const char*)sqlite3_column_text(stmt, 0);
        if (roomname != NULL) {
            entry["name"] = std::string(roomname);
        } else {
            entry["name"] = "";
        }

        if (location != NULL) {
            entry["location"] = std::string(location);
        } else {	
            entry["location"] = "";
        }
        if (uuid != NULL) {
            result[uuid] = entry;
        }
    }

    sqlite3_finalize (stmt);

    return result;
}

bool Inventory::deleteRoom(std::string uuid) {
    getFirst("BEGIN");
    std::string query = "update devices set room = '' WHERE room = ?";
    getFirst(query.c_str(), 1, uuid.c_str());
    query = "delete from rooms WHERE uuid = ?";
    getFirst(query.c_str(), 1, uuid.c_str());

    if (getRoomName(uuid) != "") {
        getFirst("ROLLBACK");
        return false;
    } else {
        getFirst("COMMIT");
        return true;
    }
}

std::string Inventory::getFirst(const char *query) {
    bool ignored = false;
    return getFirst(query, ignored, 0);
}

std::string Inventory::getFirst(const char *query, int n, ...) {
    bool ignored = false;
    va_list args;
    va_start(args, n);
    std::string ret = getFirstArgs(query, ignored, n, args);
    va_end(args);
    return ret;
}

std::string Inventory::getFirstFound(const char *query, bool &found, int n, ...) {
    va_list args;
    va_start(args, n);
    std::string ret = getFirstArgs(query, found, n, args);
    va_end(args);
    return ret;
}

std::string Inventory::getFirstArgs(const char *query, bool &found, int n, va_list args) {
    sqlite3_stmt *stmt;
    int rc, i;
    std::string result;
    found = false;

    rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if(rc != SQLITE_OK) {
        AGO_ERROR() << "sql error #" << rc << ": " << sqlite3_errmsg(db);
        return result;
    }


    for(i = 0; i < n; i++) {
        sqlite3_bind_text(stmt, i + 1, va_arg(args, char*), -1, NULL);
    }

    rc = sqlite3_step(stmt);
    switch(rc) {
        case SQLITE_ERROR:
            AGO_ERROR() << "step error: " << sqlite3_errmsg(db);
            break;
        case SQLITE_ROW:
            if (sqlite3_column_type(stmt, 0) == SQLITE_TEXT) {
                result = std::string((const char *) sqlite3_column_text(stmt, 0));
                found = true;
            }
            break;
    }

    sqlite3_finalize(stmt);

    return result;
}

std::string Inventory::getFloorplanName(std::string uuid) {
    std::string query = "SELECT name from floorplans WHERE uuid = ?";
    return getFirst(query.c_str(), 1, uuid.c_str());
}

bool Inventory::setFloorplanName(std::string uuid, std::string name) {
    if (getFloorplanName(uuid) == "") { // does not exist, create
        std::string query = "insert into floorplans (name, uuid) VALUES (?, ?)";
        AGO_DEBUG() << "creating floorplan: " << query.c_str();
        getFirst(query.c_str(), 2, name.c_str(), uuid.c_str());
    } else {
        std::string query = "update floorplans set name = ? WHERE uuid = ?";
        getFirst(query.c_str(), 2, name.c_str(), uuid.c_str());
    }

    return getFloorplanName(uuid) == name;
}

bool Inventory::setDeviceFloorplan(std::string deviceuuid, std::string floorplanuuid, int x, int y) {
    std::stringstream xstr, ystr;
    xstr << x;
    ystr << y;
    std::string query = "SELECT floorplan from devicesfloorplan WHERE floorplan = ? and device = ?";
    if (getFirst(query.c_str(), 2, floorplanuuid.c_str(), deviceuuid.c_str())==floorplanuuid) {
        // already exists, update
        query = "update devicesfloorplan set x=?, y=? WHERE floorplan = ? and device = ?";
        getFirst(query.c_str(), 4, xstr.str().c_str(), ystr.str().c_str(), floorplanuuid.c_str(), deviceuuid.c_str());

    } else {
        // create new record
        query = "insert into devicesfloorplan (x, y, floorplan, device) VALUES (?, ?, ?, ?)";
        // AGO_TRACE() << query;
        getFirst(query.c_str(), 4, xstr.str().c_str(), ystr.str().c_str(), floorplanuuid.c_str(), deviceuuid.c_str());
    }

    return true;
}

/**
 * Delete device from floorplan
 */
void Inventory::delDeviceFloorplan(std::string deviceuuid, std::string floorplanuuid)
{
    std::string query = "delete from devicesfloorplan WHERE floorplan = ? and device = ?";
    getFirst(query.c_str(), 2, floorplanuuid.c_str(), deviceuuid.c_str());
}

bool Inventory::deleteFloorplan(std::string uuid) {
    getFirst("BEGIN");
    std::string query = "delete from devicesfloorplan WHERE floorplan = ?";
    getFirst(query.c_str(), 1, uuid.c_str());
    query = "delete from floorplans WHERE uuid = ?";
    getFirst(query.c_str(), 1, uuid.c_str());
    if (getFloorplanName(uuid) != "") {
        getFirst("ROLLBACK");
        return false;
    } else {
        getFirst("COMMIT");
        return true;
    }
}

Variant::Map Inventory::getFloorplans() {
    Variant::Map result;
    sqlite3_stmt *stmt;
    int rc;

    rc = sqlite3_prepare_v2(db, "SELECT uuid, name from floorplans", -1, &stmt, NULL);
    if(rc!=SQLITE_OK) {
        AGO_ERROR() << "sql error #" << rc << ": " << sqlite3_errmsg(db);
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Variant::Map entry;
        const char *floorplanname = (const char*)sqlite3_column_text(stmt, 1);
        const char *uuid = (const char*)sqlite3_column_text(stmt, 0);
        if (floorplanname != NULL) {
            entry["name"] = std::string(floorplanname);
        } else {
            entry["name"] = "";
        } 

        // for each floorplan now fetch the device coordinates
        sqlite3_stmt *stmt2;
        int rc2;
        std::string query = "SELECT device, x, y from devicesfloorplan WHERE floorplan = ?";
        rc2 = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt2, NULL);
        if (rc2 != SQLITE_OK) {
            AGO_ERROR() << "sql error #" << rc2 << ": " << sqlite3_errmsg(db);
            continue;
        }
        sqlite3_bind_text(stmt2, 1, uuid, -1, NULL);
        while (sqlite3_step(stmt2) == SQLITE_ROW) {
            Variant::Map device;
            const char *deviceuuid = (const char*)sqlite3_column_text(stmt2, 0);
            const char *x = (const char*)sqlite3_column_text(stmt2, 1);
            const char *y = (const char*)sqlite3_column_text(stmt2, 2);
            device["x"] = atoi(x);
            device["y"] = atoi(y);
            entry[deviceuuid] = device;
        }

        sqlite3_finalize (stmt2);			

        if (uuid != NULL) {
            result[uuid] = entry;
        }
    }

    sqlite3_finalize (stmt);

    return result;
}

std::string Inventory::getLocationName(std::string uuid) {
    std::string query = "SELECT name from locations WHERE uuid = ?";
    return getFirst(query.c_str(), 1, uuid.c_str());
}

std::string Inventory::getRoomLocation(std::string uuid) {
    std::string query = "SELECT location from rooms WHERE uuid = ?";
    return getFirst(query.c_str(), 1, uuid.c_str());
}

bool Inventory::setLocationName(std::string uuid, std::string name) {
    if (getLocationName(uuid) == "") { // does not exist, create
        std::string query = "insert into locations (name, uuid) VALUES (?, ?)";
        AGO_DEBUG() << "creating location: " << query.c_str();
        getFirst(query.c_str(), 2, name.c_str(), uuid.c_str());
    } else {
        std::string query = "update locations set name = ? WHERE uuid = ?";
        getFirst(query.c_str(), 2, name.c_str(), uuid.c_str());
    }

    return getLocationName(uuid) == name;
}

bool Inventory::setRoomLocation(std::string roomuuid, std::string locationuuid) {
    std::string query = "update rooms set location = ? WHERE uuid = ?";
    getFirst(query.c_str(), 2, locationuuid.c_str(), roomuuid.c_str());
    if (getRoomLocation(roomuuid) == locationuuid) {
        return true;
    }
    return false;
}

bool Inventory::deleteLocation(std::string uuid) {
    std::string query = "update rooms set location = '' WHERE location = ?";
    getFirst(query.c_str(), 1, uuid.c_str());
    query = "delete from locations WHERE uuid = ?";
    getFirst(query.c_str(), 1, uuid.c_str());

    return getLocationName(uuid) == "";
}

Variant::Map Inventory::getLocations() {
    Variant::Map result;
    sqlite3_stmt *stmt;
    int rc;

    rc = sqlite3_prepare_v2(db, "select uuid, name from locations", -1, &stmt, NULL);
    if(rc!=SQLITE_OK) {
        AGO_ERROR() << "sql error #" << rc << ": " << sqlite3_errmsg(db);
        return result;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Variant::Map entry;
        const char *locationname = (const char*)sqlite3_column_text(stmt, 1);
        const char *uuid = (const char*)sqlite3_column_text(stmt, 0);
        if (locationname != NULL) {
            entry["name"] = std::string(locationname);
        } else {
            entry["name"] = "";
        } 
        if (uuid != NULL) {
            result[uuid] = entry;
        }
    }

    sqlite3_finalize (stmt);

    return result;
}

bool Inventory::createUser(std::string uuid, std::string username, std::string password, std::string pin, std::string description) {
    return false;
}
bool Inventory::deleteUser(std::string uuid){
    return false;
}
bool Inventory::authUser(std::string uuid){
    return false;
}
bool Inventory::setPassword(std::string uuid){
    return false;
}
bool Inventory::setPin(std::string uuid){
    return false;
}
bool Inventory::setPermission(std::string uuid, std::string permission){
    return false;
}
bool Inventory::deletePermission(std::string uuid, std::string permission){
    return false;
}
Variant::Map Inventory::getPermissions(std::string uuid){
    Variant::Map permissions;
    return permissions;
}


#ifdef INVENTORY_TEST
// gcc -DINVENTORY_TEST inventory.cpp -lqpidtypes -lsqlite3
int main(int argc, char **argv){
    Inventory inv("inventory.db");
    cout << inv.setdevicename("1234", "1235") << endl;
    cout << inv.deleteroom("1234") << endl;
    cout << inv.getdevicename("1234") << endl;
    cout << inv.getrooms() << endl;
    cout << inv.setfloorplanname("2235", "floorplan2") << endl;
    cout << inv.setdevicefloorplan("1234", "2235", 5, 2) << endl;
    cout << inv.getfloorplans() << endl;
    cout << inv.deletefloorplan("2235");
    cout << inv.getfloorplans() << endl;
}
#endif
