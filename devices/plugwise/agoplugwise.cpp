#include "agoapp.h"

#include <connection.hpp>
#include <requestfactory.hpp>
#include <responsefactory.hpp>
#include <powerconverter.hpp>
#include <request.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/thread/thread.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>

#include <list>

class circle {
   public: std::string id;
   public: int idx;
   public: bool active;
   public: std::string device_type;
   public: std::string hz;
   public: std::string hw_version;
   public: std::string fw_version;
   public: float gain_a;
   public: float gain_b;
   public: float off_tot;
   public: float off_noise;
   public: uint32_t message_send;
   public: uint32_t message_received;
   public: uint32_t ping_time;
   public: uint32_t lastRequestRTT;
   public: uint32_t averageRequestRTT;
   public: uint32_t error_count;
};

std::list<circle> circles;

using namespace agocontrol;
namespace po = boost::program_options;

static pthread_mutex_t g_criticalSection;

class AgoPlugwise: public AgoApp {
private:
    void setupApp();
    void cleanupApp();
    void plugwiseHandler();
    int  findCircleByMac(std::string device_id);
    void removeCircleByMac(std::string device_id);
    void reinitNetwork();
    qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map command);
    void appCmdLineOptions(boost::program_options::options_description &options);

    plugwise::Connection::Ptr agoCon;
    plugwise::RequestFactory::Ptr agoReqFactory;
    plugwise::ResponseFactory::Ptr agoRespFactory;

    std::string stick_mac;
    std::string circleplus_mac;

    std::string device;
    std::string interval;
public:
    AGOAPP_CONSTRUCTOR(AgoPlugwise);
};

