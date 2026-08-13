/**
 * @file init.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief initialization for usrboot
 * @version 0.1.0-dev.1
 * @date 2026-08-11
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <init/symbols.h>
#include <log.h>
#include <tay/array.h>
#include <tay/attribute.h>
#include <tay/panic.h>

#include <cstddef>

namespace __init {
    class EarlyConstructorProbe {
    private:
        size_t magic_number_;

    public:
        EarlyConstructorProbe(size_t magic_number) noexcept : magic_number_(magic_number) {}

        [[nodiscard]]
        constexpr size_t magic_number() const noexcept {
            return magic_number_;
        }
    };

    constexpr size_t CONSTRUCTOR_MAGIC = 0xDEADBEEF;

    EarlyConstructorProbe __constructor_probe(CONSTRUCTOR_MAGIC);
    // __bss_cleared 是用于标记 BSS 段是否已清零的全局变量
    // 因此应当放置在 DATA 段中
    // 并在 BSS 清零后立即设置为 true, 以便在后续的初始化过程中进行检查
    // (不放在 BSS 中是为了避免在 BSS 清零前被误判为已清零)
    SECTION(".data") constinit bool __bss_cleared = false;
    constinit bool __initializer_called           = false;
    constinit size_t __valid_initializer_cnt      = 0;

    void run_initializer(initializer_t initializer) noexcept {
        if (initializer != nullptr) {
            initializer();
            ++__valid_initializer_cnt;
        }
    }

    void run_initializers() noexcept {
        if (__initializer_called) {
            logger::error("C++ 初始化器被执行多次, 已忽略");
            return;
        }

        const tay::array_view<initializer_t> preinit_array(__preinit_array_start,
                                                           __preinit_array_end);
        const tay::array_view<initializer_t> init_array(__init_array_start, __init_array_end);

        preinit_array.foreach (run_initializer);
        init_array.foreach (run_initializer);
        __initializer_called = true;

        logger::debug("C++ 初始化器已执行, 调用了 {} 个有效初始化器", __valid_initializer_cnt);
        if (__constructor_probe.magic_number() != CONSTRUCTOR_MAGIC) {
            logger::panic("C++ constructor probe 魔数损坏");
        }
    }

    void clear_bss() noexcept {
        if (__bss_cleared) {
            logger::error("BSS 被清零多次, 已忽略");
            return;
        }
        __early_clear(reinterpret_cast<void *>(s_bss), reinterpret_cast<void *>(e_bss));
        __bss_cleared = true;
    }

}  // namespace __init

[[noreturn]] void tay::panic(const char *message) noexcept {
    ::logger::panic("{}", message);
}

int main(int argc, char **argv);

extern "C" void __early_main() {
    __init::clear_bss();
    __init::run_initializers();

    int ret = main(0, nullptr);
    logger::panic("usrboot main() 返回了 {}", ret);
}
