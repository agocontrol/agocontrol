#include "pingrequest.hpp"

using namespace plugwise;


void PingRequest::send(plugwise::Connection::Ptr con) {
  con->send_payload("000D" + _circle_mac);
}
