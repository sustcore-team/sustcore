/**
 * @file vfs.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief VFS
 * @version alpha-1.0.0
 * @date 2026-02-04
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <sus/mstring.h>
#include <sus/raii.h>
#include <vfs/ops.h>
#include <vfs/vfs.h>

#include <cassert>
#include <cstring>
#include <type_traits>

namespace path_util {
    template <typename T>
    const char *_cstr(const T &str) {
        if constexpr (std::is_convertible_v<T, const char *>) {
            return str;
        } else {
            return str.c_str();
        }
    }

    template <typename T>
    size_t _strlen(const T &str) {
        if constexpr (std::is_convertible_v<T, const char *>) {
            return strlen(str);
        } else {
            return str.length();
        }
    }

    /**
     * @brief 判断pfx是否是str的前缀
     *
     * @param str 字符串
     * @param pfx 前缀
     * @return true pfx是str的前缀
     * @return false pfx不是str的前缀
     */
    template <typename T>
    bool _prefix(const T &str, const T &pfx) {
        const char *str_c = _cstr(str);
        const char *pfx_c = _cstr(pfx);
        while (*pfx_c != 0) {
            if (*str_c != *pfx_c) {
                return false;
            }
            str_c++;
            pfx_c++;
        }
        return true;
    }

    /**
     * @brief 判断pfx是否是path的路径前缀
     *
     * @param path 规范化路径
     * @param pfx 规范化路径前缀
     * @return true pfx是path的路径前缀
     * @return false pfx不是path的路径前缀
     */
    template <typename T>
    bool prefix(const T &path, const T &pfx) {
        size_t pfx_len = _strlen(pfx);
        if (pfx_len == 1) {
            assert(pfx[0] == '/');
            return true;  // 根路径是所有路径的前缀
        }
        return _prefix(path, pfx) &&
               (path[pfx_len] == '/' || path[pfx_len] == '\0');
    }

    /**
     * @brief 规范化路径
     *
     * @param path 待规范化绝对路径(不包含'.'和'..')
     * @return util::string 规范化后的路径
     */
    template <typename T>
    util::string refine_path(const T &path) {
        util::string_builder sb;
        const char *path_c = _cstr(path);

        if (*path_c != '/') {
            sb.append('/');
        }

        while (*path_c != 0) {
            // 复制非多余的斜杠
            if (*path_c == '/') {
                // 跳过多余的斜杠
                while (*path_c == '/') {
                    path_c++;
                }
                // 只保留一个斜杠
                sb.append('/');
            } else {
                // 复制普通字符
                sb.append(*path_c);
                path_c++;
            }
        }
        if (sb[sb.length() - 1] == '/') {
            // 去掉结尾的斜杠
            sb.revert(1);
        }
        return sb.build();
    }

    /**
     * @brief 计算full_path相对于mntpt的相对路径
     *
     * @param full_path 完整路径
     * @param mntpt 挂载点路径
     * @return util::string 相对路径
     */
    template <typename T>
    util::string relative_path(const T &full_path, const T &mntpt) {
        if (!prefix(full_path, mntpt)) {
            return "";  // 不是前缀
        }
        util::string_builder sb;

        // 计算相对路径起始位置
        size_t mntpt_len     = _strlen(mntpt);
        const char *rel_path = _cstr(full_path) + mntpt_len;
        if (*rel_path == '\0') {
            return sb.build();
        }

        // 复制相对路径
        sb.append(rel_path);
        return sb.build();
    }

    /**
     * @brief 提取路径中的下一个路径项
     *
     * @param path 路径字符串
     * @return util::string 下一个路径项字符串
     */
    template <typename T>
    util::string entry(const T &path) {
        const char *path_c = _cstr(path);

        // 跳过开头的斜杠
        if (*path_c == '/') {
            path_c++;
        }

        util::string_builder sb;

        // 复制路径项
        while (*path_c != '/' && *path_c != '\0') {
            sb.append(*path_c);
            path_c++;
        }

        return sb.build();
    }

    template <typename T, typename Func>
    void foreach_entry(const T &path, Func func) {
        size_t offset   = 0;
        size_t path_len = _strlen(path);
        while (offset < path_len) {
            // 提取下一个路径项
            util::string entry_name = path_util::entry(_cstr(path) + offset);

            // 调用回调函数
            if (!func(entry_name))
                break;

            // 移动偏移量
            offset += entry_name.length() + 1;  // +1 跳过斜杠
        }
    }
};  // namespace path_util

VFS::VFS() {
    fd_cnt = 0;
}

VFS::~VFS() {}

FSErrCode VFS::register_fs(IFsDriver *driver) {
    const char *fs_name = driver->name();
    if (fs_table.contains(fs_name)) {
        return FSErrCode::INVALID_PARAM;
    }
    fs_table.put(fs_name, driver);
    return FSErrCode::SUCCESS;
}

FSErrCode VFS::unregister_fs(const char *fs_name) {
    if (!fs_table.contains(fs_name)) {
        return FSErrCode::INVALID_PARAM;
    }
    // 遍历superblock表, 确认没有挂载该文件系统
    for (auto &mnt_entry : mount_table) {
        ISuperblock *sb = mnt_entry.second;
        if (strcmp(sb->fs()->name(), fs_name) == 0) {
            return FSErrCode::BUSY;  // 该文件系统仍被挂载
        }
    }
    fs_table.get(fs_name).if_present([](IFsDriver *driver) { delete driver; });
    fs_table.remove(fs_name);
    return FSErrCode::SUCCESS;
}

