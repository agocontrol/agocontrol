#include "removecirclerequest.hpp"

using namespace plugwise;

const std::string hex_digit[16] = {"0","1","2","3","4","5","6","7","8","9","A","B","C","D","E","F"};


void RemoveCircleRequest::send(plugwise::Connection::Ptr con) {
  con->send_payload("001C" + _circleplus_mac + _device_id );
}


