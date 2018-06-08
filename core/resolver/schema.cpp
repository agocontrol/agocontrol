
#include <qpid/messaging/Connection.h>
#include <qpid/messaging/Message.h>
#include <qpid/messaging/Receiver.h>
#include <qpid/messaging/Sender.h>
#include <qpid/messaging/Session.h>
#include <qpid/messaging/Address.h>


#include "schema.h"
#include <iostream>
#include <fstream>
#include <string>

#if defined(__FreeBSD__) && YAML_CPP_VERSION <= 3
#include "yaml-cpp03/yaml.h"
#else
#include "yaml-cpp/yaml.h"
#endif

using namespace qpid::messaging;
using namespace qpid::types;

static Variant::List sequenceToVariantList(const YAML::Node &node);
static Variant::Map mapToVariantMap(const YAML::Node &node);

/// Merge two Variant Lists.
qpid::types::Variant::List mergeList(qpid::types::Variant::List a, qpid::types::Variant::List b) {
    qpid::types::Variant::List result = a;
    result.splice(result.begin(),b);
    return result;
} 

qpid::types::Variant::Map mergeMap(qpid::types::Variant::Map a, qpid::types::Variant::Map b) {
    qpid::types::Variant::Map result = a;

    for (qpid::types::Variant::Map::const_iterator it = b.begin(); it != b.end(); it++) {
        qpid::types::Variant::Map::const_iterator it_a = result.find(it->first);
        if (it_a != result.end()) {
            if ((it_a->second.getType()==VAR_MAP) && (it->second.getType()==VAR_MAP)) {
                result[it->first] = mergeMap(it_a->second.asMap(), it->second.asMap());
            } else if ((it_a->second.getType()==VAR_LIST) && (it->second.getType()==VAR_LIST)) {
                result[it->first] = mergeList(it_a->second.asList(), it->second.asList());
            } else {
                qpid::types::Variant::List list;
                list.push_front(it->second);
                list.push_front(it_a->second);
                result[it->first] = list;
            }
        } else {
            result[it->first] = it->second;
        }

    }

    return result;

}

static Variant::Map mapToVariantMap(const YAML::Node &node) {
    Variant::Map output;

#if YAML_CPP_VERSION >= 5
    for(YAML::const_iterator it=node.begin();it!=node.end();++it) {
        std::string key = it->first.as<std::string>();
        //std::cout << "Key: " << key << " Type: " << it->second.Type() <<  std::endl;
        switch(it->second.Type()) {
            case YAML::NodeType::Map:
                output[key] = mapToVariantMap(it->second);
                break;
            case YAML::NodeType::Sequence:
                output[key] = sequenceToVariantList(it->second);
                break;
            case YAML::NodeType::Scalar:
                try {
                    output[key] = it->second.as<std::string>();
                } catch(YAML::TypedBadConversion<std::string> e) {
                    std::cout << "error" << std::endl;
                }
                break;
            case YAML::NodeType::Null:
                output[key] = Variant();
                break;
            case YAML::NodeType::Undefined:
               std::cout << "Error: YAML value for " << key << " is undefined" << std::endl;
               break;
        }
    }
#else
    if (node.Type() == YAML::NodeType::Map) {
        for(YAML::Iterator it=node.begin(); it!=node.end(); ++it) {
            if (it.first().Type() == YAML::NodeType::Scalar) {
                std::string key;
                it.first() >> key;
                if (it.second().Type() == YAML::NodeType::Map) {
                    output[key] = mapToVariantMap(it.second());
                } else if (it.second().Type() == YAML::NodeType::Sequence) {
                    output[key] = sequenceToVariantList(it.second());
                } else if (it.second().Type() == YAML::NodeType::Scalar) {
                    std::string value;
                    it.second() >> value;
                    output[key] = value;
                }
            } else {
               std::cout << "Error: YAML key is not scalar" << std::endl;
            }
        }
    }
#endif
    return output;
}

static Variant::List sequenceToVariantList(const YAML::Node &node) {
    Variant::List output;

#if YAML_CPP_VERSION >= 5
    for (YAML::const_iterator it=node.begin();it!=node.end();++it) {
        switch (it->Type()) {
            case YAML::NodeType::Map:
                output.push_back(mapToVariantMap(it->second));
                break;
            case YAML::NodeType::Sequence:
                output.push_back(sequenceToVariantList(it->second));
                break;
            case YAML::NodeType::Scalar:
                output.push_back(Variant(it->as<std::string>()));
                break;
            case YAML::NodeType::Null:
                output.push_back(Variant());
                break;
            case YAML::NodeType::Undefined:
               std::cout << "Error: YAML value in array is undefined" << std::endl;
               break;
        }
    }
#else
    if (node.Type() == YAML::NodeType::Sequence) {
        for(unsigned int i=0; i<node.size(); i++) {
            if (node[i].Type() == YAML::NodeType::Sequence) {
                output.push_back(sequenceToVariantList(node[i]));
            } else if (node[i].Type() == YAML::NodeType::Map) {
                output.push_back(mapToVariantMap(node[i]));
            } else if (node[i].Type() == YAML::NodeType::Scalar) {
                std::string value;
                node[i] >> value;
                output.push_back(Variant(value));
            }
        }
    }
#endif
    return output;
}

Variant::Map parseSchema(const fs::path &file) {
#if YAML_CPP_VERSION >= 5
    YAML::Node schema = YAML::LoadFile(file.string());
    return mapToVariantMap(schema);
#else
    std::ifstream fin(file.string());
    YAML::Parser parser(fin);
    Variant::Map schema;
    YAML::Node doc;
    while(parser.GetNextDocument(doc)) {
        if (doc.Type() == YAML::NodeType::Map) {
            schema = mapToVariantMap(doc);
        }
    }
    return schema;
#endif
}

