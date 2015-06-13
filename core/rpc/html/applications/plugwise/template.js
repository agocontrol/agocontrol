/**
 * Plugwise plugin
 */
function plugwiseConfig(deviceMap) {
    //members
    var self = this;
    self.plugwiseControllerUuid = null;
    self.port = ko.observable();
    self.devices = ko.observableArray([]);
    self.stats = ko.observableArray([]);
    self.selectedRemoveDevice = ko.observable();
    self.selectedCountersDevice = ko.observable();

    //Plugwise controller uuid
    if( deviceMap!==undefined )
    {
        for( var i=0; i<deviceMap.length; i++ )
        {
            if( deviceMap[i].devicetype=='plugwisecontroller' )
            {
                self.plugwiseControllerUuid = deviceMap[i].uuid;
                break;
            }
        }
    }

    //set port
    self.setPort = function() {
        //first of all unfocus element to allow observable to save its value
        $('#setport').blur();
        var content = {
            uuid: self.plugwiseControllerUuid,
            command: 'setport',
            port: self.port()
        }

        sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                if( res.result.error==0 )
                {
                    notif.success('#sp');
                }
                else 
                {
                    //error occured
                    notif.error(res.result.msg);
                }
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //get port
    self.getPort = function() {
        var content = {
            uuid: self.plugwiseControllerUuid,
            command: 'getport'
        }

        sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                self.port(res.result.port);
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //get stats
    self.getStats = function(callback) {
        var content = {
            uuid: self.plugwiseControllerUuid,
            command: 'getstats'
        }

        sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                callback(res.result.result);
                //console.log(res.result.result);
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };



    //reset all counters
    self.resetAllCounters = function() {
        if( confirm("Reset all counters?") )
        {
            var content = {
                uuid: self.plugwiseControllerUuid,
                command: 'resetallcounters'
            }
    
            sendCommand(content, function(res)
            {
                if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                {
                    notif.success('#rc');
                }
                else
                {
                    notif.fatal('#nr', 0);
                }
            });
        }
    };

    //reset counters
    self.resetCounters = function() {
        if( confirm("Reset counters of selected device?") )
        {
            var content = {
                uuid: self.plugwiseControllerUuid,
                command: 'resetcounters',
                device: self.selectedCountersDevice().internalid
            }
            sendCommand(content, function(res)
            {
                if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                {
                    notif.success('#rc');
                }
                else
                {
                    notif.fatal('#nr', 0);
                }
            });
        }
    };

    //get devices list
    self.getDevices = function() {
        var content = {
            uuid: self.plugwiseControllerUuid,
            command: 'getdevices'
        }
        //console.log(content);
        sendCommand(content, function(res)
        {
            //console.log(res.result);
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                self.devices(res.result.devices);
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //remove device
    self.removeDevice = function() {
        if( confirm('Delete device?') )
        {
            var content = {
                uuid: self.plugwiseControllerUuid,
                command: 'remove',
                device: self.selectedRemoveDevice()
            }
    
            sendCommand(content, function(res)
            {
                if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                {
                    if( res.result.error==0 )
                    {
                        notif.success('#ds');
                        //refresh devices list
                        self.getDevices();
                    }
                    else 
                    {
                        //error occured
                        notif.error(res.result.msg);
                    }
                }
                else
                {
                    notif.fatal('#nr', 0);
                }
            });
        }
    };

    //init ui
    self.getPort();
    self.getDevices();

    //get statistics
    //self.getStats(function(circlelist) {
    //    //console.log(circlelist);
    //    for( var id in circlelist )
    //    {
    //        var newCircle = circlelist[id];
    //        //console.log(id);
    //        self.stats.push({'id':id, 'mac':newCircle['mac'], 'ping':newCircle['pingtime'], 'lastRequestRTT':newCircle['lastRequestRTT'],
    //         'averageRequestRTT':newCircle['averageRequestRTT'], 'send':newCircle['send'], 'received':newCircle['received'], 'errorcount':newCircle['errorcount']});
    //    }
        //console.log(self.stats)
    //});

}

function plugwiseDashboard(deviceMap) {
    //members
    var self = this;
    self.plugwiseControllerUuid = null;
    self.stats = ko.observableArray([]);

    //Plugwise controller uuid
    if( deviceMap!==undefined )
    {
        for( var i=0; i<deviceMap.length; i++ )
        {
            if( deviceMap[i].devicetype=='plugwisecontroller' )
            {
                self.plugwiseControllerUuid = deviceMap[i].uuid;
                break;
            }
        }
    }

    self.getStats = function() {
        var content = {
            uuid: self.plugwiseControllerUuid,
            command: 'getstats'
        }

        sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                for (device in res.result.result)
                {
                   self.stats.push(res.result.result[device]);
                }
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };


    //get statistics
    self.getStats();


}

/**
 * Entry point: mandatory!
 */
function init_plugin(fromDashboard)
{
    var model;
    var template;
    if( fromDashboard )
    {
        model = new plugwiseDashboard(deviceMap);
        template = 'plugwiseDashboard';
    }
    else
    {
        model = new plugwiseConfig(deviceMap);
        template = 'plugwiseConfig';
    }
    model.mainTemplate = function() {
        return templatePath + template;
    }.bind(model);

    return model;
}

