#include "clocksetrequest.hpp"
#include <ctime>
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace plugwise;


void ClockSetRequest::send(plugwise::Connection::Ptr con) {

   std::stringstream  datestr;
   std::stringstream  timestr;

   time_t t = time(0); // get time now 
   struct tm * now = localtime( & t ); 
   datestr << std::hex << std::uppercase << std::right << std::noshowbase << std::setfill('0') << std::setw(2) << (now->tm_year - 100) << std::setw(2) << (now->tm_mon + 1) << std::setw(4) <<  (now->tm_mday - 1) * 24 * 60 + now->tm_hour * 60 + now->tm_min;
   timestr << std::hex << std::uppercase << std::right << std::noshowbase << std::setfill('0') << std::setw(2) << now->tm_hour << std::setw(2) << now->tm_min << setw(2) << now->tm_sec << setw(2) << now->tm_wday;
   con->send_payload("0016"+_circle_plus_mac+datestr.str()+"FFFFFFFF"+timestr.str());
}
