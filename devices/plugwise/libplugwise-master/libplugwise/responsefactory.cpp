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

#include "responsefactory.hpp"
#include <enablejoiningresponse.hpp>
#include <joincircleresponse.hpp>
#include <removecircleresponse.hpp>
#include <stickinitresponse.hpp>
#include <inforesponse.hpp>
#include <pingresponse.hpp>
#include <querycircleplusresponse.hpp>
#include <clocksetresponse.hpp>
#include <datetimeinforesponse.hpp>
#include <setdatetimerequest.hpp>
#include <nodequeryresponse.hpp>
#include <calibrationresponse.hpp>
#include <powerinformationresponse.hpp>
#include <featuresetresponse.hpp>
#include <setbroadcastintervalresponse.hpp>
#include <switchresponse.hpp>
#include <boost/lexical_cast.hpp>

using namespace plugwise;

Response::Ptr ResponseFactory::receive() {
  std::string line1(_con->read_response());
  std::string line2(_con->read_response());
  // The parsing and evaluation of the response lines is done
  // within the Response class hierarchy. Here, we just 
  // peek into line2 to determine the right response class to
  // redirect to.
  uint32_t response_code=boost::lexical_cast<uint32_from_hex>(line2.substr(0,4));
  switch(response_code) {
    case 0x0000:
      return Response::Ptr(new QueryCirclePlusResponse(line1, line2));
      break;
    case 0x000D:
      return Response::Ptr(new PingResponse(line1, line2));
      break;
    case 0x0011:
      return Response::Ptr(new StickInitResponse(line1, line2));
      break;
    case 0x001D:
      return Response::Ptr(new RemoveCircleResponse(line1, line2));
      break;
    case 0x0024:
      return Response::Ptr(new InfoResponse(line1, line2));
      break;
    case 0x0027:
      return Response::Ptr(new CalibrationResponse(line1, line2));
      break;
    case 0x0013:
      return Response::Ptr(new PowerInformationResponse(line1, line2));
      break;
    case 0x0019:
      return Response::Ptr(new NodeQueryResponse(line1, line2));
      break;
    case 0x003A:
      return Response::Ptr(new DateTimeInfoResponse(line1, line2));
      break;
    case 0x003F:
      return Response::Ptr(new ClockInfoResponse(line1, line2));
      break;
  }
  // Just to keep the compiler happy.
  throw CommunicationException("received unknown response.");
}

StickInitResponse::Ptr ResponseFactory::receiveStickInitResponse() {
  std::string line1(_con->read_response());
  std::string line2(_con->read_response());
  uint32_t response_code=boost::lexical_cast<uint32_from_hex>(line2.substr(0,4));
  if (response_code == 0x0011) {
      return StickInitResponse::Ptr(new StickInitResponse(line1, line2));
  } else {
    throw DataFormatException("Expected to parse StickInitResponse");
  }
}

InfoResponse::Ptr ResponseFactory::receiveInfoResponse() {
  std::string line1(_con->read_response());
  std::string line2(_con->read_response());

  if (boost::lexical_cast<uint32_from_hex>(line1.substr(4,4)) < boost::lexical_cast<uint32_from_hex>(line2.substr(4,4))){   // message out of order
   line1 = line2;
   std::string line3(_con->read_response());
   line2 = line3;
  }
  else if (boost::lexical_cast<uint32_from_hex>(line1.substr(4,4)) > boost::lexical_cast<uint32_from_hex>(line2.substr(4,4))){   // message out of order
   std::string line3(_con->read_response());
   line2 = line3;
  }

  uint32_t response_code=boost::lexical_cast<uint32_from_hex>(line2.substr(0,4));

  if (response_code == 0x0024) {
      return InfoResponse::Ptr(new InfoResponse(line1, line2));
  } else {
    throw DataFormatException("Expected to parse InfoResponse, received different");
  }
}

PingResponse::Ptr ResponseFactory::receivePingResponse() {
  std::string line1(_con->read_response());
  std::string line2(_con->read_response());

  if (boost::lexical_cast<uint32_from_hex>(line1.substr(4,4)) < boost::lexical_cast<uint32_from_hex>(line2.substr(4,4))){   // message out of order
   line1 = line2;
   std::string line3(_con->read_response());
   line2 = line3;
  }
  else if (boost::lexical_cast<uint32_from_hex>(line1.substr(4,4)) > boost::lexical_cast<uint32_from_hex>(line2.substr(4,4))){   // message out of order
   std::string line3(_con->read_response());
   line2 = line3;
  }

  uint32_t response_code=boost::lexical_cast<uint32_from_hex>(line2.substr(0,4));

  if (response_code == 0x000E) {
      return PingResponse::Ptr(new PingResponse(line1, line2));
  } else {
    throw DataFormatException("Expected to parse PingResponse, received different");
  }
}


