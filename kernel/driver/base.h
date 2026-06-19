/**
 * @file base.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 驱动基类
 * @version alpha-1.0.0
 * @date 2026-05-31
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cap/cholder.h>
#include <device/device.h>
#include <device/resource.h>

#include <string_view>

namespace driver {
    /**
     * @brief 驱动对象基本类
     *
     * 该类不拥有 DeviceNode, 仅通过统一语义接口访问底层设备属性.
     */
    class DriverBase {
    public:
        /**
         * @brief 驱动持有的资源集合.
         *
         * Factory 应在创建设备前完成资源提取, 并通过该对象将节点与资源
         * 一并移交给驱动实例.
         */
        struct DevRes {
            const device::DeviceNode *node;
            std::vector<util::owner<device::VIrqResource *>> virqs;
            std::vector<util::owner<device::MMIOResource *>> mmios;

            /**
             * @brief 构造驱动资源包.
             *
             * @param node 设备节点引用.
             * @param virqs 设备虚拟中断资源列表.
             * @param mmios 设备 MMIO 资源列表.
             */
            DevRes(const device::DeviceNode &node,
                    std::vector<util::owner<device::VIrqResource *>> &&virqs,
                    std::vector<util::owner<device::MMIOResource *>> &&mmios)
                noexcept
                : node(&node),
                  virqs(std::move(virqs)),
                  mmios(std::move(mmios)) {}

            DevRes(const DevRes &)            = delete;
            DevRes &operator=(const DevRes &) = delete;

            /**
             * @brief 移动构造驱动资源包.
             *
             * @param other 待转移资源包.
             */
            DevRes(DevRes &&other) noexcept
                : node(other.node),
                  virqs(std::move(other.virqs)),
                  mmios(std::move(other.mmios)) {}
            /**
             * @brief 移动赋值驱动资源包.
             *
             * @param other 待转移资源包.
             * @return DevRes& 当前资源包引用.
             */
            DevRes &operator=(DevRes &&other) noexcept
            {
                if (this != &other) {
                    node = other.node;
                    virqs = std::move(other.virqs);
                    mmios = std::move(other.mmios);
                }
                return *this;
            }
        };

        /**
         * @brief 驱动对象基本类
         *
         * @param res 设备节点与资源集合.
         */
        explicit DriverBase(DevRes res) noexcept;

        /**
         * @brief 销毁驱动对象基本类
         */
        virtual ~DriverBase() noexcept;

        /**
         * @brief 获取底层设备节点.
         *
         * @return device::DeviceNode& 对应设备节点引用.
         */
        [[nodiscard]]
        const device::DeviceNode &node() const noexcept {
            return *_node;
        }

        /**
         * @brief 获取设备平台名称.
         *
         * @return const char* 平台名称字符串.
         */
        [[nodiscard]]
        device::DevicePlatform platform() const noexcept {
            return _node != nullptr ? _node->platform()
                                    : device::DevicePlatform::FDT;
        }

        /**
         * @brief 返回驱动对象的自描述 compatible 字符串.
         *
         * 该接口仅用于少量运行时调试/兼容判断，不参与工厂匹配。
         */
        [[nodiscard]]
        virtual std::string_view compatible() const noexcept {
            return {};
        }

        /**
         * @brief 获取驱动名称
         *
         * @return const char* 驱动名称
         */
        [[nodiscard]]
        virtual const char *name() const noexcept {
            return _node != nullptr ? _node->name() : "unknown";
        }

        /**
         * @brief 在 devfs 目录上挂载驱动导出的设备节点.
         *
         * @param devdir 驱动私有 holder 中的设备目录能力.
         * @return Result<void> 挂载结果.
         */
        [[nodiscard]]
        virtual Result<void> mount(CapIdx devdir) noexcept {
            (void)devdir;
            void_return();
        }

    protected:
        const device::DeviceNode *_node = nullptr;
        std::vector<util::owner<device::VIrqResource *>> _virqs;
        std::vector<util::owner<device::MMIOResource *>> _mmios;
        cap::CHolder *_holder = nullptr;

        /**
         * @brief 获取当前驱动持有的 virq 资源列表.
         *
         * @return const std::vector<util::owner<device::VIrqResource *>>& virq
         * 资源列表.
         */
        [[nodiscard]]
        const std::vector<util::owner<device::VIrqResource *>> &virq_resources()
            const noexcept {
            return _virqs;
        }

        /**
         * @brief 获取当前驱动持有的 MMIO 资源列表.
         *
         * @return const std::vector<util::owner<device::MMIOResource *>>&
         * MMIO 资源列表.
         */
        [[nodiscard]]
        const std::vector<util::owner<device::MMIOResource *>> &mmio_resources()
            const noexcept {
            return _mmios;
        }

        [[nodiscard]]
        cap::CHolder &holder() const noexcept {
            assert(_holder != nullptr);
            return *_holder;
        }

        void bind_holder(cap::CHolder &holder) noexcept {
            _holder = &holder;
        }

        /**
         * @brief 加载整数
         *
         * @param node 设备节点
         * @param prop_name 属性名称
         * @param integral_sz 整数大小
         * @return Result<sus_u64> 属性值
         */
        static Result<sus_u64> __load_integral(const device::DeviceNode &node,
                                               std::string_view prop_name,
                                               size_t integral_sz) noexcept;

        friend class DriverModel;
    };

    /**
     * @brief 固定偏移 MMIO 访问辅助类.
     *
     * @tparam T 寄存器值类型.
     * @tparam offset 相对基址的固定偏移.
     */
    template <typename T, size_t offset>
    class mmio_reference {
    private:
        volatile char *_base;

    public:
        /**
         * @brief 绑定固定偏移寄存器引用.
         *
         * @param base MMIO 映射基址.
         */
        explicit mmio_reference(volatile char *base) noexcept : _base(base) {}

        /**
         * @brief 读取寄存器值.
         *
         * @return T 当前寄存器值.
         */
        [[nodiscard]]
        T read() const noexcept {
            return *reinterpret_cast<volatile T *>(_base + offset);
        }

        /**
         * @brief 写入寄存器值.
         *
         * @param value 待写入的寄存器值.
         */
        void write(T value) noexcept {
            *reinterpret_cast<volatile T *>(_base + offset) = value;
        }

        /**
         * @brief 通过赋值语法写入寄存器值.
         *
         * @param value 待写入的寄存器值.
         * @return mmio_reference& 当前寄存器引用.
         */
        mmio_reference &operator=(T value) noexcept {
            write(value);
            return *this;
        }

        /**
         * @brief 隐式读取寄存器值.
         *
         * @return T 当前寄存器值.
         */
        operator T() const noexcept {
            return read();
        }
    };

    /**
     * @brief 运行时偏移 MMIO 访问辅助类.
     *
     * @tparam T 寄存器值类型.
     */
    template <typename T>
    class mmio_offset_reference {
    private:
        volatile char *_base   = nullptr;
        size_t _offset = 0;

    public:
        /**
         * @brief 构造运行时偏移寄存器引用.
         *
         * @param base MMIO 映射基址.
         * @param offset 相对基址的运行时偏移.
         */
        explicit mmio_offset_reference(volatile char *base, size_t offset) noexcept
            : _base(base), _offset(offset) {}

        /**
         * @brief 读取寄存器值.
         *
         * @return T 当前寄存器值.
         */
        [[nodiscard]]
        T read() const noexcept {
            return *reinterpret_cast<volatile T *>(_base + _offset);
        }

        /**
         * @brief 写入寄存器值.
         *
         * @param value 待写入的寄存器值.
         */
        void write(T value) noexcept {
            *reinterpret_cast<volatile T *>(_base + _offset) = value;
        }

        /**
         * @brief 通过赋值语法写入寄存器值.
         *
         * @param value 待写入的寄存器值.
         * @return mmio_offset_reference& 当前寄存器引用.
         */
        mmio_offset_reference &operator=(T value) noexcept {
            write(value);
            return *this;
        }

        /**
         * @brief 隐式读取寄存器值.
         *
         * @return T 当前寄存器值.
         */
        operator T() const noexcept {
            return read();
        }
    };
}  // namespace driver
