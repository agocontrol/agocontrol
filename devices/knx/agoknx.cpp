/*
   Copyright (C) 2012 Harald Klein <hari@vt100.at>

   This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

*/

#include <iostream>
#include <sstream>
#include <uuid/uuid.h>
#include <stdlib.h>

#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>

#include <tinyxml2.h>

#include <eibclient.h>
#include "Telegram.h"

#include "agoapp.h"

#ifndef KNXDEVICEMAPFILE
#define KNXDEVICEMAPFILE "maps/knx.json"
#endif

#ifndef ETSGAEXPORTMAPFILE
#define ETSGAEXPORTMAPFILE "maps/knx_etsgaexport.json"
#endif

using namespace tinyxml2;
using namespace agocontrol;

namespace fs = ::boost::filesystem;
namespace pt = boost::posix_time;

class AgoKnx: public AgoApp {
private:
    int polldelay;

    Json::Value deviceMap;

    std::string eibdurl;
    std::string time_ga;
    std::string date_ga;

    EIBConnection *eibcon;
    boost::mutex mutexCon;
    boost::thread *listenerThread;

    Json::Value commandHandler(const Json::Value& content);
    void eventHandler(const std::string& subject , const Json::Value& content);
    void sendDate();
    void sendTime();
    bool sendShortData(std::string dest, int data);
    bool sendCharData(std::string dest, int data);
    bool sendFloatData(std::string dest, float data);
    bool sendBytes(std::string dest, uint8_t *bytes, int len);
    void setupApp();
    void cleanupApp();

    bool loadDevicesXML(fs::path &filename, Json::Value& _deviceMap);
    void reportDevices();
    std::string uuidFromGA(std::string ga);
    std::string typeFromGA(const Json::Value& device, std::string ga);

    void listener();
public:
    AGOAPP_CONSTRUCTOR_HEAD(AgoKnx)
        , polldelay(0)
        {}
};

/**
 * parses the device XML file and creates a Json::Value with the data
 */
bool AgoKnx::loadDevicesXML(fs::path &filename, Json::Value& _deviceMap) {
    XMLDocument devicesFile;
    int returncode;

    AGO_DEBUG() << "trying to open device file: " << filename;
    returncode = devicesFile.LoadFile(filename.c_str());
    if (returncode != XML_NO_ERROR) {
        AGO_ERROR() << "error loading XML file, code: " << returncode;
        return false;
    }

    AGO_TRACE() << "parsing file";
    XMLHandle docHandle(&devicesFile);
    XMLElement* device = docHandle.FirstChildElement( "devices" ).FirstChild().ToElement();
    if (device) {
        XMLElement *nextdevice = device;
        while (nextdevice != NULL) {
            Json::Value content(Json::objectValue);

            AGO_TRACE() << "node: " << nextdevice->Attribute("uuid") << " type: " << nextdevice->Attribute("type");

            content["devicetype"] = nextdevice->Attribute("type");
            XMLElement *ga = nextdevice->FirstChildElement( "ga" );
            if (ga) {
                XMLElement *nextga = ga;
                while (nextga != NULL) {
                    AGO_DEBUG() << "GA: " << nextga->GetText() << " type: " << nextga->Attribute("type");
                    std::string type = nextga->Attribute("type");
/*
                    if (type=="onoffstatus" || type=="levelstatus") {
                        AGO_DEBUG() << "Requesting current status: " << nextga->GetText();
                        Telegram *tg = new Telegram();
                        eibaddr_t dest;
                        dest = Telegram::stringtogaddr(nextga->GetText());
                        tg->setGroupAddress(dest);
                        tg->setType(EIBREAD);
                        tg->sendTo(eibcon);
                    }
*/
                    content[nextga->Attribute("type")]=nextga->GetText();
                    nextga = nextga->NextSiblingElement();
                }
            }
            _deviceMap[nextdevice->Attribute("uuid")] = content;
            nextdevice = nextdevice->NextSiblingElement();
        }
    }
    return true;
}

