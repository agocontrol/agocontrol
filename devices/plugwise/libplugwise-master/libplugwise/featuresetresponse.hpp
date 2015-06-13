#ifndef LIBPLUGWISE_FEATURESETRESPONSE_HPP
#define LIBPLUGWISE_FEATURESETRESPONSE_HPP 1

#include <common.hpp>
#include <response.hpp>
#include <string>

namespace plugwise {
  class FeatureSetResponse : public Response {
    public:
      typedef std::tr1::shared_ptr<FeatureSetResponse> Ptr;
      FeatureSetResponse ( const std::string& line1, 
                          const std::string& line2) :
        Response(line1, line2) { parse_line2(); };
      virtual std::string str();
      virtual ~FeatureSetResponse() {};
      bool is_ok();
      virtual bool req_successful();
      std::string stick_mac_addr() { return _stick_mac_addr; };

    private:
      FeatureSetResponse (const FeatureSetResponse& original);
      FeatureSetResponse& operator= (const FeatureSetResponse& rhs);
      void parse_line2();
      uint32_t _response_code;
      uint32_t _resp_seq_number;
      std::string _stick_mac_addr;
      std::string _features;

  };
  
};


#endif /* LIBPLUGWISE_STICKINITRESPONSE_HPP */