CalibrationResponse::Ptr ResponseFactory::receiveCalibrationResponse() {
  std::string line1(_con->read_response());
  std::string line2(_con->read_response());
  
  if (boost::lexical_cast<uint32_from_hex>(line1.substr(4,4)) < boost::lexical_cast<uint32_from_hex>(line2.substr(4,4))){   // message out of order
   line1 = line2;
   std::string line3(_con->read_response());
   line2 = line3;
  }
  else if (boost::lexical_cast<uint32_from_hex>(line1.substr(4,4)) > boost::lexical_cast<uint32_from_hex>(line2.substr(4,4))){   // message out of order
   std::string line3(_con->read_response());
   line2 = line3;
  }

  uint32_t response_code=boost::lexical_cast<uint32_from_hex>(line2.substr(0,4)); // make sure to take response_code from final line2

  if (response_code == 0x0027) {
      return CalibrationResponse::Ptr(new CalibrationResponse(line1, line2));
  } else {
    throw DataFormatException("Expected to parse CalibrationResponse, received");
  }
}

PowerInformationResponse::Ptr ResponseFactory::receivePowerInformationResponse() {
  std::string line1(_con->read_response());
  std::string line2(_con->read_response());

  if (boost::lexical_cast<uint32_from_hex>(line1.substr(4,4)) < boost::lexical_cast<uint32_from_hex>(line2.substr(4,4))){   // message out of order
   line1 = line2;
   std::string line3(_con->read_response());
   line2 = line3;
  }

  if (boost::lexical_cast<uint32_from_hex>(line1.substr(4,4)) > boost::lexical_cast<uint32_from_hex>(line2.substr(4,4))){   // message out of order
   std::string line3(_con->read_response());
   line2 = line3;
  }

  uint32_t response_code=boost::lexical_cast<uint32_from_hex>(line2.substr(0,4)); // make sure to take response_code from final line2

  if (response_code == 0x0013) {
      return PowerInformationResponse::Ptr(new PowerInformationResponse(line1, line2));
  
  } else if ((line1 == "FFFFFFFFFFFFFFFF") && (line2 == "FFFFFFFFFFFFFFFF")) {
    throw CommunicationException("Zigbee network seems down, perform reinit");
  } else {
    throw DataFormatException("Expected to parse PowerInformationResponse");
  }

}

NodeQueryResponse::Ptr ResponseFactory::receiveNodeQueryResponse() {
  std::string line1(_con->read_response());
  std::string line2(_con->read_response());
  uint32_t response_code=boost::lexical_cast<uint32_from_hex>(line2.substr(0,4));
  if (response_code == 0x0019) {
      return NodeQueryResponse::Ptr(new NodeQueryResponse(line1, line2));
  } else {
    throw DataFormatException("Expected to parse NodeQueryResponse");
  }

}

QueryCirclePlusResponse::Ptr ResponseFactory::receiveQueryCirclePlusResponse() {
  std::string line1(_con->read_response());
  std::string line2(_con->read_response());
  uint32_t response_code=boost::lexical_cast<uint32_from_hex>(line2.substr(0,4));
  if (response_code == 0x0000) {
      return QueryCirclePlusResponse::Ptr(new QueryCirclePlusResponse(line1, line2));
  } else {
    throw DataFormatException("Expected to parse QueryCirclePlusResponse");
  }

}

ClockSetResponse::Ptr ResponseFactory::receiveClockSetResponse() {
  std::string line1(_con->read_response());
  std::string line2(_con->read_response());
  uint32_t response_code=boost::lexical_cast<uint32_from_hex>(line2.substr(0,4));
  if (response_code == 0x0000) {
      return ClockSetResponse::Ptr(new ClockSetResponse(line1, line2));
  } else {
    throw DataFormatException("Expected to parse ClockSetResponse");
  }

}

EnableJoiningResponse::Ptr ResponseFactory::receiveEnableJoiningResponse() {
  std::string line1(_con->read_response());
  std::string line2(_con->read_response());
  uint32_t response_code=boost::lexical_cast<uint32_from_hex>(line2.substr(0,4));
  if (response_code == 0x0000) {
      return EnableJoiningResponse::Ptr(new EnableJoiningResponse(line1, line2));
  } else {
    throw DataFormatException("Expected to parse EnableJoiningResponse");
  }

}

