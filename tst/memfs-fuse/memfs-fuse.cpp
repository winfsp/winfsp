/**
 * @file memfs-fuse.c
 *
 * @copyright 2015-2021 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this software
 * in accordance with the commercial license agreement provided in
 * conjunction with the software.  The terms and conditions of any such
 * commercial license agreement shall govern, supersede, and render
 * ineffective any application of the GPLv3 license to this software,
 * notwithstanding of any reference thereto in the software or
 * associated repository.
 */

#include <cerrno>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <fuse.h>
#include "compat.h"

class memfs
{
public:
    memfs() : _ino(1), _root(std::make_shared<node_t>(_ino, S_IFDIR | 00777, 0, 0))
    {
    }

    int main(int argc, char *argv[])
    {
        static fuse_operations ops =
        {
            getattr,
            0, // getdir
            readlink,
            mknod,
            mkdir,
            unlink,
            rmdir,
            symlink,
            rename,
            link,
            chmod,
            chown,
            truncate,
            0, // utime
            open,
            read,
            write,
            statfs,
            flush,
            release,
            0, // fsync
            setxattr,
            getxattr,
            listxattr,
            removexattr,
            opendir,
            readdir,
            releasedir,
            0, // fsyncdir
            init,
            0, // destroy
            0, // access
            0, // create
            ftruncate,
            fgetattr,
            0, // lock
            utimens,
            0, // bmap
            0, // flag_nullpath_ok
            0, // flag_nopath
            0, // flag_utime_omit_ok
            0, // flag_reserved
            0, // ioctl
            0, // poll
            0, // write_buf
            0, // read_buf
            0, // flock
            0, // fallocate
            0, // reserved00
            0, // reserved01
            0, // reserved02
            0, // statfs_x
            0, // setvolname
            0, // exchange
            0, // getxtimes
            0, // setbkuptime
            0, // setchgtime
            setcrtime,
#if defined(FSP_FUSE_USE_STAT_EX)
            chflags,
#else
            0, // chflags
#endif
            0, // setattr_x
            0, // fsetattr_x
        };
        return fuse_main(argc, argv, &ops, this);
    }

private:
    struct node_t
    {
        node_t(fuse_ino_t ino, fuse_mode_t mode, fuse_uid_t uid, fuse_gid_t gid, fuse_dev_t dev = 0)
            : stat()
        {
            stat.st_ino = ino;
            stat.st_mode = mode;
            stat.st_nlink = 1;
            stat.st_uid = uid;
            stat.st_gid = gid;
            stat.st_rdev = dev;
            stat.st_atim = stat.st_mtim = stat.st_ctim = stat.st_birthtim = now();
        }

        void resize(size_t size, bool capacity)
        {
            if (capacity)
            {
                const size_t unit = 64 * 1024;
                size_t newcap = (size + unit - 1) / unit * unit;
                size_t oldcap = data.capacity();
                if (newcap > oldcap)
                    data.reserve(newcap);
                else if (newcap < oldcap)
                {
                    data.resize(newcap);
                    data.shrink_to_fit();
                }
            }
            data.resize(size);
            stat.st_size = size;
        }

        struct fuse_stat stat;
        std::vector<uint8_t> data;
        std::unordered_map<std::string, std::shared_ptr<node_t>> childmap;
        std::unordered_map<std::string, std::vector<uint8_t>> xattrmap;
    };

    static fuse_timespec now()
    {
        using namespace std::chrono;
        auto now = system_clock::now();
        auto sec = floor<seconds>(now);
        auto nsec = floor<nanoseconds>(now) - floor<nanoseconds>(sec);
        return fuse_timespec
        {
            static_cast<decltype(fuse_timespec::tv_sec)>(sec.time_since_epoch().count()),
                /* std::chrono epoch is UNIX epoch in C++20 */
            static_cast<decltype(fuse_timespec::tv_nsec)>(nsec.count()),
        };
    }

    static memfs *getself()
    {
        return static_cast<memfs *>(fuse_get_context()->private_data);
    }

    static int getattr(const char *path, struct fuse_stat *stbuf)
    {
        return fgetattr(path, stbuf, nullptr);
    }

