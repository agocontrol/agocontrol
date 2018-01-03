#include <openzwave/value_classes/ValueID.h>
#include "ZWaveNode.h"

ZWaveNode::ZWaveNode(const std::string& _id, const std::string& _devicetype) {
    devicetype = _devicetype;
    id = _id;
}

ZWaveNode::~ZWaveNode() {
}

const std::string& ZWaveNode::getId() {
    return id;
}

const std::string& ZWaveNode::getDevicetype() {
    return devicetype;
}

void ZWaveNode::setDevicetype(const std::string& _devicetype) {
    devicetype = _devicetype;
}

OpenZWave::ValueID *ZWaveNode::getValueID(const std::string& label) {
    for (std::map<std::string, OpenZWave::ValueID>::iterator it = values.begin(); it != values.end(); ++it) {
        if (it->first == label) return &(it->second);
    }
    return NULL;
}

bool ZWaveNode::hasValue(OpenZWave::ValueID valueID) {
    for (std::map<std::string, OpenZWave::ValueID>::const_iterator it = values.begin(); it != values.end(); ++it) {
        if (it->second == valueID) return true;
    }
    return false;
}

bool ZWaveNode::addValue(const std::string& label, OpenZWave::ValueID valueID) {
    if (!hasValue(valueID))
        values.insert ( std::pair<std::string, OpenZWave::ValueID>  (label, valueID));
    return true;
}

std::string ZWaveNode::toString() {
    std::stringstream result;
    result << id << " (" << devicetype << "): ";
    for (std::map<std::string, OpenZWave::ValueID>::const_iterator it = values.begin(); it != values.end(); ++it) {
        result << it->first << " ";		
    }
    return result.str();
}

ZWaveNodes::ZWaveNodes() {

}

ZWaveNodes::~ZWaveNodes() {
    for (std::list<ZWaveNode*>::const_iterator it = nodes.begin(); it!= nodes.end(); ) {
        ZWaveNode *tmpNode = *it;
        it++;
        delete tmpNode;
    }
}

ZWaveNode *ZWaveNodes::findValue(OpenZWave::ValueID valueID) {
    for (std::list<ZWaveNode*>::const_iterator it = nodes.begin(); it!= nodes.end(); it++) {
        if ((*it)->hasValue(valueID)) return (*it);
    }
    return NULL;
}

ZWaveNode *ZWaveNodes::findId(const std::string& id) {
    for (std::list<ZWaveNode*>::const_iterator it = nodes.begin(); it!= nodes.end(); it++) {
        if ((*it)->getId() == id)
            return (*it);
    }
    return NULL;
}

bool ZWaveNodes::add(ZWaveNode *node) {
    nodes.push_back(node);
    return true;
}

bool ZWaveNodes::remove(const std::string& id) {
    for (std::list<ZWaveNode*>::iterator it = nodes.begin(); it!= nodes.end(); it++) {
        if ((*it)->getId() == id) {
            nodes.erase(it);
            return true;
        }
    }
    return false;	
}

std::string ZWaveNodes::toString() {
    std::stringstream result;
    for (std::list<ZWaveNode*>::iterator it = nodes.begin(); it!= nodes.end(); it++) {
        result << (*it)->toString() << std::endl;		
    }
    return result.str();
}
