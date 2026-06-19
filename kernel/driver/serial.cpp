/**
 * @file serial.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 串口设备与工厂实现
 * @version alpha-1.0.0
 * @date 2026-05-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <device/model.h>
#include <device/resource.h>
#include <driver/serial.h>
#include <logger.h>
#include <sus/units.h>
#include <sustcore/files.h>
#include <vfs/vfs.h>

namespace driver {
    namespace {
        constexpr FDTDeviceId SERIAL_FDT_IDS[] = {
            {.compatible = "ns16550a", .driver_flag = 0},
            {.compatible = nullptr, .driver_flag = 0},
        };
        constexpr DeviceId SERIAL_DEVICE_ID = {
            .fdt_ids = SERIAL_FDT_IDS,
            .pci_ids = nullptr,
        };

        class SerialIOFile final : public devfs::CharDevFile {
        private:
            SerialDevice &_device;

        public:
            SerialIOFile(inode_t inode_id, SerialDevice &device)
                : CharDevFile(inode_id), _device(device) {}

            [[nodiscard]]
            Result<size_t> write(const void *buf, size_t len) override {
                if (buf == nullptr && len != 0) {
                    unexpect_return(ErrCode::NULLPTR);
                }
                _device.write(static_cast<const char *>(buf), len);
                return len;
            }
        };

        Result<util::owner<IINode *>> create_serial_io_file(void *ctx,
                                                            inode_t inode_id) {
            auto *device = static_cast<SerialDevice *>(ctx);
            auto *file   = new SerialIOFile(inode_id, *device);
            if (file == nullptr) {
                unexpect_return(ErrCode::OUT_OF_MEMORY);
            }
            return util::owner<IINode *>(file);
        }
    }  // namespace

    /**
     * @brief 构造一个串口设备驱动.
     */
    SerialDevice::SerialDevice(DevRes res, units::frequency clock_frequency,
                               char *base) noexcept
        : DriverBase(std::move(res)),
          _clock_frequency(clock_frequency),
          _base(reinterpret_cast<volatile UART *>(base)) {
        assert(_mmios.size() >= 1);
        auto *mmio = _mmios[0].get();
        assert(mmio != nullptr);
        assert(mmio->region().size() >= UART_REG_SIZE);
    }

    /**
     * @brief 获取 UART 输入时钟频率.
     */
    units::frequency SerialDevice::clock_frequency() const noexcept {
        return _clock_frequency;
    }

    void SerialDevice::writec(char ch) noexcept {
        _base->thr = static_cast<uart_t>(ch) & 0xFF;
    }

    void SerialDevice::write(const char *str, size_t len) noexcept {
        for (size_t i = 0; i < len; ++i) {
            writec(str[i]);
        }
    }

    Result<void> SerialDevice::mount(CapIdx devdir) noexcept {
        loggers::DEVICE::DEBUG("在设备目录下创建 serial 文件!");
        auto &vfs = VFS::inst();
        auto devfs_res = vfs.devfs();
        propagate(devfs_res);
        auto &devfs = *devfs_res.value();

        auto lookup_res = holder().lookup(devdir);
        propagate(lookup_res);
        auto &dircap = *lookup_res.value();

        auto mkres =
            vfs.mkfile(dircap, "serial", flags::O_READ | flags::O_WRITE,
                       holder());
        propagate(mkres);

        lookup_res = holder().lookup(mkres.value());
        propagate(lookup_res);
        auto &filecap = *lookup_res.value();
        auto link_res = devfs.link_char(
            filecap,
            devfs::CharFactory{.ctx = this, .create = create_serial_io_file});
        propagate(link_res);
        void_return();
    }

    /**
     * @brief 获取该工厂支持的主 compatible.
     */
    const DeviceId &SerialDeviceFactory::device_id() const noexcept {
        return SERIAL_DEVICE_ID;
    }

    /**
     * @brief 基于统一设备节点创建串口设备驱动.
     */
    Result<DriverBase *> SerialDeviceFactory::create(
        const device::DeviceNode &node, device::DeviceModel &model,
        b64 driver_flag) const {
        (void)model;
        (void)driver_flag;
        loggers::DEVICE::INFO("开始创建设备驱动: serial name=%s", node.name());

        auto load_res = SerialDevice::__load_integral(
            node, SerialDevice::CLOCK_FREQUENCY_PROP, sizeof(sus_u32));
        propagate(load_res);
        auto clock_frequency = units::frequency::from_hz(load_res.value());
        if (clock_frequency.to_milihz() == 0) {
            loggers::DEVICE::ERROR("SerialDevice 创建失败: 时钟频率为 0");
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        loggers::DEVICE::DEBUG(
            "SerialDevice[%s] 时钟频率=%llu Hz", node.name(),
            static_cast<unsigned long long>(clock_frequency.to_hz()));

        auto virqs = device::DevResManager::get_virq_resource(node);
        auto mmios = device::DevResManager::get_mmio_resource(node);
        if (mmios.empty()) {
            loggers::DEVICE::ERROR("SerialDevice 创建失败: 缺少 MMIO 资源");
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        auto *mmio = mmios.front().get();
        assert(mmio != nullptr);
        if (mmio->region().size() < SerialDevice::UART_REG_SIZE) {
            loggers::DEVICE::ERROR(
                "SerialDevice 创建失败: MMIO 区域过小 size=%llu need=%llu",
                static_cast<unsigned long long>(mmio->region().size()),
                static_cast<unsigned long long>(SerialDevice::UART_REG_SIZE));
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        auto map_res = device::MMIOManager::inst().map_to_kernel(*mmio);
        if (!map_res.has_value()) {
            loggers::DEVICE::ERROR(
                "SerialDevice 创建失败: MMIO 映射失败 err=%s",
                to_cstring(map_res.error()));
            propagate_return(map_res);
        }
        auto res_pack =
            DriverBase::DevRes(node, std::move(virqs), std::move(mmios));

        auto *device = new SerialDevice(std::move(res_pack), clock_frequency,
                                        map_res.value().as<char>());
        if (device == nullptr) {
            loggers::DEVICE::ERROR("SerialDevice 创建失败: 内存不足");
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }

        loggers::DEVICE::DEBUG("创建 SerialDevice: name=%s clock=%llu",
                               device->name(),
                               static_cast<unsigned long long>(
                                   device->_clock_frequency.to_hz()));
        return device;
    }
}  // namespace driver
