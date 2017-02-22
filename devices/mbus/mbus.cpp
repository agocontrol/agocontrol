/*
   Copyright (C) 2016 Harald Klein <hari@vt100.at>

   This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

*/
#include <iostream>
#include <sstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <termios.h>
#include <stdio.h>

#include <tinyxml2.h>


#include <mbus/mbus.h>

#include "agoapp.h"


using namespace qpid::types;
using namespace std;
using namespace agocontrol;

using namespace tinyxml2;

namespace pt = boost::posix_time;

class AgoMbus: public AgoApp {
private:
    void setupApp();
    void cleanupApp();
    qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map command);
    std::string fetchXml(int address);   
    void parseXml(std::string xmlstring, bool announce = false);
    void receiveFunction();
    mbus_handle *handle;
    std::list<int> sensorList;
    boost::thread *receiveThread;
public:
    AGOAPP_CONSTRUCTOR_HEAD(AgoMbus)
        {
            // Compatability with old configuration section
            appConfigSection = "mbus";
        }
};

// commandhandler
qpid::types::Variant::Map AgoMbus::commandHandler(qpid::types::Variant::Map content) {
    string addr = content["internalid"].asString();
    if (content["command"] == "on") {
    } else if (content["command"] == "off") {
    }
    return responseSuccess();
}

int ping_address(mbus_handle *handle, mbus_frame *reply, int address)
{
    int i, ret = MBUS_RECV_RESULT_ERROR;

    memset((void *)reply, 0, sizeof(mbus_frame));

    for (i = 0; i <= handle->max_search_retry; i++)
    {
        if (mbus_send_ping_frame(handle, address, 0) == -1)
        {
            fprintf(stderr,"Scan failed. Could not send ping frame: %s\n", mbus_error_str());
            return MBUS_RECV_RESULT_ERROR;
        }

        ret = mbus_recv_frame(handle, reply);

        if (ret != MBUS_RECV_RESULT_TIMEOUT)
        {
            return ret;
        }
    }

    return ret;
}

void AgoMbus::setupApp() {

    addCommandHandler();

    if ((handle = mbus_context_serial(getConfigOption("device", "/dev/ttymxc0").c_str())) == NULL)
    {
        AGO_ERROR() << "Could not initialize M-Bus context: " <<  mbus_error_str();
    }
    if (mbus_connect(handle) == -1) {
        AGO_ERROR() << "Could not setup connection to M-bus gateway: " << mbus_error_str();
    }
    if (mbus_serial_set_baudrate(handle, atol(getConfigOption("baudrate", "9600").c_str())) == -1) {
        AGO_ERROR() << "Could not set baudrate";
    }
    AGO_INFO() << "Scanning for mbus slaves...";
    for (int address = 0; address <= MBUS_MAX_PRIMARY_SLAVES; address++) {
        AGO_TRACE() << "Testing " << std::dec << address;
        mbus_frame reply;
        int ret = ping_address(handle, &reply, address);
        if (ret == MBUS_RECV_RESULT_TIMEOUT) continue;
        if (ret == MBUS_RECV_RESULT_INVALID) {
            mbus_purge_frames(handle);
            AGO_WARNING() << "Collision at address " << std::dec << address;
            continue;
        }
        if (mbus_frame_type(&reply) == MBUS_FRAME_TYPE_ACK) {
             if (mbus_purge_frames(handle)) {
                 AGO_WARNING() << "Collision at address " << std::dec << address;
                 continue;
             }
             AGO_INFO() << "Found a M-Bus device at address " << std::dec << address;
             sensorList.push_back(address);
             AGO_TRACE() << "XML: " << fetchXml(address);
             parseXml(fetchXml(address),true);
        }
    }        
    AGO_INFO() << "Scan done.";

    receiveThread = new boost::thread(boost::bind(&AgoMbus::receiveFunction, this));
}


std::string AgoMbus::fetchXml(int address) {
    std::string xmldata;

    mbus_frame reply;
    mbus_frame_data reply_data;

    memset((void *)&reply, 0, sizeof(mbus_frame));
    memset((void *)&reply_data, 0, sizeof(mbus_frame_data));

    if (mbus_send_request_frame(handle, address) == -1)
        {
            AGO_ERROR() << "Failed to send M-Bus request frame!";
        } else {
        if (mbus_recv_frame(handle, &reply) != MBUS_RECV_RESULT_OK) {
            AGO_ERROR() << "Failed to receive M-Bus response frame: " << mbus_error_str();
        } else {
            // handle frame
            if (mbus_frame_data_parse(&reply, &reply_data) == -1) {
                AGO_ERROR() << "M-bus data parse error: " << mbus_error_str();
            } else {
                xmldata = std::string(mbus_frame_data_xml(&reply_data));
            }

        }
    }

    return xmldata;
}


