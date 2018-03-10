#include <sys/stat.h>
#include <fstream>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <boost/algorithm/string/join.hpp>
#include <boost/filesystem/fstream.hpp>

#include "agojson.h"

namespace fs = boost::filesystem;
namespace agocontrol {

class TestAgoJson : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE( TestAgoJson ) ;
    CPPUNIT_TEST( testJsonCpp );
    CPPUNIT_TEST( testParseToJson );
    CPPUNIT_TEST( testStringToConverters );
    CPPUNIT_TEST_SUITE_END();

    fs::path tempDir;
public:

    void setUp();
    void tearDown();

    void testJsonCpp();
    void testParseToJson();
    void testStringToConverters();

};

CPPUNIT_TEST_SUITE_REGISTRATION(TestAgoJson);

#define ASSERT_STR(expected, actual) \
    CPPUNIT_ASSERT_EQUAL(std::string(expected), (actual))
#define ASSERT_NOSTR(actual) \
    CPPUNIT_ASSERT_EQUAL(std::string(), (actual))

void TestAgoJson::setUp() {
    tempDir = fs::temp_directory_path() / fs::unique_path();
    CPPUNIT_ASSERT(fs::create_directory(tempDir));
    CPPUNIT_ASSERT(fs::create_directory(tempDir / "conf.d"));
   
    // XXX: try to find conf/ directory in agocontrol src root
    // assumes we're in unittest/shared/test_agoconfig.cpp
    fs::path src(__FILE__);
    fs::path src_conf_dir = src.parent_path().parent_path().parent_path() / "conf";
}

void TestAgoJson::tearDown() {
    //fs::remove_all(tempDir);
}

void TestAgoJson::testJsonCpp() {
    // These tests are mainly there to demonstrate that some often used constructs in our
    // code actually behave the way we think they do.
    Json::Value val;
    CPPUNIT_ASSERT_EQUAL(Json::nullValue, val.type());

    val["test"] = "1";
    CPPUNIT_ASSERT_EQUAL(Json::objectValue, val.type());

    val = Json::Value();
    CPPUNIT_ASSERT_EQUAL(Json::nullValue, val.type());

    val[0] = "1";
    CPPUNIT_ASSERT_EQUAL(Json::arrayValue, val.type());

    val = Json::Value();
    CPPUNIT_ASSERT_EQUAL(Json::nullValue, val.type());

    val.append("123");
    CPPUNIT_ASSERT_EQUAL(Json::arrayValue, val.type());

    // Modifying reference adjusts refered value
    Json::Value& ref = val;
    ref[1] = 2;
    ref[2] = "2";

    ASSERT_STR("123", val[0].asString());
    CPPUNIT_ASSERT_EQUAL(2, val[1].asInt());
    ASSERT_STR("2", val[2].asString());

    // Swap as expected
    Json::Value val2;
    val2.swap(val);
    CPPUNIT_ASSERT_EQUAL(Json::nullValue, val.type());
    CPPUNIT_ASSERT_EQUAL(Json::nullValue, ref.type());
    CPPUNIT_ASSERT_EQUAL(Json::arrayValue, val2.type());

    val["test2"] = "inner";
    val["test2"].swap(val2);

    CPPUNIT_ASSERT_EQUAL(Json::stringValue, val2.type());
    ASSERT_STR("inner", val2.asString());

    CPPUNIT_ASSERT_EQUAL(Json::arrayValue, val["test2"].type());
}

