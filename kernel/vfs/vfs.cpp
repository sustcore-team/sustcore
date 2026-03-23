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

#include <sus/raii.h>
#include <vfs/ops.h>
#include <vfs/vfs.h>

#include <cstring>

Result<void> VFS::register_fs(util::owner<IFsDriver *> &&driver) {
    const char *fs_name = driver->name();
    if (fs_table.contains(fs_name)) {
        return {unexpect, ErrCode::INVALID_PARAM};
    }
    fs_table.put(fs_name, driver);
    return {};
}

Result<void> VFS::unregister_fs(const char *fs_name) {
    if (!fs_table.contains(fs_name)) {
        return {unexpect, ErrCode::INVALID_PARAM};
    }
    // 遍历superblock表, 确认没有挂载该文件系统
    for (auto &mnt_entry : mount_table) {
        ISuperblock *sb = mnt_entry.second;
        if (strcmp(sb->fs()->name(), fs_name) == 0) {
            return {unexpect, ErrCode::BUSY};  // 该文件系统仍被挂载
        }
    }
    fs_table.remove(fs_name);
    if (auto driver = fs_table.get(fs_name)) {
        delete driver.value();
    }
    return {};
}

Result<void> VFS::mount(const char *fs_name, IBlockDevice *device,
                        const char *mountpoint, MountFlags flags,
                        const char *options) {
    if (mount_table.contains(mountpoint)) {
        return {unexpect, ErrCode::INVALID_PARAM};  // 已经被挂载
    }
    // 否则, 挂载该文件系统

    // 查找文件系统驱动
    auto lookup_result = fs_table.get(fs_name);
    if (!lookup_result.has_value()) {
        return {unexpect, ErrCode::INVALID_PARAM};  // 未注册该文件系统
    }
    // 获取文件系统驱动
    IFsDriver *fs_driver = lookup_result.value();

    // 挂载文件系统
    auto mount_result = fs_driver->mount(device, options);
    if (!mount_result.has_value()) {
        return {unexpect, mount_result.error()};
    }
    ISuperblock *sb = mount_result.value();

    // 记录挂载点

    // 修正挂载点路径
    // std::string mntpt = path_util::refine_path(mountpoint);
    // 添加挂载点记录
    mount_table.put(mountpoint, util::owner(sb));
    return {};
}

Result<void> VFS::umount(const char *mountpoint) {
    auto lookup_result = mount_table.get_entry(mountpoint);
    if (!lookup_result.has_value()) {
        return {unexpect, ErrCode::INVALID_PARAM};  // 未挂载该文件系统
    }

    ISuperblock *sb = lookup_result.value().second;

    // 确定没有打开的文件属于该挂载点
    // ...

    // 移除挂载
    Result<void> ret = sb->fs()->unmount(sb);
    if (!ret.has_value()) {
        return ret;
    }

    // 删除挂载点记录
    mount_table.remove(mountpoint);
    return {};
}