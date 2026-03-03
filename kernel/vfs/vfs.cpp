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

#include <cstring>

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
    fs_table.get(fs_name).if_present([](gsl::owner<IFsDriver> driver) { delete driver; });
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
    // std::string mntpt = path_util::refine_path(mountpoint);
    std::string mntpt = mountpoint;
    // 添加挂载点记录
    mount_table.put(mountpoint, sb);
    return FSErrCode::SUCCESS;
}

FSErrCode VFS::umount(const char *mountpoint) {
    auto _sb_opt = mount_table.get_entry(mountpoint);
    if (!_sb_opt.present()) {
        return FSErrCode::INVALID_PARAM;  // 未挂载该文件系统
    }

    ISuperblock *sb = _sb_opt.value().second;

    // 确定没有打开的文件属于该挂载点
    // ...

    // 移除挂载
    FSErrCode ret = sb->fs()->unmount(sb);
    if (ret != FSErrCode::SUCCESS) {
        return ret;
    }

    // 删除挂载点记录
    mount_table.remove(mountpoint);
    return FSErrCode::SUCCESS;
}