void TestAgoJson::testParseToJson() {
    Json::Value val;

    val = parseToJson("test-string");
    ASSERT_STR("test-string", val.asString());
    CPPUNIT_ASSERT_EQUAL(Json::stringValue, val.type());

    val = parseToJson("0");
    CPPUNIT_ASSERT_EQUAL(0, val.asInt());
    CPPUNIT_ASSERT_EQUAL((unsigned int)0, val.asUInt());
    CPPUNIT_ASSERT_EQUAL(Json::uintValue, val.type());

    val = parseToJson("-0");
    CPPUNIT_ASSERT_EQUAL(0, val.asInt());
    CPPUNIT_ASSERT_EQUAL((unsigned int) 0, val.asUInt());
    CPPUNIT_ASSERT_EQUAL(Json::uintValue, val.type());

    val = parseToJson("-1");
    CPPUNIT_ASSERT_EQUAL(-1, val.asInt());
    CPPUNIT_ASSERT_EQUAL(Json::intValue, val.type());
    //CPPUNIT_ASSERT_EQUAL(-1, val.asUInt());

    val = parseToJson("-1.5");
    CPPUNIT_ASSERT_EQUAL((float)-1.5, val.asFloat());
    CPPUNIT_ASSERT_EQUAL(-1.5, val.asDouble());
    CPPUNIT_ASSERT_EQUAL(Json::realValue, val.type());

    val = parseToJson("truE");
    CPPUNIT_ASSERT_EQUAL(true, val.asBool());
    CPPUNIT_ASSERT_EQUAL(1, val.asInt());
    CPPUNIT_ASSERT_EQUAL(Json::booleanValue, val.type());

    val = parseToJson("false");
    CPPUNIT_ASSERT_EQUAL(false, val.asBool());
    CPPUNIT_ASSERT_EQUAL(0, val.asInt());
    CPPUNIT_ASSERT_EQUAL(Json::booleanValue, val.type());
}

void TestAgoJson::testStringToConverters() {
    Json::Value val("1");
    Json::UInt uintVal;
    Json::Int intVal;
    double doubleVal;

    CPPUNIT_ASSERT(stringToUInt(val, uintVal));
    CPPUNIT_ASSERT_EQUAL((unsigned int) 1, uintVal);

    CPPUNIT_ASSERT(stringToInt(val, intVal));
    CPPUNIT_ASSERT_EQUAL((int) 1, intVal);

    CPPUNIT_ASSERT(stringToDouble(val, doubleVal));
    CPPUNIT_ASSERT_EQUAL((double) 1.0, doubleVal);

    val = Json::Value(1);
    CPPUNIT_ASSERT(stringToUInt(val, uintVal));
    CPPUNIT_ASSERT_EQUAL((unsigned int) 1, uintVal);

    CPPUNIT_ASSERT(stringToInt(val, intVal));
    CPPUNIT_ASSERT_EQUAL((int) 1, intVal);

    CPPUNIT_ASSERT(stringToDouble(val, doubleVal));
    CPPUNIT_ASSERT_EQUAL((double) 1.0, doubleVal);



    val = Json::Value("-1");
    CPPUNIT_ASSERT(!stringToUInt(val, uintVal));

    CPPUNIT_ASSERT(stringToInt(val, intVal));
    CPPUNIT_ASSERT_EQUAL(-1, intVal);

    CPPUNIT_ASSERT(stringToDouble(val, doubleVal));
    CPPUNIT_ASSERT_EQUAL(-1.0, doubleVal);

    val = Json::Value(-1);
    CPPUNIT_ASSERT(!stringToUInt(val, uintVal));

    CPPUNIT_ASSERT(stringToInt(val, intVal));
    CPPUNIT_ASSERT_EQUAL(-1, intVal);

    CPPUNIT_ASSERT(stringToDouble(val, doubleVal));
    CPPUNIT_ASSERT_EQUAL(-1.0, doubleVal);


    val = Json::Value("1.5");
    CPPUNIT_ASSERT(!stringToUInt(val, uintVal));
    CPPUNIT_ASSERT(!stringToInt(val, intVal));

    CPPUNIT_ASSERT(stringToDouble(val, doubleVal));
    CPPUNIT_ASSERT_EQUAL((double) 1.5, doubleVal);

    val = Json::Value(1.5);
    CPPUNIT_ASSERT(!stringToUInt(val, uintVal));
    CPPUNIT_ASSERT(!stringToInt(val, intVal));

    CPPUNIT_ASSERT(stringToDouble(val, doubleVal));
    CPPUNIT_ASSERT_EQUAL((double) 1.5, doubleVal);


    val = Json::Value("str");
    CPPUNIT_ASSERT(!stringToUInt(val, uintVal));
    CPPUNIT_ASSERT(!stringToInt(val, intVal));
    CPPUNIT_ASSERT(!stringToDouble(val, doubleVal));
}

}; /* namespace agocontrol */
