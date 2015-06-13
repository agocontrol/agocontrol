#include "switchrequest.hpp"
#include <boost/lexical_cast.hpp>
using namespace plugwise;


const std::string hex_digit[16] = {"0","1","2","3","4","5","6","7","8","9","A","B","C","D","E","F"};

void SwitchRequest::send(plugwise::Connection::Ptr con) {
     con->send_payload("0017"+_circle_mac+"0"+hex_digit[int( _mode)]);
}
