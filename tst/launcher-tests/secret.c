/*
 * Compile:
 *     - cl secret.c
 *
 * Register:
 *     - secret.reg (fix Executable path first)
 *
 * Run:
 *     - launchctl-x64 startWithSecret secret 1 nopass
 *     - launchctl-x64 startWithSecret secret 1 foobar
 */

#include <stdio.h>

int main()
{
    char pass[256];

    gets(pass);
    if (0 == strcmp("foobar", pass))
    {
        puts("OK");
        fprintf(stderr, "OK secret=\"%s\"\n", pass);
    }
    else
    {
        puts("KO");
        fprintf(stderr, "KO secret=\"%s\"\n", pass);
    }

    return 0;
}
