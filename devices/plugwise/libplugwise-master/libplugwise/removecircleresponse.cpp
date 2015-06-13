#include "removecircleresponse.hpp"
#include <sstream>
#include <iostream>
#include <boost/lexical_cast.hpp>

using namespace plugwise;

std::string RemoveCircleResponse::str() {
  std::ostringstream oss;
  oss << "Remove Circle Response: " << std::endl;
  oss << "cmd code: " << std::hex << _command_code;
  oss << ", seqno: " << _sequence_number;
  oss << ", ack: " << _acknowledge_code;
  oss << ", chksum: " << _checksum;
  oss << std::endl << "resp_code: " << _response_code;
  oss << ", resp_seq: " << _resp_seq_number;
  oss << ", circle+ mac: " << _circle_plus_mac;
  oss << ", circle mac: " << _circle_mac;
  oss << ", index: " << _index;

  return oss.str();
}

void RemoveCircleResponse::parse_line2() {
  //std::cout << "Parsing second response line: " << _line2 << std::endl;
  if (_line2.length() != 0x2E)
    throw plugwise::DataFormatException(
        "Expected 16 chars for response line 2, got " + _line2.length()); 
  _response_code =boost::lexical_cast<uint32_from_hex>(_line2.substr(0,4));
  _resp_seq_number =boost::lexical_cast<uint32_from_hex>(_line2.substr(4,4));
  _circle_plus_mac =(_line2.substr(8,16));
  _circle_mac = (_line2.substr(24,16));
  _index = (_line2.substr(40,2));
  // Skipping also the checksum (for now).
}


bool RemoveCircleResponse::req_successful() {
  if (_response_code == 0x001D) 
    return true;
  else
    return false;
}