/**
 * announces our devices in the devicemap to the resolver
 */
void AgoKnx::reportDevices() {
    for (auto it = deviceMap.begin(); it != deviceMap.end(); ++it) {
        const Json::Value& device = *it;
        agoConnection->addDevice(it.name(), device["devicetype"].asString(), true);
    }
}

/**
 * looks up the uuid for a specific GA - this is needed to match incoming telegrams to the right device
 */
std::string AgoKnx::uuidFromGA(std::string ga) {
    for (auto it = deviceMap.begin(); it != deviceMap.end(); ++it) {
        const Json::Value& device = *it;
        for (auto  itd = device.begin(); itd != device.end(); itd++) {
            if (itd->asString() == ga) {
                // AGO_TRACE() << "GA " << itd->second.asString() << " belongs to " << it.name();
                return(it.name());
            }
        }
    }	
    return("");
}

/**
 * looks up the type for a specific GA - this is needed to match incoming telegrams to the right event type
 */
std::string AgoKnx::typeFromGA(const Json::Value& device, std::string ga) {
    for (auto itd = device.begin(); itd != device.end(); itd++) {
        if (itd->asString() == ga) {
            // AGO_TRACE() << "GA " << itd->second.asString() << " belongs to " << it.name();
            return(itd.name());
        }
    }
    return("");
}
/**
 * thread to poll the knx bus for incoming telegrams
 */
void AgoKnx::listener() {
    int received = 0;

    AGO_TRACE() << "starting listener thread";
    while(!isExitSignaled()) {
        std::string uuid;
        {
            boost::lock_guard<boost::mutex> lock(mutexCon);
            received = EIB_Poll_Complete(eibcon);
        }
        switch(received) {
            case(-1):
                AGO_WARNING() << "cannot poll bus";
                try {
                    //boost::this_thread::sleep(pt::seconds(3)); FIXME: check why boost sleep interferes with EIB_Poll_complete, causing delays on status feedback
                    sleep(3);
                } catch(boost::thread_interrupted &e) {
                    AGO_DEBUG() << "listener thread cancelled";
                    break;
                }
                AGO_INFO() << "reconnecting to eibd";

                {
                    boost::lock_guard<boost::mutex> lock(mutexCon);
                    EIBClose(eibcon);
                    eibcon = EIBSocketURL(eibdurl.c_str());
                    if (!eibcon) {
                        AGO_FATAL() << "cannot reconnect to eibd";
                        signalExit();
                    } else {
                        if (EIBOpen_GroupSocket(eibcon, 0) == -1) {
                            AGO_FATAL() << "cannot reconnect to eibd";
                            signalExit();
                        } else {
                            AGO_INFO() << "reconnect to eibd succeeded";
                        }
                    }
                }
                break;

            case(0)	:
                try {
                    //boost::this_thread::sleep(pt::milliseconds(polldelay)); FIXME: check why boost sleep interferes with EIB_Poll_complete, causing delays on status feedback
                    usleep(polldelay);
                } catch(boost::thread_interrupted &e) {
                    AGO_DEBUG() << "listener thread cancelled";
                }
                break;

            default:
                Telegram tl;
                {
                    boost::lock_guard<boost::mutex> lock(mutexCon);
                    tl.receivefrom(eibcon);
                }

                AGO_DEBUG() << "received telegram from: " << Telegram::paddrtostring(tl.getSrcAddress()) << " to: " 
                    << Telegram::gaddrtostring(tl.getGroupAddress()) << " type: " << tl.decodeType() << " shortdata: "
                    << tl.getShortUserData();
                uuid = uuidFromGA(Telegram::gaddrtostring(tl.getGroupAddress()));
                if (uuid != "") {
                    std::string type = typeFromGA(deviceMap[uuid], Telegram::gaddrtostring(tl.getGroupAddress()));
                    if (type != "") {
                        AGO_DEBUG() << "handling telegram, GA from telegram belongs to: " << uuid << " - type: " << type;
                        if(type == "onoff" || type == "onoffstatus") { 
                            agoConnection->emitEvent(uuid, "event.device.statechanged", tl.getShortUserData()==1 ? 255 : 0, "");
                        } else if (type == "setlevel" || type == "levelstatus") {
                            int data = tl.getUIntData(); 
                            agoConnection->emitEvent(uuid, "event.device.statechanged", data*100/255, "");
                        } else if (type == "temperature") {
                            agoConnection->emitEvent(uuid, "event.environment.temperaturechanged", tl.getFloatData(), "degC");
                        } else if (type == "brightness") {
                            agoConnection->emitEvent(uuid, "event.environment.brightnesschanged", tl.getFloatData(), "lux");
                        } else if (type == "humidity") {
                            agoConnection->emitEvent(uuid, "event.environment.humiditychanged", tl.getFloatData(), "percent");
                        } else if (type == "airquality") {
                            agoConnection->emitEvent(uuid, "event.environment.co2changed", tl.getFloatData(), "ppm");
                        } else if (type == "windspeed") {
                            agoConnection->emitEvent(uuid, "event.environment.windspeedchanged", tl.getFloatData(), "m/s");
                        } else if (type == "energy") {
                            agoConnection->emitEvent(uuid, "event.environment.energychanged", tl.getFloatData(), "kWh");
                        } else if (type == "power") {
                            agoConnection->emitEvent(uuid, "event.environment.powerchanged", tl.getFloatData(), "kWh");
                        } else if (type == "flow") {
                            agoConnection->emitEvent(uuid, "event.environment.flowchanged", tl.getFloatData(), "l/h");
                        } else if (type == "counter") {
                        	agoConnection->emitEvent(uuid, "event.environment.counterchanged",tl.getIntData(), "Wh");
                        } else if (type == "binary") {
                            agoConnection->emitEvent(uuid, "event.security.sensortriggered", tl.getShortUserData()==1 ? 255 : 0, "");
                        }
                    }
                }
                break;
        }

    }
}

