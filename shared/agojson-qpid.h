#ifndef AGOCONTROL_AGOJSONQPID_H
#define AGOCONTROL_AGOJSONQPID_H

#include <json/json.h>
#include <qpid/messaging/Message.h>

namespace agocontrol {

    /// convert a Variant::Map to JSON representation.
    void variantMapToJson(const qpid::types::Variant::Map &map, Json::Value &root) ;
    void variantListToJson(const qpid::types::Variant::List &list, Json::Value &root) ;

    /// convert a JSON value to a Variant::Map.
    qpid::types::Variant::Map jsonToVariantMap(const Json::Value& value);

    /// convert a JSON string to a Variant::List.
    qpid::types::Variant::List jsonToVariantList(const Json::Value& value);
}


#endif //AGOCONTROL_AGOJSONQPID_H
