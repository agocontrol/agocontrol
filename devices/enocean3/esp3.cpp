#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <errno.h>

#include "esp3.h"

void *start(void *object) {
    ((esp3::ESP3 *)object)->readerFunction();
    return NULL;
}

using namespace esp3;

size_t readbuf(int fd, uint8_t *buf, size_t size) {
    size_t _size = 0;

    do {
        int numread = read(fd, buf+_size, size - _size);
        if (numread == -1) {
            std::cerr << "ERROR: can't read from device: " << errno << " - " << strerror(errno) << std::endl;
            return -1;
        }
        _size += numread;
    } while ( _size < size);
    return _size;
}

size_t writebuf(int fd, uint8_t *buf, size_t size) {
    size_t _size = 0;
    do {
        int numwrite = write(fd, buf+_size, size - _size);
        if (numwrite == -1) {
            std::cerr << "ERROR: can't write to device: " << errno << " - " << strerror(errno) << std::endl;
            return -1;
        }
        _size += numwrite;
    } while (_size < size);
    return _size;

}

size_t numbytes(int fd) {
    struct timeval tv;
        fd_set fs;
        int bytes = 0;

    FD_ZERO (&fs);
    FD_SET (fd,&fs);
    tv.tv_sec = 0;
    tv.tv_usec = 50000;
    bytes = select (FD_SETSIZE,&fs,NULL,NULL,&tv);
    return bytes;
}



void *esp3::ESP3::readerFunction() {
    int len=0, optlen=0;
    uint8_t buf[65535];
    while (true) {
        int size=0;
        pthread_mutex_lock (&serialMutex);
        if (numbytes(fd) >0) {
            size = readFrame(buf, len, optlen);
        }
        pthread_mutex_unlock (&serialMutex);
        if (size > 0) parseFrame(buf,len,optlen);
    }
}
esp3::ESP3::ESP3(std::string _devicefile) {
    devicefile = _devicefile;
    idBase = 0;
    pthread_mutex_init(&serialMutex, NULL);

}

bool esp3::ESP3::init() {
    std::cout << "opening file" << std::endl;
    fd = open(devicefile.c_str(), O_RDWR);
    if (fd == -1) {
        std::cout << "Error " << errno << " from open: " << strerror(errno) << std::endl;
        return false;
    }

    struct termios tio;
    std::cout << "tcgetattr" << std::endl;
    if (tcgetattr(fd, &tio) != 0) {

        std::cout << "Error " << errno << " from tcgetattr: " << strerror(errno) << std::endl;
        return false;
    }
    cfsetispeed(&tio, B57600);
    cfsetospeed(&tio, B57600);

    tio.c_cflag     &=  ~PARENB;        // Make 8n1
    tio.c_cflag     &=  ~CSTOPB;
    tio.c_cflag     &=  ~CSIZE;
    tio.c_cflag     |=  CS8;
    tio.c_cflag     &=  ~CRTSCTS;       // no flow control
    tio.c_lflag     =   0;          // no signaling chars, no echo, no canonical processing
    tio.c_oflag     =   0;                  // no remapping, no delays
//  tio.c_cc[VMIN]      =   0;                  // read doesn't block
//  tio.c_cc[VTIME]     =   5;                  // 0.5 seconds read timeout 

    tcflush(fd, TCIFLUSH);
    tcsetattr(fd,TCSANOW,&tio);
    if (!readIdBase()) return false; // read the id base

    pthread_t _eventThread;
    //pthread_create(&_eventThread, NULL, start, NULL);
    //readerFunction();
    // start((void*)this);
    pthread_create(&_eventThread, NULL, start, (void*)this);
    return true;
}

