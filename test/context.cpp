#include "Forte.h"
#define BOOST_TEST_MODULE "Context Unit Tests"
#include <boost/test/unit_test.hpp>


struct ContextFixture {
    Forte::Context c;
};

class TestClass {
public:
    TestClass() { ++mCount; }
    virtual ~TestClass() { --mCount; }
    int getCount(void) { return mCount; }
    static int mCount;
};
int TestClass::mCount = 0;

BOOST_FIXTURE_TEST_SUITE( context, ContextFixture );
BOOST_AUTO_TEST_CASE ( context_test1 )
{
    CLogManager logMgr;
    logMgr.BeginLogging();
    logMgr.SetGlobalLogMask(HLOG_ALL);

    // verify invalid keys throws
    BOOST_CHECK_THROW(c.GetRef<CLogManager>("forte.logManager"), EInvalidKey);
    
    c.Set("forte.config", new CServiceConfig());

    BOOST_CHECK_THROW(c.GetRef<CServiceConfig>("forte.config.fail"), EInvalidKey);

    // verify type mismatch throws
    BOOST_CHECK_THROW(c.GetRef<CLogManager>("forte.config"), ETypeMismatch);

    {
        CServiceConfig &cfg(c.GetRef<CServiceConfig>("forte.config"));
        cfg.Set("key", "value");
    }

    // verify objects are destroyed and reference counts work correctly
    BOOST_CHECK(TestClass::mCount == 0);
    c.Set("testobject", new TestClass());
    BOOST_CHECK(TestClass::mCount == 1);
    c.Remove("testobject");
    BOOST_CHECK(TestClass::mCount == 0);
    BOOST_CHECK_THROW(c.GetRef<TestClass>("testobject"), EInvalidKey);

    c.Set("testobject", new TestClass());
    BOOST_CHECK(TestClass::mCount == 1);
    {
        Forte::Context::ObjectPtr oPtr(c.Get("testobject"));
        TestClass &tc(CAST(TestClass, oPtr));
        c.Remove("testobject");
        BOOST_CHECK( tc.getCount() == 1 );
    }
    BOOST_CHECK(TestClass::mCount == 0);
}
BOOST_AUTO_TEST_SUITE_END();

