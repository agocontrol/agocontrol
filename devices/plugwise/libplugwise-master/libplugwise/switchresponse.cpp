#include "switchresponse.hpp"
#include <sstream>
#include <iostream>
#include <boost/lexical_cast.hpp>

using namespace plugwise;

std::string SwitchResponse::str() {
  std::ostringstream oss;
  oss << "Switch Response: " << std::endl;
  oss << "cmd code: " << std::hex << _command_code;
  oss << ", seqno: " << _sequence_number;
  oss << ", ack: " << _acknowledge_code;
  oss << ", chksum: " << _checksum;
  oss << std::endl << "resp_code: " << _response_code;
  oss << ", resp_seq: " << _resp_seq_number;
  return oss.str();
}

void SwitchResponse::parse_line2() {
  std::cout << "Parsing second response line: " << _line2 << ", length: " << _line2.length() << std::endl;
  if (_line2.length() != 0x20)
    throw plugwise::DataFormatException(
        "Expected 16 chars for response line 2, got " + _line2.length()); 
  _response_code =boost::lexical_cast<uint32_from_hex>(_line2.substr(0,4));
  _resp_seq_number =boost::lexical_cast<uint32_from_hex>(_line2.substr(4,4));
  _stick_mac_addr =(_line2.substr(8,16));
  // Skipping also the checksum (for now).
}


bool SwitchResponse::req_successful() {
  if (_response_code == 0x0000) 
    return true;
  else
    return false;
}