qpid::types::Variant::Map AgoPlugwise::commandHandler(qpid::types::Variant::Map command) {
    qpid::types::Variant::Map returnval;
    std::string internalid = command["internalid"].asString();

   AGO_DEBUG() << "$$$$$$" << internalid << "$$$$$$$" ;
    
   if (internalid == "plugwisecontroller") {
        AGO_TRACE() << "plugwise specific controller command received";
        if (command["command"] == "addcircle") {
           AGO_DEBUG() << "AddCircle " << internalid << command["circle"] ;

           try {

              std::cout << "CommandHandler Before lock claim" << std::endl;
              pthread_mutex_lock( &g_criticalSection );
              std::cout << "CommandHandler passed lock claim" << std::endl;

              plugwise::Request::Ptr ej_req=agoReqFactory->getEnableJoiningRequest("01");
              ej_req->send(agoCon);
              plugwise::EnableJoiningResponse::Ptr ej_resp=agoRespFactory->receiveEnableJoiningResponse();

              pthread_mutex_unlock( &g_criticalSection );
              std::cout << "CommandHandler passed lock release" << std::endl;

              AGO_DEBUG() << "### " << ej_resp->str();
              if (ej_resp->req_successful()) {
                AGO_DEBUG() << "enable joining successful.";
              } else {
                std::cout << "failed to enable joining " << std::endl;
              }

              std::cout << "CommandHandler Before lock claim" << std::endl;
              pthread_mutex_lock( &g_criticalSection );
              std::cout << "CommandHandler passed lock claim" << std::endl;

              plugwise::Request::Ptr jc_req=agoReqFactory->getJoinCircleRequest(command["circle"].asString(), "01");
              jc_req->send(agoCon);
              plugwise::JoinCircleResponse::Ptr jc_resp=agoRespFactory->receiveJoinCircleResponse();

              pthread_mutex_unlock( &g_criticalSection );
              std::cout << "CommandHandler passed lock release" << std::endl;

              AGO_DEBUG() << "### " << jc_resp->str();
              if (jc_resp->req_successful()) {
                AGO_DEBUG() << "join circle successful.";
              } else {
                std::cout << "failed to join circle " << std::endl;
              }

              agoConnection->addDevice(command["circle"].asString().c_str(), "energysensor");
              agoConnection->addDevice((command["circle"].asString()+"/SWITCH").c_str(), "switch");

           } catch (plugwise::GenericException ge) {
               AGO_INFO() << "Problem while joining circle: " << ge.what();
               pthread_mutex_unlock( &g_criticalSection );
               AGO_DEBUG() << "Released lock claim";
           }
           try {
              std::cout << "CommandHandler Before lock claim" << std::endl;
              pthread_mutex_lock( &g_criticalSection );
              std::cout << "CommandHandler passed lock claim" << std::endl;

              plugwise::Request::Ptr dj_req=agoReqFactory->getEnableJoiningRequest("00");
              dj_req->send(agoCon);
              plugwise::EnableJoiningResponse::Ptr dj_resp=agoRespFactory->receiveEnableJoiningResponse();

              pthread_mutex_unlock( &g_criticalSection );
              std::cout << "CommandHandler passed lock release" << std::endl;


              AGO_DEBUG() << "### " << dj_resp->str();
              if (dj_resp->req_successful()) {
                 AGO_DEBUG() << "disable joining successful.";
              } else {
                std::cout << "failed to disable joining " << std::endl;
              }
           } catch  (plugwise::GenericException ge) {
               AGO_INFO() << "Problem while disabling join: " << ge.what();
               pthread_mutex_unlock( &g_criticalSection );
               std::cout << "CommandHandler passed lock release" << std::endl;
           }
        
           returnval["result"] = 0;

        } else  if (command["command"] == "removecircle") {
           
           AGO_DEBUG() << "RemoveCircle " << internalid << command["circle"] ;
           if (int index = findCircleByMac(command["circle"].asString()) > 0){
              try {

                 std::cout << "CommandHandler Before lock claim" << std::endl;
                 pthread_mutex_lock( &g_criticalSection );
                 std::cout << "CommandHandler passed lock claim" << std::endl;

                 plugwise::Request::Ptr rc_req=agoReqFactory->getRemoveCircleRequest(circleplus_mac, command["circle"].asString());
                 rc_req->send(agoCon);
                 plugwise::RemoveCircleResponse::Ptr rc_resp=agoRespFactory->receiveRemoveCircleResponse();

                 pthread_mutex_unlock( &g_criticalSection );
                 std::cout << "CommandHandler passed lock release" << std::endl;

                 AGO_DEBUG() << "### " << rc_resp->str();
                 if (rc_resp->req_successful()) {
                   AGO_DEBUG() << "remove circle successful.";
                 } else {
                   std::cout << "failed to remove circle " << std::endl;
                 }

                 agoConnection->removeDevice(command["circle"].asString().c_str());
                 agoConnection->removeDevice((command["circle"].asString()+"/SWITCH").c_str());

//
// remove circle from circle list. Secured by mutex as handler might be accessing list
//
                 removeCircleByMac(command["circle"].asString().c_str());

// end of critical zone

              } catch (plugwise::GenericException ge) {
                 AGO_INFO() << "Problem while removing circle: " << ge.what();
                 pthread_mutex_unlock( &g_criticalSection );
                 AGO_DEBUG() << "Released lock claim";
              }
              returnval["result"] = 0;
           } else { // else if index > 0
              returnval["result"] = -1;
           } // end if index > 0
        } else if (command["command"] == "rebuild") {

        } else if (command["command"] == "getport") {
           
          returnval["port"] = device;

        } else if (command["command"] == "setport") {

        } else if (command["command"] == "resetallcounters") {

           pthread_mutex_lock( &g_criticalSection );

           for (std::list<circle>::iterator it=circles.begin(); it!=circles.end(); it++){
              it->message_send = 0;
              it->message_received = 0;
              it->averageRequestRTT = 0;
              it->lastRequestRTT = 0;
              it->error_count = 0;
           } // end for

           pthread_mutex_unlock( &g_criticalSection );

           returnval["result"] = 1;

        } else if (command["command"] == "resetcounters") {

           pthread_mutex_lock( &g_criticalSection );

           for (std::list<circle>::iterator it=circles.begin(); it!=circles.end(); it++){
              if (command["device"].asString() == it->id){
                 it->message_send = 0;
                 it->message_received = 0;
                 it->averageRequestRTT = 0;
                 it->lastRequestRTT = 0;
                 it->error_count = 0;
              } //end if
           } // end for

           pthread_mutex_unlock( &g_criticalSection );

           returnval["result"] = 1;

        } else if (command["command"] == "getdevices") {

           qpid::types::Variant::List devicesList;
           for (std::list<circle>::iterator it=circles.begin(); it!=circles.end(); it++){
              qpid::types::Variant::Map item;
              if (it->active) {
                 item["idx"]        = it->idx;
                 item["internalid"] = it->id;
                 item["type"]       = it->device_type;
                 item["hwversion"]  = it->hw_version;
                 item["fwversion"]  = it->fw_version;
                 item["hz"]         = it->hz;
                 devicesList.push_back(item);
              }
           } // end for

           returnval["devices"] = devicesList;
           AGO_DEBUG() << returnval ;


        } else if (command["command"] == "getstats") {
           qpid::types::Variant::Map circlelist;
           for (std::list<circle>::iterator it=circles.begin(); it!=circles.end(); it++){
              qpid::types::Variant::Map circle;
              if (it->active) {
                  circle["mac"]= it->id;
                  circle["send"] = it->message_send;
                  circle["received"] = it->message_received;
                  circle["pingtime"] = it->ping_time;
                  circle["lastRequestRTT"] = it->lastRequestRTT;
                  circle["averageRequestRTT"] = it->averageRequestRTT;
                  circle["errorcount"] = it->error_count;
                  circlelist[boost::lexical_cast<std::string>(it->idx)] = circle;
              } // end if
           } // end for
           returnval["result"] = circlelist;
        }
   } else if (command["command"] == "on") {
           
       
           AGO_DEBUG() << "Switch " << internalid << " ON";

           std::cout << "CommandHandler Before lock claim" << std::endl;
           pthread_mutex_lock( &g_criticalSection );
           std::cout << "CommandHandler Passed lock claim" << std::endl;

           try { 
              plugwise::Request::Ptr sw_req=agoReqFactory->getSwitchRequest(internalid.substr(0,16), 1);
              sw_req->send(agoCon);
              plugwise::SwitchResponse::Ptr sw_resp=agoRespFactory->receiveSwitchResponse();

              AGO_DEBUG() << "### " << sw_resp->str();
              if (sw_resp->req_successful()) {
                 AGO_DEBUG() << "switch on successful.";
                 agoConnection->emitEvent(internalid.c_str(), "event.device.statechanged", 255, "");
              } else {
                std::cout << "failed to switch circle " << std::endl;
              }
              returnval["result"] = 0;

           } catch (plugwise::GenericException ge) {
              AGO_INFO() << "Problem while querying circle: " << ge.what();
           }

           pthread_mutex_unlock( &g_criticalSection );
           std::cout << "CommandHandler Released lock claim" << std::endl;

        } else if (command["command"] == "off") {
           AGO_DEBUG() << "Switch " << internalid << " OFF";

           std::cout << "CommandHandler Before lock claim" << std::endl;
           pthread_mutex_lock( &g_criticalSection );
           std::cout << "CommandHandler Passed lock claim" << std::endl;


           try {
              plugwise::Request::Ptr sw_req=agoReqFactory->getSwitchRequest(internalid.substr(0,16), 0);
              sw_req->send(agoCon);
              plugwise::SwitchResponse::Ptr sw_resp=agoRespFactory->receiveSwitchResponse();

              AGO_DEBUG() << "### " << sw_resp->str();
              if (sw_resp->req_successful()) {
                AGO_DEBUG() << "switch off successful.";
                agoConnection->emitEvent(internalid.c_str(), "event.device.statechanged", 0, "");
              } else {
                std::cout << "failed to switch circle " << std::endl;
              }

              returnval["result"] = 0;
           } catch (plugwise::GenericException ge) {
              AGO_INFO() << "Problem while querying circle: " << ge.what();
           }

           pthread_mutex_unlock( &g_criticalSection );
           std::cout << "CommandHandler Released lock claim" << std::endl;
        }
        return returnval;
}

