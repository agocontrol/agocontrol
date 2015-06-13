#include "datetimeinforesponse.hpp"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <boost/lexical_cast.hpp>

using namespace plugwise;

std::string DateTimeInfoResponse::str() {
  std::ostringstream oss;
  try {
     oss << "Date Time Info Response: " << std::endl;
     oss << "cmd code: " << std::hex << _command_code;
     oss << ", seqno: " << _sequence_number;
     oss << ", ack: " << _acknowledge_code;
     oss << ", chksum: " << _checksum;
     oss << std::endl << "resp_code: " << _response_code;
     oss << ", resp_seq: " << _resp_seq_number;
     oss << ", mac: " << _circle_plus_mac_addr;
     oss << ", time: " << std::dec << std::right << std::setfill('0') << std::setw(2) << boost::lexical_cast<short>(_time.substr(4,2)) << ":" << std::setw(2) << boost::lexical_cast<short>(_time.substr(2,2)) << ":" << std::setw(2) << boost::lexical_cast<short>(_time.substr(0,2));
     oss << ", day of week: " << _day_of_week;
     oss << ", date: " << std::dec << (boost::lexical_cast<short>(_date.substr(4,2)) + 2000) << "-" << boost::lexical_cast<short>(_date.substr(2,2)) << "-" << boost::lexical_cast<short>(_date.substr(0,2));
   } catch (const boost::bad_lexical_cast &) {
     oss << "Error in lexical cast" ;
   };
   return oss.str();
}

void DateTimeInfoResponse::parse_line2() {
  //std::cout << "Parsing second response line: " << _line2 << std::endl;
  if (_line2.length() != 0x2A)
    throw plugwise::DataFormatException(
        "Expected 42 chars for response line 2, got " + _line2.length()); 
  _response_code =boost::lexical_cast<uint32_from_hex>(_line2.substr(0,4));
  _resp_seq_number =boost::lexical_cast<uint32_from_hex>(_line2.substr(4,4));
  _circle_plus_mac_addr =(_line2.substr(8,16));
  _time = (_line2.substr(24,6));
  _day_of_week = (_line2.substr(30,2));
  _date = (_line2.substr(32,6));
  // Skipping also the checksum (for now).
}


bool DateTimeInfoResponse::req_successful() {
  if (_response_code == 0x003A) 
    return true;
  else
    return false;
}

