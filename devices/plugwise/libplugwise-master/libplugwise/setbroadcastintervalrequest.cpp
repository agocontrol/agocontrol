#include "setbroadcastintervalrequest.hpp"

using namespace plugwise;


void SetBroadcastIntervalRequest::send(plugwise::Connection::Ptr con) {
  con->send_payload("004A" + _circle_mac + "3C01");
}
