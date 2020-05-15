"""Response codes"""
RESPONSE_SUCCESS = 'success'
RESPONSE_ERR_FAILED = 'failed'
RESPONSE_ERR_UNKNOWN_COMMAND = 'unknown.command'
RESPONSE_ERR_BAD_PARAMETERS = 'bad.parameters'
RESPONSE_ERR_NO_COMMANDS_FOR_DEVICE = 'no.commands.for.device'
RESPONSE_ERR_MISSING_PARAMETERS = 'missing.parameters'


class AgoResponse:
    """This class represents a response as obtained from AgoConnection.send_request"""

    def __init__(self, response=None, identifier=None, message=None):
        """Create a response holder, either from a received response or implicitly via
        an identifier & optional message.
        """
        if not response:
            response = response_error(identifier, message)
        else:
            assert not identifier
            assert not message

        self.response = response
        if self.is_ok():
            self.root = self.response["result"]
        elif self.is_error():
            self.root = self.response["error"]
        else:
            raise Exception("Invalid response, neither result or error present")

    def __str__(self):
        if self.is_error():
            return 'AgoResponse[ERROR] message="%s" data=[%s]' % (self.message(), str(self.data()))
        else:
            return 'AgoResponse[OK] message="%s" data=[%s]' % (self.message(), str(self.data()))

    def is_error(self):
        return "error" in self.response

    def is_ok(self):
        return "result" in self.response

    def identifier(self):
        return self.root["identifier"]

    def message(self):
        if "message" in self.root:
            return self.root["message"]

        return None

    def data(self):
        if "data" in self.root:
            return self.root["data"]

        return None


class ResponseError(Exception):
    def __init__(self, response):
        if not response.is_error():
            raise Exception("Not an error response")

        self.response = response

        super(ResponseError, self).__init__(self.message())

    def identifier(self):
        return self.response.identifier()

    def message(self):
        return self.response.message()

    def data(self):
        return self.response.data()


# Helpers to create proper responses


def response_result(iden, mess=None, data=None):
    """
    Construct a response message.

    :param iden: response 'identifier'
    :param mess: response 'message' <optional>
    :param data: response 'data' <optional>
    """

    response = {}
    result = {}

    if iden is not None:
        result['identifier'] = iden
    else:
        raise Exception('Response without identifier (param "iden") not permitted')

    if mess is not None:
        result['message'] = mess

    if data is not None:
        result['data'] = data

    response['result'] = result
    return response


def response_success(data=None, message=None):
    """
    Construct a response message with the very generic 'success' identifier.

    First parameter is data because data is more often returned than message when response is successful
    """
    return response_result(iden=RESPONSE_SUCCESS, mess=message, data=data)


def response_error(iden, mess, data=None):
    """
    Construct an response message indicating an error.

    parameters are voluntary shortened!

    :param iden: response identifier <mandatory>
    :param mess: response message <optional>
    :param data: response data <optional>
    """
    response = {}
    error = {}

    # identifier
    if iden is not None:
        error['identifier'] = iden
    else:
        # iden is mandatory
        raise Exception('Response without identifier (param "iden") not permitted')

    # message
    if mess is not None:
        error['message'] = mess
    else:
        # mess is mandatory
        raise Exception('Error response without message (param "mess") not permitted')

    # data
    if data is not None:
        error['data'] = data

    response['error'] = error
    return response


def response_unknown_command(message="Unhandled command", data=None):
    """
    Create a response which indicates that an unknown command was sent.

    :param message: A human readable message giving a hint of what failed.
    :param data: Any optional machine readable data.
    :return:
    """
    return response_error(iden=RESPONSE_ERR_UNKNOWN_COMMAND, mess=message, data=data)


def response_missing_parameters(message="Missing parameter", data=None):
    """
    Create a response which indicates missing parameter was passed to the command.

    :param message: A human readable message giving a hint of what failed.
    :param data: Any optional machine readable data.
    :return:
    """
    return response_error(iden=RESPONSE_ERR_MISSING_PARAMETERS, mess=message, data=data)


def response_bad_parameters(message="Bad parameter", data=None):
    """
    Create a response which indicates bad parameter was passed to the command.

    :param message: A human readable message giving a hint of what failed.
    :param data: Any optional machine readable data.
    :return:
    """
    return response_error(iden=RESPONSE_ERR_BAD_PARAMETERS, mess=message, data=data)


def response_failed(message, data=None):
    """
    Create a response which indicate a general failure (RESPONSE_ERR_FAILED)

    :param message: A human readable message giving a hint of what failed.
    :param data: Any optional machine readable data.
    :return:
    """
    return response_error(iden=RESPONSE_ERR_FAILED, mess=message, data=data)

