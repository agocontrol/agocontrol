#include "inforesponse.hpp"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

using namespace plugwise;


std::string InfoResponse::str() {

  std::ostringstream oss;

  oss << "Info Response: " << std::endl;
  oss << "cmd code: " << std::hex << _command_code;
  oss << ", seqno: " << _sequence_number;
  oss << ", ack: " << _acknowledge_code;
  oss << ", chksum: " << _checksum;
  oss << std::endl << "resp_code: " << _response_code;
  oss << ", resp_seq: " << _resp_seq_number;
  oss << ", stick mac: " << _stick_mac_addr;
  oss << ", date/time: " << std::dec << std::right << std::noshowbase << std::setfill('0');
  oss << 2000 + boost::lexical_cast<uint32_from_hex>(_date_time.substr(0,2)) << "-" << std::setw(2) << boost::lexical_cast<uint32_from_hex>(_date_time.substr(2,2)) << "-" << std::setw(2) << ((boost::lexical_cast<uint32_from_hex>(_date_time.substr(4,4))) / 60 / 24 + 1) << " ";
  oss << std::setw(2) << ((boost::lexical_cast<uint32_from_hex>(_date_time.substr(4,4))) % (24 * 60)) << ":" << std::setw(2) <<  ((boost::lexical_cast<uint32_from_hex>(_date_time.substr(4,4))) % 60);
  oss << ", last log: " << _last_logaddr;
  oss << ", switch: " << _relay_state;
  oss << ", hz: " << _hz;
  oss << ", hw version: " << _hw_ver;
  oss << ", fw version: " << _fw_ver;
  oss << ", type: " << _device_type;
  return oss.str();
}

void InfoResponse::parse_line2() {
  std::cout << "Parsing second response line: " << _line2 << ", length: " << _line2.length() << std::endl;
  if (_line2.length() != 70)
    throw plugwise::DataFormatException(
        "Expected 16 chars for response line 2, got " + _line2.length()); 
  _response_code =boost::lexical_cast<uint32_from_hex>(_line2.substr(0,4));
  _resp_seq_number =boost::lexical_cast<uint32_from_hex>(_line2.substr(4,4));
  _stick_mac_addr =(_line2.substr(8,16));
  _date_time = (_line2.substr(24,8));
  _last_logaddr = (_line2.substr(32, 8));
  _relay_state = (_line2.substr(40, 2));
  _hz = (_line2.substr(42, 2));
  _hw_ver = (_line2.substr(44, 12));
//  _fw_ver = (_line2.substr(56, 8));

  long int t = boost::lexical_cast<uint32_from_hex>(_line2.substr(56, 8));
  _fw_ver = std::asctime(std::localtime(&t));
  boost::erase_all(_fw_ver, "\n");

  _device_type = (_line2.substr(64, 2));
  // Skipping also the checksum (for now).
}


bool InfoResponse::req_successful() {
  if (_response_code == 0x0024) 
    return true;
  else
    return false;
}

