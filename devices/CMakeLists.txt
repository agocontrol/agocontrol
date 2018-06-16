cmake_minimum_required (VERSION 3.0)

set (AGODEVICES
  shvt250
  onvif
  example
  enocean3
  blinkm
  chromoflex
  firmata
  i2c
  irtrans_ethernet
  kwikwai
  mediaproxy
  rain8net
  zwave
  1wire
  syslog
  enigma2
  asterisk
  agoapc
  agojointspace
  raspiGPIO
  raspi1wGPIO
  raspiMCP3xxxGPIO
  gc100
  onkyo
  x10
  webcam
  temperatur.nu
  wake_on_lan
  squeezebox
  mopidy
  alert
  ipx800
  raspiCamera
  schedulerold
  tellstick
  lifx
  knx
  agodmx
  PLCBUS
  MySensors
  rrdtool
  zmcam
  ETAtouch
  yamaha
  gammu
  axisaudiotransmit
  weatherreporter
  MQTT
  NVR
  harmonyhub
  agoping
)

IF (NOT OpenCV_FOUND)
    list(REMOVE_ITEM AGODEVICES NVR)
    MESSAGE(STATUS "OpenCV not found, not building NVR device")
ENDIF (NOT OpenCV_FOUND)

IF (NOT OpenZWave_FOUND)
    list(REMOVE_ITEM AGODEVICES zwave)
    MESSAGE(STATUS "OpenZWave not found, not building zwave device")
ENDIF ()

IF (NOT EibClient_FOUND)
    list(REMOVE_ITEM AGODEVICES knx)
    MESSAGE(STATUS "eibclient not found, not building knx device")
ENDIF ()

IF (NOT OpenLighting_FOUND)
    list(REMOVE_ITEM AGODEVICES agodmx)
    MESSAGE(STATUS "OpenLighting (ola) not found, not building agodmx device")
ENDIF ()

if(NOT HAVE_I2C_SMBUS)
	list(REMOVE_ITEM AGODEVICES i2c blinkm)
    MESSAGE(STATUS "Linux i2c smbus not found, not building i2c & blinkm devices")
endif()

if(NOT LIBCURL_FOUND)
    list(REMOVE_ITEM AGODEVICES zmcam webcam)
    MESSAGE(STATUS "libcurl not found, not building zmcam & webcam devices")
endif()

IF (FREEBSD)
	# These does not build on FreeBSD currently
	list(REMOVE_ITEM AGODEVICES
		firmata # S_IRUSR
		PLCBUS # TCIFLUSH / TCIOFLUSH
		MySensors # TCIFLUSH / TCIOFLUSH
		zmcam # error: member access into incomplete type 'struct sockaddr_in'
	)
ENDIF (FREEBSD)

AgoPythonProgram(simulator DESCRIPTION "ago control Device simulator")

foreach (device ${AGODEVICES})
    # options what to build
    OPTION(BUILD_DEVICE_${device} "Build the device ${device}" ON)
    if (BUILD_DEVICE_${device})
        add_subdirectory(${device})
    endif()
endforeach(device)