RETURN_TYPE esp3::ESP3::parse_radio(uint8_t *buf, size_t len, size_t optlen) {
    if (len <1) return ADDR_OUT_OF_MEM;
    if (buf == NULL) return ADDR_OUT_OF_MEM;
    for (uint32_t i=0;i<len+optlen;i++) {
        printf("%02x",buf[i]);
    }
    std::cout << std::endl;
    if (optlen == 7) {
        printf("destination: 0x%02x%02x%02x%02x ",buf[len+1],buf[len+2],buf[len+3],buf[len+4]);
        printf("RSSI: %i ",buf[len+5]);
    } else {
        std::cout << "Optional data size:" << optlen << std::endl;
    }
    Notification *notif = NULL;
    switch (buf[0]) {
        case RORG_4BS: // 4 byte communication
            std::cout << "4BS data: ";
            printf("Sender id: 0x%02x%02x%02x%02x Status: %02x Data: %02x\n",buf[5],buf[6],buf[7],buf[8],buf[9],buf[3]);
            break;
        case RORG_RPS: // repeated switch communication
            {
                SwitchNotification *switchNotif =  new SwitchNotification();
                switchNotif->setId((buf[2]<<24) + (buf[3] << 16) + (buf[4] << 8) + buf[5]);
                std::cout << "RPS data: ";
                printf("Sender id: 0x%02x%02x%02x%02x Status: %02x Data: %02x\n",buf[2],buf[3],buf[4],buf[5],buf[6],buf[1]);
                if (buf[6] & (1 << 5)) {
                    std::cout << "T21=1 (PTM2xx)" << std::endl;
                } else {
                    std::cout << "T21=0 (PTM1xx)" << std::endl;
                }
                if (buf[6] & (1 << 4)) {
                    std::cout << "NU=1 (N-Message - normal)" << std::endl;
                    switch ((buf[1] >> 5) & 7) {
                        case 0: std::cout << "AI" << std::endl;
                            switchNotif->setRockerId(2);
                            break;
                        case 1: std::cout << "AO" << std::endl;
                            switchNotif->setRockerId(1);
                            break;
                        case 2: std::cout << "BI" << std::endl;
                            switchNotif->setRockerId(4);
                            break;
                        case 3: std::cout << "B0" << std::endl;
                            switchNotif->setRockerId(3);
                            break;
                        default:
                            break;
                    }
                    if ((buf[1] >> 4) & 1) { std::cout << "pressed" << std::endl; switchNotif->setIsPressed(true); }  else { std::cout << "released" << std::endl; switchNotif->setIsPressed(false); }
                } else {
                    std::cout << "NU=0 (U-Message - unassigned)" << std::endl;
                    switch ((buf[1] >> 5) & 7) {
                        case 0: std::cout << "no button" << std::endl;
                            break;
                        case 3: std::cout << "3 or 4 buttons" << std::endl;
                            break;
                        default:
                            break;
                    }
                    if ((buf[1] >> 4) & 1) { std::cout << "pressed" << std::endl; switchNotif->setIsPressed(true); }  else { std::cout << "released" << std::endl; switchNotif->setIsPressed(false); }
                }
                std::cout << "Repeat count: " << (buf[6] & 0xf) << std::endl;
                notif = switchNotif;
            }
                
            break;  
        default:
            printf("WARNING: Unhandled RORG: %02x\n",buf[0]);
            break;
    }
    if (handler && notif) {
        // printf("RUNNING HANDLER\n");
        handler(notif, context); 
    } else {
        if (!(handler)) printf("WARNING: No handler defined\n");
        // if (!(notif)) printf("WARNING: No notification object defined\n");

    }
    return OK;
}

bool esp3::ESP3::sendFrame(uint8_t frametype, uint8_t *databuf, uint16_t datalen, uint8_t *optdata, uint8_t optdatalen) {
        uint8_t crc=0;
    uint8_t buf[1024];
        int len=0;

    buf[len++]=SER_SYNCH_CODE;
    buf[len++]=(datalen >> 8) & 0xff; // len
    buf[len++]=datalen & 0xff;
    buf[len++]=optdatalen;
    buf[len++]=frametype;
        crc = proc_crc8(crc, buf[1]);
        crc = proc_crc8(crc, buf[2]);
        crc = proc_crc8(crc, buf[3]);
        crc = proc_crc8(crc, buf[4]);
    buf[len++]=crc;
    crc = 0;
    for (int i=0;i<datalen;i++) {
        buf[len]=databuf[i];
        crc=proc_crc8(crc, buf[len++]);
    }
    for (int i=0;i<optdatalen;i++) {
        buf[len]=optdata[i];
        crc=proc_crc8(crc, buf[len++]);
    }
    buf[len++]=crc; // should be 0x38
    return writebuf(fd,buf,len) == len ? true : false;
}