void AgoKnx::eventHandler(const std::string& subject , const Json::Value& content) {
    if (subject == "event.environment.timechanged") {
        // send time/date every hour
        if (content["minute"].asInt() == 0) {
            sendDate();
            sendTime();
        }
    }
}


void AgoKnx::sendDate() {
    uint8_t datebytes[3];   
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    datebytes[0] = lt->tm_mday;
    datebytes[1] = lt->tm_mon + 1;
    datebytes[2] = lt->tm_year - 100;
    Telegram *tg_date = new Telegram();
    tg_date->setUserData(datebytes,3);
    tg_date->setGroupAddress(Telegram::stringtogaddr(date_ga));

    boost::lock_guard<boost::mutex> lock(mutexCon);
    AGO_TRACE() << "sending telegram";
    tg_date->sendTo(eibcon);
}


void AgoKnx::sendTime() {
    uint8_t timebytes[3];   
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    timebytes[0]=((lt->tm_wday?lt->tm_wday:7)<<5) + lt->tm_hour;
    timebytes[1]= lt->tm_min;
    timebytes[2] = lt->tm_sec;
    Telegram *tg_time = new Telegram();
    tg_time->setUserData(timebytes,3);
    tg_time->setGroupAddress(Telegram::stringtogaddr(time_ga));

    boost::lock_guard<boost::mutex> lock(mutexCon);
    AGO_TRACE() << "sending telegram";
    tg_time->sendTo(eibcon);
}

bool AgoKnx::sendBytes(std::string dest, uint8_t *bytes, int len) {
    Telegram *tg = new Telegram();
    tg->setGroupAddress(Telegram::stringtogaddr(dest));
    tg->setUserData(bytes, len);
    AGO_TRACE() << "sending " << len << " bytes to " << dest;

    boost::lock_guard<boost::mutex> lock(mutexCon);
    bool result = tg->sendTo(eibcon);
    AGO_DEBUG() << "Result: " << result;
    return result;
}

