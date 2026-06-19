/**
 * @file virtio.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief virtio MMIO 通用探测、初始化与队列管理
 * @version alpha-1.0.0
 * @date 2026-06-10
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/device.h>
#include <device/pci.h>
#include <device/resource.h>
#include <driver/base.h>
#include <driver/factory.h>
#include <sus/types.h>
#include <sustcore/addr.h>
#include <sustcore/errcode.h>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace virtio {
    using u8   = uint8_t;
    using u16  = uint16_t;
    using u32  = uint32_t;
    using u64  = uint64_t;
    using le16 = uint16_t;
    using le32 = uint32_t;
    using le64 = uint64_t;
    using be16 = uint16_t;
    using be32 = uint32_t;
    using be64 = uint64_t;

    constexpr const char *VIRTIO_MMIO_COMPATIBLE = "virtio,mmio";
    constexpr u32 MAGIC_VALUE                         = 0x74726976;
    constexpr u32 VERSION_LEGACY                     = 0x1;
    constexpr u32 VERSION_MODERN                     = 0x2;

    /**
     * @brief virtio 设备状态寄存器位定义.
     *
     * 这些标志共同描述通用初始化状态机的推进阶段.
     */
    namespace device_status {
        constexpr u32 ACKNOWLEDGE = 1u << 0;
        constexpr u32 DRIVER      = 1u << 1;
        constexpr u32 DRIVER_OK   = 1u << 2;
        constexpr u32 FEATURES_OK = 1u << 3;
        constexpr u32 NEEDS_RESET = 1u << 6;
        constexpr u32 FAILED      = 1u << 7;
    }  // namespace device_status

    /**
     * @brief virtio 通用 feature 位定义.
     *
     * 这里只保留当前通用初始化与队列路径会使用到的公共特性.
     */
    namespace common_feature {
        constexpr u64 RING_INDIRECT_DESC = 1ull << 28;
        constexpr u64 RING_EVENT_IDX     = 1ull << 29;
        constexpr u64 VERSION_1          = 1ull << 32;
    }  // namespace common_feature

    /**
     * @brief virtio 设备类型编号.
     *
     * 取值与 virtio MMIO 的 `device_id` 一致.
     */
    enum class DeviceType : u32 {
        INVALID           = 0,
        NETWORK           = 1,
        BLOCK             = 2,
        CONSOLE           = 3,
        ENTROPY           = 4,
        MEMORY_BALLOON    = 5,
        IOMEM             = 6,
        RPMSG             = 7,
        SCSI_HOST         = 8,
        TRANSPORT_9P      = 9
    };

    /**
     * @brief virtio MMIO 公共寄存器偏移定义.
     *
     * 驱动应优先复用这些命名偏移, 避免在实现中散落裸常量.
     */
    namespace offset {
        constexpr size_t MAGIC_VALUE        = 0x000;
        constexpr size_t VERSION            = 0x004;
        constexpr size_t DEVICE_ID          = 0x008;
        constexpr size_t VENDOR_ID          = 0x00C;
        constexpr size_t DEVICE_FEATURE     = 0x010;
        constexpr size_t DEVICE_FEATURE_SEL = 0x014;
        constexpr size_t DRIVER_FEATURE     = 0x020;
        constexpr size_t DRIVER_FEATURE_SEL = 0x024;
        constexpr size_t GUEST_PAGE_SIZE    = 0x028;
        constexpr size_t QUEUE_SEL          = 0x030;
        constexpr size_t QUEUE_NUM_MAX      = 0x034;
        constexpr size_t QUEUE_NUM          = 0x038;
        constexpr size_t QUEUE_ALIGN        = 0x03C;
        constexpr size_t QUEUE_PFN          = 0x040;
        constexpr size_t QUEUE_READY        = 0x044;
        constexpr size_t QUEUE_NOTIFY       = 0x050;
        constexpr size_t INTERRUPT_STATUS   = 0x060;
        constexpr size_t INTERRUPT_ACK      = 0x064;
        constexpr size_t STATUS             = 0x070;
        constexpr size_t QUEUE_DESC_LO      = 0x080;
        constexpr size_t QUEUE_DESC_HI      = 0x084;
        constexpr size_t QUEUE_DRIVER_LO    = 0x090;
        constexpr size_t QUEUE_DRIVER_HI    = 0x094;
        constexpr size_t QUEUE_DEVICE_LO    = 0x0A0;
        constexpr size_t QUEUE_DEVICE_HI    = 0x0A4;
        constexpr size_t SHM_SEL            = 0x0AC;
        constexpr size_t SHM_LEN_LO         = 0x0B0;
        constexpr size_t SHM_LEN_HI         = 0x0B4;
        constexpr size_t SHM_BASE_LO        = 0x0B8;
        constexpr size_t SHM_BASE_HI        = 0x0BC;
        constexpr size_t QUEUE_RESET        = 0x0C0;
        constexpr size_t CONFIG_GENERATION  = 0x0FC;
        constexpr size_t CONFIG_SPACE       = 0x100;
        constexpr size_t TOTAL_SIZE         = 0x100;
    }  // namespace offset

    namespace pci_cap {
        constexpr u8 CAP_ID_VENDOR   = 0x09;
        constexpr u8 CFG_COMMON      = 1;
        constexpr u8 CFG_NOTIFY      = 2;
        constexpr u8 CFG_ISR         = 3;
        constexpr u8 CFG_DEVICE      = 4;
        constexpr u8 CFG_PCI         = 5;
        constexpr u8 CFG_SHARED_MEM  = 8;
        constexpr u8 CFG_VENDOR_DATA = 9;
    }  // namespace pci_cap

    struct VirtioPciCap {
        b8 cap_vndr = 0;
        b8 cap_next = 0;
        b8 cap_len = 0;
        b8 cfg_type = 0;
        b8 bar = 0;
        b8 id = 0;
        b8 padding[2]{};
        le32 offset = 0;
        le32 length = 0;
    };

    struct VirtioPciNotifyCap : public VirtioPciCap {
        le32 notify_off_multiplier = 0;
    };

    struct VirtioPciCommonCfg {
        le32 device_feature_select;
        le32 device_feature;
        le32 driver_feature_select;
        le32 driver_feature;
        le16 msix_config;
        le16 num_queues;
        u8 device_status;
        u8 config_generation;
        le16 queue_select;
        le16 queue_size;
        le16 queue_msix_vector;
        le16 queue_enable;
        le16 queue_notify_off;
        le64 queue_desc;
        le64 queue_driver;
        le64 queue_device;
    };

    struct VirtioPciRegion {
        bool valid = false;
        b8 bar = 0;
        b64 cpu_addr = 0;
        b32 length = 0;
    };

    /**
     * @brief virtio MMIO 通用寄存器布局.
     *
     * 该结构只覆盖公共寄存器窗口, 不包含设备私有配置空间内容.
     */
    struct CommonConfig {
        le32 magic_value;
        le32 version;
        le32 device_id;
        le32 vendor_id;
        le32 device_features;
        le32 device_features_sel;
        le32 reserved_0x018;
        le32 reserved_0x01C;
        le32 driver_features;
        le32 driver_features_sel;
        le32 guest_page_size;
        le32 reserved_0x02C;
        le32 queue_sel;
        le32 queue_num_max;
        le32 queue_num;
        le32 queue_align;
        le32 queue_pfn;
        le32 queue_ready;
        le32 reserved_0x048;
        le32 reserved_0x04C;
        le32 queue_notify;
        le32 reserved_0x054;
        le32 reserved_0x058;
        le32 reserved_0x05C;
        le32 interrupt_status;
        le32 interrupt_ack;
        le32 reserved_0x068;
        le32 reserved_0x06C;
        le32 status;
        le32 reserved_0x074;
        le32 reserved_0x078;
        le32 reserved_0x07C;
        le32 queue_desc_low;
        le32 queue_desc_high;
        le32 reserved_0x088;
        le32 reserved_0x08C;
        le32 queue_driver_low;
        le32 queue_driver_high;
        le32 reserved_0x098;
        le32 reserved_0x09C;
        le32 queue_device_low;
        le32 queue_device_high;
        le32 reserved_0x0A8;
        le32 shm_sel;
        le32 shm_len_low;
        le32 shm_len_high;
        le32 shm_base_low;
        le32 shm_base_high;
        le32 queue_reset;
        le32 reserved_0x0C4[14];
        le32 config_generation;
    };
    static_assert(sizeof(CommonConfig) == offset::TOTAL_SIZE,
                  "CommonConfig must be exactly 256 bytes");

    /**
     * @brief 通用探测所得的 virtio 基础信息.
     *
     * 工厂探测阶段先收敛这组只读事实, 具体设备驱动据此判断
     * 是否能够接管节点并执行后续初始化.
     */
    struct ProbeInfo {
        bool valid                        = false;
        bool legacy                       = false;
        u32 magic_value                   = 0;
        u32 version                       = 0;
        u32 device_id                     = 0;
        u32 vendor_id                     = 0;
        u32 status                        = 0;
        u32 device_features               = 0;
        const device::MMIOResource *mmio  = nullptr;
    };

    /**
     * @brief 简单 DMA 缓冲区.
     *
     * 保存一段连续页框对应的内核地址、物理地址以及释放所需页数.
     */
    struct DmaBuffer {
        void *kaddr      = nullptr;
        PhyAddr paddr    = PhyAddr::null;
        size_t size      = 0;
        size_t page_cnt  = 0;
    };

    /**
     * @brief virtio transport 抽象.
     *
     * 负责屏蔽不同 transport 的寄存器与配置空间访问差异.
     */
    class Transport {
    public:
        enum class Kind {
            MMIO,
            PCI,
        };

        explicit Transport(ProbeInfo probe_info) noexcept
            : _probe_info(probe_info) {}
        virtual ~Transport() = default;

        [[nodiscard]]
        virtual Kind kind() const noexcept = 0;

        [[nodiscard]]
        virtual const char *name() const noexcept = 0;

        [[nodiscard]]
        virtual u32 read_reg32(size_t reg_offset) const noexcept = 0;
        virtual void write_reg32(size_t reg_offset, u32 value) noexcept = 0;

        [[nodiscard]]
        virtual Result<u8> read_config_u8(size_t config_offset) const noexcept = 0;
        [[nodiscard]]
        virtual Result<u16> read_config_u16(
            size_t config_offset) const noexcept = 0;
        [[nodiscard]]
        virtual Result<u32> read_config_u32(
            size_t config_offset) const noexcept = 0;
        [[nodiscard]]
        virtual Result<u64> read_config_u64(
            size_t config_offset) const noexcept = 0;

        [[nodiscard]]
        const ProbeInfo &probe_info() const noexcept {
            return _probe_info;
        }

    protected:
        ProbeInfo _probe_info;
    };

    /**
     * @brief virtio-mmio transport 实现.
     */
    class TransportMMIO final : public Transport {
    public:
        TransportMMIO(ProbeInfo probe_info, const device::MMIOResource &mmio,
                      char *mmio_base) noexcept;
        ~TransportMMIO() override = default;

        [[nodiscard]]
        Kind kind() const noexcept override {
            return Kind::MMIO;
        }

        [[nodiscard]]
        const char *name() const noexcept override {
            return "virtio-mmio";
        }

        [[nodiscard]]
        u32 read_reg32(size_t reg_offset) const noexcept override;
        void write_reg32(size_t reg_offset, u32 value) noexcept override;

        [[nodiscard]]
        Result<u8> read_config_u8(size_t config_offset) const noexcept override;
        [[nodiscard]]
        Result<u16> read_config_u16(size_t config_offset) const noexcept override;
        [[nodiscard]]
        Result<u32> read_config_u32(size_t config_offset) const noexcept override;
        [[nodiscard]]
        Result<u64> read_config_u64(size_t config_offset) const noexcept override;

        [[nodiscard]]
        const device::MMIOResource &mmio() const noexcept {
            return *_mmio;
        }

    private:
        const device::MMIOResource *_mmio = nullptr;
        volatile CommonConfig *_regs      = nullptr;
    };

    /**
     * @brief virtio-pci transport 预留骨架.
     */
    class TransportPCI final : public Transport {
    public:
        TransportPCI(const pci::PCIDeviceNode &node, ProbeInfo probe_info) noexcept;
        ~TransportPCI() override;

        [[nodiscard]]
        Kind kind() const noexcept override {
            return Kind::PCI;
        }

        [[nodiscard]]
        const char *name() const noexcept override {
            return "virtio-pci";
        }

        [[nodiscard]]
        u32 read_reg32(size_t reg_offset) const noexcept override;
        void write_reg32(size_t reg_offset, u32 value) noexcept override;

        [[nodiscard]]
        Result<u8> read_config_u8(size_t config_offset) const noexcept override;
        [[nodiscard]]
        Result<u16> read_config_u16(size_t config_offset) const noexcept override;
        [[nodiscard]]
        Result<u32> read_config_u32(size_t config_offset) const noexcept override;
        [[nodiscard]]
        Result<u64> read_config_u64(size_t config_offset) const noexcept override;

        [[nodiscard]]
        bool modern() const noexcept {
            return _modern;
        }

        [[nodiscard]]
        bool queue_uses_modern_layout() const noexcept {
            return _modern;
        }

        [[nodiscard]]
        Result<u16> max_queue_size(u16 queue_index) noexcept;
        Result<void> setup_queue(u16 queue_index, u16 queue_size,
                                 PhyAddr desc, PhyAddr driver_area,
                                 PhyAddr device_area) noexcept;
        Result<void> notify_queue(u16 queue_index) noexcept;

    private:
        [[nodiscard]]
        Result<void> parse_capabilities() noexcept;
        [[nodiscard]]
        Result<void> load_probe_info() noexcept;
        [[nodiscard]]
        Result<void> parse_vendor_cap(b16 offset) noexcept;
        [[nodiscard]]
        Result<VirtioPciRegion> read_region(b16 cap_offset,
                                            b8 expected_cfg_type) const noexcept;
        [[nodiscard]]
        Result<volatile char *> map_bar_region(const VirtioPciRegion &region,
                                               util::owner<device::MMIOResource *> &resource) noexcept;

        const pci::PCIDeviceNode *_node = nullptr;
        bool _modern = false;
        VirtioPciRegion _common_cfg{};
        VirtioPciRegion _notify_cfg{};
        VirtioPciRegion _device_cfg{};
        b32 _notify_off_multiplier = 0;
        util::owner<device::MMIOResource *> _common_resource =
            util::owner<device::MMIOResource *>(nullptr);
        util::owner<device::MMIOResource *> _notify_resource =
            util::owner<device::MMIOResource *>(nullptr);
        util::owner<device::MMIOResource *> _device_resource =
            util::owner<device::MMIOResource *>(nullptr);
        volatile char *_common_base = nullptr;
        volatile char *_notify_base = nullptr;
        volatile char *_device_base = nullptr;
    };

    /**
     * @brief legacy virtqueue 描述符项.
     */
    struct VirtQueueDescLegacy {
        le64 addr;
        le32 len;
        le16 flags;
        le16 next;
    };
    static_assert(sizeof(VirtQueueDescLegacy) == 16);

    /**
     * @brief legacy virtqueue avail ring 头部.
     */
    struct VirtQueueAvailRingLegacy {
        le16 flags;
        le16 index;
    };

    /**
     * @brief legacy virtqueue used ring 单项.
     */
    struct VirtQueueUsedElemLegacy {
        le32 id;
        le32 len;
    };

    /**
     * @brief legacy virtqueue used ring 头部.
     */
    struct VirtQueueUsedRingLegacy {
        le16 flags;
        le16 index;
    };

    /**
     * @brief modern virtqueue 内存布局描述.
     *
     * 当前仅保留结构定义, 供后续 modern virtqueue 实现复用.
     */
    struct VirtQueueModern {
        le64 driver_to_device_paddr;
        le32 driver_to_device_size;
        le64 device_to_driver_paddr;
        le32 device_to_driver_size;
        le64 avail_offset;
    };

    namespace vring {
        constexpr u16 INVALID_DESC = 0xFFFF;
        constexpr u16 NO_INTERRUPT = 0x0001;

        namespace desc_flag {
            constexpr u16 NEXT     = 1;
            constexpr u16 WRITE    = 2;
            constexpr u16 INDIRECT = 4;
        }  // namespace desc_flag

        /**
         * @brief 一段待提交到 virtqueue 的缓冲视图.
         *
         * 调用方负责保证该物理缓冲在请求完成前持续有效.
         */
        struct BufferView {
            PhyAddr paddr = PhyAddr::null;
            size_t size   = 0;
            bool writable = false;
        };
    }  // namespace vring

    /**
     * @brief legacy virtqueue 运行时对象.
     *
     * 聚合一条 legacy 队列的 DMA 内存布局、自由描述符链
     * 以及 avail/used ring 消费进度.
     */
    struct VirtQueueLegacy {
        u16 queue_index                = 0;
        u16 size                       = 0;
        DmaBuffer ring                 = {};
        VirtQueueDescLegacy *desc      = nullptr;
        volatile le16 *avail_flags     = nullptr;
        volatile le16 *avail_index     = nullptr;
        volatile le16 *avail_ring      = nullptr;
        volatile le16 *used_flags      = nullptr;
        volatile le16 *used_index      = nullptr;
        volatile VirtQueueUsedElemLegacy *used_ring = nullptr;
        size_t avail_offset            = 0;
        size_t used_offset             = 0;
        size_t ring_bytes              = 0;
        u16 free_head                  = vring::INVALID_DESC;
        u16 free_count                 = 0;
        u16 last_used_index            = 0;
        u16 last_avail_index           = 0;
    };

    /**
     * @brief virtio 设备通用运行时基类.
     *
     * 负责公共 MMIO 访问、feature 协商、初始化状态机和
     * legacy virtqueue 的提交/轮询回收逻辑.
     */
    class VirtioDriverBase : public driver::DriverBase {
    public:
        /**
         * @brief 构造一个 virtio 通用设备对象.
         *
         * @param res 统一设备资源包.
         * @param probe_info 工厂探测得到的公共信息.
         * @param mmio_base 已映射的 MMIO 基址.
         */
        VirtioDriverBase(DevRes res, ProbeInfo probe_info,
                         util::owner<Transport *> transport) noexcept;
        /**
         * @brief 销毁 virtio 通用设备对象并回收队列 DMA 资源.
         */
        ~VirtioDriverBase() noexcept override;

        [[nodiscard]]
        bool legacy() const noexcept {
            return _probe_info.legacy;
        }

        [[nodiscard]]
        DeviceType device_type() const noexcept {
            return static_cast<DeviceType>(_probe_info.device_id);
        }

        [[nodiscard]]
        u32 device_id() const noexcept {
            return _probe_info.device_id;
        }

        [[nodiscard]]
        u32 vendor_id() const noexcept {
            return _probe_info.vendor_id;
        }

        [[nodiscard]]
        u64 device_features() const noexcept {
            return _device_features;
        }

        [[nodiscard]]
        u64 negotiated_features() const noexcept {
            return _negotiated_features;
        }

    protected:
        /**
         * @brief 执行 virtio 通用初始化前半段.
         *
         * 前置条件是设备已被识别为当前驱动支持的 transport 版本.
         * 成功后保证 feature 交集已经写回, 且设备接受 `FEATURES_OK`.
         *
         * @param supported_features 驱动声明支持的 feature 集合.
         */
        [[nodiscard]]
        Result<void> begin_init(u64 supported_features) noexcept;
        /**
         * @brief 执行 virtio 通用初始化收尾阶段.
         *
         * 成功后保证设备状态进入 `DRIVER_OK`.
         */
        [[nodiscard]]
        Result<void> finish_init() noexcept;

        /**
         * @brief 从设备私有配置空间读取一个 8 位值.
         */
        [[nodiscard]]
        Result<u8> read_config_u8(size_t config_offset) const noexcept;
        /**
         * @brief 从设备私有配置空间读取一个 16 位值.
         */
        [[nodiscard]]
        Result<u16> read_config_u16(size_t config_offset) const noexcept;
        /**
         * @brief 从设备私有配置空间读取一个 32 位值.
         */
        [[nodiscard]]
        Result<u32> read_config_u32(size_t config_offset) const noexcept;
        /**
         * @brief 从设备私有配置空间读取一个 64 位值.
         *
         * 当前通过两次 32 位读取拼接结果, 贴合 MMIO 寄存器宽度.
         */
        [[nodiscard]]
        Result<u64> read_config_u64(size_t config_offset) const noexcept;

        /**
         * @brief 初始化一条 legacy virtqueue.
         *
         * 该函数负责分配连续 DMA 区域、构造自由描述符链并将 PFN
         * 写入设备寄存器.
         */
        [[nodiscard]]
        Result<VirtQueueLegacy *> init_queue_legacy(
            u16 queue_index, u16 requested_size) noexcept;
        [[nodiscard]]
        Result<VirtQueueLegacy *> init_queue_modern(
            u16 queue_index, u16 requested_size) noexcept;
        /**
         * @brief 为一组缓冲区分配并构造一条 legacy 描述符链.
         */
        [[nodiscard]]
        Result<u16> queue_add_chain_legacy(
            VirtQueueLegacy &queue,
            const std::vector<vring::BufferView> &buffers) noexcept;
        /**
         * @brief 将描述符链头加入 avail ring.
         */
        Result<void> queue_submit_legacy(VirtQueueLegacy &queue,
                                         u16 head_desc) noexcept;
        /**
         * @brief 向设备发送一次 queue notify.
         */
        Result<void> queue_notify_legacy(VirtQueueLegacy &queue) noexcept;
        /**
         * @brief 判断设备是否已经在 used ring 中回收新的请求.
         */
        [[nodiscard]]
        Result<bool> queue_can_pop_legacy(VirtQueueLegacy &queue) noexcept;
        /**
         * @brief 从 used ring 中取出一个已完成请求.
         */
        [[nodiscard]]
        Result<VirtQueueUsedElemLegacy> queue_pop_used_legacy(
            VirtQueueLegacy &queue) noexcept;
        /**
         * @brief 将一条描述符链归还到自由链表.
         */
        Result<void> queue_free_chain_legacy(VirtQueueLegacy &queue,
                                             u16 head_desc) noexcept;
        /**
         * @brief 提交缓冲区链并以轮询方式等待设备完成.
         *
         * 成功后保证该请求对应的描述符链已经回收到自由链表.
         */
        [[nodiscard]]
        Result<VirtQueueUsedElemLegacy> queue_submit_and_poll_legacy(
            VirtQueueLegacy &queue,
            const std::vector<vring::BufferView> &buffers) noexcept;

        /**
         * @brief 分配一段连续 DMA 缓冲区.
         */
        [[nodiscard]]
        Result<DmaBuffer> alloc_dma_buffer(size_t size) noexcept;
        /**
         * @brief 释放由 `alloc_dma_buffer()` 分配的 DMA 缓冲区.
         */
        void free_dma_buffer(DmaBuffer &buffer) noexcept;

        /**
         * @brief 读取指定偏移处的 32 位公共寄存器.
         */
        [[nodiscard]]
        u32 read_reg32(size_t reg_offset) const noexcept;
        /**
         * @brief 写入指定偏移处的 32 位公共寄存器.
         */
        void write_reg32(size_t reg_offset, u32 value) noexcept;

        /**
         * @brief 读取设备状态寄存器.
         */
        [[nodiscard]]
        u32 status() const noexcept;
        /**
         * @brief 覆盖写入设备状态寄存器.
         */
        void set_status(u32 status) noexcept;

        /**
         * @brief 读取设备当前暴露的全部公共 feature 位.
         */
        [[nodiscard]]
        Result<u64> read_device_features() noexcept;
        /**
         * @brief 将驱动协商后的公共 feature 位写回设备.
         */
        Result<void> write_driver_features(u64 features) noexcept;

        ProbeInfo _probe_info;
        util::owner<Transport *> _transport =
            util::owner<Transport *>(nullptr);
        u64 _device_features     = 0;
        u64 _negotiated_features = 0;
        std::vector<VirtQueueLegacy> _queues;
    };

    /**
     * @brief virtio 具体设备子工厂接口.
     *
     * 用于在 `virtio,mmio` 总工厂内部按 `device_id` 二次分发.
     */
    class IVirtioDeviceFactory {
    public:
        virtual ~IVirtioDeviceFactory() = default;

        /**
         * @brief 返回该子工厂服务的 virtio 设备类型.
         */
        [[nodiscard]]
        virtual DeviceType device_type() const noexcept = 0;

        /**
         * @brief 在设备类型命中后再次裁决是否接管该节点.
         */
        [[nodiscard]]
        virtual bool probe(const device::DeviceNode &node,
                           device::DeviceModel &model,
                           const ProbeInfo &info) const noexcept = 0;

        /**
         * @brief 基于通用探测结果创建具体 virtio 驱动对象.
         */
        [[nodiscard]]
        virtual Result<driver::DriverBase *> create(
            const device::DeviceNode &node, device::DeviceModel &model,
            const ProbeInfo &info) const = 0;
    };

    /**
     * @brief virtio MMIO 通用工厂.
     *
     * 负责识别 `virtio,mmio` 节点, 收敛公共探测信息, 然后转发给
     * 具体 virtio 子工厂.
     */
    class VirtioMmioFactory final : public driver::IDeviceFactory {
    public:
        [[nodiscard]]
        const driver::DeviceId &device_id() const noexcept override;

        /**
         * @brief 在普通设备工厂匹配阶段识别合法 virtio-mmio 节点.
         */
        [[nodiscard]]
        bool probe(const device::DeviceNode &node,
                   device::DeviceModel &model,
                   b64 driver_flag) const noexcept override;

        /**
         * @brief 执行通用探测并向对应 virtio 子工厂转发创建请求.
         */
        [[nodiscard]]
        Result<driver::DriverBase *> create(
            const device::DeviceNode &node,
            device::DeviceModel &model,
            b64 driver_flag) const override;
    };

    /**
     * @brief 向 virtio 子工厂注册表登记一个具体设备工厂.
     */
    [[nodiscard]]
    bool register_factory(const IVirtioDeviceFactory &factory) noexcept;
    /**
     * @brief 判断给定 magic 是否匹配 virtio-mmio 规范.
     */
    [[nodiscard]]
    bool is_virtio_magic(u32 magic_value) noexcept;
    /**
     * @brief 判断当前实现是否支持给定 virtio 版本号.
     */
    [[nodiscard]]
    bool is_supported_version(u32 version) noexcept;
    /**
     * @brief 将版本号转换为 transport 名称.
     */
    [[nodiscard]]
    const char *transport_version_name(u32 version) noexcept;
    /**
     * @brief 将 `device_id` 转换为人类可读的设备类型名称.
     */
    [[nodiscard]]
    const char *device_type_name(u32 device_id) noexcept;
    /**
     * @brief 将状态寄存器值转换为调试用状态名称.
     */
    [[nodiscard]]
    const char *device_status_name(u32 status) noexcept;
    /**
     * @brief 对给定节点执行一次只读 virtio-mmio 通用探测.
     *
     * 该函数不会创建设备对象, 只负责收敛日志与公共元信息.
     */
    [[nodiscard]]
    Result<ProbeInfo> probe_mmio_device(const device::DeviceNode &node) noexcept;
}  // namespace virtio
