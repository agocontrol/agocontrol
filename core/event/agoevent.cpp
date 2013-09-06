#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>

#include <string>
#include <iostream>
#include <sstream>
#include <cerrno>

#include "agoclient.h"
#include "bool.h"

#ifndef EVENTMAPFILE
#define EVENTMAPFILE "/etc/opt/agocontrol/eventmap.json"
#endif

using namespace std;
using namespace agocontrol;

qpid::types::Variant::Map eventmap;
AgoConnection *agoConnection;

void replaceString(std::string& subject, const std::string& search, const std::string& replace) {
	size_t pos = 0;
	while ((pos = subject.find(search, pos)) != std::string::npos) {
		subject.replace(pos, search.length(), replace);
		pos += replace.length();
	}
}

// example event:eb68c4a5-364c-4fb8-9b13-7ea3a784081f:{action:{command:on, uuid:25090479-566d-4cef-877a-3e1927ed4af0}, criteria:{0:{comp:eq, lval:hour, rval:7}, 1:{comp:eq, lval:minute, rval:1}}, event:event.environment.timechanged, nesting:(criteria["0"] and criteria["1"])}


void eventHandler(std::string subject, qpid::types::Variant::Map content) {
	// iterate event map and match for event name
	qpid::types::Variant::Map inventory = agoConnection->getInventory();
	for (qpid::types::Variant::Map::const_iterator it = eventmap.begin(); it!=eventmap.end(); it++) { 
		qpid::types::Variant::Map event = it->second.asMap();
		if (event["event"] == subject) {
			cout << "found matching event: " << event << endl;
			qpid::types::Variant::Map criteria; // this holds the criteria evaluation results for each criteria
			std::string nesting = event["nesting"].asString();
			for (qpid::types::Variant::Map::const_iterator crit = event["criteria"].asMap().begin(); crit!= event["criteria"].asMap().end(); crit++) {
				cout << "criteria[" << crit->first << "] - " << crit->second << endl;
				qpid::types::Variant::Map element = crit->second.asMap();
				try {
					if (element["comp"] == "eq") {
						qpid::types::Variant lval = content[element["lval"].asString()];
						qpid::types::Variant rval = element["rval"];
						cout << "lval: " << lval << " (" << getTypeName(lval.getType()) << ")" << endl;
						cout << "rval: " << rval << " (" << getTypeName(rval.getType()) << ")" << endl;
						if (lval.getType()==qpid::types::VAR_STRING || rval.getType()==qpid::types::VAR_STRING) { // compare as string
							criteria[crit->first] = lval.asString() == rval.asString(); 
						} else {
							criteria[crit->first] = lval.isEqualTo(rval);
						}
						cout << lval << " == " << rval << " : " <<  criteria[crit->first] << endl;
					}
					if (element["comp"] == "lt") {
						float lval = event[element["lval"].asString()];
						float rval = element["rval"];
						criteria[crit->first] = lval < rval;
						cout << lval << " < " << rval << " : " << criteria[crit->first] << endl;
					}
					if (element["comp"] == "gt") {
						float lval = event[element["lval"].asString()];
						float rval = element["rval"];
						criteria[crit->first] = lval > rval;
						cout << lval << " > " << rval << " : " << criteria[crit->first] << endl;
					}
				} catch ( const std::exception& error) {
					stringstream errorstring;
					errorstring << error.what();
					cout << "ERROR, exception occured" << errorstring.str() << endl;
					criteria[crit->first] = false;
				}
				stringstream token; token << "criteria[\"" << crit->first << "\"]";
				stringstream boolval; boolval << criteria[crit->first];
				replaceString(nesting, token.str(), boolval.str()); 
			}
			replaceString(nesting, "and", "&");
			replaceString(nesting, "or", "|");
			nesting += ";";
			cout << "nesting prepared: " << nesting << endl;
			if (evaluateNesting(nesting)) {
				agoConnection->sendMessage(event["action"].asMap());
			}
		}	
	}

}

qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map content) {
	qpid::types::Variant::Map returnval;
	std::string internalid = content["internalid"].asString();
	if (internalid == "eventcontroller") {
		if (content["command"] == "setevent") {
			try {
				cout << "setevent request" << endl;
				qpid::types::Variant::Map newevent = content["eventmap"].asMap();
				cout << "event content:" << newevent << endl;
				std::string eventuuid = content["event"].asString();
				if (eventuuid == "") eventuuid = generateUuid();
				cout << "event uuid:" << eventuuid << endl;
				eventmap[eventuuid] = newevent;
				agoConnection->addDevice(eventuuid.c_str(), "event", true);
				if (variantMapToJSONFile(eventmap, EVENTMAPFILE)) {
					returnval["result"] = 0;
					returnval["event"] = eventuuid;
				} else {
					returnval["result"] = -1;
				}
			} catch (qpid::types::InvalidConversion) {
                                returnval["result"] = -1;
                        } catch (...) {
                                returnval["result"] = -1;
				returnval["error"] = "exception";
			}
		} else if (content["command"] == "getevent") {
			try {
				std::string event = content["event"].asString();
				cout << "getevent request:" << event << endl;
				returnval["result"] = 0;
				returnval["eventmap"] = eventmap[event].asMap();
				returnval["event"] = event;
			} catch (qpid::types::InvalidConversion) {
				returnval["result"] = -1;
			}
                } else if (content["command"] == "delevent") {
			std::string event = content["event"].asString();
			cout << "delevent request:" << event << endl;
			returnval["result"] = -1;
			if (event != "") {
				qpid::types::Variant::Map::iterator it = eventmap.find(event);
				if (it != eventmap.end()) {
					cout << "removing ago device" << event << endl;
					agoConnection->removeDevice(it->first.c_str());
					eventmap.erase(it);
					if (variantMapToJSONFile(eventmap, EVENTMAPFILE)) {
						returnval["result"] = 0;
					}
				}
			}
                } 
	}
	return returnval;
}

int main(int argc, char **argv) {
	agoConnection = new AgoConnection("event");	
	agoConnection->addDevice("eventcontroller", "eventcontroller");
	agoConnection->addHandler(commandHandler);
	agoConnection->addEventHandler(eventHandler);

	eventmap = jsonFileToVariantMap(EVENTMAPFILE);
	// cout  << eventmap;
	for (qpid::types::Variant::Map::const_iterator it = eventmap.begin(); it!=eventmap.end(); it++) {
		cout << "adding event:" << it->first << ":" << it->second << endl;	
		agoConnection->addDevice(it->first.c_str(), "event", true);
	}
	agoConnection->run();
}