    static int fgetattr(const char *path, struct fuse_stat *stbuf, struct fuse_file_info *fi)
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        auto node = self->get_node(path, fi);
        if (!node)
            return -ENOENT;
        *stbuf = node->stat;
        return 0;
    }

    static int readlink(const char *path, char *buf, size_t size)
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        auto node = self->get_node(path);
        if (!node)
            return -ENOENT;
        if (S_IFLNK != (node->stat.st_mode & S_IFMT))
            return EINVAL;
        size = (std::min)(size - 1, node->data.size());
        std::memcpy(buf, node->data.data(), size);
        buf[size] = '\0';
        return 0;
    }

    static int mknod(const char *path, fuse_mode_t mode, fuse_dev_t dev)
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        return self->make_node(path, mode, dev);
    }

    static int mkdir(const char *path, fuse_mode_t mode)
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        return self->make_node(path, S_IFDIR | (mode & 07777), 0);
    }

    static int unlink(const char *path)
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        return self->remove_node(path, false);
    }

    static int rmdir(const char *path)
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        return self->remove_node(path, true);
    }

    static int symlink(const char *dstpath, const char *srcpath)
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        return self->make_node(srcpath, S_IFLNK | 00777, 0, dstpath);
    }

    static int rename(const char *oldpath, const char *newpath)
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        auto oldlookup = self->lookup_node(oldpath);
        auto oldprnt = std::get<0>(oldlookup);
        auto oldname = std::get<1>(oldlookup);
        auto oldnode = std::get<2>(oldlookup);
        if (!oldnode)
            return -ENOENT;
        auto newlookup = self->lookup_node(newpath);
        auto newprnt = std::get<0>(newlookup);
        auto newname = std::get<1>(newlookup);
        auto newnode = std::get<2>(newlookup);
        if (!newprnt)
            return -ENOENT;
        if (newname.empty())
            // guard against directory loop creation
            return -EINVAL;
        if (oldprnt == newprnt && oldname == newname)
            return 0;
        if (newnode)
        {
            if (int errc = self->remove_node(newpath, S_IFDIR == (oldnode->stat.st_mode & S_IFMT)))
                return errc;
        }
        oldprnt->childmap.erase(oldname);
        newprnt->childmap[newname] = oldnode;
        return 0;
    }

    static int link(const char *oldpath, const char *newpath)
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        auto oldlookup = self->lookup_node(oldpath);
        auto oldnode = std::get<2>(oldlookup);
        if (!oldnode)
            return -ENOENT;
        auto newlookup = self->lookup_node(newpath);
        auto newprnt = std::get<0>(newlookup);
        auto newname = std::get<1>(newlookup);
        auto newnode = std::get<2>(newlookup);
        if (!newprnt)
            return -ENOENT;
        if (newnode)
            return -EEXIST;
        oldnode->stat.st_nlink++;
        newprnt->childmap[newname] = oldnode;
        oldnode->stat.st_ctim = newprnt->stat.st_ctim = newprnt->stat.st_mtim = now();
        return 0;
    }

    static int chmod(const char *path, fuse_mode_t mode)
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        auto node = self->get_node(path);
        if (!node)
            return -ENOENT;
        node->stat.st_mode = (node->stat.st_mode & S_IFMT) | (mode & 07777);
        node->stat.st_ctim = now();
        return 0;
    }

    static int chown(const char *path, fuse_uid_t uid, fuse_gid_t gid)
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        auto node = self->get_node(path);
        if (!node)
            return -ENOENT;
        if (-1 != uid)
            node->stat.st_uid = uid;
        if (-1 != gid)
            node->stat.st_gid = gid;
        node->stat.st_ctim = now();
        return 0;
    }

    static int truncate(const char *path, fuse_off_t size)
    {
        return ftruncate(path, size, nullptr);
    }

    static int ftruncate(const char *path, fuse_off_t size,
        struct fuse_file_info *fi)
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        auto node = self->get_node(path, fi);
        if (!node)
            return -ENOENT;
        if (SIZE_MAX < size)
            return -EFBIG;
        node->resize(static_cast<size_t>(size), true);
        node->stat.st_ctim = node->stat.st_mtim = now();
#if defined(FSP_FUSE_USE_STAT_EX)
        node->stat.st_flags |= UF_ARCHIVE;
#endif
        return 0;
    }

    static int open(const char *path, struct fuse_file_info *fi)
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        return self->open_node(path, false, fi);
    }

    static int read(const char *path, char *buf, size_t size, fuse_off_t off,
        struct fuse_file_info *fi)
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        auto node = self->get_node(path, fi);
        if (!node)
            return -ENOENT;
        fuse_off_t endoff = (std::min)(
            off + static_cast<fuse_off_t>(size), static_cast<fuse_off_t>(node->data.size()));
        if (off > endoff)
            return 0;
        std::memcpy(buf, node->data.data() + off, static_cast<int>(endoff - off));
        node->stat.st_atim = now();
        return static_cast<int>(endoff - off);
    }

    static int write(const char *path, const char *buf, size_t size, fuse_off_t off,
        struct fuse_file_info *fi)
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        auto node = self->get_node(path, fi);
        if (!node)
            return -ENOENT;
        fuse_off_t endoff = off + static_cast<fuse_off_t>(size);
        if (SIZE_MAX < endoff)
            return -EFBIG;
        if (node->data.size() < endoff)
            node->resize(static_cast<size_t>(endoff), true);
        std::memcpy(node->data.data() + off, buf, static_cast<int>(endoff - off));
        node->stat.st_ctim = node->stat.st_mtim = now();