bool AgoKnx::sendShortData(std::string dest, int data) {
    Telegram *tg = new Telegram();
    tg->setGroupAddress(Telegram::stringtogaddr(dest));
    tg->setShortUserData(data > 0 ? 1 : 0);
    AGO_TRACE() << "sending value " << data << " as short data telegram to " << dest;
    boost::lock_guard<boost::mutex> lock(mutexCon);
    bool result = tg->sendTo(eibcon);
    AGO_DEBUG() << "Result: " << result;
    return result;
}

bool AgoKnx::sendCharData(std::string dest, int data) {
    Telegram *tg = new Telegram();
    tg->setGroupAddress(Telegram::stringtogaddr(dest));
    tg->setDataFromChar(data);
    AGO_TRACE() << "sending value " << data << " as char data telegram to " << dest;
    boost::lock_guard<boost::mutex> lock(mutexCon);
    bool result = tg->sendTo(eibcon);
    AGO_DEBUG() << "Result: " << result;
    return result;
}

bool AgoKnx::sendFloatData(std::string dest, float data) {
    Telegram *tg = new Telegram();
    tg->setGroupAddress(Telegram::stringtogaddr(dest));
    tg->setDataFromFloat(data);
    AGO_TRACE() << "sending value " << data << " as float data telegram to " << dest;
    boost::lock_guard<boost::mutex> lock(mutexCon);
    bool result = tg->sendTo(eibcon);
    AGO_DEBUG() << "Result: " << result;
    return result;
}