bool esp3::ESP3::readIdBase() {
    uint8_t buf[65536];
    buf[0] = CO_RD_IDBASE;
    int size, len, optlen;

    pthread_mutex_lock (&serialMutex);
    sendFrame(PACKET_COMMON_COMMAND,buf,1,NULL,0);
    size = readFrame(buf, len, optlen);
    pthread_mutex_unlock (&serialMutex);
    if (size < 11) {
        std::cout << "ERROR: invalid length in CO_RD_IDBASE reply" << std::endl;
        return false;
    }
    if (buf[4] != PACKET_RESPONSE) {
        std::cout << "ERROR: invalid packet type in CO_RD_IDBASE reply" << std::endl;
        return false;
    }
    if (buf[6] != RET_OK) {
        std::cout << "ERROR: return code not OK in CO_RD_IDBASE reply" << std::endl;
        return false;
    }
    printf("Received ID Base: 0x%02x%02x%02x%02x\n", buf[7],buf[8],buf[9],buf[10]);
    /* for (uint32_t i=0;i<len+optlen+6;i++) {
        printf("%02x ",buf[i]);
    } */
    idBase = (buf[7] << 24) + (buf[8] << 16) + (buf[9] << 8) + buf[10];
    return true;
}

uint32_t esp3::ESP3::getIdBase() {
    return idBase;
}

int esp3::ESP3::readFrame(uint8_t *buf, int &datalen, int &optdatalen) {
    size_t len =0;
    uint32_t packetsize =0;
    uint32_t datasize =0 ;
    uint32_t optionaldatasize =0 ;
    uint8_t crc = 0;

    do { // search for the sync code
        readbuf(fd,buf,1);
    } while (buf[0] != SER_SYNCH_CODE);

    len = readbuf(fd,buf+1,5); // read 
    if (len == -1 || len != 5) {
        std::cerr << "ERROR: can't read size" << std::endl;
        return -1;
    }

    crc = proc_crc8(crc, buf[1]);
    crc = proc_crc8(crc, buf[2]);
    crc = proc_crc8(crc, buf[3]);
    crc = proc_crc8(crc, buf[4]);
    if (crc != buf[5]) {
        std::cout << "ERROR: header crc checksum invalid!" << std::endl;
        printf("crc calc: %02x crc frame: %02x\n",crc,buf[5]);
        for (uint32_t i=0;i<6;i++) {
            printf("%02x ",buf[i]);
        }
        return -1;
    }

    datasize = (( (uint32_t) buf[1])<<8) + buf[2];
    optionaldatasize = buf[3];
    packetsize = ESP3_HEADER_LENGTH + datasize + optionaldatasize + 3; // 1byte sync, 2byte for two crc8

    len = readbuf(fd, buf+6, datasize+optionaldatasize+1);
    if (len == -1 || len != datasize+optionaldatasize+1) {
        std::cerr << "ERROR: datasize invalid" << std::endl;
        return -1;
    }

    crc = 0;
    for (uint32_t i=6;i<datasize+optionaldatasize+6;i++) {
        crc = proc_crc8(crc, buf[i]);
    }
    if (crc != buf[packetsize-1]) {
        std::cout << "ERROR: data crc checksum invalid!" << std::endl;
        printf("crc calc: %02x crc frame: %02x\n",crc,buf[packetsize-1]);
        for (uint32_t i=0;i<datasize+optionaldatasize+6+1;i++) {
            printf("%02x ",buf[i]);
        }
        std::cout << std::endl;
        return -1;
    }
    datalen = datasize;
    optdatalen = optionaldatasize;
    return 6 + datasize + optionaldatasize;
}

void esp3::ESP3::parseFrame(uint8_t *buf, int datasize, int optionaldatasize) {
    switch (buf[4]) {
        case PACKET_RADIO:
            std::cout << "RADIO Frame" << std::endl;
            parse_radio(buf+6,datasize,optionaldatasize);
            break;
        case PACKET_RESPONSE:
            std::cout << "RESPONSE Frame" << std::endl;
            std::cout << "content: ";
            for (uint32_t i=0;i<datasize+optionaldatasize+6;i++) {
                printf("%02x ",buf[i]);
            }
            std::cout << std::endl;
            break;
        case PACKET_RADIO_SUB_TEL:
            std::cout << "RADIO_SUB_TEL Frame" << std::endl;
            break;
        case PACKET_EVENT:
            std::cout << "EVENT Frame" << std::endl;
            break;
        case PACKET_COMMON_COMMAND:
            std::cout << "COMMON_COMMAND Frame" << std::endl;
            break;
        case PACKET_SMART_ACK_COMMAND:
            std::cout << "SMART_ACK_COMMAND Frame" << std::endl;
            break;
        case PACKET_REMOTE_MAN_COMMAND:
            std::cout << "REMOTE_MAN_COMMAND Frame" << std::endl;
            break;
        case PACKET_RADIO_MESSAGE:
            std::cout << "RADIO_MESSAGE Frame" << std::endl;
            break;
        case PACKET_RADIO_ADVANCED:
            std::cout << "RADIO_ADVANCED Frame" << std::endl;
            break;
        default:
            std::cout << "Unknown frame type" << std::endl;
            break;
    }
}

