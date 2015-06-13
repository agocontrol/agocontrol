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



#include "connection.hpp"
#include <fcntl.h>
#include <termios.h>
#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include <iterator>
#include <boost/crc.hpp>
#include <boost/thread/thread.hpp>
#include <iomanip>
#include <sys/ioctl.h>

namespace plugwise {


  enum response_state_t {
    start, 
    prefix_1, prefix_2, prefix_3,
    message
  };

  typedef boost::crc_optimal<16, 0x1021, 0, 0, false, false> crc_plugwise_type;

  fd_set rfds;
  struct timeval tv;
  int retval;




  Connection::Connection(const std::string& device) :
    _device(device)
  {
    // initialize device
    struct termios tio;
    memset(&tio,0,sizeof(tio));
    tio.c_iflag=0;
    tio.c_oflag=0;
    tio.c_cflag=CS8|CREAD|CLOCAL;			// 8n1, see termios.h for more information
    tio.c_lflag=0;
    tio.c_cc[VMIN]=1;
    tio.c_cc[VTIME]=5;

    _tty_fd=open(_device.c_str(), O_RDWR);	//  | O_NONBLOCK);
    cfsetospeed(&tio,B115200);			// 115200 baud
    cfsetispeed(&tio,B115200);			// 115200 baud
    tcsetattr(_tty_fd,TCSANOW,&tio);
    
    sleep(2); //required to make flush work, for some reason
    tcflush(_tty_fd, TCIOFLUSH);

    int controlbits = TIOCM_RTS;		// toggle RTS bit

    ioctl(_tty_fd, TIOCMBIC, &controlbits);
    boost::this_thread::sleep( boost::posix_time::milliseconds(100));
    ioctl(_tty_fd, TIOCMBIS, &controlbits);
    boost::this_thread::sleep( boost::posix_time::milliseconds(100));
    ioctl(_tty_fd, TIOCMBIC, &controlbits);

    FD_ZERO(&rfds);
    FD_SET(_tty_fd, &rfds);

  }

  Connection::~Connection() {
    close(_tty_fd);
  }


  void Connection::reconnect() {

    close(_tty_fd);
   
    // initialize device
    struct termios tio;
    memset(&tio,0,sizeof(tio));
    tio.c_iflag=0;
    tio.c_oflag=0;
    tio.c_cflag=CS8|CREAD|CLOCAL;                       // 8n1, see termios.h for more information
    tio.c_lflag=0;
    tio.c_cc[VMIN]=1;
    tio.c_cc[VTIME]=5;

    _tty_fd=open(_device.c_str(), O_RDWR);                      //  | O_NONBLOCK);
    cfsetospeed(&tio,B115200);                  // 115200 baud
    cfsetispeed(&tio,B115200);                  // 115200 baud
    tcsetattr(_tty_fd,TCSANOW,&tio);

    sleep(2); //required to make flush work, for some reason
    tcflush(_tty_fd,TCIOFLUSH);

    int controlbits = TIOCM_RTS;                // toggle RTS bit

    ioctl(_tty_fd, TIOCMBIC, &controlbits);
    boost::this_thread::sleep( boost::posix_time::milliseconds(100));
    ioctl(_tty_fd, TIOCMBIS, &controlbits);
    boost::this_thread::sleep( boost::posix_time::milliseconds(100));
    ioctl(_tty_fd, TIOCMBIC, &controlbits);

    FD_ZERO(&rfds);
    FD_SET(_tty_fd, &rfds);
 
  }

  void Connection::send_payload(const std::string& payload) {
    //unsigned char initcode[]="\x05\x05\x03\x03\x30\x30\x30\x41\x42\x34\x33\x43\x0d\x0a";
    unsigned char prefix[]="\x05\x05\x03\x03";
    //unsigned char crc16[] = "\x42\x34\x33\x43";
    crc_plugwise_type  crc16_calc;
    crc16_calc.process_bytes( payload.c_str(), payload.length());
    std::ostringstream oss;
    oss.setf(std::ios::right, std::ios::adjustfield);
    oss << std::setfill('0') << std::setw(4) << std::hex << std::uppercase << crc16_calc.checksum();
    std::string crc16(oss.str());
    unsigned char suffix[]= "\x0d\x0a";

#ifdef ENABLE_LOGGING
    std::cout.setf(std::ios_base::hex, std::ios_base::basefield);
    std::cout.setf(std::ios_base::showbase);
    std::cout << "--> Payload: " << payload << ", CRC16: " << crc16.c_str() << std::endl;
#endif
//    tcflush(_tty_fd,TCIOFLUSH);
    write(_tty_fd, &prefix, 4);
    write(_tty_fd, payload.c_str(), payload.length());
    write(_tty_fd, crc16.c_str(), 4);
    write(_tty_fd, &suffix, 2);
  }

  std::string Connection::read_response() {
    // We need a state machine to detect the boundaries of the message.
    std::string response;
    enum response_state_t state=start;
    bool received=false;
    unsigned char c;
    std::vector<unsigned char> buffer;

    tv.tv_sec = 5;
    tv.tv_usec = 0;

#ifdef ENABLE_LOGGING
//    std::cout << "<-- Received: ";
#endif

    while (!received) {
     tv.tv_sec = 60;
     tv.tv_usec = 0;

     retval = select(20, &rfds, NULL, NULL, &tv);
     /* Don't rely on the value of tv now! */

     if (retval == -1) {
      std::cout << "Select error" << std::endl;
      break;
     } else if (retval) {

      if (read(_tty_fd,&c,1)>0) {
        // if new data is available on the serial port, print it out
        //std::cout << &c;
#ifdef ENABLE_LOGGING
//       std::cout << std::hex << std::setfill('0') << std::setw(2) << int(c) << ' ';
#endif
        switch (state) {
          case start:
            if (c == '\x05') state = prefix_1; break;
          case prefix_1:
            if (c == '\x05') state = prefix_2; break;
          case prefix_2:
            if (c == '\x03') state = prefix_3; break;
          case prefix_3:
            if (c == '\x03') state = message; break;
          case message:
            if (c != '\x0d') {
              buffer.push_back(c);
            } else {
              received = true; 
#ifdef ENABLE_LOGGING
//              std::cout << std::endl;
#endif
            }
            break;
        }
      }
     } else {
      break;
     }
    }
    if (buffer.size() >= 4) { //only pass message of minimal length, very short fragments may result after time out
      std::ostringstream oss;
      std::vector<unsigned char>::iterator it;
      for (it = buffer.begin(); it != buffer.end(); ++it) 
        oss << (*it);
      response = oss.str();
#ifdef ENABLE_LOGGING
      std::cout << "<-- Response: " << response << std::endl;
#endif
    } else {
      response = "FFFFFFFFFFFFFFFF";
#ifdef ENABLE_LOGGING
      std::cout << "<-- Response: " << response << std::endl;
#endif
    }
    
    return response;
  }
};

