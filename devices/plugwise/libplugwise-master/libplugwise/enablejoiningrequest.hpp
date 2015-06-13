/**
 * This file is part of libplugwise.
 *
 * (c) Fraunhofer ITWM - Mathias Dalheimer <dalheimer@itwm.fhg.de>, 2010
 *
 * libplugwise is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * libplugwise is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libplugwise. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef LIBPLUGWISE_ENABLEJOININGREQUEST_HPP
#define LIBPLUGWISE_ENABLEJOININGREQUEST_HPP 1

#include <request.hpp>
#include <connection.hpp>

namespace plugwise {
  class EnableJoiningRequest : public Request {
    public:
      typedef std::tr1::shared_ptr<EnableJoiningRequest> Ptr;
      EnableJoiningRequest (std::string mode) :
        _mode(mode) {};
      void send(plugwise::Connection::Ptr con);
      virtual ~EnableJoiningRequest() {};

    private:
      EnableJoiningRequest (const EnableJoiningRequest& original);
      EnableJoiningRequest& operator= (const EnableJoiningRequest& rhs);
      std::string _mode;
      
  };
  
};


#endif /* LIBPLUGWISE_STICKINITREQUEST_HPP */

