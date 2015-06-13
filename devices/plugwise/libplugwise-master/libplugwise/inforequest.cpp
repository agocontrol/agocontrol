#include "inforequest.hpp"

using namespace plugwise;


void InfoRequest::send(plugwise::Connection::Ptr con) {
  con->send_payload("0023"+_circle_mac);
}
