from abc import abstractmethod


class AgoTransportConfigError(RuntimeError):
    pass


class AgoTransport:
    @abstractmethod
    def start(self):
        raise NotImplementedError()

    @abstractmethod
    def prepare_shutdown(self):
        raise NotImplementedError()

    @abstractmethod
    def shutdown(self):
        raise NotImplementedError()

    @abstractmethod
    def is_active(self):
        raise NotImplementedError()

    @abstractmethod
    def send_message(self, message):
        raise NotImplementedError()

    @abstractmethod
    def send_request(self, message, timeout):
        raise NotImplementedError()

    @abstractmethod
    def fetch_message(self, timeout):
        """
        Wait for an incoming messages from the message bus.

        If the returned object has a reply_function defined, the sender expects the receiver to send
        a reply to the message using this method.

        :param timeout: How long to wait, in seconds.
        :return: An object which holds the message body and an optional reply_function
        :rtype: AgoTransportMessage
        """

        raise NotImplementedError()
    
    
class AgoTransportMessage:
    def __init__(self, message, reply_function):
        self.message = message
        self.reply_function = reply_function

    @property
    def content(self):
        return self.message['content']
