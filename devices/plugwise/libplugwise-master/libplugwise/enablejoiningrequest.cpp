#include "enablejoiningrequest.hpp"

using namespace plugwise;


void EnableJoiningRequest::send(plugwise::Connection::Ptr con) {
  con->send_payload("0008" + _mode);
}
