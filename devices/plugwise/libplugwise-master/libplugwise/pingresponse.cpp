#include "pingresponse.hpp"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <boost/lexical_cast.hpp>

using namespace plugwise;

std::string PingResponse::str() {
  std::ostringstream oss;
  oss << "Ping response: " << std::endl;
  oss << "cmd code: " << std::hex << _command_code;
  oss << ", seqno: " << _sequence_number;
  oss << ", ack: " << _acknowledge_code;
  oss << ", chksum: " << _checksum;
  oss << std::endl << "resp_code: " << _response_code;
  oss << ", resp_seq: " << _resp_seq_number;
  oss << ", circle mac: " << _circle_mac_addr;
  oss << ", qin: " << std::dec << _qin;
  oss << ", qout: " << _qout;
  oss << ", ping time: " << _ping_time;
  return oss.str();
}

void PingResponse::parse_line2() {
  //std::cout << "Parsing second response line: " << _line2 << std::endl;
  if (_line2.length() != 0x24)
    throw plugwise::DataFormatException(
        "Expected 16 chars for response line 2, got " + _line2.length()); 
  _response_code =boost::lexical_cast<uint32_from_hex>(_line2.substr(0,4));
  _resp_seq_number =boost::lexical_cast<uint32_from_hex>(_line2.substr(4,4));
  _circle_mac_addr =(_line2.substr(8,16));
  _qin = boost::lexical_cast<uint32_from_hex>(_line2.substr(24,2));
  _qout = boost::lexical_cast<uint32_from_hex>(_line2.substr(26,2));
  _ping_time = boost::lexical_cast<uint32_from_hex>(_line2.substr(28,4));
  // Skipping also the checksum (for now).
}


bool PingResponse::req_successful() {
  if (_response_code == 0x000E) 
    return true;
  else
    return false;
}

