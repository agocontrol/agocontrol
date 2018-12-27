/**
 * Ipx plugin
 */
function ipxConfig(agocontrol)
{
    //members
    var self = this;
    self.agocontrol = agocontrol;
    self.controllerUuid = null;
    self.boards = ko.observableArray([]);
    self.boardIp = ko.observable();
    self.devswitch = ko.observable(true);
    self.devdrapes = ko.observable(false);
    self.selectedOutputPin1 = ko.observable();
    self.selectedOutputPin2 = ko.observable();
    self.selectedAnalogPin1 = ko.observable();
    self.selectedCounterPin1 = ko.observable();
    self.selectedDigitalPin1 = ko.observable();
    self.selectedDigitalType = ko.observable();
    self.selectedBoard = ko.observable();
    self.selectedBoardUuid = ko.pureComputed(function() {
        if( self.selectedBoard() )
        {
            return self.selectedBoard().uuid;
        }
        return null;
    });
    self.selectedOutputType = ko.observable();
    self.selectedOutputType.subscribe(function(newVal) {
        if (newVal == "switch") {
            self.devswitch(true);
            self.devdrapes(false);
        } else if (newVal == "drapes") {
            self.devswitch(false);
            self.devdrapes(true);
        }
    });
    self.selectedAnalogType = ko.observable();
    self.allDevices = ko.observableArray([]);
    self.outputDevices = ko.observableArray([]);
    self.digitalDevices = ko.observableArray([]);
    self.binaryDevices = ko.observableArray([]);
    self.pushbuttonDevices = ko.observableArray([]);
    self.analogDevices = ko.observableArray([]);
    self.counterDevices = ko.observableArray([]);
    self.selectedDeviceToForce = ko.observable();
    self.selectedDeviceToDelete = ko.observable();
    self.selectedCounterToReset = ko.observable();
    self.selectedDeviceState = ko.observable();
    self.selectedLinkBinary = ko.observable();
    self.selectedLinkOutput = ko.observable();
    self.selectedLinkToDelete = ko.observable();
    self.links = ko.observableArray([]);

    //ipx controller uuid and boards
    if( self.agocontrol.devices()!==undefined )
    {
        for( var i=0; i<self.agocontrol.devices().length; i++ )
        {
            if( self.agocontrol.devices()[i].devicetype=='ipx800controller' )
            {
                self.controllerUuid = self.agocontrol.devices()[i].uuid;
                break;
            }
        }
    }

    //edit row
    self.makeEditable = function(item, td, tr)
    {
        if( $(td).hasClass('change_name') )
        {
            self.agocontrol.makeFieldDeviceNameEditable(td, item);
        }
            
        if( $(td).hasClass('change_room') )
        {
            self.agocontrol.makeFieldDeviceRoomEditable(td, item);
        }
    };

    self.grid = new ko.agoGrid.viewModel({
        data: self.allDevices,
        pageSize: 25,
        columns: [
            {headerText:'Name', rowText:'name'},
            {headerText:'Room', rowText:'room'},
            {headerText:'Type', rowText:'type'},
            {headerText:'Connections', rowText:'inputs'},
            {headerText:'Uuid', rowText:'uuid'},
            {headerText:'Actions', rowText:''}
        ],
        rowCallback: self.makeEditable,
        rowTemplate: 'rowTemplate'
    });

    //get device from inventory according to its internalid
    self.getDeviceInfos = function(obj) {
        var infos = {};
        for( var i=0; i<self.agocontrol.devices().length; i++ )
        {
            if( self.agocontrol.devices()[i].internalid==obj.internalid )
            {
                infos['room'] = self.agocontrol.devices()[i].room;
                infos['uuid'] = self.agocontrol.devices()[i].uuid;
                if( self.agocontrol.devices()[i].name() && self.agocontrol.devices()[i].name().length>0 )
                {
                    infos['name'] = self.agocontrol.devices()[i].name();
                }
                else
                {
                    //always return a name
                    infos['name'] = obj.internalid;
                }
                infos['internalid'] = obj.internalid;
                break;
            }
        }
        return infos;
    };
 
    //get device name. Always returns something
    self.getDeviceName = function(obj) {
        var name = '';
        var found = false;
        for( var i=0; i<self.agocontrol.devices().length; i++ )
        {
            if( self.agocontrol.devices()[i].internalid==obj.internalid )
            {
                if( self.agocontrol.devices()[i].name().length!=0 )
                {
                    name = self.agocontrol.devices()[i].name();
                }
                else
                {
                    name = obj.internalid;
                }
                found = true;
                break;
            }
        }

        if( !found )
        {
            //nothing found
            name = obj.internalid;
        }

        name += ' ('+obj.type;
        if( obj.inputs )
            name += '['+obj.inputs+']';
        name += ')';
        return name;
    };

    //get link name. Always returns something
    self.getLinkName = function(obj)
    {
        var outputName = self.getDeviceName(obj.output);
        var binaryName = self.getDeviceName(obj.binary);
        return binaryName+' => '+outputName;
    };

    //update devices
    self.updateDevices = function(devices, links)
    {
        //clear previous content
        self.outputDevices.removeAll();
        self.digitalDevices.removeAll();
        self.binaryDevices.removeAll();
        self.pushbuttonDevices.removeAll();
        self.analogDevices.removeAll();
        self.counterDevices.removeAll();
        self.allDevices.removeAll();
        self.links.removeAll();
    
        //append outputs
        var i=0;
        for( i=0; i<devices.outputs.length; i++ )
        {
            var infos = self.getDeviceInfos(devices.outputs[i]);
            if( devices.outputs[i].type=='odrapes' )
                infos['type'] = 'Output drapes';
            else if( devices.outputs[i].type=='oswitch' )
                infos['type'] = 'Output switch';
            else
                infos['type'] = devices.outputs[i].type;
            infos['inputs'] = 'O'+devices.outputs[i].inputs;
            var dev = infos;
            self.outputDevices.push(dev);
            self.allDevices.push(dev);
        }

        //append digitals
        for( i=0; i<devices.digitals.length; i++ )
        {
            var infos = self.getDeviceInfos(devices.digitals[i]);
            if( devices.digitals[i].type=='dbinary' )
                infos['type'] = 'Digital binary';
            else if( devices.digitals[i].type=='dpushbutton' )
                infos['type'] = 'Digital pushbutton';
            else
                infos['type'] = devices.digitals[i].type;
            infos['inputs'] = 'D'+devices.digitals[i].inputs;
            var dev = infos;
            self.digitalDevices.push(dev);
            if( devices.digitals[i].type=='dbinary' )
            {
                self.binaryDevices.push(dev);
            }
            else if( devices.digitals[i].type=='dpushbutton' )
            {
                self.pushbuttonDevices.push(dev);
            }
            self.allDevices.push(dev);
        }

        //append analogs
        for( i=0; i<devices.analogs.length; i++ )
        {
            var infos = self.getDeviceInfos(devices.analogs[i]);
            if( devices.analogs[i].type=='atemperature' )
                infos['type'] = 'Analog temperature';
            else if( devices.analogs[i].type=='alight' )
                infos['type'] = 'Analog light';
            else if( devices.analogs[i].type=='avolt' )
                infos['type'] = 'Analog volt';
            else if( devices.analogs[i].type=='ahumidity' )
                infos['type'] = 'Analog humidity';
            else if( devices.analogs[i].type=='abinary' )
                infos['type'] = 'Analog binary';
            else
                infos['type'] = devices.analogs[i].type;
            infos['inputs'] = 'A'+devices.analogs[i].inputs;
            var dev = infos;
            self.analogDevices.push(dev);
            if( devices.analogs[i].type=='abinary' )
            {
                self.binaryDevices.push(dev);
            }
            self.allDevices.push(dev);
        }

        //append counters
        for( i=0; i<devices.counters.length; i++ )
        {
            var infos = self.getDeviceInfos(devices.counters[i]);
            infos['type'] = 'Counter';
            infos['inputs'] = 'C'+devices.counters[i].inputs;
            var dev = infos;
            self.counterDevices.push(dev);
            self.allDevices.push(dev);
        }

        //append links
        if( links )
        {
            for( i=0; i<links.length; i++ )
            {
                var link = {'name':self.getLinkName(links[i]), 'output':links[i].output, 'binary':links[i].binary};
                self.links.push(link);
            }
        }
    };

    //get board status
    self.getBoardStatus = function()
    {
        if( self.selectedBoardUuid )
        {
            var content = {};
            content.uuid = self.selectedBoardUuid;
            content.command = 'status';
            self.agocontrol.sendCommand(content, function(res) {
                if( !res.error )
                {
                    //console.log('STATUS res:', res);
                    $('#currentoutputs').html(res.result.data.status.outputs);
                    $('#currentanalogs').html(res.result.data.status.analogs);
                    $('#currentcounters').html(res.result.data.status.counters);
                    $('#currentdigitals').html(res.result.data.status.digitals);

                    //console.log('BOARD DEVICES:', res.result.data.devices);
                    self.updateDevices(res.result.data.devices, res.result.data.links);
                    //console.log("ALLDEVICES", self.allDevices());
                }
            });
        }
        else
        {
            notif.warning('Please select a board first');
        }
    };

    //update UI
    self.updateUi = function()
    {
        self.getBoardStatus();
    };

    //return board uuid (or null if not found)
    self.getBoardUuid = function(internalid)
    {
        if( internalid )
        {
            for( var i=0; i<self.agocontrol.devices().length; i++ )
            {
                if( self.agocontrol.devices()[i].devicetype=='ipx800v3board' && self.agocontrol.devices()[i].internalid==internalid)
                {
                    return self.agocontrol.devices()[i].uuid;
                }
            }
        }
        return null;
    };

    //used to refresh automatically device associated to selected board
    self.selectedBoard.subscribe(function(newVal) {
        //update selected ipx board uuid
        self.selectedBoardUuid = self.getBoardUuid(newVal);
        if( self.selectedBoardUuid )
        {
            self.updateUi();
        }
    });

    //add a device
    self.addDevice = function(content, callback)
    {
        self.agocontrol.sendCommand(content, function(res)
        {
            if( !res.error )
            {
                notif.success(res.result.result.message);
                if( callback!==undefined )
                  callback();
            }
        });
    };

    //get boards
    self.getBoards = function()
    {
        if( self.controllerUuid )
        {
            var content = {};
            content.uuid = self.controllerUuid;
            content.command = 'getboards';
            self.agocontrol.sendCommand(content, function(res) {
                if( !res.error )
                {
                    self.boards.removeAll();
                    self.boards(res.result.data.boards);
                    //console.log("BOARDS:", res.result.databoards);
    
                    //select first board
                    if( self.boards().length>0 )
                    {
                        //update selectedBoard
                        self.selectedBoard(self.boards()[0]);
                        //no need to updateUi, it's performed automatically when selectedBoard is updated
                    }
                }
            });
        }
    }

    //get devices
    self.getDevices = function()
    {
        if( self.selectedBoardUuid )
        {
            var content = {};
            content.uuid = self.selectedBoardUuid;
            content.command = 'getdevices';
            self.agocontrol.sendCommand(content, function(res) {
                if( !res.error )
                {
                    console.log('BOARD DEVICES:', res.result.data.devices);
                    //update devices
                    self.updateDevices(res.result.data.devices, null);
                }
            });
        }
    };

    //add new IPX board
    self.addBoard = function()
    {
        if( self.controllerUuid )
        {
            var content = {};
            content.uuid = self.controllerUuid;
            content.command = 'addboard';
            content.ip = self.boardIp();
            self.addDevice(content, function() {
                self.getBoards();
            });
        }
        else
        {
            notif.fatal('#nr');
        }
    };

    //add new output
    self.addOutput = function()
    {
        if( self.selectedBoardUuid )
        {
            var content = {};
            content.uuid = self.selectedBoardUuid;
            content.command = 'adddevice';
            if( self.selectedOutputType()=="switch" )
            {
                content.type = "oswitch";
                content.pin1 = self.selectedOutputPin1();
            }
            else if( self.selectedOutputType()=="drapes" )
            {
                content.type = "odrapes";
                content.pin1 = self.selectedOutputPin1();
                content.pin2 = self.selectedOutputPin2();
            }
            self.addDevice(content, function() {
                self.updateUi();
            });
        }
        else
        {
            notif.warning('Please select a board first');
        }
    };

    //add new analog
    self.addAnalog = function()
    {
        if( self.selectedBoardUuid )
        {
            var content = {};
            content.uuid = self.selectedBoardUuid;
            content.command = 'adddevice';
            content.type = "a"+self.selectedAnalogType();
            content.pin1 = self.selectedAnalogPin1();
            self.addDevice(content, function() {
                self.updateUi();
            });
        }
        else
        {
            notif.warning('Please select a board first');
        }
    };

    //add new counter
    self.addCounter = function()
    {
        if( self.selectedBoardUuid )
        {
            var content = {};
            content.uuid = self.selectedBoardUuid;
            content.command = 'adddevice';
            content.type = 'counter';
            content.pin1 = self.selectedCounterPin1();
            self.addDevice(content, function() {
                self.updateUi();
            });
        }
        else
        {
            notif.warning('Please select a board first');
        }
    };

    //add new digital
    self.addDigital = function()
    {
        if( self.selectedBoardUuid )
        {
            var content = {};
            content.uuid = self.selectedBoardUuid;
            content.command = 'adddevice';
            content.type = self.selectedDigitalType();
            content.pin1 = self.selectedDigitalPin1();
            self.addDevice(content, function() {
                self.updateUi();
            });
        }
        else
        {
            notif.warning('Please select a board first');
        }
    };

    //add link between digital binary and output
    self.addLink = function() {
        if( self.selectedBoardUuid )
        {
            var content = {};
            content.uuid = self.selectedBoardUuid;
            content.command = 'addlink';
            content.binary = self.selectedLinkBinary().internalid;
            content.output = self.selectedLinkOutput().internalid;
            self.addDevice(content, function() {
                self.updateUi();
            });
        }
        else
        {
            notif.warning('Please select a board first');
        }
    };

    //delete link
    self.deleteLink = function()
    {
        if( self.selectedBoardUuid )
        {
            if( confirm('Really delete selected link?') )
            {
                var content = {};
                content.uuid = self.selectedBoardUuid;
                content.output = self.selectedLinkToDelete().output.internalid;
                content.digital = self.selectedLinkToDelete().binary.internalid;
                content.command = 'deletelink';
                self.agocontrol.sendCommand(content, function(res) {
                    if( !res.error )
                    {
                        notif.success('Link deleted');
                        self.updateUi();
                    }
                });
            }
        }
    };

    //force drape state
    self.forceState = function()
    {
        if( self.selectedBoardUuid )
        {
            var content = {};
            content.uuid = self.selectedBoardUuid;
            content.device = self.selectedDeviceToForce().internalid;
            content.command = 'forcestate';
            content.state = self.selectedDeviceState();
            self.agocontrol.sendCommand(content, function(res) {
                if( !res.error ) {
                    notif.success('State forced successfully');
                }
            });
        }
        else
        {
            notif.warning('Please select a board first');
        }
    };

    //all on
    self.allOn = function()
    {
        if( self.selectedBoardUuid )
        {
            var content = {};
            content.uuid = self.selectedBoardUuid;
            content.command = 'allon';
            self.agocontrol.sendCommand(content);
        }
    };
    
    //all off
    self.allOff = function()
    {
        if( self.selectedBoardUuid )
        {
            var content = {};
            content.uuid = self.selectedBoardUuid;
            content.command = 'alloff';
            self.agocontrol.sendCommand(content);
        }
    };

    //delete board
    self.deleteBoard = function()
    {
        if( self.selectedBoardUuid )
        {
            if( confirm('Really delete selected board?') )
            {
                var content = {};
                content.uuid = self.controllerUuid;
                content.command = 'deleteboard';
                content.ip = self.selectedBoardUuid;
                self.agocontrol.sendCommand(content, function(res) {
                    if( !res.error )
                    {
                        notif.success(res.result.message);
                        self.getBoards();
                    }
                });
            }
        }
    };

    //delete device
    self.deleteDevice = function()
    {
        if( self.selectedBoardUuid )
        {
            if( confirm('Really delete selected device?') )
            {
                var content = {};
                content.uuid = self.selectedBoardUuid;
                content.device = self.selectedDeviceToDelete().internalid;
                content.command = 'deletedevice';
                self.agocontrol.sendCommand(content, function(res) {
                    if( !res.error )
                    {
                        notif.success('Device deleted');
                        self.updateUi();
                    }
                });
            }
        }
    };

    //reset counter
    self.resetCounter = function()
    {
        if( self.selectedBoardUuid )
        {
            var content = {};
            content.uuid = self.selectedBoardUuid;
            content.device = self.selectedCounterToReset().internalid;
            content.command = 'reset';
            self.agocontrol.sendCommand(content, function(res) {
                if( !res.error )
                {
                    notif.success('Counter reseted');
                }
            });
        }
    };

    //save config
    self.saveConfig = function()
    {
        if( self.selectedBoardUuid )
        {
            var content = {};
            content.uuid = self.selectedBoardUuid;
            content.command = 'saveconfig';
            self.agocontrol.sendCommand(content);
        }
    };

    //get boards
    self.getBoards();
} 

/**
 * Entry point: mandatory!
 */
function init_template(path, params, agocontrol)
{
    ko.bindingHandlers.jqTabs = {
        init: function(element, valueAccessor) {
        var options = valueAccessor() || {};
            setTimeout( function() { $(element).tabs(options); }, 0);
        }
    };

    var model = new ipxConfig(agocontrol);
    return model;
}