Json::Value AgoKnx::commandHandler(const Json::Value& content) {
    checkMsgParameter(content, "internalid", Json::stringValue);
    checkMsgParameter(content, "command", Json::stringValue);
    std::string internalid = content["internalid"].asString();
    std::string command = content["command"].asString();
    AGO_TRACE() << "received command " << content["command"] << " for device " << internalid;

    if (internalid == "knxcontroller")
    {
        Json::Value returnData;

        if (command == "adddevice")
        {
            checkMsgParameter(content, "devicemap", Json::objectValue);
            // checkMsgParameter(content, "devicetype", Json::stringValue);

            Json::Value newdevice = content["devicemap"];
            AGO_TRACE() << "adding knx device: " << newdevice;

            std::string deviceuuid;
            if(content.isMember("device"))
                deviceuuid = content["device"].asString();
            else
                deviceuuid = generateUuid();

            /* XXX: No control over what's feed in here.
             * Web UI currently sends something like this:
             * {devicetype: str, <device params>:<some id>}
             */
            deviceMap[deviceuuid] = newdevice;
            agoConnection->addDevice(deviceuuid, newdevice["devicetype"].asString(), true);
            if (writeJsonFile(deviceMap, getConfigPath(KNXDEVICEMAPFILE)))
            {
                returnData["device"] = deviceuuid;
                return responseSuccess(returnData);
            }
            return responseFailed("Failed to write knx device map file");
        }
        else if (command == "getdevice")
        {
            checkMsgParameter(content, "device", Json::stringValue);
            std::string device = content["device"].asString();

            AGO_TRACE() << "getdevice request: " << device;
            if (!deviceMap.isMember(device))
                return responseError(RESPONSE_ERR_NOT_FOUND, "Device not found");

            returnData["devicemap"] = deviceMap[device];
            returnData["device"] = device;

            return responseSuccess(returnData);
        }
        else if (command == "getdevices")
        {
            /*Json::Value devicelist;
            for (Json::Value::const_iterator it = devicemap.begin(); it != devicemap.end(); ++it) {
                Json::Value device;
                device = it->second;
                devicelist.push_back(device);
            }*/
            returnData["devices"] = deviceMap;
            return responseSuccess(returnData);
        }
        else if (command == "deldevice")
        {
            checkMsgParameter(content, "device", Json::stringValue);
            
            std::string device = content["device"].asString();
            AGO_TRACE() << "deldevice request:" << device;
            if(deviceMap.isMember(device)) {
                AGO_DEBUG() << "removing ago device" << device;
                agoConnection->removeDevice(device);
                deviceMap.removeMember(device);
                if (!writeJsonFile(deviceMap, getConfigPath(KNXDEVICEMAPFILE)))
                {
                    return responseFailed("Failed to write knx device map file");
                }
            }
            return responseSuccess();

        } 
        else if (command == "uploadfile")
        {
            checkMsgParameter(content, "filepath", Json::stringValue);

            XMLDocument etsExport;
            std::string etsdata = content["filepath"].asString();
            AGO_TRACE() << "parse ets export request:" << etsdata;
            /*
            if (etsExport.Parse(etsdata.c_str()) != XML_NO_ERROR)
                return responseFailed("Failed to parse XML input data");
            */
            int returncode = etsExport.LoadFile(etsdata.c_str());
            if (returncode != XML_NO_ERROR) {
                AGO_ERROR() << "error loading XML file '" << etsdata << "', code: " << returncode;
                return responseFailed("Failed to parse XML input data");
            }

            XMLHandle docHandle(&etsExport);
            XMLElement* groupRange = docHandle.FirstChildElement("GroupAddress-Export").FirstChild().ToElement();
            if (groupRange) {
                XMLElement *nextRange = groupRange;
                Json::Value rangeMap;
                while (nextRange != NULL) {
                    AGO_TRACE() << "node: " << nextRange->Attribute("Name");
                    XMLElement *middleRange = nextRange->FirstChildElement( "GroupRange" );
                    if (middleRange)
                    {
                        XMLElement *nextMiddleRange = middleRange;
                        Json::Value middleMap;
                        while (nextMiddleRange != NULL)
                        {
                            AGO_TRACE() << "middle: " << nextMiddleRange->Attribute("Name");
                            XMLElement *groupAddress = nextMiddleRange->FirstChildElement("GroupAddress");
                            if (groupAddress)
                            {
                                XMLElement *nextGroupAddress = groupAddress;
                                Json::Value groupMap;
                                while (nextGroupAddress != NULL)
                                {
                                    AGO_TRACE() << "Group: " << nextGroupAddress->Attribute("Name") << " Address: " << nextGroupAddress->Attribute("Address");
                                    groupMap[nextGroupAddress->Attribute("Name")]=nextGroupAddress->Attribute("Address");
                                    nextGroupAddress = nextGroupAddress->NextSiblingElement();
                                }
                                middleMap[nextMiddleRange->Attribute("Name")]=groupMap;

                            }
                            nextMiddleRange = nextMiddleRange->NextSiblingElement();

                        }
                        rangeMap[nextRange->Attribute("Name")]=middleMap;
                    }
                    nextRange = nextRange->NextSiblingElement();
                }
                returnData["groupmap"]=rangeMap;
                writeJsonFile(rangeMap, getConfigPath(ETSGAEXPORTMAPFILE));
            } else 
                return responseFailed("No 'GroupAddress-Export' tag found");

            return responseSuccess(returnData);

        }
        else if (command == "getgacontent")
        {
            readJsonFile(returnData["groupmap"], getConfigPath(ETSGAEXPORTMAPFILE));
            return responseSuccess(returnData);
        }
        return responseUnknownCommand();
    }

    if(!deviceMap.isMember(internalid))
       return responseError(RESPONSE_ERR_INTERNAL, "Device not found in internal deviceMap");

    const Json::Value& device(deviceMap[internalid]);

    bool result;
    if (command == "on") {
        if (device["devicetype"]=="drapes") {
            result = sendShortData(device["onoff"].asString(),0);
        } else {
            result = sendShortData(device["onoff"].asString(),1);
        }
    } else if (command == "off") {
        if (device["devicetype"]=="drapes") {
            result = sendShortData(device["onoff"].asString(),1);
        } else {
            result = sendShortData(device["onoff"].asString(),0);
        }
    } else if (command == "stop") {
        result = sendShortData(device["stop"].asString(),1);
    } else if (command == "push") {
        result = sendShortData(device["push"].asString(),0);
    } else if (command == "setlevel") {
        checkMsgParameter(content, "level", Json::intValue);
        result = sendCharData(device["setlevel"].asString(), content["level"].asInt() * 255 / 100);
    } else if (command == "settemperature") {
        checkMsgParameter(content, "temperature", Json::realValue);
        result = sendFloatData(device["settemperature"].asString(), content["temperature"].asFloat());
    } else if (command == "setcolor") {
        checkMsgParameter(content, "red", Json::intValue);
        checkMsgParameter(content, "green", Json::intValue);
        checkMsgParameter(content, "blue", Json::intValue);
        if (device.isMember("setcolor")) {
            uint8_t buf[3];
            buf[0]= content["red"].asInt();
            buf[1]= content["green"].asInt();
            buf[2]= content["blue"].asInt();
            result = sendBytes(device["setcolor"].asString(), buf, 3);
        } else {
            result = sendCharData(device["red"].asString(), content["red"].asInt());
            result &= sendCharData(device["green"].asString(), content["green"].asInt());
            result &= sendCharData(device["blue"].asString(), content["blue"].asInt());
        }
    } else {
        return responseUnknownCommand();
    }

    if (result)
    {
        return responseSuccess();
    }
    return responseError(RESPONSE_ERR_INTERNAL, "Cannot send KNX Telegram");
}

