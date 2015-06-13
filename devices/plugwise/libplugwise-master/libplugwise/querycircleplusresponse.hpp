#ifndef LIBPLUGWISE_QUERYCIRCLEPLUSRESPONSE_HPP
#define LIBPLUGWISE_QUERYCIRCLEPLUSRESPONSE_HPP 1

#include <common.hpp>
#include <response.hpp>
#include <string>

namespace plugwise {
  class QueryCirclePlusResponse : public Response {
    public:
      typedef std::tr1::shared_ptr<QueryCirclePlusResponse> Ptr;
      QueryCirclePlusResponse ( const std::string& line1, 
                          const std::string& line2) :
        Response(line1, line2) { parse_line2(); };
      virtual std::string str();
      virtual ~QueryCirclePlusResponse() {};
      bool is_ok();
      virtual bool req_successful();
      std::string circle_plus_mac_addr() { return _circle_plus_mac_addr; };

    private:
      QueryCirclePlusResponse (const QueryCirclePlusResponse& original);
      QueryCirclePlusResponse& operator= (const QueryCirclePlusResponse& rhs);
      void parse_line2();
      uint32_t _response_code;
      uint32_t _resp_seq_number;
      std::string _circle_plus_mac_addr;

  };
  
};


#endif /* LIBPLUGWISE_STICKINITRESPONSE_HPP */

