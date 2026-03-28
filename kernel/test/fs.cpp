/**
 * @file fs.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 文件系统测试
 * @version alpha-1.0.0
 * @date 2026-03-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <fs/tarfs.h>
#include <kio.h>
#include <mem/alloc.h>
#include <symbols.h>
#include <test/fs.h>
#include <vfs/vfs.h>

#include <cstdint>

namespace test::fs {

    static RamDiskDevice* make_initrd();
    class CaseMountOpenRead : public TestCase {
    public:
        CaseMountOpenRead() : TestCase("VFS 挂载 initrd 并读取 license") {}

        void _run(void* env) const noexcept override {
            auto* vfs = static_cast<VFS*>(env);
            tassert(vfs != nullptr, "测试环境应提供已初始化的 VFS 实例");

            RamDiskDevice* initrd = make_initrd();
            tassert(initrd != nullptr, "应能成功创建 initrd RamDisk 设备");

            action("挂载 tarfs 到根目录 /");
            auto mount_res =
                vfs->mount("tarfs", initrd, "/", MountFlags::NONE, nullptr);
            tassert(mount_res.has_value(), "应能将 tarfs 挂载到根目录 /");

            action("打开 initrd 中的 /license 文件");
            auto open_res = vfs->open("/license");
            tassert(open_res.has_value(), "应能成功打开 /license 文件");

            VFileAccessor* acc = open_res.value();
            VINode* vind       = acc->obj();
            auto file_res      = vind->inode()->as_file();
            tassert(file_res.has_value(), "VINode 应能被视为 IFile");

            IFile* file            = file_res.value();
            auto file_size_res     = file->size();
            tassert(file_size_res.has_value() && file_size_res.value() > 0,
                    "license 文件大小应大于 0");

            uint8_t head[32] = {0};
            auto read_res    = file->read(0, head, sizeof(head));
            tassert(read_res.has_value() && read_res.value() > 0,
                    "应能成功读取 license 文件前缀");

            bool non_zero = false;
            for (size_t i = 0; i < read_res.value(); ++i) {
                non_zero |= head[i] != 0;
            }
            tassert(non_zero, "读取到的文件内容不应全为 0");

            auto close_res = vfs->close(open_res.value());
            tassert(close_res.has_value(), "应能成功关闭 /license 访问器");

            auto tidy_res = vfs->tidy_up();
            tassert(tidy_res.has_value(), "tidy_up 应成功整理 dentry/inode 缓存");

            auto umount_res = vfs->umount("/");
            tassert(umount_res.has_value(), "卸载根目录 / 应成功");

            delete initrd;
        }
    };

    class CaseMountBusyUmount : public TestCase {
    public:
        CaseMountBusyUmount() : TestCase("VFS 忙状态阻止卸载") {}

        void _run(void* env) const noexcept override {
            auto* vfs = static_cast<VFS*>(env);
            tassert(vfs != nullptr, "测试环境应提供已初始化的 VFS 实例");

            RamDiskDevice* initrd = make_initrd();
            tassert(initrd != nullptr, "应能成功创建 initrd RamDisk 设备");

            auto mount_res =
                vfs->mount("tarfs", initrd, "/", MountFlags::NONE, nullptr);
            tassert(mount_res.has_value(), "应能将 tarfs 挂载到根目录 /");

            auto open_res = vfs->open("/license");
            tassert(open_res.has_value(), "第一次打开 /license 应成功");

            auto open_res2 = vfs->open("/license");
            tassert(open_res2.has_value(), "第二次打开 /license 应成功");

            action("文件仍在打开时卸载, 应返回 BUSY");
            auto busy_umount = vfs->umount("/");
            tassert(!busy_umount.has_value() &&
                        busy_umount.error() == ErrCode::BUSY,
                    "有文件打开时卸载应被拒绝 (BUSY)");

            auto close_res2 = vfs->close(open_res2.value());
            tassert(close_res2.has_value(), "应能成功关闭第二个 /license 访问器");

            action("仍有一个访问器打开时再次卸载, 仍应 BUSY");
            busy_umount = vfs->umount("/");
            tassert(!busy_umount.has_value() &&
                        busy_umount.error() == ErrCode::BUSY,
                    "仍有文件打开时卸载应被拒绝 (BUSY)");

            auto close_res = vfs->close(open_res.value());
            tassert(close_res.has_value(), "应能成功关闭第一个 /license 访问器");

            auto tidy_res = vfs->tidy_up();
            tassert(tidy_res.has_value(), "tidy_up 应成功整理 dentry/inode 缓存");

            auto umount_res = vfs->umount("/");
            tassert(umount_res.has_value(), "释放所有访问器后卸载应成功");

            delete initrd;
        }
    };

    class CaseOpenMissingFile : public TestCase {
    public:
        CaseOpenMissingFile() : TestCase("VFS 打开不存在文件应失败") {}

        void _run(void* env) const noexcept override {
            auto* vfs = static_cast<VFS*>(env);
            tassert(vfs != nullptr, "测试环境应提供已初始化的 VFS 实例");

            RamDiskDevice* initrd = make_initrd();
            tassert(initrd != nullptr, "应能成功创建 initrd RamDisk 设备");

            auto mount_res =
                vfs->mount("tarfs", initrd, "/", MountFlags::NONE, nullptr);
            tassert(mount_res.has_value(), "应能将 tarfs 挂载到根目录 /");

            auto missing_open = vfs->open("/this_file_should_not_exist");
            tassert(!missing_open.has_value(), "打开不存在文件应失败");

            auto umount_res = vfs->umount("/");
            tassert(umount_res.has_value(), "卸载根目录 / 应成功");

            delete initrd;
        }
    };

    class CaseMountParamValidation : public TestCase {
    public:
        CaseMountParamValidation() : TestCase("VFS 挂载参数与重复挂载检查") {}

        void _run(void* env) const noexcept override {
            auto* vfs = static_cast<VFS*>(env);
            tassert(vfs != nullptr, "测试环境应提供已初始化的 VFS 实例");

            RamDiskDevice* initrd = make_initrd();
            tassert(initrd != nullptr, "应能成功创建 initrd RamDisk 设备");

            action("挂载未注册文件系统应失败");
            auto invalid_mount =
                vfs->mount("unknownfs", initrd, "/", MountFlags::NONE,
                           nullptr);
            tassert(!invalid_mount.has_value() &&
                        invalid_mount.error() == ErrCode::INVALID_PARAM,
                    "未注册文件系统的挂载应被拒绝");

            auto mount_res =
                vfs->mount("tarfs", initrd, "/", MountFlags::NONE, nullptr);
            tassert(mount_res.has_value(), "首次将 tarfs 挂载到根目录 / 应成功");

            action("同一挂载点重复挂载应失败");
            auto duplicate_mount =
                vfs->mount("tarfs", initrd, "/", MountFlags::NONE, nullptr);
            tassert(!duplicate_mount.has_value() &&
                        duplicate_mount.error() == ErrCode::INVALID_PARAM,
                    "同一挂载点重复挂载应被拒绝");

            auto umount_res = vfs->umount("/");
            tassert(umount_res.has_value(), "卸载根目录 / 应成功");

            delete initrd;
        }
    };

    static RamDiskDevice* make_initrd() {
        size_t sz             = (char*)&e_initrd - (char*)&s_initrd;
        RamDiskDevice* device = new RamDiskDevice(&s_initrd, sz, 1);
        return device;
    }

    static void* setup_vfs() {
        VFS* vfs = new VFS();
        auto ret = vfs->register_fs(
            util::owner(static_cast<IFsDriver*>(new tarfs::TarFSDriver())));
        if (!ret.has_value()) {
            delete vfs;
            return nullptr;
        }
        return vfs;
    }

    static void teardown_vfs(void* env) {
        delete static_cast<VFS*>(env);
    }

    void collect_tests(TestFramework& framework) {
        auto cases = util::ArrayList<TestCase*>();
        cases.push_back(new CaseMountOpenRead());
        cases.push_back(new CaseMountBusyUmount());
        cases.push_back(new CaseOpenMissingFile());
        cases.push_back(new CaseMountParamValidation());
        framework.add_category(
            new TestCategory("fs", std::move(cases), setup_vfs, teardown_vfs));
    }
}  // namespace test::fs