/**
 * @file constructors.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核 C++ 运行时启动初始化
 * @version 0.1.0-dev.1
 * @date 2026-08-03
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <boot/common/bytes.h>
#include <boot/early_priv.h>
#include <boot/sections.h>
#include <log.h>
#include <tay/array.h>

namespace __init {
    class EarlyConstructorProbe {
    private:
        size_t magic_number_;

    public:
        BOOT_INIT_TEXT EarlyConstructorProbe(size_t magic_number) noexcept
            : magic_number_(magic_number) {}

        [[nodiscard]]
        BOOT_INIT_TEXT constexpr size_t magic_number() const noexcept {
            return magic_number_;
        }
    };

    constexpr size_t CONSTRUCTOR_MAGIC = 0xDEADBEEF;

    BOOT_INIT_BSS EarlyConstructorProbe __constructor_probe(CONSTRUCTOR_MAGIC);
    // __bss_cleared 是用于标记 BSS 段是否已清零的全局变量
    // 因此应当放置在 DATA 段中
    // 并在 BSS 清零后立即设置为 true, 以便在后续的初始化过程中进行检查
    // (不放在 BSS 中是为了避免在 BSS 清零前被误判为已清零)
    BOOT_INIT_DATA constinit bool __bss_cleared            = false;
    BOOT_INIT_BSS constinit bool __initializer_called      = false;
    BOOT_INIT_BSS constinit size_t __valid_initializer_cnt = 0;

    extern "C" char s_bss[], e_bss[], s_init_bss[], e_init_bss[];

    using initializer_t = void (*)();
    extern "C" initializer_t __preinit_array_start[], __preinit_array_end[];
    extern "C" initializer_t __init_array_start[], __init_array_end[];

    BOOT_INIT_TEXT void run_initializer(initializer_t initializer) noexcept {
        if (initializer != nullptr) {
            initializer();
            ++__valid_initializer_cnt;
        }
    }

    BOOT_INIT_TEXT void run_initializers() noexcept {
        if (__initializer_called) {
            kernel::log::error("C++ 初始化器被执行多次, 已忽略");
            return;
        }

        const tay::array_view<initializer_t> preinit_array(__preinit_array_start,
                                                           __preinit_array_end);
        const tay::array_view<initializer_t> init_array(__init_array_start, __init_array_end);

        preinit_array.foreach (run_initializer);
        init_array.foreach (run_initializer);
        __initializer_called = true;

        kernel::log::debug("C++ 初始化器已执行, 调用了 {} 个有效初始化器", __valid_initializer_cnt);
        if (__constructor_probe.magic_number() != CONSTRUCTOR_MAGIC) {
            kernel::log::panic("C++ constructor probe 魔数损坏");
        }
    }

    BOOT_INIT_TEXT void clear_bss() noexcept {
        if (__bss_cleared) {
            kernel::log::error("BSS 被清零多次, 已忽略");
            return;
        }
        __early_clear(reinterpret_cast<void *>(s_bss), reinterpret_cast<void *>(e_bss));
        __early_clear(reinterpret_cast<void *>(s_init_bss), reinterpret_cast<void *>(e_init_bss));
        __bss_cleared = true;
    }
}  // namespace __init

namespace boot::early_internal {
    BOOT_INIT_TEXT void clear_bss() noexcept {
        __init::clear_bss();
    }

    BOOT_INIT_TEXT void run_initializers() noexcept {
        __init::run_initializers();
    }
}  // namespace boot::early_internal
