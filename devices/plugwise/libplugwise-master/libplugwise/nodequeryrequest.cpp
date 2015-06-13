#include "nodequeryrequest.hpp"
#include <boost/lexical_cast.hpp>
using namespace plugwise;


const std::string hex_digit[16] = {"0","1","2","3","4","5","6","7","8","9","A","B","C","D","E","F"};

void NodeQueryRequest::send(plugwise::Connection::Ptr con) {
     con->send_payload("0018"+_circle_mac+hex_digit[int( _index / 16)]+hex_digit[_index % 16]);
}