void AgoMbus::cleanupApp() {
    AGO_INFO() << "Disconnecting from M-Bus";
    receiveThread->interrupt();
    receiveThread->join();
    mbus_disconnect(handle);
    mbus_context_free(handle);
}

void AgoMbus::parseXml(std::string xmlstring, bool announce) {
    XMLDocument sensor;
    int returncode = 0;

    AGO_TRACE() << "parsing XML string";
    returncode = sensor.Parse(xmlstring.c_str());
    if (returncode != XML_NO_ERROR) {
        AGO_ERROR() << "error parsing XML, code: " << returncode;
    } else {

        XMLHandle docHandle(&sensor);
        XMLElement* slaveInformation = docHandle.FirstChildElement( "MBusData" ).ToElement()->FirstChildElement("SlaveInformation");
        if (slaveInformation) {  
                        std::string sensorId;
                        std::string manufacturerId;
            // AGO_TRACE() << "Found SlaveInformation tag";
            XMLElement *id = slaveInformation->FirstChildElement("Id");
            if (id) sensorId =  id->GetText();
            XMLElement *manufacturer = slaveInformation->FirstChildElement("Manufacturer");
            if (manufacturer) manufacturerId = manufacturer->GetText();
            if (announce) AGO_INFO() << "Manufacturer: " << manufacturerId << " " << "Sensor Id: " << sensorId;
            XMLElement* dataRecord = docHandle.FirstChildElement( "MBusData" ).ToElement()->FirstChildElement( "DataRecord" )->ToElement();
            if (dataRecord) {
                XMLElement *nextDataRecord = dataRecord;
                while (nextDataRecord != NULL) {
                                        std::string recordId, functionName, unitName, valueString;
                                        recordId = nextDataRecord->Attribute("id");
                    XMLElement *function = nextDataRecord->FirstChildElement( "Function" );
                    if (function) functionName = function->GetText();
                    XMLElement *unit = nextDataRecord->FirstChildElement( "Unit" );
                    if (unit) unitName = unit->GetText();
                    XMLElement *value = nextDataRecord->FirstChildElement( "Value" );
                    if (value) valueString = value->GetText();
                    if (announce) AGO_INFO()  << "Record id: " << recordId << ";" << functionName << ";Value " << valueString << " " << unitName;
                    
                    std::string internalid = manufacturerId + "-" + sensorId + "/" + recordId;
                    if (unitName.find("deg C") != std::string::npos) {
                        float value = atol(valueString.c_str());
                        if (unitName.find("1e-1"))  value = value / 10;
                        if (unitName.find("1e-2"))  value = value / 100;
                        if (announce) agoConnection->addDevice(internalid.c_str(), "temperaturesensor");
                        agoConnection->emitEvent(internalid.c_str(), "event.environment.temperaturechanged", value, "degC");
                        
                    } else if (unitName.find("Volume")!= std::string::npos) {
                        float value = atol(valueString.c_str());
                        if (unitName.find("1e-1"))  value = value / 10;
                        if (unitName.find("1e-2"))  value = value / 100;
                        if (announce) agoConnection->addDevice(internalid.c_str(), "flowmeter");
                        agoConnection->emitEvent(internalid.c_str(), "event.environment.volumechanged", value, "m^3");

                    } else if (unitName.find("Energy")!= std::string::npos) {
                        float value = atol(valueString.c_str());
                        if (unitName.find("10 kWh"))  value = value * 10;
                        if (announce) agoConnection->addDevice(internalid.c_str(), "energymeter");
                        agoConnection->emitEvent(internalid.c_str(), "event.environment.energychanged", value, "kWh");

                    } else if (unitName.find("Power")!= std::string::npos) {
                        float value = atol(valueString.c_str());
                        if (unitName.find("100 W"))  value = value * 100;
                        if (announce) agoConnection->addDevice(internalid.c_str(), "powermeter");
                        agoConnection->emitEvent(internalid.c_str(), "event.environment.powerchanged", value, "W");

                    }
                    nextDataRecord = nextDataRecord->NextSiblingElement();
                }
            }
        }
    }

}

void AgoMbus::receiveFunction() {
    while(!isExitSignaled()) {
    for(std::list<int>::iterator list_iter = sensorList.begin(); list_iter != sensorList.end(); list_iter++) {
        parseXml(fetchXml(*list_iter));
    }
        boost::this_thread::sleep(pt::seconds(30));
    }
}

AGOAPP_ENTRY_POINT(AgoMbus);
