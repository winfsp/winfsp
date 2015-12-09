#include <tlib/testsuite.h>

int WinFspDiskTests = 1;
int WinFspNetTests = 0;

int main(int argc, char *argv[])
{
    TESTSUITE(mount_tests);
    tlib_run_tests(argc, argv);
    return 0;
}
