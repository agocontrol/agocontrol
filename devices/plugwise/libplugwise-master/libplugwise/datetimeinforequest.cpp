#include "datetimeinforequest.hpp"

using namespace plugwise;


void DateTimeInfoRequest::send(plugwise::Connection::Ptr con) {
  con->send_payload("0029" + _circle_plus_mac);
}
