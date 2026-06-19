/**
 * @file virtio-blk.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief virtio 块设备驱动
 * @version alpha-1.0.0
 * @date 2026-06-10
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <device/resource.h>
#include <device/pci.h>
#include <bio/blk.h>
#include <driver/virtio/virtio-blk.h>
#include <logger.h>
#include <sus/raii.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
    virtio::VirtioBlkDriverFactory g_virtio_blk_factory;
    bool g_virtio_blk_factory_initialized = false;

    /**
     * @brief 当前块驱动声明支持的 feature 集.
     *
     * 只保留当前调试读块路径真正会受益的特性位.
     */
    constexpr virtio::u64 SUPPORTED_BLK_FEATURES =
        virtio::feature::MASK_SIZE_MAX | virtio::feature::MASK_SEG_MAX |
        virtio::feature::MASK_GEOMETRY | virtio::feature::MASK_RO |
        virtio::feature::MASK_BLK_SIZE | virtio::feature::MASK_FLUSH |
        virtio::feature::MASK_TOPOLOGY | virtio::feature::MASK_CONFIG_WCE |
        virtio::feature::MASK_MQ | virtio::feature::MASK_DISCARD |
        virtio::feature::MASK_WRITE_ZEROES |
        virtio::common_feature::RING_INDIRECT_DESC |
        virtio::common_feature::RING_EVENT_IDX;

    constexpr size_t DEBUG_DUMP_OFFSET = 0x400;
    constexpr size_t DEBUG_DUMP_SIZE   = 0x200;
    constexpr size_t DEBUG_LINE_BYTES  = 16;

    /**
     * @brief 将一个字节转换为 hexdump 可显示字符.
     */
    [[nodiscard]]
    char printable_byte(unsigned char ch) noexcept {
        return isprint(ch) != 0 ? static_cast<char>(ch) : '.';
    }
}  // namespace

namespace virtio {
    VirtioBlkDriver::VirtioBlkDriver(DevRes res, ProbeInfo probe_info,
                                     util::owner<Transport *> transport) noexcept
        : VirtioDriverBase(std::move(res), probe_info, std::move(transport)),
          _config(),
          _request_queue(nullptr),
          _request_header(),
          _status_byte(),
          _block_size(DEFAULT_BLOCK_SIZE),
          _sector_size(DEFAULT_SECTOR_SIZE),
          _capacity_sectors(0) {}

    VirtioBlkDriver::~VirtioBlkDriver() noexcept {
        // 请求头与状态字节由驱动独占持有, 由析构统一回收.
        free_dma_buffer(_request_header);
        free_dma_buffer(_status_byte);
    }

    Result<void> VirtioBlkDriver::init() noexcept {
        // 先完成 virtio 通用初始化, 再加载块设备配置并创建 request 队列.
        auto init_res = begin_init(SUPPORTED_BLK_FEATURES);
        propagate(init_res);

        auto config_res = load_config();
        propagate(config_res);

        Result<VirtQueueLegacy *> queue_res = std::unexpected(
            ErrCode::NOT_SUPPORTED);
        if (_transport.get() != nullptr &&
            _transport->kind() == Transport::Kind::PCI)
        {
            auto *transport_pci = static_cast<TransportPCI *>(_transport.get());
            queue_res = init_queue_modern(REQUEST_QUEUE_INDEX, 8);
        } else {
            queue_res = init_queue_legacy(REQUEST_QUEUE_INDEX, 8);
        }
        propagate(queue_res);
        _request_queue = queue_res.value();

        auto header_res = alloc_dma_buffer(sizeof(BlkReq));
        propagate(header_res);
        _request_header = header_res.value();

        auto status_res = alloc_dma_buffer(sizeof(u8));
        propagate(status_res);
        _status_byte = status_res.value();

        auto finish_res = finish_init();
        propagate(finish_res);

        loggers::DEVICE::INFO(
            "VirtioBlkDriver 初始化完成: node=%s capacity_sectors=%llu block_size=%llu",
            name(), static_cast<unsigned long long>(_capacity_sectors),
            static_cast<unsigned long long>(_block_size));
        void_return();
    }

