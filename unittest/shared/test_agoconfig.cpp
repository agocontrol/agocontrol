#include <sys/stat.h>
#include <fstream>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <boost/algorithm/string/join.hpp>
#include <boost/filesystem/fstream.hpp>

#include "agoapp.h"
#include "agoconfig.h"

namespace fs = boost::filesystem;
namespace agocontrol {

class TestAgoConfig : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE( TestAgoConfig ) ;
    CPPUNIT_TEST( testBasicGet );
    CPPUNIT_TEST( testFallbackGet );
    CPPUNIT_TEST( testSimulatedAppGet );
    CPPUNIT_TEST( testBasicSet );
    CPPUNIT_TEST( testSetError );
    CPPUNIT_TEST( testAppSet );
    CPPUNIT_TEST( testAppGet );
    CPPUNIT_TEST( testAppGetSection );
    CPPUNIT_TEST_SUITE_END();

    ConfigNameList empty;
    std::list<std::string>::const_iterator i;

    fs::path tempDir;
public:

    void setUp();
    void tearDown();

    void testBasicGet();
    void testFallbackGet();
    void testSimulatedAppGet();
    void testBasicSet();
    void testSetError();

    void testAppGet();
    void testAppGetSection();
    void testAppSet();
};

CPPUNIT_TEST_SUITE_REGISTRATION(TestAgoConfig);

// Dummy app which does nothing
class DummyUnitTestApp: public AgoApp {
public:
    AGOAPP_CONSTRUCTOR_HEAD(DummyUnitTestApp) {
        appConfigSection = "test";
    }
};

#define ASSERT_STR(expected, actual) \
    CPPUNIT_ASSERT_EQUAL(std::string(expected), (actual))
#define ASSERT_NOSTR(actual) \
    CPPUNIT_ASSERT_EQUAL(std::string(), (actual))
#define ASSERT_MAP_STR(expected, key, map) \
    CPPUNIT_ASSERT(map.find(key) != map.end()); \
    ASSERT_STR(expected, map.find(key)->second)

void TestAgoConfig::setUp() {
    tempDir = fs::temp_directory_path() / fs::unique_path();
    CPPUNIT_ASSERT(fs::create_directory(tempDir));
    CPPUNIT_ASSERT(fs::create_directory(tempDir / "conf.d"));
   
    // XXX: try to find conf/ directory in agocontrol src root
    // assumes we're in unittest/shared/test_agoconfig.cpp
    fs::path src(__FILE__);
    fs::path src_conf_dir = src.parent_path().parent_path().parent_path() / "conf";

    // Copy lens and system.conf from src dir
    fs::path lens = src_conf_dir / "agocontrol.aug";
    CPPUNIT_ASSERT_MESSAGE(
            std::string("Failed to find lens at ") + lens.string(),
            fs::exists(lens));

    AgoClientInternal::_reset();
    AgoClientInternal::setConfigDir(tempDir);

    fs::copy_file(lens, tempDir / "agocontrol.aug");
    fs::copy_file(src_conf_dir / "conf.d" / "system.conf", getConfigPath("conf.d/system.conf"));

    // Create dumm test file
    fs::fstream fs(getConfigPath("conf.d/test.conf"), std::fstream::out);
    fs << "[test]\n"
        "existing_key=value\n"
        "[system]\n"
        "password=testpwd\n";
    fs.close();

    // Important: init augeas AFTER all files are created
    CPPUNIT_ASSERT(augeas_init());
}

void TestAgoConfig::tearDown() {
    //fs::remove_all(tempDir);
}

void TestAgoConfig::testBasicGet() {
    ASSERT_NOSTR( getConfigSectionOption("test", "key", ""));
    ASSERT_NOSTR(getConfigSectionOption("test", "key", nullptr));
    ASSERT_STR("fallback", getConfigSectionOption("test", "key", "fallback"));

    ASSERT_STR("localhost", getConfigSectionOption("system", "broker", nullptr));
    ASSERT_STR("value", getConfigSectionOption("test", "existing_key", nullptr));

    auto map = getConfigSection("test", nullptr);

    CPPUNIT_ASSERT_EQUAL((size_t)1, map.size());
    ASSERT_MAP_STR("value", "existing_key", map);
}

void TestAgoConfig::testFallbackGet() {
    ConfigNameList section("test");
    section.add("system");

    ASSERT_NOSTR(getConfigSectionOption(section, "key", ""));

    // default-value fallback
    ASSERT_STR("fallback", getConfigSectionOption(section, "nonexisting", "fallback"));

    // Overriden in test.conf
    ASSERT_STR("testpwd", getConfigSectionOption(section, "password", nullptr));

    // from system.conf
    ASSERT_STR("localhost", getConfigSectionOption(section, "broker", nullptr));

    // From test.conf
    ASSERT_STR("value", getConfigSectionOption(section, "existing_key", nullptr));
}

