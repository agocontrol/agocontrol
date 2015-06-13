#ifndef LIBPLUGWISE_REMOVECIRCLERESPONSE_HPP
#define LIBPLUGWISE_REMOVECIRCLERESPONSE_HPP 1

#include <common.hpp>
#include <response.hpp>
#include <iomanip>
#include <string>

namespace plugwise {
  class RemoveCircleResponse : public Response {
    public:
      typedef std::tr1::shared_ptr<RemoveCircleResponse> Ptr;
      RemoveCircleResponse ( const std::string& line1, 
                          const std::string& line2) :
        Response(line1, line2) { parse_line2(); };
      virtual std::string str();
      virtual ~RemoveCircleResponse() {};
      bool is_ok();
      virtual bool req_successful();
      std::string stick_mac_addr() { return _circle_plus_mac; };

    private:
      RemoveCircleResponse (const RemoveCircleResponse& original);
      RemoveCircleResponse& operator= (const RemoveCircleResponse& rhs);
      void parse_line2();
      uint32_t _response_code;
      uint32_t _resp_seq_number;
      std::string _circle_plus_mac;
      std::string _circle_mac;
      std::string _index;
  };
  
};


#endif /* LIBPLUGWISE_STICKINITRESPONSE_HPP */

