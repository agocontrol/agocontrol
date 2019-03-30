
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

