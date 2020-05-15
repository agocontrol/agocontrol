import errno
import logging
import json
import sys
import time
import uuid

from agoclient import agoproto
from agoclient.agotransport import AgoTransportConfigError
from agoclient.config import get_config_option, get_config_path

__all__ = ["AgoConnection"]


class AgoConnection:
    """This is class will handle the connection to ago control."""

    def __init__(self, instance):
        """The constructor."""
        self.instance = instance
        self.uuidmap_file = get_config_path('uuidmap/' + self.instance + '.json')
        self.log = logging.getLogger('connection')
        self.transport = None

        messaging = str(get_config_option("system", "messaging", "qpid"))
        broker = str(get_config_option("system", "broker", "localhost"))
        username = str(get_config_option("system", "username", "agocontrol"))
        password = str(get_config_option("system", "password", "letmein"))

        if messaging == 'qpid':
            try:
                from agoclient.agotransport_qpid import AgoQpidTransport
            except ImportError:
                self.log.exception("Cannot use qpid messaging, failed to load transport")
                raise AgoTransportConfigError()

            self.transport = AgoQpidTransport(broker, username, password)
        elif messaging == 'mqtt':
            try:
                from agoclient.agotransport_mqtt import AgoMqttTransport
            except ImportError:
                self.log.exception("Cannot use mqtt messaging, failed to load transport")
                raise AgoTransportConfigError()

            self.transport = AgoMqttTransport(instance, broker, username, password)
        else:
            raise RuntimeError('Invalid messaging type ' + messaging)

        self.devices = {}
        self.uuids = {}
        self.handler = None
        self.eventhandler = None
        self.agocontroller = None
        self.inventory = None
        self.inventory_last_update = 0
        self.inventory_max_age = 60
        self.load_uuid_map()
        self._shutting_down = False

    def start(self):
        return self.transport.start()

    def __del__(self):
        self.shutdown()

    def prepare_shutdown(self):
        self._shutting_down = True
        if self.transport:
            self.transport.prepare_shutdown()

    def shutdown(self):
        if self.transport:
            self.transport.shutdown()
            self.transport = None

    def add_handler(self, handler):
        """Add a command handler to be called when
        a command for a local device arrives."""
        self.handler = handler

    def add_event_handler(self, eventhandler):
        """Add an event handler to be called when an event arrives."""
        self.eventhandler = eventhandler

    def internal_id_to_uuid(self, internalid):
        """Convert a local (internal) id to an agocontrol UUID."""
        for uuid in self.uuids:
            if self.uuids[uuid] == internalid:
                return uuid

    def uuid_to_internal_id(self, uuid):
        """Convert an agocontrol UUID to a local (internal) id."""
        try:
            return self.uuids[uuid]
        except KeyError:
            self.log.warning("Cannot translate uuid %s to internal id", uuid)
            return None

    def store_uuid_map(self):
        """Store the mapping (dict) of UUIDs to
        internal ids into a JSON file."""
        try:
            with open(self.uuidmap_file, 'w') as outfile:
                json.dump(self.uuids, outfile)
        except (OSError, IOError) as exception:
            self.log.error("Cannot write uuid map file: %s", exception)
        except ValueError as exception:  # includes simplejson error
            self.log.error("Cannot encode uuid map: %s", exception)

    def load_uuid_map(self):
        """Read the mapping (dict) of UUIDs to
        internal ids from a JSON file."""
        try:
            with open(self.uuidmap_file, 'r') as infile:
                self.uuids = json.load(infile)
        except (OSError, IOError) as exception:
            if exception.errno == errno.ENOENT:
                # This is not fatal, it just haven't been created yet
                self.log.debug("Cannot find uuid map file: %s", exception)
            else:
                self.log.error("Cannot load uuid map file: %s", exception)
        except ValueError as exception:  # includes simplejson error
            self.log.error("Cannot decode uuid map from file: %s", exception)

    def emit_device_announce(self, uuid, device, initial_name):
        """Send a device announce event, this will
        be honored by the resolver component.
        You can find more information regarding the resolver
        here: http://wiki.agocontrol.com/index.php/Resolver """
        content = {}
        content["devicetype"] = device["devicetype"]
        content["uuid"] = uuid
        content["internalid"] = device["internalid"]
        content["handled-by"] = self.instance

        if initial_name != None:
            content["initial_name"] = initial_name

        self.send_message("event.device.announce", content)

    def emit_device_discover(self, uuid, device):
        """Send a device discover event, this will
        be honored by the resolver component.
        You can find more information regarding the resolver
        here: http://wiki.agocontrol.com/index.php/Resolver """
        content = {}
        content["devicetype"] = device["devicetype"]
        content["uuid"] = uuid
        content["internalid"] = device["internalid"]
        content["handled-by"] = self.instance
        self.send_message("event.device.discover", content)

    def emit_device_remove(self, uuid):
        """Send a device remove event to the resolver"""
        content = {}
        content["uuid"] = uuid
        self.send_message("event.device.remove", content)

    def emit_device_stale(self, uuid, stale):
        """Send a device stale event to the resolver"""
        content = {}
        content["uuid"] = uuid
        content["stale"] = stale
        self.send_message("event.device.stale", content)

    def add_device(self, internalid, devicetype, initial_name=None):
        """Add a device. Announcement to ago control will happen
        automatically. Commands to this device will be dispatched
        to the command handler.
        The devicetype corresponds to an entry in the schema.
        If an initial_name is set, the device will be given that name when it's first seen."""
        if self.internal_id_to_uuid(internalid) is None:
            self.uuids[str(uuid.uuid4())] = internalid
            self.store_uuid_map()

        device = {}
        device["devicetype"] = devicetype
        device["internalid"] = internalid
        device["stale"] = 0

        self.devices[self.internal_id_to_uuid(internalid)] = device
        self.emit_device_announce(self.internal_id_to_uuid(internalid), device, initial_name)

    def remove_device(self, internalid):
        """Remove a device."""
        if self.internal_id_to_uuid(internalid) is not None:
            self.emit_device_remove(self.internal_id_to_uuid(internalid))
            del self.devices[self.internal_id_to_uuid(internalid)]

    def suspend_device(self, internalid):
        """suspend a device"""
        uuid = self.internal_id_to_uuid(internalid)
        if uuid:
            if uuid in self.devices:
                self.devices[uuid]["stale"] = 1
                self.emit_device_stale(uuid, 1)

    def resume_device(self, internalid):
        """resume a device"""
        uuid = self.internal_id_to_uuid(internalid)
        if uuid:
            if uuid in self.devices:
                self.devices[uuid]["stale"] = 0
                self.emit_device_stale(uuid, 0)

    def is_device_stale(self, internalid):
        """return True if a device is stale"""
        uuid = self.internal_id_to_uuid(internalid)
        if uuid:
            if self.devices[uuid]["stale"] == 0:
                return False
            else:
                return True
        else:
            return False

    def send_message(self, subject, content):
        """Method to send an agocontrol message with a subject. Subject can be None if necessary"""
        message = dict(
            content=content,
            instance=self.instance  # XXX: This does not exist in C++
        )

        if subject:
            message['subject'] = subject

        return self.transport.send_message(message)

    def send_request(self, content, timeout=3.0):
        """Send message and fetch reply.
        
        Response will be an AgoResponse object
        """
        message = dict(
            content=content,
            instance=self.instance  # XXX: This does not exist in C++
        )

        return self.transport.send_request(message, timeout)

    def get_inventory(self, cached_allowed=False):
        """Returns the inventory from the resolver. Return value is a dict. If cached_allowed is true, it may return a cached version"""
        if cached_allowed:
            if self.inventory_last_update > 0 and (time.time() - self.inventory_last_update) < self.inventory_max_age:
                return self.inventory

        content = {}
        content["command"] = "inventory"
        response = self.send_request(content)
        if response.is_ok():
            self.inventory = response.data()
            self.inventory_last_update = time.time()
            return self.inventory
        else:
            # TODO: Report errors properly?
            self.inventory = None
            self.inventory_last_update = 0
            return {}
            # raise ResponseError(response)

    def get_agocontroller(self, allow_cache=True):
        """Returns the uuid of the agocontroller device"""
        if self.agocontroller:
            return self.agocontroller

        retry = 10
        while retry > 0 and not self._shutting_down:
            try:
                inventory = self.get_inventory(allow_cache)

                if inventory is not None and "devices" in inventory:
                    devices = inventory['devices']
                    for uuid in list(devices.keys()):
                        d = devices[uuid]
                        if d == None:
                            continue

                        if d['devicetype'] == 'agocontroller':
                            self.log.debug("agoController found: %s", uuid)
                            self.agocontroller = uuid
                            return uuid
            except agoproto.ResponseError as e:
                self.log.warning("Unable to resolve agocontroller (%s), retrying", e)
            else:
                # self.log.warning("Unable to resolve agocontroller, not in inventory response? retrying")
                self.log.warning("Unable to resolve agocontroller, retrying")

            allow_cache = False
            time.sleep(1)
            retry -= 1

        self.log.warning("Failed to resolve agocontroller, giving up")
        return None

    def emit_event(self, internal_id, event_type, level, unit):
        """This will send an event. Ensure level is of correct data type for the event!"""
        content = {}
        content["uuid"] = self.internal_id_to_uuid(internal_id)
        # XXX: C++ version will call Variant.parse on any string values,
        # which may convert it to other type i.e. int if it is
        # a plain int. There does not seem to be any similar qpid
        # function in Python.
        content["level"] = level
        content["unit"] = unit
        return self.send_message(event_type, content)

    def emit_event_raw(self, internal_id, event_type, content):
        """This will send content as event"""
        _content = content
        _content["uuid"] = self.internal_id_to_uuid(internal_id)
        return self.send_message(event_type, content)

    def maybe_set_device_name(self, internal_id, proposed_name):
        """Set the device name, unless the inventory already has a name set"""
        if proposed_name == '': return

        inv = self.get_inventory(True)
        device_uuid = self.internal_id_to_uuid(internal_id)
        dev = inv.get('devices', {}).get(device_uuid)
        self.log.debug("For %s / %s got device %s", internal_id, device_uuid, dev)
        if dev is None or dev['name'] == '':
            self.set_device_name(internal_id, proposed_name)

    def set_device_name(self, internal_id, name):
        """Set the device name, unconditionally"""
        controller = self.get_agocontroller()
        if not controller:
            raise Exception("No controller available, cannot set name")

        content = {}
        content["command"] = "setdevicename"
        content["uuid"] = controller
        content["device"] = self.internal_id_to_uuid(internal_id)
        content["name"] = name

        self.send_message(None, content)
        self.log.debug("'setdevicename' message sent for %s, name=%s", internal_id, name)

    def report_devices(self):
        """Report all our devices."""
        self.log.debug("Reporting child devices")
        for device in self.devices:
            # only report not stale device
            # if not "stale" in self.devices[device]:
            #    self.devices[device]["stale"] = 0
            # if self.devices[device]["stale"]==0:
            self.emit_device_discover(device, self.devices[device])

    def run(self):
        """This will start command and event handling.
        Be aware that this is blocking."""
        self.log.debug("Startup complete, waiting for messages")
        while self.transport and self.transport.is_active() and not self._shutting_down:
            transport_message = self.transport.fetch_message(10.0)
            if not transport_message:
                continue

            try:
                content = transport_message.message.get('content', None)

                self.log.trace("Processing message: %s", content)

                if content and 'command' in content:
                    if content['command'] == 'discover':
                        self.report_devices()
                    else:
                        if 'uuid' in content and content['uuid'] in self.devices:
                            # this is for one of our children
                            myid = self.uuid_to_internal_id(content["uuid"])

                            if myid is not None and self.handler:
                                returnval = self.handler(myid, content)
                                if returnval is None:
                                    logging.error("No return value from Handler for %s, not valid behaviour", content)
                                    returnval = agoproto.response_failed(
                                        message='Component "%s" has not been update properly, please contact developers with logs' % self.instance)

                                if transport_message.reply_function:
                                    replydata = {}
                                    if isinstance(returnval, dict):
                                        replydata = returnval
                                    else:
                                        replydata["result"] = returnval

                                    transport_message.reply_function(replydata)

                if 'subject' in transport_message.message:
                    subj = transport_message.message['subject']
                    if 'event' in subj and self.eventhandler:
                        self.eventhandler(subj, content)
            except:
                self.log.exception("Failed to handle incoming message: %s", transport_message)
