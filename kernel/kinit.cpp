/**
 * @file kinit.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief kinit 线程
 * @version alpha-1.0.0
 * @date 2026-06-09
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <bio/blk.h>
#include <bio/block.h>
#include <bio/request.h>
#include <cap/cholder.h>
#include <device/int.h>
#include <device/model.h>
#include <driver/model.h>
#include <driver/rtc/goldfish.h>
#include <driver/serial.h>
#include <driver/virtio/virtio-blk.h>
#include <driver/virtio/virtio.h>
#include <env.h>
#include <exe/task.h>
#include <kinit.h>
#include <logger.h>
#include <object/perm.h>
#include <sus/logger.h>
#include <sus/owner.h>
#include <sus/units.h>
#include <sustcore/capability.h>
#include <sustcore/files.h>
#include <symbols.h>
#include <task/scheduler.h>
#include <task/task.h>
#include <task/wait.h>
#include <vfs/device.h>
#include <vfs/ops.h>
#include <vfs/tarfs.h>
#include <vfs/tmpfs.h>
#include <vfs/vfs.h>

#include <cassert>
#include <cstring>
#include <string>

extern void register_timekeeper_log_test();

namespace {
    constexpr const char *INITRD_PATH           = "/initrd/";
    constexpr const char *INITMOD_PATH          = "/initrd/init.mod";
    constexpr const char *SETUPMOD_PATH         = "/initrd/setup.mod";
    constexpr const char *DEFAULTMOD_PATH       = "/initrd/default.mod";
    constexpr const char *TEST_IMG_PATH         = "/test_img/";
    constexpr const char *TEST_IMG_FEATURE_PATH = "/test_img/feature.mk";
    constexpr size_t FILE_READ_CHUNK_SIZE       = 256;

    util::owner<RamDiskDevice *> g_initrd_device =
        util::owner<RamDiskDevice *>(nullptr);

    util::owner<RamDiskDevice *> make_initrd() {
        auto e_initrd_ptr = reinterpret_cast<char *>(&e_initrd);
        auto s_initrd_ptr = reinterpret_cast<char *>(&s_initrd);
        size_t sz         = e_initrd_ptr - s_initrd_ptr;
        auto device       = util::owner(new RamDiskDevice(&s_initrd, sz, 1));
        loggers::SUSTCORE::INFO("initrd大小为 %u KB =  %u MB", sz / 1024,
                                sz / 1024 / 1024);
        return device;
    }

    Result<std::string> read_vfs_file_text(const char *path) {
        auto file_res = VFS::inst().__debug_open(path);
        if (!file_res.has_value()) {
            loggers::SUSTCORE::ERROR("打开文件失败: path=%s err=%s", path,
                                     to_cstring(file_res.error()));
        }
        propagate(file_res);
        auto *file = file_res.value();
        util::Guard file_guard([file]() { file->destruct(); });

        auto size_res = VFS::inst().size(*file);
        if (!size_res.has_value()) {
            loggers::SUSTCORE::ERROR("获取文件大小失败: path=%s err=%s", path,
                                     to_cstring(size_res.error()));
        }
        propagate(size_res);
        std::string content(size_res.value(), '\0');

        size_t offset = 0;
        while (offset < content.size()) {
            const auto chunk =
                std::min(FILE_READ_CHUNK_SIZE, content.size() - offset);
            auto read_res = VFS::inst().read(*file, static_cast<off_t>(offset),
                                             content.data() + offset, chunk);
            if (!read_res.has_value()) {
                loggers::SUSTCORE::ERROR(
                    "读取文件失败: path=%s offset=%u err=%s", path,
                    static_cast<unsigned>(offset),
                    to_cstring(read_res.error()));
            }
            propagate(read_res);
            if (read_res.value() == 0) {
                break;
            }
            offset += read_res.value();
        }
        content.resize(offset);
        return content;
    }

    void log_text_lines(const char *prefix, const std::string &content) {
        size_t line_begin = 0;
        while (line_begin < content.size()) {
            const auto line_end = content.find('\n', line_begin);
            const auto line_len = line_end == std::string::npos
                                      ? content.size() - line_begin
                                      : line_end - line_begin;
            loggers::SUSTCORE::INFO("%s%.*s", prefix,
                                    static_cast<int>(line_len),
                                    content.data() + line_begin);
            if (line_end == std::string::npos) {
                break;
            }
            line_begin = line_end + 1;
        }
        if (content.empty()) {
            loggers::SUSTCORE::INFO("%s", prefix);
        }
    }

    Result<void> init_vfs() {
        loggers::SUSTCORE::INFO("初始化块设备管理器与VFS");
        blk::BlkManager::init();
        VFS::init();
        auto &vfs = VFS::inst();

        auto register_res = vfs.register_fs<tarfs::TarFSDriver>();
        propagate(register_res);

        register_res = vfs.register_fs<tmpfs::TmpFSDriver>();
        propagate(register_res);

        register_res = vfs.register_fs<devfs::DevFSDriver>();
        propagate(register_res);

        loggers::SUSTCORE::INFO("创建 Initrd 块设备");
        g_initrd_device = make_initrd();
        auto devno_res =
            blk::BlkManager::inst().register_device(util::nnullforce(
                static_cast<IBlockDeviceOps *>(g_initrd_device.get())));
        if (!devno_res.has_value()) {
            propagate_return(devno_res);
        }

        loggers::SUSTCORE::INFO("挂载 tmpfs");
        auto mount_res = vfs.mount("tmpfs", "/", nullptr);
        propagate(mount_res);
        loggers::SUSTCORE::INFO("tmpfs 挂载完毕");

        loggers::SUSTCORE::INFO("挂载 tarfs");
        mount_res = vfs.mount("tarfs", devno_res.value(), INITRD_PATH,
                              MountFlags::NONE, nullptr);
        propagate(mount_res);
        loggers::SUSTCORE::INFO("tarfs 挂载完毕");

        // TODO: 挂载一个 sysfs, 此处先使用 mkdir 来处理
        auto cur_pcb = schd::Scheduler::inst().current_pcb();
        if (cur_pcb == nullptr)
        {
            unexpect_return(ErrCode::NULLPTR);
        }
        auto &cur_holder = *cur_pcb->cholder;

        loggers::SUSTCORE::INFO("创建 /sys/ 目录");
        auto open_res = 
            vfs.open_dir("/", cur_holder, perm::vdir::EXEC | perm::vdir::READ | perm::vdir::WRITE);
        propagate(open_res);
        auto root_cap_idx = open_res.value();
        auto lookup_res = cur_holder.lookup(root_cap_idx);
        propagate(lookup_res);
        auto root_cap = *lookup_res.value();

        auto mkdir_res = vfs.mkdir(root_cap, "sys/", flags::O_READ | flags::O_WRITE | flags::O_EXECUTE, cur_holder);
        propagate(mkdir_res);

        auto remove_res = cur_holder.remove(root_cap_idx);
        propagate(remove_res);
        remove_res = cur_holder.remove(mkdir_res.value());
        propagate(remove_res);

        loggers::SUSTCORE::INFO("挂载 devfs");
        mount_res = vfs.mount("devfs", devfs::DEVFS_MOUNT_PATH, nullptr);
        propagate(mount_res);
        loggers::SUSTCORE::INFO("devfs 挂载完毕");
        void_return();
    }

    Result<void> init_driver_model() {
        auto &device_model = device::DeviceModel::inst();
        auto activate_res =
            driver::DriverModel::inst().activate_runtime(
                device_model.non_irq_device_nodes());
        propagate(activate_res);

        // 开始注册各个设备驱动
        // auto register_res = driver::DriverModel::inst().register_factory(
        //     util::owner<driver::IDeviceFactory *>(
        //         new driver::SerialDeviceFactory()));
        // propagate(register_res);

        auto register_res = driver::DriverModel::inst().register_factory(
            util::owner<driver::IDeviceFactory *>(
                new driver::GoldfishRTCFactory()));
        propagate(register_res);

        register_res = driver::DriverModel::inst().register_factory(
            util::owner<driver::IDeviceFactory *>(
                new virtio::VirtioMmioFactory()));
        propagate(register_res);
        void_return();
    }

    Result<void> load_runtime_init() {
        auto load_res = task::TaskManager::inst().load_init(INITMOD_PATH);
        if (!load_res.has_value()) {
            loggers::SUSTCORE::ERROR("加载初始进程失败! 错误码: %s",
                                     to_cstring(load_res.error()));
            propagate_return(load_res);
        }
        auto &task = load_res.value();
        assert(task->threads.size() == 1);
        auto *init_tcb = &task->threads.front();
        if (!schd::Scheduler::inst().wakeup_new(init_tcb)) {
            loggers::SUSTCORE::ERROR("唤醒初始进程失败");
            unexpect_return(ErrCode::CREATION_FAILED);
        }
        void_return();
    }

    [[noreturn]]
    void block_kinit_forever() {
        auto block_wd  = wait::alloc_reason();
        auto block_res = schd::Scheduler::inst().block_current(block_wd);
        if (!block_res.has_value()) {
            loggers::SUSTCORE::FATAL("阻塞 kinit 失败: %s",
                                     to_cstring(block_res.error()));
            panic("阻塞 kinit 失败");
        }
        panic("kinit 被意外唤醒");
    }
}  // namespace

void kinit_runtime_entry() {
    loggers::SUSTCORE::INFO("进入 kinit 运行时初始化阶段");
    Interrupt::sti();

    auto init_res = init_vfs();
    if (!init_res.has_value()) {
        loggers::SUSTCORE::FATAL("kinit 初始化 VFS 失败: %s",
                                 to_cstring(init_res.error()));
        panic("kinit 初始化 VFS 失败");
    }
    loggers::SUSTCORE::INFO("已初始化 VFS");

    init_res = init_driver_model();
    if (!init_res.has_value()) {
        loggers::SUSTCORE::FATAL("kinit 初始化 DriverModel 失败: %s",
                                 to_cstring(init_res.error()));
        panic("kinit 初始化 DriverModel 失败");
    }
    loggers::SUSTCORE::INFO("已初始化 DriverModel");

#ifdef __CONF_KERNEL_TIMEKEEPER_TEST
    register_timekeeper_log_test();
#endif

    loggers::SUSTCORE::INFO("开始加载用户 init 进程");
    init_res = load_runtime_init();
    if (!init_res.has_value()) {
        loggers::SUSTCORE::FATAL("kinit 加载用户 init 失败: %s",
                                 to_cstring(init_res.error()));
        panic("kinit 加载用户 init 失败");
    }

    loggers::SUSTCORE::INFO("用户 init 进程加载完毕 ; kinit 线程休眠");
    block_kinit_forever();
}
