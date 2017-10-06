#!/usr/bin/python
#################################################################
#
# Getting metrics related to your network
#
# Developed by  Joakim Lindbom
#               (Joakim.Lindbom@gmail.com)
#               2017-10-05
#
AGO_NETMETRICS_VERSION = '0.0.1
#
#################################################################



import optparse
import logging, syslog
from lxml import etree
import urllib2
import threading
import sys, getopt, os, time
from qpid.log import enable, DEBUG, WARN
from qpid.messaging import Message
import agogeneral
import speedtest

import xml.etree.cElementTree as ET

import agoclient

debug = False
InternetSpeed_DevId = "InternetSpeed"

# route stderr to syslog
class LogErr:
        def write(self, data):
                syslog.syslog(syslog.LOG_ERR, data)

syslog.openlog(sys.argv[0], syslog.LOG_PID, syslog.LOG_DAEMON)

logging.basicConfig(filename='/var/log/netmetrics.log', format='%(asctime)s %(levelname)s:%(message)s', level=logging.INFO) #level=logging.DEBUG
#logging.setLevel( logging.INFO )

def info (text):
    logging.info (text)
    syslog.syslog(syslog.LOG_INFO, text)
    if debug:
        print "INF " + text + "\n"
def debug (text):
    logging.debug (text)
    syslog.syslog(syslog.LOG_DEBUG, text)
    if debug:
        print "DBG " + text + "\n"
def error (text):
    logging.error(text)
    syslog.syslog(syslog.LOG_ERR, text)
    if debug:
        print "ERR " + text + "\n"
def warning(text):
    logging.warning (text)
    syslog.syslog(syslog.LOG_WARNING, text)
    if debug:
        print "WRN " + text + "\n"


def getInternetSpeed ():
    servers = []
    # If you want to test against a specific server
    # servers = [1234]

    s = speedtest.Speedtest()
    s.get_servers(servers)
    s.get_best_server()
    s.download()
    s.upload()

    results_dict = s.results.dict()

    return results_dict['download']/1024/1024, results_dict['upload']/1024/1024,

class speedMonitor(threading.Thread):
    def __init__(self):
        threading.Thread.__init__(self)

    def run(self):
        while (True):
            Download, Upload = getInternetSpeed()
            client.emit_event(InternetSpeed_DevId, "event.environment.speedchanged", Download, "Mbps")
            time.sleep(3600) # 1 h

info( "+------------------------------------------------------------")
info( "+ netmetrics.py startup. Version=" + AGO_NETMETRICS_VERSION )
info( "+------------------------------------------------------------")

client = agoclient.AgoConnection("netmetrics")
if (agoclient.get_config_option("netmetrics", "debug", "false").lower() == "true"):
    debug = True


#client.add_device(InternetSpeed_DevId, "InternetSpeed")

background = speedMonitor()
background.setDaemon(True)
background.start()

client.run()