#if defined(FSP_FUSE_USE_STAT_EX)
        node->stat.st_flags |= UF_ARCHIVE;
#endif
        return static_cast<int>(endoff - off);
    }

    static int statfs(const char *path, struct fuse_statvfs *stbuf)
    {
        std::memset(stbuf, 0, sizeof *stbuf);
        return 0;
    }

    static int flush(const char *path, struct fuse_file_info *fi)
    {
        return -ENOSYS;
    }

    static int release(const char *path, struct fuse_file_info *fi)
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        return self->close_node(fi);
    }

    static int setxattr(const char *path, const char *name0, const char *value, size_t size,
        int flags)
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        auto node = self->get_node(path);
        if (!node)
            return -ENOENT;
        if (0 == std::strcmp("com.apple.ResourceFork", name0))
            return -ENOTSUP;
        std::string name = name0;
        if (XATTR_CREATE == flags)
        {
            if (node->xattrmap.end() != node->xattrmap.find(name))
                return -EEXIST;
        }
        else if (XATTR_REPLACE == flags)
        {
            if (node->xattrmap.end() == node->xattrmap.find(name))
                return -ENOATTR;
        }
        node->xattrmap[name].assign(value, value + size);
        return 0;
    }

    static int getxattr(const char *path, const char *name0, char *value, size_t size)
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        auto node = self->get_node(path);
        if (!node)
            return -ENOENT;
        if (0 == std::strcmp("com.apple.ResourceFork", name0))
            return -ENOTSUP;
        std::string name = name0;
        auto iter = node->xattrmap.find(name);
        if (node->xattrmap.end() == iter)
            return -ENOATTR;
        if (0 != size)
        {
            if (iter->second.size() > size)
                return -ERANGE;
            std::memcpy(value, iter->second.data(), iter->second.size());
        }
        return static_cast<int>(iter->second.size());
    }

    static int listxattr(const char *path, char *namebuf, size_t size)
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        auto node = self->get_node(path);
        if (!node)
            return -ENOENT;
        size_t copysize = 0;
        for (auto elem : node->xattrmap)
        {
            size_t namesize = elem.first.size() + 1;
            if (0 != size)
            {
                if (copysize + namesize > size)
                    return -ERANGE;
                std::memcpy(namebuf + copysize, elem.first.c_str(), namesize);
                copysize += namesize;
            }
        }
        return static_cast<int>(copysize);
    }

    static int removexattr(const char *path, const char *name0)
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        auto node = self->get_node(path);
        if (!node)
            return -ENOENT;
        if (0 == std::strcmp("com.apple.ResourceFork", name0))
            return -ENOTSUP;
        std::string name = name0;
        return node->xattrmap.erase(name) ? 0 : -ENOATTR;
    }

    static int opendir(const char *path, struct fuse_file_info *fi)
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        return self->open_node(path, true, fi);
    }

    static int readdir(const char *path, void *buf, fuse_fill_dir_t filler, fuse_off_t off,
        struct fuse_file_info *fi)
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        auto node = self->get_node(path, fi);
        if (!node)
            return -ENOENT;
        filler(buf, ".", &node->stat, 0);
        filler(buf, "..", nullptr, 0);
        for (auto elem : node->childmap)
            if (0 != filler(buf, elem.first.c_str(), &elem.second->stat, 0))
                break;
        return 0;
    }

    static int releasedir(const char *path, struct fuse_file_info *fi)
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        return self->close_node(fi);
    }

    static void *init(struct fuse_conn_info *conn)
    {
#if defined(FSP_FUSE_CAP_READDIR_PLUS)
        conn->want |= (conn->capable & FSP_FUSE_CAP_READDIR_PLUS);
#endif

#if defined(FSP_FUSE_USE_STAT_EX) && defined(FSP_FUSE_CAP_STAT_EX)
        conn->want |= (conn->capable & FSP_FUSE_CAP_STAT_EX);
#endif

        return getself();
    }

    static int utimens(const char *path, const struct fuse_timespec tmsp[2])
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        auto node = self->get_node(path);
        if (!node)
            return -ENOENT;
        if (tmsp)
        {
            node->stat.st_ctim = now();
            node->stat.st_atim = tmsp[0];
            node->stat.st_mtim = tmsp[1];
        }
        else
            node->stat.st_ctim = node->stat.st_atim = node->stat.st_mtim = now();
        return 0;
    }

    static int setcrtime(const char *path, const struct fuse_timespec *tmsp)
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        auto node = self->get_node(path);
        if (!node)
            return -ENOENT;
        if (tmsp)
        {
            node->stat.st_ctim = now();
            node->stat.st_birthtim = tmsp[0];
        }
        else
            node->stat.st_ctim = node->stat.st_birthtim = now();
        return 0;
    }

