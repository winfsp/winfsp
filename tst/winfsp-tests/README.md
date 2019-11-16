## WINFSP-TESTS

Winfsp-tests is a file system test suite that is used to test WinFsp and the file systems that ship with it. It is not intended for use by end users. If you have downloaded winfsp-tests, you likely wanted to download the WinFsp installer instead (i.e. the file that is named `winfsp-VERSION.msi`).

**WINFSP-TESTS IS UNSUPPORTED SOFTWARE. IT MAY CRASH OR LOCK UP YOUR COMPUTER, CREATE ZOMBIE/UNKILLABLE PROCESSES OR CORRUPT YOUR DATA. DO NOT USE UNLESS YOU KNOW WHAT YOU ARE DOING. DO NOT POST BUGS/ISSUES/QUESTIONS AGAINST WINFSP-TESTS UNLESS YOU ALSO POST THE FIX. YOU HAVE BEEN WARNED!**

## USAGE

Winfsp-tests has two different test modes: a default internal mode in which it runs its tests against an embedded copy of MEMFS and an external mode in which it runs its tests against the file system in the current directory.

Unless you are doing WinFsp development there should never be a need to run winfsp-tests in the default internal mode. However the external mode can be useful to test third party file systems that do not ship with WinFsp.

To run winfsp-tests in external mode, you must use the `--external` command line option. I also recommend using the `--resilient` command line option, to have winfsp-tests improve test flakiness by retrying failed operations.

```
> winfsp-tests-x64 --external --resilient
create_test............................ OK 0.00s
create_fileattr_test................... OK 0.00s
...
--- COMPLETE ---
```

Specific tests to be run may be specified. For example, to run all tests that start with `rename`:

```
> winfsp-tests-x64 --external --resilient rename*
rename_test............................ OK 0.01s
rename_open_test....................... OK 0.00s
...
--- COMPLETE ---
```

To exclude a test or tests use the `-` prefix. For example, to run all `create` tests, except the `create_fileattr_test`:

```
> winfsp-tests-x64 --external --resilient create* -create_fileattr_test
create_test............................ OK 0.05s
create_readonlydir_test................ OK 0.01s
...
--- COMPLETE ---
```

By default only regular tests are run. To include optional or long running tests use the `+` prefix. For example, to run all tests use `+*`; to run oplock tests use `+oplock*`:

```
> winfsp-tests-x64 --external --resilient +oplock*
oplock_level1_test..................... OK 1.26s
oplock_level2_test..................... OK 2.46s
...
--- COMPLETE ---
```

To list tests without running them use the `--list` option:

```
> winfsp-tests-x64 --external --resilient --list +oplock*
oplock_level1_test
oplock_level2_test
...
```

If a test fails the test suite stops immediately with an assertion failure. There is no additional explanation of the problem and you have to study the winfsp-tests source code to understand the failure and determine a fix for your file system. Additionally there may be garbage files remaining in the file system as winfsp-tests does not cleanup after itself.

**NOTE**: Some tests require Administrator privileges in order to run.