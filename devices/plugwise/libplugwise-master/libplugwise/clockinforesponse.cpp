#include "clockinforesponse.hpp"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <boost/lexical_cast.hpp>

using namespace plugwise;

std::string ClockInfoResponse::str() {
  std::ostringstream oss;
  oss << "Clock Info Response: " << std::endl;
  oss << "cmd code: " << std::hex << _command_code;
  oss << ", seqno: " << _sequence_number;
  oss << ", ack: " << _acknowledge_code;
  oss << ", chksum: " << _checksum;
  oss << std::endl << "resp_code: " << _response_code;
  oss << ", resp_seq: " << _resp_seq_number;
  oss << ", mac: " << _circle_plus_mac_addr;
  oss << ", time: " << std::noshowbase << std::setfill('0') << std::dec << std::setw(2) << boost::lexical_cast<uint32_from_hex>(_time.substr(0,2)) << ":" <<  std::setw(2) << boost::lexical_cast<uint32_from_hex>(_time.substr(2,2)) << ":" <<  std::setw(2) << boost::lexical_cast<uint32_from_hex>(_time.substr(4,2));
  oss << ", day of week: " << _day_of_week;
  return oss.str();
}

void ClockInfoResponse::parse_line2() {
  //std::cout << "Parsing second response line: " << _line2 << std::endl;
  if (_line2.length() != 0x2A)
    throw plugwise::DataFormatException(
        "Expected 16 chars for response line 2, got " + _line2.length()); 
  _response_code =boost::lexical_cast<uint32_from_hex>(_line2.substr(0,4));
  _resp_seq_number =boost::lexical_cast<uint32_from_hex>(_line2.substr(4,4));
  _circle_plus_mac_addr =(_line2.substr(8,16));
  _time = (_line2.substr(24,6));
  _day_of_week = (_line2.substr(30,2));
  // Skipping also the checksum (for now).
}


bool ClockInfoResponse::req_successful() {
  if (_response_code == 0x003F) 
    return true;
  else
    return false;
}

