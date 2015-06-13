#include "joincirclerequest.hpp"

using namespace plugwise;


void JoinCircleRequest::send(plugwise::Connection::Ptr con) {
  con->send_payload("0007" + _permission + _device_id);
}
