#include "querycircleplusrequest.hpp"
#include <boost/lexical_cast.hpp>
using namespace plugwise;


void QueryCirclePlusRequest::send(plugwise::Connection::Ptr con) {
  con->send_payload("004E"+_circle_plus_mac);
}
