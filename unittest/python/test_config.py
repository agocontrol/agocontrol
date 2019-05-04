import pytest

import agoclient.config as config
from agoclient.agoapp import AgoApp

import os, os.path, shutil

# Find checkedout code dir:
# We're in subdir python/agoclient/
devdir = os.path.realpath(os.path.dirname(os.path.realpath(__file__)) + "/../..")

class AgoTest(AgoApp):
    pass


@pytest.fixture
def setup_config(tmpdir):
    # Create dummy conf

    conf_d = tmpdir.mkdir('conf.d')

    # Custom dir, we must place agocontrol.aug into this dir
    # TODO: or add some dir in loadpath?
    shutil.copyfile("%s/conf/agocontrol.aug" % devdir, str(tmpdir.join("agocontrol.aug")))
    shutil.copyfile("%s/conf/conf.d/system.conf" % devdir, str(conf_d.join("system.conf")))

    config.set_config_dir(str(tmpdir))

    with open(config.get_config_path('conf.d/test.conf'), 'w') as f:
        f.write("[test]\n"+
                "test_value_0=100\n"+
                "test_value_blank=\n"+
                "local=4\n"+
                "units=inv\n"+

                "[system]\n"+
                "password=testpwd\n"+

                "[sec2]\n"+
                "some=value\n"+
                "test_value_blank=not blank\n")

    # Kill augeas
    config.augeas = None

    yield str(tmpdir)

    tmpdir.remove()

@pytest.fixture
def app_sut(setup_config):
    app = AgoTest()

    yield app


gco = config.get_config_option
sco = config.set_config_option

class TestConfig:
    def test_get_path(self, setup_config):
        assert config.get_config_path("conf.d") == "%s/conf.d" % setup_config
        assert config.get_config_path("conf.d/") == "%s/conf.d/" % setup_config
        assert config.get_config_path("/conf.d/") == "%s/conf.d/" % setup_config

    def test_get_basic(self, setup_config):
        assert gco('system', 'broker') == 'localhost'
        assert gco('system', 'not_set') is None

        assert gco('test', 'test_value_blank') is None
        assert gco('test', 'local') == '4'

        # Test with defaults
        assert gco('system', 'broker', 'def') == 'localhost'
        assert gco('system', 'not_set', 'def') == 'def'

    def test_get_multiapp(self, setup_config):
        assert gco('system', 'broker', app="test") is None
        assert gco('system', 'broker', app=["test", "system"]) == 'localhost'
        assert gco('system', 'password', app=["test", "system"]) == 'testpwd'

    def test_get_multisection(self, setup_config):
        # Must explicitly tell app=test, with differnet section name
        assert gco('sec2', 'some', app='test') == 'value'
        assert gco(['sec2', 'test'], 'test_value_blank', app='test') == 'not blank'
        assert gco(['system'], 'units') == 'SI'
        assert gco(['test', 'system'], 'units') == 'inv'

    def test_get_section(self, setup_config):
        d = config.get_config_section(['system'], ['system'])
        assert d['broker'] == 'localhost'
        assert d['password'] == 'letmein'
        assert d['uuid'] == '00000000-0000-0000-000000000000'

        d = config.get_config_section(['system'], ['test', 'system'])
        assert d['password'] == 'testpwd'
        assert d['broker'] == 'localhost'
        assert d['uuid'] == '00000000-0000-0000-000000000000'


    def test_set_basic(self, setup_config):
        assert sco('system', 'new_value', '666')
        assert gco('system', 'new_value') == '666'

        assert sco('system', 'test_string', 'goodbye')
        assert gco('system', 'test_string') == 'goodbye'

    def test_set_new(self, setup_config):
        assert not os.path.exists(config.get_config_path("conf.d/newfile.conf"))
        assert sco('newfile', 'new_value', '666') == True

        assert os.path.exists(config.get_config_path("conf.d/newfile.conf"))

        assert gco('newfile', 'new_value') == '666'

    def test_unwritable(self, setup_config):
        if os.getuid() == 0:
            # TODO: Fix builders so they dont run as root, then change to self.fail instead.
            #self.fail("You cannot run this test as. Also, do not develop as root!")
            print("You cannot run this test as. Also, do not develop as root!")
            return

        with open(config.get_config_path('conf.d/blocked.conf'), 'w') as f:
            f.write("[blocked]\nnop=nop\n")

        os.chmod(config.get_config_path('conf.d/blocked.conf'), 0o444)

        assert gco('blocked', 'nop') == 'nop'

        # This shall fail, and write log msg
        assert not sco('blocked', 'new_value', '666')

        os.chmod(config.get_config_path('conf.d/blocked.conf'), 0)

        # reload
        config.augeas = None
        assert gco('blocked', 'nop') == None
        assert gco('blocked', 'nop', 'def') == 'def'


class TestAppConfig:
    def test_get_basic(self, app_sut):
        assert app_sut.get_config_option('test_value_0') == '100'
        assert app_sut.get_config_option('test_value_1') is None
        assert app_sut.get_config_option('units') == 'inv'

    def test_get_section(self, app_sut):
        # from system.conf:
        assert app_sut.get_config_option('broker', section='system') == 'localhost'
        # from test.conf:
        assert app_sut.get_config_option('password', section='system') == 'testpwd'

    def test_get_app(self, app_sut):
        # Will give None, since we're looking in section test, which is not in system.conf
        assert app_sut.get_config_option('test_value_0', app=['other', 'system']) is None

        # Will give system.confs password
        assert app_sut.get_config_option('password', app=['other', 'system'], section='system') == 'letmein'


    def test_get_section(self, app_sut):
        d = app_sut.get_config_section()
        assert d == dict(test_value_0='100',
                         test_value_blank=None,
                         local='4',
                         units='inv')

        d = app_sut.get_config_section(['system'], ['system'])
        assert d['broker'] == 'localhost'
        assert d['password'] == 'letmein'
        assert d['uuid'] == '00000000-0000-0000-000000000000'

        d = app_sut.get_config_section(['system'], ['test', 'system'])
        assert d['password'] == 'testpwd'
        assert d['broker'] == 'localhost'
        assert d['uuid'] == '00000000-0000-0000-000000000000'
        assert 'test_value_blank' not in d  # from test section

    def test_set_basic(self, app_sut):
        assert app_sut.set_config_option('new_value', '666') == True
        assert app_sut.get_config_option('new_value') == '666'

        assert app_sut.set_config_option('test_string', 'goodbye') == True
        assert app_sut.get_config_option('test_string') == 'goodbye'

    def test_set_section(self, app_sut):
        assert app_sut.set_config_option('some_value', '0', 'sec2') == True
        assert app_sut.get_config_option('some_value') is None
        assert app_sut.get_config_option('some_value', section='sec2') == '0'

    def test_set_new_file(self, app_sut):
        assert not os.path.exists(config.get_config_path("conf.d/newfile.conf"))
        assert app_sut.set_config_option('new_value', '666', section='newfile')
        assert os.path.exists(config.get_config_path("conf.d/newfile.conf"))

        assert app_sut.get_config_option('new_value', section='newfile') == '666'

