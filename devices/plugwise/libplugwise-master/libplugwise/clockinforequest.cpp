#include "clockinforequest.hpp"

using namespace plugwise;


void ClockInfoRequest::send(plugwise::Connection::Ptr con) {
  con->send_payload("003E" + _circle_plus_mac);
}
