#include <winfsp/winfsp.h>
#include <tlib/testsuite.h>

void eventlog_test(void)
{
    /* this is not a real test! */

    FspEventLog(EVENTLOG_INFORMATION_TYPE, L"EventLog %s message", L"informational");
    FspEventLog(EVENTLOG_WARNING_TYPE, L"EventLog %s message", L"warning");
    FspEventLog(EVENTLOG_ERROR_TYPE, L"EventLog %s message", L"error");
}

void eventlog_tests(void)
{
    TEST_OPT(eventlog_test);
}
