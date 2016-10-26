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


class AgoMbus: public AgoApp {
private:
    void setupApp();
    void cleanupApp();
    qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map command);
    std::string fetchXml(int address);   
    void parseXml(std::string xmlstring);
    mbus_handle *handle;
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
    // for (int address = 0; address <= MBUS_MAX_PRIMARY_SLAVES; address++) {
    for (int address = 90; address <= 99; address++) {
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
             AGO_TRACE() << "XML: " << fetchXml(address);
             parseXml(fetchXml(address));
        }
    }        
    AGO_INFO() << "Scan done.";
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
    mbus_disconnect(handle);
    mbus_context_free(handle);
}

/*
<MBusData>

    <SlaveInformation>
        <Id>61303893</Id>
        <Manufacturer>KAM</Manufacturer>
        <Version>11</Version>
        <ProductName></ProductName>
        <Medium>Heat: Outlet</Medium>
        <AccessNumber>3</AccessNumber>
        <Status>00</Status>
        <Signature>0000</Signature>
    </SlaveInformation>

    <DataRecord id="0">
        <Function>Instantaneous value</Function>
        <StorageNumber>0</StorageNumber>
        <Unit>Fabrication number</Unit>
        <Value>61303893</Value>
        <Timestamp>2016-10-26T22:28:25</Timestamp>
    </DataRecord>
*/
void AgoMbus::parseXml(std::string xmlstring) {
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
			AGO_TRACE() << "Found SlaveInformation tag";
			XMLElement *id = slaveInformation->FirstChildElement("Id");
			if (id) AGO_INFO() << "Sensor Id: " << id->GetText();
			XMLElement *manufacturer = slaveInformation->FirstChildElement("Manufacturer");
			if (manufacturer) AGO_INFO() << "Manufacturer: " << manufacturer->GetText();
			XMLElement* dataRecord = docHandle.FirstChildElement( "MBusData" ).ToElement()->FirstChildElement( "DataRecord" )->ToElement();
			if (dataRecord) {
				XMLElement *nextDataRecord = dataRecord;
				while (nextDataRecord != NULL) {
					XMLElement *function = nextDataRecord->FirstChildElement( "Function" );
					if (function) AGO_INFO() << "Function " << function->GetText();
	
					nextDataRecord = nextDataRecord->NextSiblingElement();
				}
			}
		}
    }
/*
            AGO_TRACE() << "node: " << nextdevice->Attribute("uuid") << " type: " << nextdevice->Attribute("type");

            content["devicetype"] = nextdevice->Attribute("type");
            XMLElement *ga = nextdevice->FirstChildElement( "ga" );
            if (ga) {
                XMLElement *nextga = ga;
                while (nextga != NULL) {
                    AGO_DEBUG() << "GA: " << nextga->GetText() << " type: " << nextga->Attribute("type");
*/
}
AGOAPP_ENTRY_POINT(AgoMbus);