FSErrCode VFS::mount(const char *fs_name, IBlockDevice *device,
                     const char *mountpoint, MountFlags flags,
                     const char *options) {
    if (mount_table.contains(mountpoint)) {
        return FSErrCode::INVALID_PARAM;  // 已经被挂载
    }
    // 否则, 挂载该文件系统

    // 查找文件系统驱动
    auto _fs_driver_opt = fs_table.get(fs_name);
    if (!_fs_driver_opt.present()) {
        return FSErrCode::INVALID_PARAM;  // 未注册该文件系统
    }
    // 获取文件系统驱动
    IFsDriver *fs_driver = _fs_driver_opt.value();

    // 挂载文件系统
    auto _sb_opt = fs_driver->mount(device, options);
    if (!_sb_opt.present()) {
        return _sb_opt.error();
    }
    ISuperblock *sb = _sb_opt.value();

    // 记录挂载点

    // 修正挂载点路径
    util::string mntpt = path_util::refine_path(mountpoint);
    // 添加挂载点记录
    mount_table.put(mntpt, sb);
    return FSErrCode::SUCCESS;
}

FSErrCode VFS::umount(const char *mountpoint) {
    auto _sb_opt = mount_table.get_entry(mountpoint);
    if (!_sb_opt.present()) {
        return FSErrCode::INVALID_PARAM;  // 未挂载该文件系统
    }

    ISuperblock *sb = _sb_opt.value().second;

    // 遍历 Open File List, 确认没有文件正在使用该挂载点
    for (auto &file_entry : open_file_list) {
        VFile *vfile = file_entry.second;
        if (vfile->sb == sb) {
            return FSErrCode::BUSY;  // 有文件仍在使用该挂载点
        }
    }

    // 移除挂载
    FSErrCode ret = sb->fs()->unmount(sb);
    if (ret != FSErrCode::SUCCESS) {
        return ret;
    }

    // 删除挂载点记录
    mount_table.remove(mountpoint);
    return FSErrCode::SUCCESS;
}

FSOptional<VFile *> VFS::_open(const char *path, int flags) {
    // TODO: check path validity

    // 选择 path 中从 / 开始尽可能长的路径前缀作为挂载点
    // 修正路径
    util::string refined_path = path_util::refine_path(path);

    // 遍历挂载点表, 寻找最长匹配前缀
    ISuperblock *target_sb = nullptr;
    util::string target_mntpt;
    size_t target_mntpt_len = 0;
    for (auto &mnt_entry : mount_table) {
        util::string mntpt = mnt_entry.first;
        // path_util::prefix 保证 mntpt 是 path 的前缀, 且是路径前缀
        if (path_util::prefix(refined_path, mntpt)) {
            size_t mntpt_len = mntpt.length();
            if (mntpt_len > target_mntpt_len) {
                target_mntpt     = mntpt;
                target_sb        = mnt_entry.second;
                target_mntpt_len = mntpt_len;
            }
        }
    }

    if (target_sb == nullptr) {
        return FSErrCode::INVALID_PARAM;  // 未找到匹配的挂载点
    }

    util::string rel_path =
        path_util::relative_path(refined_path, target_mntpt);

    // 通过超级块获得根目录项
    auto _root_inode_opt = target_sb->root();
    if (!_root_inode_opt.present()) {
        return _root_inode_opt.error();  // 无法获得根目录项
    }

    IINode *curinode  = _root_inode_opt.value();
    FSErrCode errflag = FSErrCode::SUCCESS;
    // 解析相对路径, 查找文件
    path_util::foreach_entry(
        rel_path, [&curinode, &errflag](const util::string &entry_name) {
            auto _dir_opt = curinode->as_directory();
            if (!_dir_opt.present()) {
                errflag = _dir_opt.error();  // 不是目录
                return false;
            }
            IDirectory *dir = _dir_opt.value();

            // 获得对应的目录项
            auto _dentry_opt = dir->lookup(entry_name.c_str());
            if (!_dentry_opt.present()) {
                errflag = _dentry_opt.error();  // 未找到对应目录项
                return false;
            }
            IDentry *dentry = _dentry_opt.value();

            // 获得inode
            auto _inode_opt = dentry->inode();
            if (!_inode_opt.present()) {
                errflag = _inode_opt.error();  // 无法获得i节点
                return false;
            }
            curinode = _inode_opt.value();
            return true;
        });

    if (errflag != FSErrCode::SUCCESS) {
        return errflag;
    }

    // 当前inode即为目标文件的inode
    // 将inode作为文件打开
    auto _file_opt = curinode->as_file();
    if (!_file_opt.present()) {
        return _file_opt.error();  // 不是文件
    }

    IFile *ifile = _file_opt.value();
    VFile *vfile = new VFile{.fd    = next_fd(),
                             .fs    = target_sb->fs(),
                             .sb    = target_sb,
                             .ifile = ifile};
    open_file_list.put(vfile->fd, vfile);
    return vfile;
}

FSErrCode VFS::_close(VFile *vfile) {
    if (!open_file_list.contains(vfile->fd)) {
        return FSErrCode::INVALID_PARAM;  // 文件未打开
    }
    open_file_list.remove(vfile->fd);
    // delete vfile->ifile;
    delete vfile;
    return FSErrCode::SUCCESS;
}

FSOptional<size_t> VFS::_read(VFile *file, void *buf, size_t len)
{
    if (!open_file_list.contains(file->fd)) {
        return FSErrCode::INVALID_PARAM;  // 文件未打开
    }
    return file->ifile->read(buf, len);
}