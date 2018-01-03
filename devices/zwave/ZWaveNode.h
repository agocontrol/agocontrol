#include <stdio.h>
#include <unistd.h>
#include <string>
#include <sstream>
#include <list>
#include <map>

#include <openzwave/value_classes/ValueStore.h>
#include <openzwave/value_classes/Value.h>
#include <openzwave/value_classes/ValueBool.h>

class ZWaveNode {
    protected:
        std::string devicetype;
        std::string id;	
        std::map<std::string, OpenZWave::ValueID> values;
    public:
        ZWaveNode(const std::string& id, const std::string& devicetype);
        ~ZWaveNode();
        const std::string& getId();
        const std::string& getDevicetype();
        void setDevicetype(const std::string& devicetype);
        bool hasValue(OpenZWave::ValueID valueID);
        bool addValue(const std::string& label, OpenZWave::ValueID valueID);
        OpenZWave::ValueID *getValueID(const std::string& label);
        std::string toString();
};

class ZWaveNodes {
    public:
        std::list<ZWaveNode*> nodes;
        ZWaveNodes();
        ~ZWaveNodes();
        ZWaveNode *findValue(OpenZWave::ValueID valueID);
        ZWaveNode *findId(const std::string& id);
        std::string toString();
        bool add(ZWaveNode *node);
        bool remove(const std::string& id);
};
