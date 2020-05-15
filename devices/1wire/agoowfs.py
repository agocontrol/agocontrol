#! /usr/bin/env python

import threading
import time
import logging
import ow

import agoclient
from agoclient import agoproto


class AgoOwfs(agoclient.AgoApp):
    def message_handler(self, internalid, content):
        for sensor in self.root.sensors():
            if sensor._path == internalid:
                if "command" in content:
                    if content["command"] == "on":
                        print("switching on: " + internalid)
                        self.connection.emit_event(internalid, "event.device.statechanged", 255, "")
                    elif content["command"] == "off":
                        print("switching off: " + internalid)
                        self.connection.emit_event(internalid, "event.device.statechanged", 0, "")
                    else:
                        return agoproto.response_unknown_command()

                    return agoproto.response_success()
                else:
                    return agoproto.response_bad_parameters()

    def app_cmd_line_options(self, parser):
        """App-specific command line options"""
        parser.add_argument('-i', '--interval', type=float,
                            default=5,
                            help="How many seconds (int/float) to wait between sent messages")

    def setup_app(self):
        self.connection.add_handler(self.message_handler)
        self.device = self.get_config_option("device", "/dev/usbowfs")

        self.sensors = {}

        try:
            ow.init(str(self.device))
        except ow.exNoController:
            self.log.info("can't open one wire device, aborting")
            time.sleep(5)
            exit(-1)

        self.log.info("reading devices")
        self.root = ow.Sensor('/')

        for _sensor in self.root.sensors():
            if _sensor._type == 'DS18S20' or _sensor._type == 'DS18B20':
                self.connection.add_device(_sensor._path, "temperaturesensor")
            if _sensor._type == 'DS2438':
                try:
                    if ow.owfs_get('%s/MultiSensor/type' % _sensor._path) == 'MS-TL':
                        self.connection.add_device(_sensor._path, "temperaturesensor")
                        self.connection.add_device(_sensor._path + "-brightness", "brightnesssensor")
                    if ow.owfs_get('%s/MultiSensor/type' % _sensor._path) == 'MS-TH':
                        self.connection.add_device(_sensor._path, "temperaturesensor")
                        self.connection.add_device(_sensor._path + "-humidity", "humiditysensor")
                    if ow.owfs_get('%s/MultiSensor/type' % _sensor._path) == 'MS-T':
                        self.connection.add_device(_sensor._path, "temperaturesensor")
                except (ow.exUnknownSensor, Exception) as ex:
                    self.log.error('Failed to identify sensor type', ex)

            if _sensor._type == 'DS2406':
                _sensor.PIO_B = '0'
                _sensor.latch_B = '0'
                _sensor.set_alarm = '111'
                self.connection.add_device(_sensor._path, "switch")
                self.connection.add_device(_sensor._path, "binarysensor")

        self.log.info("Starting owfs thread")
        self.background = OwfsThread(self, self.args.interval)
        self.background.connection = self.connection
        self.background.setDaemon(True)
        self.background.start()

        inventory = self.connection.get_inventory()
        self.log.info("Inventory has %d entries", len(inventory))

    def cleanup_app(self):
        # Unfortunately, there is no good way to wakeup the python sleep().
        # In this particular case, we can just let it die. Since it's a daemon thread,
        # it will.

        # self.background.join()
        pass


class OwfsThread(threading.Thread):
    def __init__(self, app, interval):
        threading.Thread.__init__(self)
        self.app = app
        self.interval = interval

    def run(self):
        level = 0
        counter = 0
        log = logging.getLogger('OwfsThread')
        while not self.app.is_exit_signaled():

            # while (True):
            try:
                all_sensors = self.app.sensors
                ROOT = ow.Sensor('/')
                for sensor in ROOT.sensors():
                    if (sensor._type == 'DS18S20' or sensor._type == 'DS18B20'
                            or sensor._type == 'DS2438'):
                        temp = round(float(sensor.temperature), 1)
                        if sensor._path in self.app.sensors:
                            if 'temp' in all_sensors[sensor._path]:
                                if (abs(all_sensors[sensor._path]['temp'] -
                                        temp) > 0.5):
                                    log.debug("Sending enviromnet changes on sensor % (%.2f dgr C)", sensor._path, temp)
                                    self.app.connection.emit_event(sensor._path, "event.environment.temperaturechanged",
                                                                   temp, "degC")
                                    all_sensors[sensor._path]['temp'] = temp
                        else:
                            self.app.connection.emit_event(sensor._path,
                                                           "event.environment.temperaturechanged",
                                                           temp, "degC")
                            all_sensors[sensor._path] = {}
                            all_sensors[sensor._path]['temp'] = temp

                    if sensor._type == 'DS2438':
                        try:
                            if (ow.owfs_get('%s/MultiSensor/type' % sensor._path) == 'MS-TL'):
                                rawvalue = float(ow.owfs_get('/uncached%s/VAD' % sensor._path))
                                if rawvalue > 10:
                                    rawvalue = 0
                                lightlevel = int(round(20 * rawvalue))
                                if 'light' in all_sensors[sensor._path]:
                                    if (abs(all_sensors[sensor._path]['light'] -
                                            lightlevel) > 5):
                                        self.app.connection.emit_event(sensor._path + "-brightness",
                                                                       "event.environment.brightnesschanged",
                                                                       lightlevel, "percent")
                                        all_sensors[sensor._path]['light'] = lightlevel
                                else:
                                    self.app.connection.emit_event(sensor._path + "-brightness",
                                                                   "event.environment.brightnesschanged",
                                                                   lightlevel, "percent")
                                    all_sensors[sensor._path]['light'] = lightlevel
                            if ow.owfs_get('%s/MultiSensor/type' % sensor._path) == 'MS-TH':
                                humraw = ow.owfs_get('/uncached%s/humidity' % sensor._path)
                                humidity = round(float(humraw))
                                if 'hum' in all_sensors[sensor._path]:
                                    if abs(all_sensors[sensor._path]['hum'] - humidity) > 2:
                                        self.app.connection.emit_event(sensor._path + "-humidity",
                                                                       "event.environment.humiditychanged",
                                                                       humidity, "percent")
                                        all_sensors[sensor._path]['hum'] = humidity
                                else:
                                    self.app.connection.emit_event(sensor._path + "-humidity",
                                                                   "event.environment.humiditychanged",
                                                                   humidity, "percent")
                                    all_sensors[sensor._path]['hum'] = humidity
                        except (ow.exUnknownSensor, Exception) as e:
                            log.error('Unknown sensor', e)

                    if sensor._type == 'DS2406':
                        if sensor.latch_B == '1':
                            sensor.latch_B = '0'
                            if sensor.sensed_B == '1':
                                self.app.connection.emit_event(sensor._path,
                                                               "event.security.sensortriggered", 255, "")
                            else:
                                self.app.connection.emit_event(sensor._path,
                                                               "event.security.sensortriggered", 0, "")
            except ow.exUnknownSensor:
                pass

            log.trace("Next command in %f seconds...", self.interval)
            time.sleep(self.interval)


if __name__ == "__main__":
    AgoOwfs().main()
