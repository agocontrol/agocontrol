#include "nodequeryresponse.hpp"
#include <sstream>
#include <iostream>
#include <boost/lexical_cast.hpp>

using namespace plugwise;

std::string NodeQueryResponse::str() {
  std::ostringstream oss;
  oss << "Node Query Response: " << std::endl;
  oss << "cmd code: " << std::hex << _command_code;
  oss << ", seqno: " << _sequence_number;
  oss << ", ack: " << _acknowledge_code;
  oss << ", chksum: " << _checksum;
  oss << std::endl << "resp_code: " << _response_code;
  oss << ", resp_seq: " << _resp_seq_number;
  oss << ", circle+ mac: " << _stick_mac_addr;
  oss << ", circle mac: " << _circle_mac_addr;
  oss << ", circle index: "<< _circle_index;
  return oss.str();
}

void NodeQueryResponse::parse_line2() {
  std::cout << "Parsing second response line: " << _line2 << ", length: " << _line2.length() << std::endl;
  if (_line2.length() != 0x2e)
    throw plugwise::DataFormatException(
        "Expected 16 chars for response line 2, got " + _line2.length()); 
  _response_code =boost::lexical_cast<uint32_from_hex>(_line2.substr(0,4));
  _resp_seq_number =boost::lexical_cast<uint32_from_hex>(_line2.substr(4,4));
  _stick_mac_addr =(_line2.substr(8,16));
  _circle_mac_addr = (_line2.substr(24,16));
  _circle_index = (_line2.substr(40,2));
  // Skipping also the checksum (for now).
}


bool NodeQueryResponse::req_successful() {
  if (_response_code == 0x0019) 
    return true;
  else
    return false;
}