void AgoPlugwise::appCmdLineOptions(boost::program_options::options_description &options) {
    options.add_options()
        ("test,T", po::bool_switch(),
         "A custom option which can be set or not set")
        ("test-string", po::value<std::string>(),
         "This can be a string")
        ;
}

//
// AgoPlugwise::findCircleByMac
// find circle by MAC address and return index in circle list of circle+
//

int  AgoPlugwise::findCircleByMac(std::string device_id) {
   int found = -1;

   pthread_mutex_lock( &g_criticalSection );
   
   for (std::list<circle>::iterator it=circles.begin(); it!=circles.end(); it++){
      if (it->id == device_id) {
         found = it->idx;
      }
   }

   pthread_mutex_unlock( &g_criticalSection );
   return found;
}

//
// AgoPlugwise::removeCircleByMac
// remove circle from our circle list identified by MAC
//

void AgoPlugwise::removeCircleByMac(std::string device_id) {
    
   pthread_mutex_lock( &g_criticalSection );

   for (std::list<circle>::iterator it=circles.begin(); it!=circles.end(); ){
      if (it->id == device_id) {
         it =circles.erase(it);
      } else {
         ++it;
      }
   }
   pthread_mutex_unlock( &g_criticalSection );

}

//
// Reinit network after missing rewsponses
// Thread: PlugwiseHandler
//


