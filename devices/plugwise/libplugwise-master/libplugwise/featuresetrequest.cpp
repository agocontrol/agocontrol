#include "featuresetrequest.hpp"

using namespace plugwise;


void FeatureSetRequest::send(plugwise::Connection::Ptr con) {
  con->send_payload("005F"+_circle_plus_mac);
}
