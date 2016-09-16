#include <tlib/testsuite.h>

#include "winfsp-tests.h"

int NtfsTests = 0;
int WinFspDiskTests = 1;
int WinFspNetTests = 1;

int main(int argc, char *argv[])
{
    TESTSUITE(fuse_opt_tests);
    TESTSUITE(posix_tests);
    TESTSUITE(eventlog_tests);
    TESTSUITE(path_tests);
    TESTSUITE(mount_tests);
    TESTSUITE(timeout_tests);
    TESTSUITE(memfs_tests);
    TESTSUITE(create_tests);
    TESTSUITE(info_tests);
    TESTSUITE(security_tests);
    TESTSUITE(rdwr_tests);
    TESTSUITE(flush_tests);
    TESTSUITE(lock_tests);
    TESTSUITE(dirctl_tests);
    TESTSUITE(reparse_tests);

    tlib_run_tests(argc, argv);
    return 0;
}
