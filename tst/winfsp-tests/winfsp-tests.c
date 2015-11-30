#include <tlib/testsuite.h>

int main(int argc, char *argv[])
{
    TESTSUITE(mount_tests);
    tlib_run_tests(argc, argv);
    return 0;
}