#if defined(FSP_FUSE_USE_STAT_EX)
    static int chflags(const char *path, uint32_t flags)
    {
        auto self = getself();
        std::lock_guard<std::mutex> lock(self->_mutex);
        auto node = self->get_node(path);
        if (!node)
            return -ENOENT;
        node->stat.st_flags = flags;
        node->stat.st_ctim = now();
        return 0;
    }
#endif

    std::tuple<std::shared_ptr<node_t>, std::string, std::shared_ptr<node_t>>
        lookup_node(const char *path, node_t *ancestor = nullptr)
    {
        auto prnt = _root;
        std::string name;
        auto node = prnt;
        for (const char *part = path, *p; *part; part = p + !!(*p))
        {
            for (p = part; *p && '/' != *p; p++)
                ;
            if (part == p)
                continue;
            prnt = node;
            if (!node)
                break;
            name.assign(part, p);
            auto iter = node->childmap.find(name);
            node = node->childmap.end() != iter ? iter->second : nullptr;
            if (ancestor && node.get() == ancestor)
            {
                name.assign(""); // special case loop condition
                break;
            }
        }
        return std::make_tuple(prnt, name, node);
    }

    int make_node(const char *path, fuse_mode_t mode, fuse_dev_t dev, const char *data = nullptr)
    {
        auto lookup = lookup_node(path);
        auto prnt = std::get<0>(lookup);
        auto name = std::get<1>(lookup);
        auto node = std::get<2>(lookup);
        if (!prnt)
            return -ENOENT;
        if (node)
            return -EEXIST;
        fuse_context *context = fuse_get_context();
        node = std::make_shared<node_t>(++_ino, mode, context->uid, context->gid, dev);
#if defined(FSP_FUSE_USE_STAT_EX)
        if (S_IFDIR != (mode & S_IFMT))
            node->stat.st_flags |= UF_ARCHIVE;
#endif
        if (data)
        {
            node->resize(std::strlen(data), false);
            std::memcpy(node->data.data(), data, node->data.size());
        }
        prnt->childmap[name] = node;
        prnt->stat.st_ctim = prnt->stat.st_mtim = node->stat.st_ctim;
        return 0;
    }

    int remove_node(const char *path, bool dir)
    {
        auto lookup = lookup_node(path);
        auto prnt = std::get<0>(lookup);
        auto name = std::get<1>(lookup);
        auto node = std::get<2>(lookup);
        if (!node)
            return -ENOENT;
        if (!dir && S_IFDIR == (node->stat.st_mode & S_IFMT))
            return -EISDIR;
        if (dir && S_IFDIR != (node->stat.st_mode & S_IFMT))
            return -ENOTDIR;
        if (0 < node->childmap.size())
            return -ENOTEMPTY;
        node->stat.st_nlink--;
        prnt->childmap.erase(name);
        node->stat.st_ctim = prnt->stat.st_ctim = prnt->stat.st_mtim = now();
        return 0;
    }

    int open_node(const char *path, bool dir, struct fuse_file_info *fi)
    {
        auto node = std::get<2>(lookup_node(path));
        if (!node)
            return -ENOENT;
        if (!dir && S_IFDIR == (node->stat.st_mode & S_IFMT))
            return -EISDIR;
        if (dir && S_IFDIR != (node->stat.st_mode & S_IFMT))
            return -ENOTDIR;
        // A file descriptor is a raw pointer to a shared_ptr.
        // This has the effect of incrementing the shared_ptr
        // refcount, thus keeping an open node around even
        // if the node is unlinked.
        fi->fh = (uint64_t)(uintptr_t)new std::shared_ptr<node_t>(node);
        return 0;
    }

    int close_node(struct fuse_file_info *fi)
    {
        delete (std::shared_ptr<node_t> *)(uintptr_t)fi->fh;
        return 0;
    }

    std::shared_ptr<node_t> get_node(const char *path, struct fuse_file_info *fi = nullptr)
    {
        if (!fi)
            return std::get<2>(lookup_node(path));
        else
            return *(std::shared_ptr<node_t> *)(uintptr_t)fi->fh;
    }

private:
    std::mutex _mutex;
    fuse_ino_t _ino;
    std::shared_ptr<node_t> _root;
};

int main(int argc, char *argv[])
{
    return memfs().main(argc, argv);
}
