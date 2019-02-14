/**
 * Model class
 *
 * @returns {VariablesConfig}
 */
function VariablesConfig(agocontrol)
{
    var self = this;
    self.agocontrol = agocontrol;
    self.newVariable = ko.observable('');

    var setVar = function(variableName, value) {
        var variable = self.agocontrol.getVariable(variableName)
        if(variable && variable() === value)
            // No-op
            return Promise.resolve();

        var content = {};
        content.variable = variableName;
        content.value = value;
        content.command = 'setvariable';
        content.uuid = self.agocontrol.agoController;

        return self.agocontrol
            .sendCommand(content)
            .then(function(res) {
                if(!variable) {
                    self.agocontrol.initVariable(
                        content.variable,
                        content.value
                    );
                }else{
                    variable(content.value);
                }
            });
    };

    self.makeEditable = function(item, td, tr)
    {
        if( $(td).hasClass('edit_var') )
        {
            $(td).editable(
                function(value, settings)
                {
                    var varName = $(this).data('variable');

                    setVar(varName, value);
                    return value;
                },
                {
                    data : function(value, settings) {
                        return value;
                    },
                    onblur : "cancel"
                }
            ).click();
        }
    };

    self.grid = new ko.agoGrid.viewModel({
        data: self.agocontrol.variables,
        columns: [
            {headerText:'Name', rowText:'variable'},
            {headerText:'Value', rowText:'value'},
            {headerText:'Actions', rowText:''}
        ],
        rowCallback: self.makeEditable,
        rowTemplate: 'rowTemplate',
        boxStyle: 'box-primary'
    });

    self.createVariable = function(data, event)
    {
        var variableName = self.newVariable().trim();
        if(variableName.trim() === '') {
            notif.error('A variable must have a name');
            return;
        }

        if(self.agocontrol.getVariable(variableName) != null) {
            notif.error('Variable already exists');
            return;
        }

        self.agocontrol.block($('#agoGrid'));
        setVar(variableName, "true")
            .then(function(){
                self.newVariable('');
            })
            .finally(function(){
                self.agocontrol.unblock($('#agoGrid'));
            });
    };

    self.toggleBoolean = function(item, event)
    {
        var currentValue = item.booleanValue();
        if(currentValue === null) return;

        setVar(item.variable, currentValue ? 'false' : 'true');
    };

    self.deleteVariable = function(item, event)
    {
        $("#confirmPopup").data('item', item);
        $("#confirmPopup").modal('show');
    };

    self.doDeleteVariable = function()
    {
        self.agocontrol.block($('#agoGrid'));
        $("#confirmPopup").modal('hide');

        var item = $("#confirmPopup").data('item');
        var content = {};
        content.variable = item.variable;
        content.uuid = self.agocontrol.agoController;
        content.command = 'delvariable';
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                self.agocontrol.variables.remove(function(e) {
                    return e.variable == item.variable;
                });
            })
            .catch(function(err) {
                notif.error("Error while deleting variable!");
            })
            .finally(function() {
                self.agocontrol.unblock($('#agoGrid'));
            });
    };
}

/**
 * Initalizes the model
 */
function init_template(path, params, agocontrol)
{
    var model = new VariablesConfig(agocontrol);
    return model;
}
