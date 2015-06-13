#ifndef LIBPLUGWISE_INFORESPONSE_HPP
#define LIBPLUGWISE_INFORESPONSE_HPP 1

#include <common.hpp>
#include <response.hpp>
#include <string>

namespace plugwise {
  class InfoResponse : public Response {
    public:
      typedef std::tr1::shared_ptr<InfoResponse> Ptr;
      InfoResponse ( const std::string& line1, 
                          const std::string& line2) :
        Response(line1, line2) { parse_line2(); };
      virtual std::string str();
      virtual ~InfoResponse() {};
      bool is_ok();
      virtual bool req_successful();
      std::string stick_mac_addr() { return _stick_mac_addr; };
      std::string get_hw_version() { return _hw_ver; };
      std::string get_fw_version() { return _fw_ver; };
      std::string get_hz() { return _hz; };
      std::string get_device_type() { return _device_type; };

    private:
      InfoResponse (const InfoResponse& original);
      InfoResponse& operator= (const InfoResponse& rhs);
      void parse_line2();
      uint32_t _response_code;
      uint32_t _resp_seq_number;
      std::string _stick_mac_addr;
      std::string  _date_time;
      std::string  _last_logaddr;
      std::string  _relay_state;
      std::string  _hz;
      std::string  _hw_ver;
      std::string  _fw_ver;
      std::string  _device_type;

  };
  
};


#endif /* LIBPLUGWISE_STICKINITRESPONSE_HPP */

