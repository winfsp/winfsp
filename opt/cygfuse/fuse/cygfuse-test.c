#include <fuse.h>

int main()
{
    return !(FUSE_VERSION == fuse_version());
}
