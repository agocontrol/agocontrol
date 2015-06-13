#include "setdatetimerequest.hpp"
#include <ctime>
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace plugwise;


void SetDateTimeRequest::send(plugwise::Connection::Ptr con) {

   time_t t = time(0); // get time now
   struct tm * now = localtime( & t );

   std::stringstream  timestr;

   timestr << std::dec << std::uppercase << std::right << std::noshowbase << std::setfill('0');
   timestr << std::setw(2) << now->tm_sec << std::setw(2) << now->tm_min << std::setw(2) << now->tm_hour ;
   timestr << std::setw(2) << now->tm_wday;
   timestr << std::setw(2) << now->tm_mday << std::setw(2) << (now->tm_mon + 1) << std::setw(2) << (now->tm_year - 100);

   con->send_payload("0028" + _circle_plus_mac + timestr.str());
}
