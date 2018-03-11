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
  scheduler
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
)

FIND_PACKAGE(OpenCV)

IF (NOT OpenCV_FOUND)
    list(REMOVE_ITEM AGODEVICES NVR)
    MESSAGE(STATUS "OpenCV not found, NOT building NVR device")
ENDIF (NOT OpenCV_FOUND)

FIND_PACKAGE(OpenZWave)
IF (NOT OpenZWave_FOUND)
	list(REMOVE_ITEM AGODEVICES zwave)
	MESSAGE(STATUS "OpenZWave not found, NOT building zwave device")
ENDIF ()

FIND_PACKAGE(EibClient)
IF (NOT EibClient_FOUND)
	list(REMOVE_ITEM AGODEVICES knx)
	MESSAGE(STATUS "eibclient not found, NOT building knx device")
ENDIF ()

find_package(PkgConfig)
pkg_check_modules(OpenLighting libola)
IF (NOT OpenLighting_FOUND)
	list(REMOVE_ITEM AGODEVICES agodmx)
	MESSAGE(STATUS "OpenLighting (ola) not found, NOT building agodmx device")
ENDIF ()

IF (FREEBSD)
	# These does not build on FreeBSD currently
	list(REMOVE_ITEM AGODEVICES
		blinkm # linux/i2c-dev.h
		firmata # S_IRUSR
		i2c # linux/i2c-dev.h
		PLCBUS # TCIFLUSH / TCIOFLUSH
		MySensors # TCIFLUSH / TCIOFLUSH
		zmcam # error: member access into incomplete type 'struct sockaddr_in'
	)
ENDIF (FREEBSD)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/agosimulator.py ${CMAKE_CURRENT_BINARY_DIR}/agosimulator.py COPYONLY)

foreach (device ${AGODEVICES})
    # options what to build
    OPTION(BUILD_DEVICE_${device} "Build the device ${device}" ON)
    if (BUILD_DEVICE_${device})
        add_subdirectory(${device})
    endif()
endforeach(device)