void AgoPlugwise::reinitNetwork() {

  std::cout << "Reinit network Before lock claim" << std::endl;
  pthread_mutex_lock( &g_criticalSection );
  std::cout << "Reinit network passed lock claim" << std::endl;

  AGO_INFO() << "Reinit serial connection to Plugwise stick";
  agoCon->reconnect();
  
  try {
      AGO_DEBUG() << "### Initializing stick";
      plugwise::Request::Ptr si_req=agoReqFactory->getStickInitRequest();
      si_req->send(agoCon);
      plugwise::StickInitResponse::Ptr si_resp=agoRespFactory->receiveStickInitResponse();

      AGO_DEBUG() << si_resp->str();
      if (si_resp->req_successful()) {
        AGO_DEBUG() << "Stick initialization successful.";
      } else {
        AGO_DEBUG() << "Stick initialization failed";
        return;
      }
   } catch  (plugwise::GenericException ge) {
      AGO_INFO() << "Problem while reinitializing network " << ge.what();
   }


  try {
      plugwise::Request::Ptr dj_req=agoReqFactory->getEnableJoiningRequest("00");
      dj_req->send(agoCon);
      plugwise::EnableJoiningResponse::Ptr dj_resp=agoRespFactory->receiveEnableJoiningResponse();

      AGO_DEBUG() << "### " << dj_resp->str();
      if (dj_resp->req_successful()) {
         AGO_DEBUG() << "disable joining successful.";
      } else {
         std::cout << "failed to disable joining " << std::endl;
      }
   } catch  (plugwise::GenericException ge) {
      AGO_INFO() << "Problem while reinitializing network " << ge.what();
   }

   try {
      AGO_DEBUG() << "### Info request stick";
      plugwise::Request::Ptr in_req=agoReqFactory->getInfoRequest(stick_mac);
      in_req->send(agoCon);
      plugwise::InfoResponse::Ptr in_resp=agoRespFactory->receiveInfoResponse();

      AGO_DEBUG() << "### " << in_resp->str();
      if (in_resp->req_successful()) {
        AGO_DEBUG() << "initialization successful.";
      } else {
        AGO_DEBUG() << "Stick info request failed";
        return;
      }
   } catch  (plugwise::GenericException ge) {
      AGO_INFO() << "Problem while reinitializing network " << ge.what();
   }

   try {
      AGO_DEBUG() << "### Info request circle+";
      plugwise::Request::Ptr inc_req=agoReqFactory->getInfoRequest(circleplus_mac);
      inc_req->send(agoCon);
      plugwise::InfoResponse::Ptr inc_resp=agoRespFactory->receiveInfoResponse();

      AGO_DEBUG() << "### " << inc_resp->str();
      if (inc_resp->req_successful()) {
        AGO_DEBUG() << "Circle+ info request successful.";
      } else {
        AGO_DEBUG() << "Circle+ info request failed";
        return;
      }
   } catch  (plugwise::GenericException ge) {
      AGO_INFO() << "Problem while reinitializing network " << ge.what();
   }

   try{
      AGO_DEBUG() << "### Date Time request circle+ ";
      plugwise::Request::Ptr dt_req=agoReqFactory->getDateTimeInfoRequest(circleplus_mac);
      dt_req->send(agoCon);
      plugwise::DateTimeInfoResponse::Ptr dt_resp=agoRespFactory->receiveDateTimeInfoResponse();

      AGO_DEBUG() << "### " << dt_resp->str();
      if (dt_resp->req_successful()) {
        AGO_DEBUG() << "Circle+ date time info request successful.";
      } else {
        AGO_DEBUG() << "Circle+ date timeinfo request failed";
        return;
      }
   } catch  (plugwise::GenericException ge) {
      AGO_INFO() << "Problem while reinitializing network " << ge.what();
   }


   try{
      AGO_DEBUG() << "### Clock info";
      plugwise::Request::Ptr ci_req=agoReqFactory->getClockInfoRequest(circleplus_mac);
      ci_req->send(agoCon);
      plugwise::ClockInfoResponse::Ptr ci_resp=agoRespFactory->receiveClockInfoResponse();
      AGO_DEBUG() << "### " << ci_resp->str();
      if (ci_resp->req_successful()) {
        AGO_DEBUG() << "Circle+ clock info request successful.";
      } else {
        AGO_DEBUG() << "Circle+ clock info request failed";
        return;
      }

   } catch  (plugwise::GenericException ge) {
      AGO_INFO() << "Problem while reinitializing network " << ge.what();
   }

   pthread_mutex_unlock( &g_criticalSection );
   std::cout << "Reinit network passed lock release" << std::endl;

}

void AgoPlugwise::plugwiseHandler() {
    AGO_INFO() << "Starting Plugwise poll thread" ;

  // iterate over all circle IDs given on the command line, query and print.
  while (1) {
    for (std::list<circle>::iterator it=circles.begin(); it!=circles.end(); it++){
       if (it->active){
          try {

            AGO_DEBUG() << "### Sending power information request ";

            AGO_DEBUG() << "Before lock claim";
            pthread_mutex_lock( &g_criticalSection );
            AGO_DEBUG() << "Passed lock claim";

            plugwise::Request::Ptr pi_req=agoReqFactory->getPowerInformationRequest(it->id);
            boost::posix_time::ptime mst1 = boost::posix_time::microsec_clock::local_time();

            pi_req->send(agoCon);
            it->message_send++;

            plugwise::PowerInformationResponse::Ptr pi_resp=agoRespFactory->receivePowerInformationResponse();
            pthread_mutex_unlock( &g_criticalSection );
            AGO_DEBUG() << "Released lock claim";


            AGO_DEBUG() << "### " << pi_resp->str();
            if (pi_resp->req_successful()) {
               boost::posix_time::ptime mst2 = boost::posix_time::microsec_clock::local_time();
               boost::posix_time::time_duration msdiff = mst2 - mst1;
               it->lastRequestRTT = msdiff.total_milliseconds();
               if (it->averageRequestRTT == 0) {
                  it->averageRequestRTT = it->lastRequestRTT;
               } else {
                  it->averageRequestRTT = ( it->averageRequestRTT + it->lastRequestRTT ) >> 1;
               }
               it->message_received++;
              AGO_DEBUG() << "power info request successful.";
            } else {
              AGO_DEBUG() << "Circle " << it->id << ": failed to read power information from circle ";
              it->error_count++;
            }

            AGO_DEBUG() << "Converting to current power consumption...";
            plugwise::PowerConverter::Ptr pc(new plugwise::PowerConverter(it->gain_a, it->gain_b, it->off_tot, it->off_noise));
            double watt=pc->convertToWatt2(pi_resp);
            AGO_INFO() << "Circle " <<  it->id << ": power usage " << watt << " Watt";
  
            agoConnection->emitEvent(it->id.c_str(), "event.environment.energychanged", (float) watt, "Watt");
 
          } catch (plugwise::CommunicationException ge) {
              AGO_INFO() << "Circle " << it->id << ": communication error while querying circle: " << ge.what();
              it->error_count++;
              pthread_mutex_unlock( &g_criticalSection );
              AGO_DEBUG() << "Released lock claim";
              AgoPlugwise::reinitNetwork();
          } catch (plugwise::GenericException ge) {
              AGO_INFO() << "Circle " << it->id << ": problem while querying circle: " << ge.what();
              it->error_count++;
              pthread_mutex_unlock( &g_criticalSection );
              AGO_DEBUG() << "Released lock claim";
          }
       }
    }
    AGO_DEBUG() << "Sleeping during " << AgoPlugwise::interval.c_str() << " seconds";
    boost::this_thread::sleep( boost::posix_time::seconds(atoi(AgoPlugwise::interval.c_str())) );
  }
  AGO_INFO() << "Plugwise poll thread ended";
}

