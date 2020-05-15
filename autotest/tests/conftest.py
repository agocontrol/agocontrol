import logging
import pytest

# Automatically imports
pytest_plugins = ['testlib.qpid_transport', 'testlib.mqtt_transport']

logging.basicConfig()


# TODO: use pytest-variables instead?
@pytest.fixture
def variables(request):
    return dict(transport=request.config.getoption("--transport"))


def pytest_addoption(parser):
    parser.addoption(
        "--transport", action="store", default="qpid", help="transport to test: qpid or mqtt"
    )


@pytest.fixture
def transport_adapter(request, variables):
    if variables['transport'] == 'qpid':
        adapter = request.getfixturevalue('qpid_transport_adapter')
    elif variables['transport'] == 'mqtt':
        adapter = request.getfixturevalue('mqtt_transport_adapter')
    else:
        raise AssertionError('Invalid transport configured')

    yield adapter

    adapter.shutdown()