    Result<void> VirtioBlkDriver::load_config() noexcept {
        // 当前只读取调试读块路径真正需要的关键字段, 避免过早依赖完整配置面.
        auto capacity_res = read_config_u64(offsetof(BlkConfig, capacity));
        propagate(capacity_res);
        _config.capacity = capacity_res.value();

        auto size_max_res = read_config_u32(offsetof(BlkConfig, size_max));
        propagate(size_max_res);
        _config.size_max = size_max_res.value();

        auto seg_max_res = read_config_u32(offsetof(BlkConfig, seg_max));
        propagate(seg_max_res);
        _config.seg_max = seg_max_res.value();

        auto blk_size_res = read_config_u32(offsetof(BlkConfig, blk_size));
        propagate(blk_size_res);
        _config.blk_size = blk_size_res.value();

        auto num_queues_res = read_config_u16(offsetof(BlkConfig, num_queues));
        propagate(num_queues_res);
        _config.num_queues = num_queues_res.value();

        _capacity_sectors = static_cast<size_t>(_config.capacity);
        _block_size =
            _config.blk_size != 0 ? static_cast<size_t>(_config.blk_size)
                                  : DEFAULT_BLOCK_SIZE;
        _sector_size = DEFAULT_SECTOR_SIZE;

        if (_block_size == 0 || (_block_size % _sector_size) != 0) {
            loggers::DEVICE::ERROR(
                "virtio-blk 非法块大小: node=%s block_size=%llu sector_size=%llu",
                name(), static_cast<unsigned long long>(_block_size),
                static_cast<unsigned long long>(_sector_size));
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        void_return();
    }

    Result<size_t> VirtioBlkDriver::block_sz() const noexcept {
        return _block_size;
    }

    Result<size_t> VirtioBlkDriver::block_cnt() const noexcept {
        if (_block_size == 0) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        const auto total_bytes =
            static_cast<unsigned long long>(_capacity_sectors) * _sector_size;
        return static_cast<size_t>(total_bytes / _block_size);
    }

    Result<bool> VirtioBlkDriver::readonly() const {
        return (negotiated_features() & feature::MASK_RO) != 0;
    }

    Result<void> VirtioBlkDriver::bind_request_queue(
        util::nonnull<blk::BlockRequestQueue *> queue) {
        if (_queue != nullptr && _queue != queue.get()) {
            unexpect_return(ErrCode::BUSY);
        }
        _queue = queue.get();
        void_return();
    }

    Result<void> VirtioBlkDriver::process_request(blk::BlockRequest &req) {
        if (_queue == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        Result<size_t> result = std::unexpected(ErrCode::INVALID_PARAM);
        switch (req.op) {
            case blk::BlockOp::READ:
                result = read_blocks(req.lba, req.block_count, req.buffer);
                break;
            case blk::BlockOp::WRITE:
                result = write_blocks(req.lba, req.block_count, req.buffer);
                break;
            case blk::BlockOp::FLUSH: {
                auto flush_res = submit_flush();
                if (flush_res.has_value()) {
                    result = 0;
                } else {
                    result = std::unexpected(flush_res.error());
                }
                break;
            }
            default: break;
        }

        auto complete_res =
            _queue->complete(util::nnullforce(&req), std::move(result));
        propagate(complete_res);
        void_return();
    }

    Result<void> VirtioBlkDriver::run_request_loop() {
        if (_queue == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        while (true) {
            auto req_res = _queue->wait_and_dequeue();
            if (!req_res.has_value()) {
                if (req_res.error() == ErrCode::FUTURE_CANCLED) {
                    break;
                }
                propagate_return(req_res);
            }

            auto *req = req_res.value();
            auto mark_res = _queue->mark_processing(util::nnullforce(req));
            propagate(mark_res);
            auto process_res = process_request(*req);
            propagate(process_res);
        }
        void_return();
    }

    Result<size_t> VirtioBlkDriver::submit_rw(u32 req_type, size_t sector,
                                              void *buffer, size_t bytes,
                                              bool device_writes) noexcept {
        // 先构造 virtio-blk 请求头与状态字节, 再把 header/data/status
        // 组织成一条三段描述符链提交到 request 队列.
        if (_request_queue == nullptr || !_request_header.kaddr ||
            !_status_byte.kaddr)
        {
            unexpect_return(ErrCode::FAILURE);
        }
        if (bytes == 0) {
            return 0;
        }

        auto *header = static_cast<BlkReq *>(_request_header.kaddr);
        auto *status = static_cast<u8 *>(_status_byte.kaddr);
        header->type     = req_type;
        header->reserved = 0;
        header->sector   = sector;
        *status          = 0xFF;

        auto data_paddr = convert_pointer(static_cast<char *>(buffer));
        std::vector<vring::BufferView> bufs;
        bufs.reserve(3);
        bufs.push_back(vring::BufferView{
            .paddr    = _request_header.paddr,
            .size     = sizeof(BlkReq),
            .writable = false,
        });
        bufs.push_back(vring::BufferView{
            .paddr    = data_paddr,
            .size     = bytes,
            .writable = device_writes,
        });
        bufs.push_back(vring::BufferView{
            .paddr    = _status_byte.paddr,
            .size     = sizeof(u8),
            .writable = true,
        });

        // TODO: 当前为了先接入块设备模型, 请求完成仍采用同步轮询.
        // 后续将改为 virtio-blk 中断驱动完成路径, 以避免 busy waiting.
        // 若先做过渡实现, 可在中断回调中采用“关中断 - 复制数据 - 开中断”的
        // 临时简化策略, 然后再继续细化临界区粒度.
        auto used_res = queue_submit_and_poll_legacy(*_request_queue, bufs);
        propagate(used_res);

        // 最后检查设备返回的状态字节, 明确区分传输成功与设备级失败.
        if (*status != reqstatus::OK) {
            loggers::DEVICE::ERROR(
                "virtio-blk 请求失败: node=%s type=%u sector=%llu status=%u",
                name(), static_cast<unsigned>(req_type),
                static_cast<unsigned long long>(sector),
                static_cast<unsigned>(*status));
            unexpect_return(ErrCode::IO_ERROR);
        }
        return bytes;
    }

    Result<size_t> VirtioBlkDriver::read_blocks(size_t lba, size_t block_count,
                                                void *buffer) noexcept {
        // 先做边界与对齐前置条件检查, 成功后按逻辑块换算为底层扇区请求.
        if (buffer == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        if (_block_size == 0 || _sector_size == 0 ||
            (_block_size % _sector_size) != 0)
        {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        const auto total_blocks = block_cnt();
        propagate(total_blocks);
        if (lba >= total_blocks.value() ||
            block_count > total_blocks.value() - lba)
        {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }

        const size_t sectors_per_block = _block_size / _sector_size;
        const size_t sector            = lba * sectors_per_block;
        const size_t bytes             = block_count * _block_size;
        return submit_rw(reqtype::IN, sector, buffer, bytes, true);
    }

    Result<size_t> VirtioBlkDriver::write_blocks(size_t lba, size_t block_count,
                                                 const void *buffer) noexcept {
        // 写路径与读路径共享同一套边界检查与扇区换算规则.
        if (buffer == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        if (_block_size == 0 || _sector_size == 0 ||
            (_block_size % _sector_size) != 0)
        {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        const auto total_blocks = block_cnt();
        propagate(total_blocks);
        if (lba >= total_blocks.value() ||
            block_count > total_blocks.value() - lba)
        {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }

        const size_t sectors_per_block = _block_size / _sector_size;
        const size_t sector            = lba * sectors_per_block;
        const size_t bytes             = block_count * _block_size;
        return submit_rw(reqtype::OUT, sector, const_cast<void *>(buffer), bytes,
                         false);
    }

    Result<void> VirtioBlkDriver::submit_flush() noexcept {
        // flush 请求没有数据段, 只需要 header 与 status 两段描述符.
        if (_request_queue == nullptr || !_request_header.kaddr ||
            !_status_byte.kaddr)
        {
            unexpect_return(ErrCode::FAILURE);
        }

        auto *header = static_cast<BlkReq *>(_request_header.kaddr);
        auto *status = static_cast<u8 *>(_status_byte.kaddr);
        header->type     = reqtype::FLUSH;
        header->reserved = 0;
        header->sector   = 0;
        *status          = 0xFF;

        std::vector<vring::BufferView> bufs;
        bufs.reserve(2);
        bufs.push_back(vring::BufferView{
            .paddr    = _request_header.paddr,
            .size     = sizeof(BlkReq),
            .writable = false,
        });
        bufs.push_back(vring::BufferView{
            .paddr    = _status_byte.paddr,
            .size     = sizeof(u8),
            .writable = true,
        });

        // TODO: flush 完成路径当前同样通过同步轮询等待, 后续将统一迁移到
        // 基于 virtio-blk 完成中断的请求收尾流程.
        auto used_res = queue_submit_and_poll_legacy(*_request_queue, bufs);
        propagate(used_res);
        if (*status != reqstatus::OK) {
            unexpect_return(ErrCode::IO_ERROR);
        }
        void_return();
    }

    Result<void> VirtioBlkDriver::__debug_print_blocks() noexcept {
        // 先按整块读取覆盖目标窗口的最小块范围, 再只截取需要对比的字节区间.
        const auto total_blocks = block_cnt();
        propagate(total_blocks);
        if (total_blocks.value() == 0) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        const size_t first_block = DEBUG_DUMP_OFFSET / _block_size;
        const size_t end_offset  = DEBUG_DUMP_OFFSET + DEBUG_DUMP_SIZE;
        const size_t last_block  = (end_offset + _block_size - 1) / _block_size;
        const size_t block_count = last_block - first_block;

        std::vector<u8> blocks(block_count * _block_size, 0);
        auto read_res = read_blocks(first_block, block_count, blocks.data());
        propagate(read_res);

        const size_t window_offset = DEBUG_DUMP_OFFSET - first_block * _block_size;
        const u8 *window           = blocks.data() + window_offset;
        loggers::DEVICE::INFO(
            "virtio-blk 调试窗口: node=%s image_offset=[0x%llx,0x%llx)",
            name(), static_cast<unsigned long long>(DEBUG_DUMP_OFFSET),
            static_cast<unsigned long long>(DEBUG_DUMP_OFFSET + DEBUG_DUMP_SIZE));

        // 最后按 hexdump 风格输出固定窗口, 便于与宿主机镜像逐行对比.
        std::array<char, 128> line{};
        for (size_t line_offset = 0; line_offset < DEBUG_DUMP_SIZE;
             line_offset += DEBUG_LINE_BYTES)
        {
            int pos = snprintf(
                line.data(), line.size(), "%08llx  ",
                static_cast<unsigned long long>(DEBUG_DUMP_OFFSET + line_offset));
            for (size_t i = 0; i < DEBUG_LINE_BYTES; ++i) {
                pos += snprintf(
                    line.data() + pos, line.size() - static_cast<size_t>(pos),
                    "%02x%s", static_cast<unsigned>(window[line_offset + i]),
                    i == 7 ? "  " : " ");
            }
            pos += snprintf(line.data() + pos,
                            line.size() - static_cast<size_t>(pos), " |");
            for (size_t i = 0; i < DEBUG_LINE_BYTES; ++i) {
                line[static_cast<size_t>(pos)] =
                    printable_byte(window[line_offset + i]);
                ++pos;
            }
            line[static_cast<size_t>(pos)] = '|';
            ++pos;
            line[static_cast<size_t>(pos)]   = '\0';
            loggers::DEVICE::INFO("%s", line.data());
        }
        void_return();
    }

    bool VirtioBlkDriverFactory::probe(const device::DeviceNode &node,
                                       device::DeviceModel &model,
                                       const ProbeInfo &info) const noexcept {
        (void)node;
        (void)model;
        return info.valid &&
               static_cast<DeviceType>(info.device_id) == DeviceType::BLOCK;
    }

    Result<driver::DriverBase *> VirtioBlkDriverFactory::create(
        const device::DeviceNode &node, device::DeviceModel &model,
        const ProbeInfo &info) const {
        (void)model;
        // 按 transport 构造具体访问后端, 然后再创建统一 virtio-blk 驱动对象.
        auto virqs = device::DevResManager::get_virq_resource(node);
        auto mmios = device::DevResManager::get_mmio_resource(node);
        util::owner<Transport *> transport =
            util::owner<Transport *>(nullptr);
        if (node.platform() == device::DevicePlatform::FDT) {
            if (mmios.empty()) {
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }

            auto *mmio = mmios.front().get();
            if (mmio == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }
            auto map_res = device::MMIOManager::inst().map_to_kernel(*mmio);
            propagate(map_res);
            transport = util::owner<Transport *>(
                new TransportMMIO(info, *mmio, map_res.value().as<char>()));
        } else if (node.platform() == device::DevicePlatform::PCI) {
            auto *pci_node = node.as<pci::PCIDeviceNode>();
            if (pci_node == nullptr) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            transport = util::owner<Transport *>(
                new TransportPCI(*pci_node, info));
        } else {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }

        auto *driver = new VirtioBlkDriver(
            driver::DriverBase::DevRes(node, std::move(virqs), std::move(mmios)),
            info, std::move(transport));
        if (driver == nullptr) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }

        auto init_res = driver->init();
        if (!init_res.has_value()) {
            delete driver;
            propagate_return(init_res);
        }

        auto dump_res = driver->__debug_print_blocks();
        if (!dump_res.has_value()) {
            loggers::DEVICE::ERROR(
                "virtio-blk 调试窗口读取失败: node=%s err=%s", node.name(),
                to_cstring(dump_res.error()));
        }

        if (!blk::BlkManager::initialized()) {
            loggers::DEVICE::ERROR("virtio-blk 创建失败: BlkManager 未初始化");
            delete driver;
            unexpect_return(ErrCode::FAILURE);
        }

        auto devno_res = blk::BlkManager::inst().register_device(
            util::nnullforce(static_cast<IBlockDeviceOps *>(driver)));
        if (!devno_res.has_value()) {
            delete driver;
            propagate_return(devno_res);
        }
        loggers::DEVICE::INFO("virtio-blk 已接入块层: node=%s devno=%u",
                              node.name(),
                              static_cast<unsigned>(devno_res.value()));
        return static_cast<driver::DriverBase *>(driver);
    }

    void init_virtio_blk_factory() noexcept {
        // 子工厂注册必须保持幂等, 避免 FDT provider 重复初始化时二次登记.
        if (g_virtio_blk_factory_initialized) {
            return;
        }
        g_virtio_blk_factory_initialized = register_factory(g_virtio_blk_factory);
    }
}  // namespace virtio