void AgoPlugwise::setupApp() {
    AGO_INFO() << "Plugwise starting up";

    device=getConfigOption("device", "/dev/plugwise");
    AGO_INFO() << "Config device is '" << device << "'";

    AgoPlugwise::interval=getConfigOption("interval", "60");
    AGO_INFO() << "Config interval is '" << interval << "'";

    // do some device specific setup
    std::string test_param;

    if (cli_vars.count("test-string"))
        test_param = cli_vars["test-string"].as<std::string>();

    AGO_INFO() << "Param Test string is " << test_param;

    /* Read a configuration value "test_option" from our example.conf file
     * This will look in the configuration file conf.d/<apyp name>.conf
     * under the [<app name>] section.
     * In this example, this means example.conf and [example] section
     */

    pthread_mutexattr_t mutexattr;

    pthread_mutexattr_init ( &mutexattr );
    pthread_mutexattr_settype( &mutexattr, PTHREAD_MUTEX_RECURSIVE );

    pthread_mutex_init( &g_criticalSection, NULL );
    pthread_mutexattr_destroy( &mutexattr );


    plugwise::Connection::Ptr con(new plugwise::Connection(device));
    agoCon = con;

    plugwise::RequestFactory::Ptr reqFactory(new plugwise::RequestFactory());
    agoReqFactory = reqFactory;

    plugwise::ResponseFactory::Ptr respFactory(new plugwise::ResponseFactory(con));
    agoRespFactory = respFactory;

    try {

      AGO_DEBUG() << "### Initializing stick";
      plugwise::Request::Ptr si_req=reqFactory->getStickInitRequest();
      si_req->send(con);
      plugwise::StickInitResponse::Ptr si_resp=respFactory->receiveStickInitResponse();

      AGO_DEBUG() << si_resp->str();
      if (si_resp->req_successful()) {
        AGO_DEBUG() << "Stick initialization successful.";
      } else {
        AGO_DEBUG() << "Stick initialization failed";
        return;
      }

      stick_mac = si_resp->stick_mac_addr();
      circleplus_mac = si_resp->circle_mac_addr();

      AGO_DEBUG() << "### Info request stick";
      plugwise::Request::Ptr in0_req=reqFactory->getInfoRequest(stick_mac);
      in0_req->send(con);
      plugwise::InfoResponse::Ptr in0_resp=respFactory->receiveInfoResponse();

      AGO_DEBUG() << "### " << in0_resp->str();
      if (in0_resp->req_successful()) {
        AGO_DEBUG() << "Stick info request successful.";
      } else {
        AGO_DEBUG() << "Stick info request failed";
        return;
      }

      AGO_DEBUG() << "### Info request stick #2";
      plugwise::Request::Ptr in2_req=reqFactory->getInfoRequest(stick_mac);
      in2_req->send(con);
      plugwise::InfoResponse::Ptr in2_resp=respFactory->receiveInfoResponse();

      AGO_DEBUG() << "### " << in2_resp->str();
      if (in2_resp->req_successful()) {
        AGO_DEBUG() << "initialization successful.";
      } else {
        AGO_DEBUG() << "Stick info request #2 failed";
        return;
      }

      AGO_DEBUG() << "### Info request circle+ <0023>";
      plugwise::Request::Ptr inc_req=reqFactory->getInfoRequest(circleplus_mac);
      inc_req->send(con);
      plugwise::InfoResponse::Ptr inc_resp=respFactory->receiveInfoResponse();

      AGO_DEBUG() << "### " << inc_resp->str();
      if (inc_resp->req_successful()) {
        AGO_DEBUG() << "Circle+ info request successful.";
      } else {
        AGO_DEBUG() << "Circle+ info request failed";
        return;
      }


      AGO_DEBUG() << "### Sending calibration request to circle+ <0026>";
      plugwise::Request::Ptr ca_req=reqFactory->getCalibrationRequest(circleplus_mac);
      ca_req->send(con);
      plugwise::CalibrationResponse::Ptr ca_resp=respFactory->receiveCalibrationResponse();
      AGO_DEBUG() << "### " << ca_resp->str();
      if (ca_resp->req_successful()) {
        AGO_DEBUG() << "calibration successful.";
      } else {
        std::cout << "failed to read calibration values from circle " << std::endl;
        return;
      }


      AGO_DEBUG() << "### Feature request <005F>";
      plugwise::Request::Ptr fs_req=reqFactory->getFeatureSetRequest(circleplus_mac);
      fs_req->send(con);
      plugwise::FeatureSetResponse::Ptr fs_resp=respFactory->receiveFeatureSetResponse();

      AGO_DEBUG() << "### " << fs_resp->str();
      if (fs_resp->req_successful()) {
        AGO_DEBUG() << "Feature set successful.";
      } else {
        std::cout << "failed to reset stick" << std::endl;
        return ;
      }


      AGO_DEBUG() << "### Info request #2 circle+ <0023>";
      plugwise::Request::Ptr inc1_req=reqFactory->getInfoRequest(circleplus_mac);
      inc1_req->send(con);
      plugwise::InfoResponse::Ptr inc1_resp=respFactory->receiveInfoResponse();

      AGO_DEBUG() << "### " << inc1_resp->str();
      if (inc1_resp->req_successful()) {
        AGO_DEBUG() << "Circle info request #2 successful.";
      } else {
        AGO_DEBUG() << "Circle info request #2 failed";
        return;
      }

      AGO_DEBUG() << "### Info request #4 circle+ <0023>";
      plugwise::Request::Ptr inc4_req=reqFactory->getInfoRequest(circleplus_mac);
      inc4_req->send(con);
      plugwise::InfoResponse::Ptr inc4_resp=respFactory->receiveInfoResponse();

      AGO_DEBUG() << "### " << inc4_resp->str();
      if (inc4_resp->req_successful()) {
        AGO_DEBUG() << "initialization successful.";
      } else {
        std::cout << "failed to initialize stick" << std::endl;
        return;
      }


      for (int i=0; i<64 ; i++){
         circle new_circle;

         AGO_DEBUG() << "### Node Query <0018>";
         plugwise::Request::Ptr nq_req=reqFactory->getNodeQueryRequest(circleplus_mac, i);
         nq_req->send(con);
         plugwise::NodeQueryResponse::Ptr nq_resp=respFactory->receiveNodeQueryResponse();
         AGO_DEBUG() << "### " << nq_resp->str();
         if (nq_resp->req_successful()) {
           AGO_DEBUG() << "Node query successful.";
         } else {
           std::cout << "failed to query node" << std::endl;
           continue;
         }
         
         new_circle.id = nq_resp->circle_mac_addr();
         new_circle.idx = i;
         new_circle.active = false;
         new_circle.message_send = 0;
         new_circle.message_received = 0;
         new_circle.ping_time = 0;
         new_circle.lastRequestRTT = 0;
         new_circle.averageRequestRTT = 0;
         new_circle.error_count = 0;

         circles.push_back(new_circle);

         AGO_INFO() << "Found circle " <<  nq_resp->circle_mac_addr();
      }

      AGO_DEBUG() << "###";
      AGO_DEBUG() << "### Completed circle discovery phase";
      AGO_DEBUG() << "###";


      AGO_DEBUG() << "### Info request stick #4 <0023>";
      plugwise::Request::Ptr in4_req=reqFactory->getInfoRequest(stick_mac);
      in4_req->send(con);
      plugwise::InfoResponse::Ptr in4_resp=respFactory->receiveInfoResponse();

      AGO_DEBUG() << "### " << in4_resp->str();
      if (in4_resp->req_successful()) {
        AGO_DEBUG() << "initialization successful.";
      } else {
        AGO_DEBUG() << "Stick info request #4 failed";
        return;
      }

      AGO_DEBUG() << "### Info request circle+ #4 <0023>";
      plugwise::Request::Ptr inc5_req=reqFactory->getInfoRequest(circleplus_mac);
      inc5_req->send(con);
      plugwise::InfoResponse::Ptr inc5_resp=respFactory->receiveInfoResponse();

      AGO_DEBUG() << "### " << inc5_resp->str();
      if (inc5_resp->req_successful()) {
        AGO_DEBUG() << "Circle+ info request successful.";
      } else {
        AGO_DEBUG() << "Circle+ info request failed";
        return;
      }

      AGO_DEBUG() << "### Set broadcast request circle+ <004A>";
      plugwise::Request::Ptr sbc_req=reqFactory->getSetBroadcastIntervalRequest(circleplus_mac);
      sbc_req->send(con);
      plugwise::SetBroadcastIntervalResponse::Ptr sbc_resp=respFactory->receiveSetBroadcastIntervalResponse();

      AGO_DEBUG() << "### " << sbc_resp->str();
      if (sbc_resp->req_successful()) {
        AGO_DEBUG() << "Circle+  set broadcast interval request successful.";
      } else {
        AGO_DEBUG() << "Circle+ set broadcast intervalrequest failed";
        return;
      }




//      AGO_DEBUG() << "### Date Time request circle+ <0028>";
//      plugwise::Request::Ptr dt_req=reqFactory->getDateTimeInfoRequest(circleplus_mac);
//      dt_req->send(con);
//      plugwise::DateTimeInfoResponse::Ptr dt_resp=respFactory->receiveDateTimeInfoResponse();

//      AGO_DEBUG() << "### " << dt_resp->str();
//      if (dt_resp->req_successful()) {
//        AGO_DEBUG() << "Circle+ date time info request successful.";
//      } else {
//        AGO_DEBUG() << "Circle+ date timeinfo request failed";
//        return;
//      }


      AGO_DEBUG() << "### Reset request circle+ <0008>";
      plugwise::Request::Ptr rst2_req=agoReqFactory->getEnableJoiningRequest("01");
      rst2_req->send(agoCon);
      plugwise::EnableJoiningResponse::Ptr rst2_resp=agoRespFactory->receiveEnableJoiningResponse();
      AGO_DEBUG() << "### " << rst2_resp->str();
      if (rst2_resp->req_successful()) {
        AGO_DEBUG() << "reset circle+successful.";
      } else {
        std::cout << "failed to reset circle+" << std::endl;
        return;
      }


      AGO_DEBUG() << "###";
      AGO_DEBUG() << "### Getting data from circles in network";
      AGO_DEBUG() << "###";


      for (int i=0 ; i< 100 ; i++){
         boost::this_thread::sleep(boost::posix_time::milliseconds(250));

         for (std::list<circle>::iterator it=circles.begin(); it!=circles.end(); it++){

           if ((it->id != "FFFFFFFFFFFFFFFF") && (it->active == false)){
              circle new_circle; // to store all circle data from the circle in this loop

              try {
                 AGO_DEBUG() << "### Node info <0023>";
                 plugwise::Request::Ptr ni_req=reqFactory->getInfoRequest(it->id);
                 ni_req->send(con);
                 plugwise::InfoResponse::Ptr ni_resp=respFactory->receiveInfoResponse();
                 AGO_DEBUG() << "### " << ni_resp->str();
                 if (ni_resp->req_successful()) {
                    AGO_DEBUG() << "Circle node info request successful.";
                    it->hw_version = ni_resp->get_hw_version();
                    it->fw_version = ni_resp->get_fw_version();
                    it->hz =         ni_resp->get_hz();
                    it->device_type =ni_resp->get_device_type(); 
                 } else {
                    AGO_DEBUG() << "Circle node info  request failed";
                    continue;
                 }
              } catch (plugwise::CommunicationException ge) {
                 AGO_INFO() << "Circle " << it->id << ": communication error while querying circle: " << ge.what();
                 it->error_count++;
                 AGO_DEBUG() << "Released lock claim";
                 AgoPlugwise::reinitNetwork();
              } catch (plugwise::GenericException ge) {
                 it->error_count++;
                 AGO_INFO() << "Circle " << it->id << ": problem while requesting node info.";
                 AGO_INFO() << "Circle " << it->id << ": skipping circle: " << ge.what();
                 continue;
              }

              try {
                 AGO_DEBUG() << "### Clock info <003E>";
                 plugwise::Request::Ptr ci_req=reqFactory->getClockInfoRequest(it->id);
                 ci_req->send(con);
                 plugwise::ClockInfoResponse::Ptr ci_resp=respFactory->receiveClockInfoResponse();
                 AGO_DEBUG() << "### " << ci_resp->str();
                 if (ci_resp->req_successful()) {
                    AGO_DEBUG() << "Circle clock info request successful.";
                 } else {
                    it->error_count++;
                    AGO_DEBUG() << "Circle clock info request failed";
                    continue;
                 }
              } catch (plugwise::GenericException ge) {
                 AGO_INFO() << "Circle " << it->id << ": problem while requesting clock info.";
                 AGO_INFO() << "Circle " << it->id << ": skipping node: " << ge.what();
                 it->error_count++;
                 continue;
              }

              try {
                 AGO_DEBUG() << "### Sending calibration request <0026>";

                 plugwise::Request::Ptr ca_req=agoReqFactory->getCalibrationRequest(it->id);
                 ca_req->send(agoCon);
                 plugwise::CalibrationResponse::Ptr ca_resp=agoRespFactory->receiveCalibrationResponse();

                 AGO_DEBUG() << "### " << ca_resp->str();
                 if (ca_resp->req_successful()) {
                    AGO_DEBUG() << "calibration successful.";
                    it->gain_a = ca_resp->get_gain_a();
                    it->gain_b = ca_resp->get_gain_b();
                    it->off_tot = ca_resp->get_off_tot();
                    it->off_noise = ca_resp->get_off_noise();
                    it->active = true;
                 } else {
                    it->error_count++;
                    std::cout << "failed to read calibration values from circle " << std::endl;
                    continue;
                 }
              } catch (plugwise::GenericException ge) {
                 AGO_INFO() << "Circle " << it->id << ": problem while requesting calibration info.";
                 AGO_INFO() << "Circle " << it->id << ": " << ge.what();
                 it->error_count++;
                 continue;
              } // end of try

              try {
                 AGO_DEBUG() << "### Ping request ";

                 plugwise::Request::Ptr pi_req=agoReqFactory->getPingRequest(it->id);
                 pi_req->send(agoCon);
                 plugwise::PingResponse::Ptr pi_resp=agoRespFactory->receivePingResponse();

                 AGO_DEBUG() << "### " << pi_resp->str();
                 if (pi_resp->req_successful()) {
                    AGO_DEBUG() << "Ping successful.";
                    it->ping_time = pi_resp->ping_time();
                 } else {
                    it->error_count++;
                    std::cout << "failed to ping circle " << std::endl;
                    continue;
                 }
              } catch (plugwise::GenericException ge) {
                 AGO_INFO() << "Circle " << it->id << ": problem while requesting ping.";
                 AGO_INFO() << "Circle " << it->id << ": skipping circle " << ge.what();
                 it->error_count++;
                 continue;
              } // end of try

            } // end of if
         } // end of for

//         AGO_DEBUG() << "### Info request stick #4 <0023>";
//         plugwise::Request::Ptr in5_req=reqFactory->getInfoRequest(stick_mac);
//         in5_req->send(con);
//         plugwise::InfoResponse::Ptr in5_resp=respFactory->receiveInfoResponse();

//         AGO_DEBUG() << "### " << in5_resp->str();
//         if (in5_resp->req_successful()) {
//            AGO_DEBUG() << "initialization successful.";
//         } else {
//            AGO_DEBUG() << "Stick info request #4 failed";
//            return;
//         }


//         AGO_DEBUG() << "### Info request circle+ #4 <0023>";
//         plugwise::Request::Ptr inc6_req=reqFactory->getInfoRequest(circleplus_mac);
//         inc6_req->send(con);
//         plugwise::InfoResponse::Ptr inc6_resp=respFactory->receiveInfoResponse();

//         AGO_DEBUG() << "### " << inc5_resp->str();
//         if (inc6_resp->req_successful()) {
//            AGO_DEBUG() << "Circle+ info request successful.";
//         } else {
//            AGO_DEBUG() << "Circle+ info request failed";
//            return;
//         }
      } // end of outer for


      AGO_DEBUG() << "### Clock set request circle+ <0016>";
      plugwise::Request::Ptr cs_req=reqFactory->getClockSetRequest(circleplus_mac);
      cs_req->send(con);
      plugwise::ClockSetResponse::Ptr cs_resp=respFactory->receiveClockSetResponse();

      AGO_DEBUG() << "### " << cs_resp->str();
      if (cs_resp->req_successful()) {
        AGO_DEBUG() << "Circle+ clock set request successful.";
      } else {
        AGO_DEBUG() << "Circle+ clock set request failed";
        return;
      }


      AGO_DEBUG() << "### Set date time request circle+ <0028>";
      plugwise::Request::Ptr sdt_req=reqFactory->getSetDateTimeRequest(circleplus_mac);
      sdt_req->send(con);
      plugwise::SetDateTimeResponse::Ptr sdt_resp=respFactory->receiveSetDateTimeResponse();

      AGO_DEBUG() << "### " << sdt_resp->str();
      if (sdt_resp->req_successful()) {
        AGO_DEBUG() << "Circle+  set date time request successful.";
      } else {
        AGO_DEBUG() << "Circle+ clock date time set request failed";
        return;
      }


      AGO_INFO() << "Plugwise configuration complete";
      AGO_INFO() << "Stick   MAC: " << stick_mac;
      AGO_INFO() << "Circle+ MAC: " << circleplus_mac;
      for (std::list<circle>::iterator it=circles.begin(); it!=circles.end(); it++){
         if (it->active){
            AGO_INFO() << "Circle  MAC: " << it->id << ", index " << it->idx << ", Active: " << it->active << ", HW version: " << it->hw_version << ", FW version: " << it->fw_version << ", gain_a: " << it->gain_a << ", gain_b: " << it->gain_b << ", offset noise: " << it->off_noise << ", offset tot: " << it->off_tot << ", device type: " << it->device_type << ", Hz: " << it->hz << ", Ping time: " << it->ping_time ;
            agoConnection->addDevice(it->id.c_str(), "energysensor");
            agoConnection->addDevice((it->id + "/SWITCH").c_str(), "switch");
         } else if (it->id != "FFFFFFFFFFFFFFFF"){ // configured in circle+ however not found
            AGO_INFO() << "Circle  MAC: " << it->id << ", index " << it->idx << ", Active: " << it->active ;
         }
      }

    } catch (plugwise::GenericException ge) {
      AGO_INFO() << "Problem while initializing plugwise stick.";
      AGO_INFO() << "Aborting: " << ge.what();
    }


    if(false) {
        // Other startup failure
        AGO_FATAL() << "setup failed";
        pthread_mutex_destroy( &g_criticalSection );
        throw StartupError();
    }

    // add our command handler
    agoConnection->addDevice("plugwisecontroller", "plugwisecontroller");
    addCommandHandler();

    boost::thread t(boost::bind(&AgoPlugwise::plugwiseHandler, this));
    t.detach();
}

void AgoPlugwise::cleanupApp() {
    pthread_mutex_destroy( &g_criticalSection );
}


/* Finally create an application main entry point, this will create
 * the int main(...) {} part */
AGOAPP_ENTRY_POINT(AgoPlugwise);