void TestAgoConfig::testSimulatedAppGet() {
    ConfigNameList section("test");
    section.add("system");

    ConfigNameList app("test");

    ASSERT_NOSTR(getConfigSectionOption(section, "key", "", app));

    // default-value fallback
    ASSERT_STR("fallback", getConfigSectionOption(section, "nonexisting", "fallback", app));

    // Overriden in test.conf
    ASSERT_STR("testpwd", getConfigSectionOption(section, "password", nullptr, app));
    auto map = getConfigSection(section, app);
    CPPUNIT_ASSERT_EQUAL((size_t)2, map.size());
    ASSERT_MAP_STR("testpwd", "password", map);
    ASSERT_MAP_STR("value", "existing_key", map);

    // from system.conf; will not be found since we only look in app test
    ASSERT_NOSTR(getConfigSectionOption(section, "broker", nullptr, app));

    // From test.conf
    ASSERT_STR("value", getConfigSectionOption(section, "existing_key", nullptr, app));

    // Add system to app, and make sure we now can read broker
    app.add("system");
    ASSERT_STR("localhost", getConfigSectionOption(section, "broker", nullptr));

    map = getConfigSection(section, app);
    CPPUNIT_ASSERT(map.size() >= 8);
    ASSERT_MAP_STR("testpwd", "password", map);
    ASSERT_MAP_STR("value", "existing_key", map);
    ASSERT_MAP_STR("00000000-0000-0000-000000000000", "uuid", map);
    ASSERT_MAP_STR("SI", "units", map);
}

void TestAgoConfig::testBasicSet() {
    ASSERT_STR("value", getConfigSectionOption("test", "existing_key", nullptr));
    ASSERT_NOSTR(getConfigSectionOption("test", "new_key", nullptr));

    CPPUNIT_ASSERT(setConfigSectionOption("test", "new_key", "val"));
    ASSERT_STR("val", getConfigSectionOption("test", "new_key", nullptr));

    // Test blank app, should be same as nullptr
    CPPUNIT_ASSERT(setConfigSectionOption("test", "new_key", "val2", ""));
    ASSERT_STR("val2", getConfigSectionOption("test", "new_key", nullptr));

    // New file
    ASSERT_NOSTR(getConfigSectionOption("test", "new_key", nullptr, "new_file"));
    CPPUNIT_ASSERT(setConfigSectionOption("test", "new_key", "val", "new_file"));
    ASSERT_STR("val", getConfigSectionOption("test", "new_key", nullptr, "new_file"));
}

void TestAgoConfig::testSetError() {
    // Make file non-writeable and make sure we handle it properly
    // In augeas 1.3.0 it segfaults, we've got workarounds to avoid that.
    // https://github.com/hercules-team/augeas/issues/178
    if(getuid() == 0) {
        // This test is inneffective when run as root, but some envs such as docker build env is root.
        std::cerr << "IGNORED: You cannot run this test as root. Also, do not develop as root!";
        return;
    }

    chmod(getConfigPath("conf.d/test.conf").c_str(), 0);

    CPPUNIT_ASSERT(! setConfigSectionOption("test", "new_key", "val"));

    ASSERT_NOSTR(getConfigSectionOption("test", "new_key", nullptr));

    CPPUNIT_ASSERT(! setConfigSectionOption("test", "new_key", "val", "//bad/path"));
}

void TestAgoConfig::testAppGet() {
    DummyUnitTestApp app;
    ASSERT_STR("value", app.getConfigOption("existing_key", nullptr));
    ASSERT_NOSTR(app.getConfigOption("nonexisting_key", nullptr));

    // This should go directly to system.conf, only
    ASSERT_STR("localhost", app.getConfigOption("broker", nullptr, "system"));
    ASSERT_STR("letmein", app.getConfigOption("password", nullptr, "system"));
    ASSERT_NOSTR(app.getConfigOption("nonexisting_key", nullptr, "system"));
    ASSERT_NOSTR(app.getConfigOption("existing_key", nullptr, "system"));

    // This shall fall back on system.conf
    ASSERT_STR("localhost", app.getConfigOption("broker", nullptr, ExtraConfigNameList("system")));
    ASSERT_STR("testpwd", app.getConfigOption("password", nullptr, ExtraConfigNameList("system")));
    ASSERT_NOSTR(app.getConfigOption("nonexisting_key", nullptr, ExtraConfigNameList("system")));
    ASSERT_STR("value", app.getConfigOption("existing_key", nullptr, ExtraConfigNameList("system")));
}

void TestAgoConfig::testAppGetSection() {
    DummyUnitTestApp app;

    // This should get the app only
    auto map = app.getConfigSection(nullptr, nullptr);
    CPPUNIT_ASSERT_EQUAL((size_t)1, map.size());
    ASSERT_MAP_STR("value", "existing_key", map);

    // This sould only get system
    map = app.getConfigSection("system", "system");
    ASSERT_MAP_STR("letmein", "password", map);

    // This should fall back on system
    map = app.getConfigSection("system", ExtraConfigNameList("system"));
    ASSERT_MAP_STR("testpwd", "password", map);
    ASSERT_MAP_STR("00000000-0000-0000-000000000000", "uuid", map);
    ASSERT_MAP_STR("SI", "units", map);

    CPPUNIT_ASSERT(map.find("existing_key") == map.end());
}

void TestAgoConfig::testAppSet() {
    DummyUnitTestApp app;
    ASSERT_NOSTR(app.getConfigOption("nonexisting_key", nullptr));
    CPPUNIT_ASSERT(app.setConfigOption("nonexisting_key", "val"));
    ASSERT_STR("val", app.getConfigOption("nonexisting_key", nullptr));

    // This should go directly to system.conf
    ASSERT_STR("localhost", app.getConfigOption("broker", nullptr, "system"));
    CPPUNIT_ASSERT(app.setConfigOption("broker", "remotehost", "system"));
    ASSERT_STR("remotehost", app.getConfigOption("broker", nullptr, "system"));

    // Make sure we can set custom app + section
    ASSERT_NOSTR(app.getConfigOption("broker", nullptr, "section", "system"));
    CPPUNIT_ASSERT(app.setConfigOption("broker", "remotehost", "section", "system"));
    ASSERT_STR("remotehost", app.getConfigOption("broker", nullptr, "section", "system"));
}

}; /* namespace agocontrol */