bool esp3::ESP3::fourbsCentralCommandDimLevel(uint16_t rid, uint8_t level, uint8_t speed) {
    uint8_t buf[65535];
    uint32_t addr = idBase + rid;

    buf[0]=0xa5;
    buf[1]=0x2;
    buf[2]=level;
    buf[3]=speed;
    buf[4]=0x09; // DB0.0=1 & DB0.3=1
    buf[5]=(addr >> 24) & 0xff;
    buf[6]=(addr >> 16) & 0xff;
    buf[7]=(addr >> 8) & 0xff;
    buf[8]=addr & 0xff;
    buf[9]=0x30; // status

    int size, len, optlen;
    pthread_mutex_lock (&serialMutex);
    sendFrame(PACKET_RADIO,buf,10,NULL,0);
    size = readFrame(buf, len, optlen);
    pthread_mutex_unlock (&serialMutex);

    if (size != 7) {
        std::cout << "ERROR: invalid length in reply" << std::endl;
        return false;
    }
    if (buf[4] != PACKET_RESPONSE) {
        std::cout << "ERROR: invalid packet type in reply" << std::endl;
        return false;
    }
    if (buf[6] != RET_OK) {
        std::cout << "ERROR: return code not OK" << std::endl;
        return false;
    }
    return true;
}

bool esp3::ESP3::fourbsCentralCommandDimOff(uint16_t rid) {
    uint8_t buf[65535];
    uint32_t addr = idBase + rid;

    buf[0]=0xa5;
    buf[1]=0x2;
    buf[2]=0;
    buf[3]=0;
    buf[4]=0x08; // DB0.3=1
    buf[5]=(addr >> 24) & 0xff;
    buf[6]=(addr >> 16) & 0xff;
    buf[7]=(addr >> 8) & 0xff;
    buf[8]=addr & 0xff;
    buf[9]=0x30; // status

    int size, len, optlen;
    pthread_mutex_lock (&serialMutex);
    sendFrame(PACKET_RADIO,buf,10,NULL,0);
    size = readFrame(buf, len, optlen);
    pthread_mutex_unlock (&serialMutex);
    if (size>0) parseFrame(buf,len,optlen);

    if (size != 7) {
        std::cout << "ERROR: invalid length in reply" << std::endl;
        return false;
    }
    if (buf[4] != PACKET_RESPONSE) {
        std::cout << "ERROR: invalid packet type in reply" << std::endl;
        return false;
    }
    if (buf[6] != RET_OK) {
        std::cout << "ERROR: return code not OK" << std::endl;
        return false;
    }
    return true;
}

bool esp3::ESP3::fourbsCentralCommandDimTeachin(uint16_t rid) {
    uint8_t buf[65535];
    uint32_t addr = idBase + rid;

    buf[0]=0xa5;
    buf[1]=0x2;
    buf[2]=0;
    buf[3]=0;
    buf[4]=0x0; // DB0.3=0 -> teach in
    buf[5]=(addr >> 24) & 0xff;
    buf[6]=(addr >> 16) & 0xff;
    buf[7]=(addr >> 8) & 0xff;
    buf[8]=addr & 0xff;
    buf[9]=0x30; // status

    int size, len, optlen;
    pthread_mutex_lock (&serialMutex);
    sendFrame(PACKET_RADIO,buf,10,NULL,0);
    size = readFrame(buf, len, optlen);
    pthread_mutex_unlock (&serialMutex);
    if (size>0) parseFrame(buf,len,optlen);

    return false;
}