JoinCircleResponse::Ptr ResponseFactory::receiveJoinCircleResponse() {
  std::string line1(_con->read_response());
  std::string line2(_con->read_response());
  uint32_t response_code=boost::lexical_cast<uint32_from_hex>(line2.substr(0,4));
  if (response_code == 0x0000) {
      return JoinCircleResponse::Ptr(new JoinCircleResponse(line1, line2));
  } else {
    throw DataFormatException("Expected to parse JoinCircle Response");
  }

}

RemoveCircleResponse::Ptr ResponseFactory::receiveRemoveCircleResponse() {
  std::string line1(_con->read_response());
  std::string line2(_con->read_response());
  uint32_t response_code=boost::lexical_cast<uint32_from_hex>(line2.substr(0,4));
  if (response_code == 0x001D) {
      return RemoveCircleResponse::Ptr(new RemoveCircleResponse(line1, line2));
  } else {
    throw DataFormatException("Expected to parse RemoveCircle Response");
  }

}

ClockInfoResponse::Ptr ResponseFactory::receiveClockInfoResponse() {
  std::string line1(_con->read_response());
  std::string line2(_con->read_response());

  if (boost::lexical_cast<uint32_from_hex>(line1.substr(4,4)) < boost::lexical_cast<uint32_from_hex>(line2.substr(4,4))){   // message out of order
   line1 = line2;
   std::string line3(_con->read_response());
   line2 = line3;
  }

  if (boost::lexical_cast<uint32_from_hex>(line1.substr(4,4)) > boost::lexical_cast<uint32_from_hex>(line2.substr(4,4))){   // message out of order
   std::string line3(_con->read_response());
   line2 = line3;
  }

  uint32_t response_code=boost::lexical_cast<uint32_from_hex>(line2.substr(0,4)); // make sure to take response_code from final line2

  if (response_code == 0x003F) {
      return ClockInfoResponse::Ptr(new ClockInfoResponse(line1, line2));
  } else {
    throw DataFormatException("Expected to parse ClockInfoResponse");
  }

}

DateTimeInfoResponse::Ptr ResponseFactory::receiveDateTimeInfoResponse() {
  std::string line1(_con->read_response());
  std::string line2(_con->read_response());
  uint32_t response_code=boost::lexical_cast<uint32_from_hex>(line2.substr(0,4));
  if (response_code == 0x003A) {
      return DateTimeInfoResponse::Ptr(new DateTimeInfoResponse(line1, line2));
  } else {
    throw DataFormatException("Expected to parse DateTimeInfoResponse");
  }

}

SetDateTimeResponse::Ptr ResponseFactory::receiveSetDateTimeResponse() {
  std::string line1(_con->read_response());
  std::string line2(_con->read_response());
  uint32_t response_code=boost::lexical_cast<uint32_from_hex>(line2.substr(0,4));
  if (response_code == 0x0000) {
      return SetDateTimeResponse::Ptr(new SetDateTimeResponse(line1, line2));
  } else {
    throw DataFormatException("Expected to parse SetDateTimeResponse");
  }

}


FeatureSetResponse::Ptr ResponseFactory::receiveFeatureSetResponse() {
  std::string line1(_con->read_response());
  std::string line2(_con->read_response());
  uint32_t response_code=boost::lexical_cast<uint32_from_hex>(line2.substr(0,4));
  if (response_code == 0x0060) {
      return FeatureSetResponse::Ptr(new FeatureSetResponse(line1, line2));
  } else {
    throw DataFormatException("Expected to parse ClockInfoResponse");
  }

}

SetBroadcastIntervalResponse::Ptr ResponseFactory::receiveSetBroadcastIntervalResponse() {
  std::string line1(_con->read_response());
  std::string line2(_con->read_response());
  uint32_t response_code=boost::lexical_cast<uint32_from_hex>(line2.substr(0,4));
  if (response_code == 0x0000) {
      return SetBroadcastIntervalResponse::Ptr(new SetBroadcastIntervalResponse(line1, line2));
  } else {
    throw DataFormatException("Expected to parse SetBroadcastIntervalResponse");
  }

}


SwitchResponse::Ptr ResponseFactory::receiveSwitchResponse() {
  std::string line1(_con->read_response());
  std::string line2(_con->read_response());
  uint32_t response_code=boost::lexical_cast<uint32_from_hex>(line2.substr(0,4));
  if (response_code == 0x0000) {
      return SwitchResponse::Ptr(new SwitchResponse(line1, line2));
  } else {
    throw DataFormatException("Expected to parse SwitchResponse");
  }

}

