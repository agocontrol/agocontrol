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

#include <mosquittopp.h>

#include "agoapp.h"


using namespace qpid::types;
using namespace std;
using namespace agocontrol;

using namespace tinyxml2;

namespace pt = boost::posix_time;


class NRMKMqttWrapper : public mosqpp::mosquittopp
{
public:
	NRMKMqttWrapper(const char *id, const char *host, int port);
	~NRMKMqttWrapper();

	void on_connect(int rc);
	void on_message(const struct mosquitto_message *message);
	void on_subcribe(int mid, int qos_count, const int *granted_qos);
};

NRMKMqttWrapper::NRMKMqttWrapper(const char *id, const char *host, int port) : mosquittopp(id)
{
	mosqpp::lib_init();			// Initialize libmosquitto

	int keepalive = 120; // seconds
	connect(host, port, keepalive);		// Connect to MQTT Broker
}
void NRMKMqttWrapper::on_message(const struct mosquitto_message *message)
{
	if(message->payloadlen){
		// printf("%s %s\n", message->topic, message->payload);
	}else{
		printf("%s (null)\n", message->topic);
	}
        AGO_INFO() << "Topic received: " << message->topic;
}
void NRMKMqttWrapper::on_connect(int rc)
{
	printf("Connected with code %d. \n", rc);

	if (rc == 0)
	{
		subscribe(NULL, "command/IGot");
	}
}
void NRMKMqttWrapper::on_subcribe(int mid, int qos_count, const int *granted_qos)
{
	printf("Subscription succeeded. \n");
}


class AgoMbus: public AgoApp {
private:
    void setupApp();
    void cleanupApp();
    qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map command);
    void parseXml(std::string xmlstring, bool announce = false);
    void receiveFunction();
    boost::thread *receiveThread;
    NRMKMqttWrapper * mqttHdl;
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

void AgoMbus::setupApp() {

    addCommandHandler();
    mqttHdl = new NRMKMqttWrapper("agombus_mqtt", getConfigOption("broker", "127.0.0.1").c_str(), 1883);
    receiveThread = new boost::thread(boost::bind(&AgoMbus::receiveFunction, this));
}


void AgoMbus::cleanupApp() {
    receiveThread->interrupt();
    receiveThread->join();
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
						if (unitName.find("1e-2"))  value = value / 100;
						if (announce) agoConnection->addDevice(internalid.c_str(), "temperaturesensor");
						agoConnection->emitEvent(internalid.c_str(), "event.environment.temperaturechanged", value, "degC");
						
					} else if (unitName.find("Volume")!= std::string::npos) {
						float value = atol(valueString.c_str());
						if (unitName.find("1e-2"))  value = value / 100;
						if (announce) agoConnection->addDevice(internalid.c_str(), "flowmeter");
						agoConnection->emitEvent(internalid.c_str(), "event.environment.volumechanged", value, "m^3");

					} else if (unitName.find("Energy")!= std::string::npos) {
						float value = atol(valueString.c_str());
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
	int res = mqttHdl->loop();						// Keep MQTT connection		
	if (res)
		mqttHdl->reconnect();
    }
}

AGOAPP_ENTRY_POINT(AgoMbus);