void AgoKnx::setupApp() {
    fs::path devicesFile;

    // parse config
    eibdurl=getConfigOption("url", "ip:127.0.0.1");
    polldelay=atoi(getConfigOption("polldelay", "5000").c_str());
    devicesFile=getConfigOption("devicesfile", getConfigPath("/knx/devices.xml"));


    AGO_INFO() << "connecting to eibd"; 
    eibcon = EIBSocketURL(eibdurl.c_str());
    if (!eibcon) {
        AGO_FATAL() << "can't connect to eibd url:" << eibdurl;
        throw StartupError();
    }

    if (EIBOpen_GroupSocket (eibcon, 0) == -1)
    {
        EIBClose(eibcon);
        AGO_FATAL() << "can't open EIB Group Socket";
        throw StartupError();
    }

    addCommandHandler();
    if (getConfigOption("sendtime", "0")!="0") {
        time_ga = getConfigOption("timega", "3/3/3");
        date_ga = getConfigOption("datega", "3/3/4");
        addEventHandler();
    }

    agoConnection->addDevice("knxcontroller", "knxcontroller");

    // check if old XML file exists and convert it to a json map
    if (fs::exists(devicesFile)) {
        AGO_DEBUG() << "Found XML config file, converting to json map";
        // load xml file into map
        if (!loadDevicesXML(devicesFile, deviceMap)) {
            AGO_FATAL() << "can't load device xml";
            throw StartupError();
        }
        // write json map
        AGO_DEBUG() << "Writing json map into " << getConfigPath(KNXDEVICEMAPFILE);
        writeJsonFile(deviceMap, getConfigPath(KNXDEVICEMAPFILE));
        AGO_INFO() << "XML devices file has been converted to a json map. Renaming old file.";
        fs::rename(devicesFile, fs::path(devicesFile.string() + ".converted"));
    } else {
        AGO_DEBUG() << "Loading json device map";
        readJsonFile(deviceMap, getConfigPath(KNXDEVICEMAPFILE));
    }

    // announce devices to resolver
    reportDevices();

    AGO_DEBUG() << "Spawning thread for KNX listener";
    listenerThread = new boost::thread(boost::bind(&AgoKnx::listener, this));
}

void AgoKnx::cleanupApp() {
    AGO_TRACE() << "waiting for listener thread to stop";
    listenerThread->interrupt();
    listenerThread->join();
    AGO_DEBUG() << "closing eibd connection";
    EIBClose(eibcon);
}

AGOAPP_ENTRY_POINT(AgoKnx);

