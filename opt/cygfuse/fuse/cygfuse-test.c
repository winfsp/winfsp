#include <fuse.h>

int main()
{
    int (*notify)(struct fuse *, const char *, uint32_t) = fuse_notify;
    int (*invalidate)(struct fuse *, const char *) = fuse_invalidate;

    return !(FUSE_VERSION == fuse_version()) || 0 == notify || 0 == invalidate;
}
