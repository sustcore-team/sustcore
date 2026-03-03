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

namespace test::fs {

    // static RamDiskDevice* __make_initrd() {
    //     size_t sz             = (char*)&e_initrd - (char*)&s_initrd;
    //     RamDiskDevice* device = new RamDiskDevice(&s_initrd, sz, 1);
    //     return device;
    // }

    // class CaseTarfsMount : public TestCase {
    // public:
    //     CaseTarfsMount() : TestCase("TarFS 挂载与卸载测试") {}
    //     void _run(void* env) const noexcept override {
    //         VFS* vfs = static_cast<VFS*>(env);

    //         RamDiskDevice* initrd = __make_initrd();

    //         expect("将 tarfs 挂载到 /initrd");
    //         FSErrCode code =
    //             vfs->mount("tarfs", initrd, "/initrd", MountFlags::NONE, "");
    //         tassert(code == FSErrCode::SUCCESS, "vfs.mount() 成功");

    //         action("卸载 /initrd");
    //         code = vfs->umount("/initrd");
    //         test(code == FSErrCode::SUCCESS, "vfs.umount() 成功");
    //     }
    // };

    // class CaseVFSReadOnly : public TestCase {
    // public:
    //     CaseVFSReadOnly() : TestCase("VFS 只读操作测试 (TarFS)") {}
    //     void _run(void* env) const noexcept override {
    //         VFS* vfs              = static_cast<VFS*>(env);
    //         RamDiskDevice* initrd = __make_initrd();
    //         vfs->mount("tarfs", initrd, "/initrd", MountFlags::NONE, "");

    //         expect("打开存在的内核源文件: /initrd/src/kernel/main.cpp");
    //         auto file_opt = vfs->_open("/initrd/src/kernel/main.cpp", 0);
    //         tassert(file_opt.present(), "文件打开成功");

    //         VFile* file  = file_opt.value();
    //         char* buffer = new char[128];

    //         action("读取文件内容 (前 127 字节)");
    //         FSOptional<size_t> read_bytes_opt = vfs->_read(file, buffer, 127);
    //         if (test(read_bytes_opt.present(), "读取操作成功")) {
    //             size_t read_bytes = read_bytes_opt.value();
    //             test(read_bytes > 0, "读取字节数大于 0");
    //             buffer[read_bytes > 0 ? read_bytes : 0] = '\0';
    //             kprintfln("%s", buffer);
    //         }

    //         action("关闭并清理");
    //         vfs->_close(file);
    //         delete[] buffer;
    //         vfs->umount("/initrd");
    //     }
    // };

    // static void* setup_vfs() {
    //     VFS* vfs = new VFS();
    //     vfs->register_fs(new tarfs::TarFSDriver());
    //     return vfs;
    // }

    // static void teardown_vfs(void* env) {
    //     delete static_cast<VFS*>(env);
    // }

    void collect_tests(TestFramework& framework) {
    }
}  // namespace test::fs