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

#include <connection.hpp>
#include <requestfactory.hpp>
#include <responsefactory.hpp>
#include <powerconverter.hpp>
#include <request.hpp>

#include <boost/lexical_cast.hpp>

#include <list>

std::list<std::string> circle_list;

int main(int argc,char** argv) {

  if (argc < 3) {
    std::cout << "Please start with " << argv[0] << 
      " <stick device> "
      " <circle-id> [<additional-circle-id> <...>]" << std::endl;
    std::cout << "For example: " << argv[0] 
      << " /dev/ttyUSB0 000D6F00007293BD" << std::endl;
    return -4;
  }

  plugwise::Connection::Ptr con(new plugwise::Connection(argv[1]));
  plugwise::RequestFactory::Ptr reqFactory(new plugwise::RequestFactory());
  plugwise::ResponseFactory::Ptr respFactory(new plugwise::ResponseFactory(con));

  try {

//    LOG("### Reset stick");
//    plugwise::Request::Ptr reset_req=reqFactory->getResetRequest("00");
//    reset_req->send(con);
//    plugwise::Response::Ptr si_resp=respFactory->receive();
//    plugwise::ResetResponse::Ptr reset_resp=respFactory->receiveResetResponse();
//
//    LOG("### " << reset_resp->str());
//  
//  if (reset_resp->req_successful()) {
//      LOG("Stick reset successful.");
//    } else {
//      std::cout << "failed to reset stick" << std::endl;
//      return -1;
//    }


    LOG("### Initializing stick");
    plugwise::Request::Ptr si_req=reqFactory->getStickInitRequest();
    si_req->send(con);
    plugwise::StickInitResponse::Ptr si_resp=respFactory->receiveStickInitResponse();

    LOG("### " << si_resp->str());
    if (si_resp->req_successful()) {
      LOG("initialization successful.");
    } else {
      std::cout << "failed to initialize stick" << std::endl;
      return -1;
    }


    LOG("### Info request stick");
    plugwise::Request::Ptr in0_req=reqFactory->getInfoRequest(si_resp->stick_mac_addr());
    in0_req->send(con);
    plugwise::InfoResponse::Ptr in0_resp=respFactory->receiveInfoResponse();

    LOG("### " << in0_resp->str());
    if (in0_resp->req_successful()) {
      LOG("initialization successful.");
    } else {
      std::cout << "failed to initialize stick" << std::endl;
      return -1;
   }


//    LOG("### Reset stick");
//    plugwise::Request::Ptr reset_req=reqFactory->getResetRequest("00");
//    reset_req->send(con);
//    plugwise::ResetResponse::Ptr reset_resp=respFactory->receiveResetResponse();

//    LOG("### " << reset_resp->str());

//    if (reset_resp->req_successful()) {
//      LOG("Stick reset successful.");
//    } else {
//      std::cout << "failed to reset stick" << std::endl;
//      return -1;
//    }




//    LOG("### Initializing circle+");
//    plugwise::Request::Ptr qc_req=reqFactory->getQueryCirclePlusRequest(si_resp->circle_mac_addr());
//    qc_req->send(con);
//    plugwise::QueryCirclePlusResponse::Ptr qc_resp=respFactory->receiveQueryCirclePlusResponse();

//    LOG("### " << qc_resp->str());
//    if (qc_resp->req_successful()) {
//      LOG("initialization successful.");
//    } else {
//      std::cout << "failed to initialize circle+" << std::endl;
//      return -1;
//    }


//    LOG("### Initializing stick #5");
//    plugwise::Request::Ptr si2_req=reqFactory->getStickInitRequest();
//    si2_req->send(con);
//    plugwise::StickInitResponse::Ptr si2_resp=respFactory->receiveStickInitResponse();

//    LOG("### " << si2_resp->str());
//    if (si2_resp->req_successful()) {
//      LOG("initialization successful.");
//    } else {
//      std::cout << "failed to initialize stick" << std::endl;
//      return -1;
//    }



    LOG("### Info request stick #2");
    plugwise::Request::Ptr in2_req=reqFactory->getInfoRequest(si_resp->stick_mac_addr());
    in2_req->send(con);
    plugwise::InfoResponse::Ptr in2_resp=respFactory->receiveInfoResponse();

    LOG("### " << in2_resp->str());
    if (in2_resp->req_successful()) {
      LOG("initialization successful.");
    } else {
      std::cout << "failed to initialize stick" << std::endl;
      return -1;
    }

    LOG("### Info request stick #3");
    plugwise::Request::Ptr in3_req=reqFactory->getInfoRequest(si_resp->stick_mac_addr());
    in3_req->send(con);
    plugwise::InfoResponse::Ptr in3_resp=respFactory->receiveInfoResponse();

    LOG("### " << in3_resp->str());
    if (in3_resp->req_successful()) {
      LOG("initialization successful.");
    } else {
      std::cout << "failed to initialize stick" << std::endl;
      return -1;
    }



    LOG("### Info request circle+");
    plugwise::Request::Ptr inc_req=reqFactory->getInfoRequest(si_resp->circle_mac_addr());
    inc_req->send(con);
    plugwise::InfoResponse::Ptr inc_resp=respFactory->receiveInfoResponse();

    LOG("### " << inc_resp->str());
    if (inc_resp->req_successful()) {
      LOG("initialization successful.");
    } else {
      std::cout << "failed to initialize stick" << std::endl;
      return -1;
    }


//    LOG("### Set clock request circle+");
//    plugwise::Request::Ptr cs_req=reqFactory->getClockSetRequest(si_resp->circle_mac_addr());
//    cs_req->send(con);
//    plugwise::ClockSetResponse::Ptr cs_resp=respFactory->receiveClockSetResponse();

//    LOG("### " << cs_resp->str());
//    if (cs_resp->req_successful()) {
//      LOG("initialization successful.");
//    } else {
//      std::cout << "failed to initialize stick" << std::endl;
//      return -1;
//    }


//    LOG("### Clock info request circle+");
//    plugwise::Request::Ptr ci_req=reqFactory->getClockInfoRequest(si_resp->circle_mac_addr());
//    ci_req->send(con);
//    plugwise::ClockInfoResponse::Ptr ci_resp=respFactory->receiveClockInfoResponse();

//    LOG("### " << ci_resp->str());
//    if (ci_resp->req_successful()) {
//      LOG("initialization successful.");
//    } else {
//      std::cout << "failed to receive clock info" << std::endl;
//      return -1;
//    }


    LOG("### Sending calibration request ");
    plugwise::Request::Ptr ca_req=reqFactory->getCalibrationRequest(si_resp->circle_mac_addr());
    ca_req->send(con);
    plugwise::CalibrationResponse::Ptr ca_resp=respFactory->receiveCalibrationResponse();
    LOG("### " << ca_resp->str());
    if (ca_resp->req_successful()) {
      LOG("calibration successful.");
    } else {
      std::cout << "failed to read calibration values from circle " << std::endl;
      return -2;
    }


//   LOG("### Reset stick 2");
//    plugwise::Request::Ptr reset2_req=reqFactory->getResetRequest("00");
//    reset2_req->send(con);
//    plugwise::ResetResponse::Ptr reset2_resp=respFactory->receiveResetResponse();

//    LOG("### " << reset2_resp->str());
//    if (reset2_resp->req_successful()) {
//      LOG("Stick reset successful.");
//    } else {
//      std::cout << "failed to reset stick" << std::endl;
//      return -1;
//    }

    LOG("### Info request #2 circle+");
    plugwise::Request::Ptr inc1_req=reqFactory->getInfoRequest(si_resp->circle_mac_addr());
    inc1_req->send(con);
    plugwise::InfoResponse::Ptr inc1_resp=respFactory->receiveInfoResponse();

    LOG("### " << inc1_resp->str());
    if (inc1_resp->req_successful()) {
      LOG("initialization successful.");
    } else {
      std::cout << "failed to initialize stick" << std::endl;
      return -1;
    }



    LOG("### Featreu request ");
    plugwise::Request::Ptr fs_req=reqFactory->getFeatureSetRequest(si_resp->circle_mac_addr());
    fs_req->send(con);
    plugwise::FeatureSetResponse::Ptr fs_resp=respFactory->receiveFeatureSetResponse();

    LOG("### " << fs_resp->str());
    if (fs_resp->req_successful()) {
      LOG("Feature set successful.");
    } else {
      std::cout << "failed to reset stick" << std::endl;
      return -1;
    }

    LOG("### Info request #4 circle+");
    plugwise::Request::Ptr inc4_req=reqFactory->getInfoRequest(si_resp->circle_mac_addr());
    inc4_req->send(con);
    plugwise::InfoResponse::Ptr inc4_resp=respFactory->receiveInfoResponse();

    LOG("### " << inc4_resp->str());
    if (inc4_resp->req_successful()) {
      LOG("initialization successful.");
    } else {
      std::cout << "failed to initialize stick" << std::endl;
      return -1;
    }

    LOG("### Info request #5 circle+");
    plugwise::Request::Ptr inc5_req=reqFactory->getInfoRequest(si_resp->circle_mac_addr());
    inc5_req->send(con);
    plugwise::InfoResponse::Ptr inc5_resp=respFactory->receiveInfoResponse();

    LOG("### " << inc5_resp->str());
    if (inc5_resp->req_successful()) {
      LOG("initialization successful.");
    } else {
      std::cout << "failed to initialize stick" << std::endl;
      return -1;
    }


    for (int i=0; i<32 ; i++){
       LOG("### Node Query");
//       usleep (500000);
       plugwise::Request::Ptr nq_req=reqFactory->getNodeQueryRequest(si_resp->circle_mac_addr(), i);
       nq_req->send(con);
       plugwise::NodeQueryResponse::Ptr nq_resp=respFactory->receiveNodeQueryResponse();

       if (nq_resp->circle_mac_addr() != "FFFFFFFFFFFFFFFF"){
         circle_list.push_back(nq_resp->circle_mac_addr());
       }

       LOG("### " << i << ": " << nq_resp->str());
    }


    for (std::list<std::string>::iterator it=circle_list.begin(); it!=circle_list.end(); it++){
      LOG("$$$ " << *it );
    }

//    if (nq_resp->req_successful()) {
//      LOG("initialization successful.");
//    } else {
//      std::cout << "failed to perform nodes query" << std::endl;
//      return -1;
//    }





  } catch (plugwise::GenericException ge) {
    std::cout << "Problem while initializing plugwise stick." << std::endl;
    std::cout << "Aborting: " << ge.what() << std::endl;
  }

  // iterate over all circle IDs given on the command line, query and print.
  for (std::list<std::string>::iterator it=circle_list.begin(); it!=circle_list.end(); it++){
    try {
      LOG("### Sending calibration request ");
      plugwise::Request::Ptr ca_req=reqFactory->getCalibrationRequest(*it);
      ca_req->send(con);
      plugwise::CalibrationResponse::Ptr ca_resp=respFactory->receiveCalibrationResponse();
      LOG("### " << ca_resp->str());
      if (ca_resp->req_successful()) {
        LOG("calibration successful.");
      } else {
        std::cout << "failed to read calibration values from circle " << std::endl;
        return -2;
      }

      LOG("### Sending power information request ");
      plugwise::Request::Ptr pi_req=reqFactory->getPowerInformationRequest(*it);
      pi_req->send(con);
      plugwise::PowerInformationResponse::Ptr pi_resp=
        respFactory->receivePowerInformationResponse();
      LOG("### " << pi_resp->str());
      if (pi_resp->req_successful()) {
        LOG("power info request successful.");
      } else {
        std::cout << "failed to read calibration values from circle " << std::endl;
        return -3;
      }

      LOG("Converting to current power consumption...");
      plugwise::PowerConverter::Ptr pc(new plugwise::PowerConverter(ca_resp));
      double watt=pc->convertToWatt(pi_resp);
      std::cout << *it << ": " << watt << " Watt" << std::endl;
    } catch (plugwise::GenericException ge) {
      std::cout << "Problem while querying circle: " << ge.what() << std::endl;
    }
  }

}
