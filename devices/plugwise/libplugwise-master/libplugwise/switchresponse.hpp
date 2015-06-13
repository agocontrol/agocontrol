#ifndef LIBPLUGWISE_SWITCHRESPONSE_HPP
#define LIBPLUGWISE_SWITCHRESPONSE_HPP 1

#include <common.hpp>
#include <response.hpp>
#include <string>

namespace plugwise {
  class SwitchResponse : public Response {
    public:
      typedef std::tr1::shared_ptr<SwitchResponse> Ptr;
      SwitchResponse ( const std::string& line1, 
                          const std::string& line2) :
        Response(line1, line2) { parse_line2(); };
      virtual std::string str();
      virtual ~SwitchResponse() {};
      bool is_ok();
      virtual bool req_successful();
      std::string circle_mac_addr() { return _circle_mac_addr; };

    private:
      SwitchResponse (const SwitchResponse& original);
      SwitchResponse& operator= (const SwitchResponse& rhs);
      void parse_line2();
      uint32_t _response_code;
      uint32_t _resp_seq_number;
      std::string _stick_mac_addr;
      std::string _circle_mac_addr;
      std::string _circle_index;

  };
  
};


#endif /* LIBPLUGWISE_SWITCHRESPONSE_HPP */