bool esp3::ESP3::fourbsCentralCommandSwitchOn(uint16_t rid) {
    uint8_t buf[65535];
    uint32_t addr = idBase + rid;

    buf[0]=0x7;
    // buf[0]=0xa5;
    buf[1]=0x1;
    buf[2]=0;
    buf[3]=0;
    buf[4]=0x09; // DB0.3=1
    buf[5]=(addr >> 24) & 0xff;
    buf[6]=(addr >> 16) & 0xff;
    buf[7]=(addr >> 8) & 0xff;
    buf[8]=addr & 0xff;
    buf[9]=0x30; // status

    int size, len, optlen;
    pthread_mutex_lock (&serialMutex);
    sendFrame(PACKET_RADIO,buf,10,NULL,0);
    size = readFrame(buf, len, optlen);
    pthread_mutex_unlock (&serialMutex);
    if (size>0) parseFrame(buf,len,optlen);

    if (size != 7) {
        std::cout << "ERROR: invalid length in reply" << std::endl;
        return false;
    }
    if (buf[4] != PACKET_RESPONSE) {
        std::cout << "ERROR: invalid packet type in reply" << std::endl;
        return false;
    }
    if (buf[6] != RET_OK) {
        std::cout << "ERROR: return code not OK" << std::endl;
        return false;
    }
    return true;
}
bool esp3::ESP3::fourbsCentralCommandSwitchOff(uint16_t rid) {
    uint8_t buf[65535];
    uint32_t addr = idBase + rid;

    buf[0]=0x7;
    // buf[0]=0xa5;
    buf[1]=0x1;
    buf[2]=0;
    buf[3]=0;
    buf[4]=0x08; // DB0.3=1
    buf[5]=(addr >> 24) & 0xff;
    buf[6]=(addr >> 16) & 0xff;
    buf[7]=(addr >> 8) & 0xff;
    buf[8]=addr & 0xff;
    buf[9]=0x30; // status

    int size, len, optlen;
    pthread_mutex_lock (&serialMutex);
    sendFrame(PACKET_RADIO,buf,10,NULL,0);
    size = readFrame(buf, len, optlen);
    pthread_mutex_unlock (&serialMutex);
    if (size>0) parseFrame(buf,len,optlen);

    if (size != 7) {
        std::cout << "ERROR: invalid length in reply" << std::endl;
        return false;
    }
    if (buf[4] != PACKET_RESPONSE) {
        std::cout << "ERROR: invalid packet type in reply" << std::endl;
        return false;
    }
    if (buf[6] != RET_OK) {
        std::cout << "ERROR: return code not OK" << std::endl;
        return false;
    }
    return true;
}
bool esp3::ESP3::fourbsCentralCommandSwitchTeachin(uint16_t rid) {
    uint8_t buf[65535];
    uint32_t addr = idBase + rid;

    buf[0]=0x7;
    // buf[0]=0xa5;
    buf[1]=0x1;
    buf[2]=0;
    buf[3]=0;
    buf[4]=0x0; // DB0.3=0 -> teach in
    buf[5]=(addr >> 24) & 0xff;
    buf[6]=(addr >> 16) & 0xff;
    buf[7]=(addr >> 8) & 0xff;
    buf[8]=addr & 0xff;
    buf[9]=0x30; // status

    int size, len, optlen;
    pthread_mutex_lock (&serialMutex);
    sendFrame(PACKET_RADIO,buf,10,NULL,0);
    size = readFrame(buf, len, optlen);
    pthread_mutex_unlock (&serialMutex);
    if (size>0) parseFrame(buf,len,optlen);

    return false;
}

bool esp3::ESP3::setHandler(pfnOnNotification_t _handler, void *_context) {
    context = _context;
    handler = _handler;
    return handler ? true : false;
}

esp3::Notification::Notification() {
}
esp3::Notification::~Notification() {
}

uint32_t esp3::Notification::getId() const {
    return id;
}

void esp3::Notification::setId(uint32_t _id) {
    id = _id;
}

esp3::SwitchNotification::SwitchNotification() {
    isPressed = false;
    rockerId = 0;
    id= 0;
}
esp3::SwitchNotification::~SwitchNotification() {
}

void esp3::SwitchNotification::setRockerId(int _id) {
    rockerId = _id;
}

int esp3::SwitchNotification::getRockerId() const {
    return rockerId;
}

void esp3::SwitchNotification::setIsPressed(bool pressed) {
    isPressed = pressed;
}

bool esp3::SwitchNotification::getIsPressed() const {
    return isPressed;
}
