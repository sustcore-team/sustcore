/**
 * @file main.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核Main函数
 * @version alpha-1.0.0
 * @date 2025-11-17
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <arch/description.h>
#if defined(__ARCH_riscv64__)
#include <arch/riscv64/csr.h>
#include <arch/riscv64/device/fdt_helper.h>
#include <arch/riscv64/device/platform.h>
#include <arch/riscv64/mem/sv39.h>
#elif defined(__ARCH_loongarch64__)
#include <arch/loongarch64/fdt_helper.h>
extern "C" char __la_boot_stack, __la_boot_stack_top;
#endif
#include <arch/trait.h>
#include <device/fdt.h>
#include <device/int.h>
#include <device/model.h>
#include <device/resource.h>
#include <env.h>
#include <kinit.h>
#include <libfdt.h>
#include <logger.h>
#include <mem/alloc.h>
#include <mem/gfp.h>
#include <mem/kaddr.h>
#include <mem/slub.h>
#include <mem/vma.h>
#include <sus/logger.h>
#include <sus/nonnull.h>
#include <sus/path.h>
#include <sus/raii.h>
#include <sus/tree.h>
#include <sus/types.h>
#include <sus/units.h>
#include <sustcore/addr.h>
#include <sustcore/boot.h>
#include <symbols.h>
#include <task/scheduler.h>
#include <task/task.h>
#include <task/wait.h>
#include <test/framework.h>

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstring>

env::PaddedHartContext __hart_context[MAX_HARTS] = {};

namespace env {
    static Environment _env;
    static bool _env_initialized = false;

    bool initialized() {
        return _env_initialized;
    }

    Environment &inst() {
        if (!initialized()) {
            panic("Environment 未初始化!");
        }
        return _env;
    }

    void construct() {
        // call the constructor here
        new (&_env) Environment();
        _env_initialized = true;
    }

    /**
     * @brief 初始化当前 hart 的运行时状态与定时器.
     */
    void init_hart() {
        if (!device::DeviceModel::initialized()) {
            panic("DeviceModel 尚未初始化, 当前 hart 上下文失败!");
            return;
        }

        [[maybe_unused]] auto &device_model = device::DeviceModel::inst();
        [[maybe_unused]] auto &irqman       = device_model.interrupt();
        [[maybe_unused]] auto &cpus         = device_model.cpus();
        [[maybe_unused]] auto *ctx          = hart_ctx;

        if (ctx == nullptr) {
            panic("当前 hart 上下文无效");
        }
        ctx->reset_runtime();
        loggers::SUSTCORE::INFO("初始化 hart %u",
                                static_cast<unsigned>(ctx->hart_id()));

        // 设置当前 hart 的 CPU 对象指针, 供后续 CLINT 定时器使用.
        device::Cpu *cpu = nullptr;
        for (auto &cpu_owner : cpus.cpus) {
            if (cpu_owner != nullptr &&
                cpu_owner->id() == static_cast<device::cpuid_t>(ctx->hart_id()))
            {
                cpu = cpu_owner.get();
                break;
            }
        }
        if (cpu == nullptr) {
            loggers::SUSTCORE::FATAL("无法为 hart %u 找到 CPU 对象",
                                     static_cast<unsigned>(ctx->hart_id()));
            panic("无法为 hart 找到 CPU 对象");
        }
        ctx->cpu() = cpu;

        auto ret = Initialization::init_clock();
        if (!ret.has_value()) {
            loggers::SUSTCORE::FATAL("初始化时钟失败: %s",
                                     to_cstring(ret.error()));
            panic("初始化时钟失败");
        }
    }
}  // namespace env

namespace {
    [[nodiscard]]
    void *bootinfo_fdt_ptr(const BootInfoHeader *bootinfo) noexcept {
        auto fdt_pa = bootinfo_fdt(bootinfo);
        if (!fdt_pa.nonnull()) {
            return nullptr;
        }
        return convert<KpaAddr>(fdt_pa).addr();
    }

    /**
     * @brief 固定周期触发调度器 tick 的到期动作.
     */
    class SchedulerTickAction final : public device::ExpireAction {
    public:
        /**
         * @brief 使用当前 deadline 与周期构造 tick 动作.
         *
         * @param deadline 首次到期时间.
         * @param period 周期长度.
         */
        SchedulerTickAction(units::time deadline, units::time period) noexcept
            : device::ExpireAction(deadline), _period(period) {}

        /**
         * @brief 处理一次调度 tick 并重新安排下一次到期时间.
         *
         * @param event 本次时钟事件.
         */
        void expire(const device::ClockEvent &event) noexcept override {
            TimerTickEvent tick_event{
                .last  = event.last,
                .now   = event.now,
                .delta = event.now - event.last,
            };
            schd::Scheduler::inst().do_tick(tick_event);

            auto *time_keeper = env::hart_ctx != nullptr
                                    ? env::hart_ctx->time_keeper()
                                    : nullptr;
            if (time_keeper == nullptr) {
                loggers::SUSTCORE::ERROR("SchedulerTickAction 缺少 TimeKeeper");
                return;
            }

            set_deadline(event.now + _period);
            revive();
            time_keeper->enqueue(util::owner<device::ExpireAction *>(this));
        }

    private:
        units::time _period{};
    };

#ifdef __CONF_KERNEL_TIMEKEEPER_TEST
    /**
     * @brief 指数级增长延迟的 TimeKeeper 调试动作.
     */
    class TimeKeeperLogAction final : public device::ExpireAction {
    public:
        /**
         * @brief 构造一个时间记录动作.
         *
         * @param deadline 首次到期时间.
         * @param interval 本次触发间隔.
         */
        TimeKeeperLogAction(units::time lasttime, units::time deadline,
                            units::time interval) noexcept
            : device::ExpireAction(deadline),
              _interval(interval),
              _lasttime(lasttime) {}

        /**
         * @brief 打印当前时间并安排下一次指数级触发.
         *
         * @param event 本次时钟事件.
         */
        void expire(const device::ClockEvent &event) noexcept override {
            // 计算实际触发间隔, 以验证 timer 的准确性.
            units::time real_interval = event.now - _lasttime;

            // 计算相对误差万分比
            // (real_interval - _interval) / _interval * 10000 => (real_interval
            // - _interval) * 10000 / _interval
            int64_t error_ppm = (real_interval - _interval) * 10000 / _interval;
            int64_t integral_part   = error_ppm / 100;
            int64_t fractional_part = error_ppm % 100;

            loggers::TIMER::DEBUG(
                "TimeKeeper 测试触发: 现在 = %llu ns, 计划间隔 = %llu ns, "
                "实际间隔 = %llu ns, 相对误差 = %d.%04d%%",
                static_cast<unsigned long long>(event.now.to_nanoseconds()),
                static_cast<unsigned long long>(_interval.to_nanoseconds()),
                static_cast<unsigned long long>(real_interval.to_nanoseconds()),
                static_cast<int>(integral_part),
                static_cast<int>(fractional_part));

            auto *time_keeper = env::hart_ctx != nullptr
                                    ? env::hart_ctx->time_keeper()
                                    : nullptr;
            if (time_keeper == nullptr) {
                loggers::TIMER::ERROR("TimeKeeperLogAction 缺少 TimeKeeper");
                return;
            }

            units::time next_interval = _interval * 2;
            _lasttime = event.now;
            _interval = next_interval;
            set_deadline(event.now + next_interval);
            revive();
            time_keeper->enqueue(
                util::owner<device::ExpireAction *>(this));
        }

    private:
        units::time _interval{};
        units::time _lasttime{};
    };
#endif

    /**
     * @brief 注册当前 hart 的周期性调度 tick 动作.
     */
    void register_scheduler_tick_action() {
        auto *time_keeper =
            env::hart_ctx != nullptr ? env::hart_ctx->time_keeper() : nullptr;
        if (time_keeper == nullptr || time_keeper->source() == nullptr) {
            loggers::SUSTCORE::FATAL("无法注册调度 tick 动作: TimeKeeper 无效");
            panic("无法注册调度 tick 动作");
        }

        constexpr units::time kTickPeriod = units::time::from_milliseconds(10);
        units::time now =
            time_keeper->source()->to_ns(time_keeper->source()->now());
        auto *action = new SchedulerTickAction(now + kTickPeriod, kTickPeriod);
        if (action == nullptr) {
            loggers::SUSTCORE::FATAL("无法分配 SchedulerTickAction");
            panic("无法分配 SchedulerTickAction");
        }
        time_keeper->enqueue(util::owner<device::ExpireAction *>(action));
    }

#ifdef __CONF_KERNEL_TIMEKEEPER_TEST
    /**
     * @brief 注册指数级增长的 TimeKeeper 日志测试动作.
     */
    void register_timekeeper_log_test() {
        auto *time_keeper =
            env::hart_ctx != nullptr ? env::hart_ctx->time_keeper() : nullptr;
        if (time_keeper == nullptr || time_keeper->source() == nullptr) {
            loggers::SUSTCORE::ERROR(
                "TimeKeeper 测试注册失败: TimeKeeper 无效");
            return;
        }

        constexpr units::time first_interval =
            units::time::from_milliseconds(10);
        units::time now =
            time_keeper->source()->to_ns(time_keeper->source()->now());
        auto *action =
            new TimeKeeperLogAction(now, now + first_interval, first_interval);
        if (action == nullptr) {
            loggers::SUSTCORE::FATAL("无法分配 TimeKeeperLogAction");
            panic("无法分配 TimeKeeperLogAction");
        }
        time_keeper->enqueue(util::owner<device::ExpireAction *>(action));
    }
#endif
}  // namespace

namespace key {
    using namespace env::key;
    struct main : public tmm, main_kernel_pgd, bootinfo {
    public:
        main() = default;
    };
}  // namespace key

Result<void> init_scheduler() {
    loggers::SUSTCORE::INFO("创建 KERNEL 进程");
    auto kernel_res = task::TaskManager::inst().create_kernel_task();
    if (!kernel_res.has_value()) {
        loggers::SUSTCORE::ERROR("创建KERNEL进程失败! 错误码: %s",
                                 to_cstring(kernel_res.error()));
        propagate_return(kernel_res);
    }

    loggers::SUSTCORE::INFO("创建 IDLE 线程");
    auto idle_res = task::TaskManager::inst().create_idle_thread();
    if (!idle_res.has_value()) {
        loggers::SUSTCORE::ERROR("创建idle内核线程失败! 错误码: %s",
                                 to_cstring(idle_res.error()));
        propagate_return(idle_res);
    }

    loggers::SUSTCORE::INFO("创建 KINIT 线程");
    auto kinit_res = task::TaskManager::inst().create_kinit_thread();
    if (!kinit_res.has_value()) {
        loggers::SUSTCORE::ERROR("创建 kinit 内核线程失败! 错误码: %s",
                                 to_cstring(kinit_res.error()));
        propagate_return(kinit_res);
    }

    env::hart_ctx->current_tcb() = nullptr;
    env::hart_ctx->current_pcb() = nullptr;
    schd::Scheduler::init(idle_res.value(), kinit_res.value());
    schd::Scheduler::inst().init();
    register_scheduler_tick_action();
    void_return();
}

Result<void> run_pre_bootstrap_tests() {
#ifdef __CONF_KERNEL_TESTS
    loggers::SUSTCORE::INFO("开始运行内核测试");
    TestFramework framework;
    collect_tests(framework);
    framework.run_all();
    loggers::SUSTCORE::INFO("内核测试完成");
#endif
    void_return();
}

void init_kop();
extern void *dtb_ptr;
extern BootInfoHeader *bootinfo_ptr;
void env_setup();

void map_kpa_region(PageMan &man, PhyArea parea) {
    if (parea.nullable()) {
        return;
    }

    VirAddr vaddr = VirAddr(convert<KpaAddr>(parea.begin).arith());

    man.map_range<true>(vaddr, parea.begin, parea.size(),
                        PageMan::page_flags(PageMan::RWX::RW, false, true));
}

extern "C" void post_init(void);

void env_setup() {
    loggers::SUSTCORE::INFO("开始设置内核环境");

    // construct the env
    env::construct();
    auto &e = env::inst();

    if (bootinfo_ptr == nullptr) {
        panic("BootInfo 为空");
    }

    if (bootinfo_ptr->info_sz == 0 ||
        bootinfo_ptr->info_sz > env::Environment::MAX_BOOTINFO_SIZE)
    {
        panic("BootInfo 大小非法");
    }

    loggers::SUSTCORE::INFO("复制 BootInfo 到 Environment 静态缓冲区");
    memmove(e.bootinfo_storage(env::key::bootinfo()),
            reinterpret_cast<const void *>(bootinfo_ptr),
            bootinfo_ptr->info_sz);
    e.bootinfo_size(env::key::bootinfo()) = bootinfo_ptr->info_sz;
    bootinfo_ptr                          = e.bootinfo(env::key::bootinfo());
    if (bootinfo_ptr == nullptr) {
        panic("Environment 中 BootInfo 不可用");
    }

    dtb_ptr = bootinfo_fdt_ptr(bootinfo_ptr);
    if (dtb_ptr == nullptr) {
        panic("BootInfo FDT 为空");
    }

    loggers::SUSTCORE::INFO("开始读取 BootInfo 内嵌 FDT");
    int dtb_size = fdt_totalsize(dtb_ptr);
    if (dtb_size <= 0) {
        panic("BootInfo 内嵌 FDT 大小非法");
    }

    loggers::SUSTCORE::INFO("初始化 FDT 数据");
    if (FDTHelper::fdt_init(dtb_ptr) == nullptr) {
        panic("FDT初始化失败");
    }

    loggers::SUSTCORE::INFO("从 BootInfo 装载内存区域");
    PhyAddr upper_bound = PhyAddr::null;
    for (size_t i = 0; i < bootinfo_ptr->region_cnt; ++i) {
        const auto &reg = bootinfo_regions(bootinfo_ptr)[i];
        PhyAddr start   = reg.area.begin;
        PhyAddr end     = reg.area.end;
        loggers::SUSTCORE::INFO("BootInfo 内存区域 %u: [%p, %p) Status: %d",
                                static_cast<unsigned>(i), start.addr(),
                                end.addr(), static_cast<int>(reg.status));
        if (upper_bound < end) {
            upper_bound = end;
        }
    }

    loggers::SUSTCORE::INFO("初始化GFP");
    GFP::pre_init();

    loggers::SUSTCORE::INFO("将内存区域加入到GFP中");
    for (size_t i = 0; i < bootinfo_ptr->region_cnt; i++) {
        const auto &reg = bootinfo_regions(bootinfo_ptr)[i];
        if (reg.status != MemRegion::MemoryStatus::FREE) {
            continue;
        }

        size_t pages = reg.area.size() / PAGESIZE;
        if (pages == 0) {
            continue;
        }
        loggers::SUSTCORE::INFO("桥接阶段加入可用内存区域 [%p, %p), 共 %u 页",
                                reg.area.begin.addr(), reg.area.end.addr(),
                                static_cast<unsigned>(pages));
        RawGFPImpl::put_page(reg.area.begin, pages);
    }

    // 初始化 ker_paddr 与 PageMan
    ker_paddr::init();
    PageMan::init();

    // 重新建立内核页表
    loggers::SUSTCORE::INFO("重新建立正式内核页表");
    auto gfp_res = GFP::get_free_page(1);
    if (!gfp_res.has_value()) {
        panic("无法分配新的内核页表根");
    }

    PhyAddr new_pgd                                     = gfp_res.value();
    env::inst().main_kernel_pgd(key::main_kernel_pgd()) = new_pgd;
    PageMan::make_root(new_pgd);
    PageMan kernelman(new_pgd);

    loggers::SUSTCORE::INFO("建立 KVA 映射");
    ker_paddr::mapping_kernel_areas(kernelman);

    loggers::SUSTCORE::INFO("建立 KPA 映射");
    // 加入 bootinfo 中的可回收区域与可用内存区域到 KPA 映射
    for (size_t i = 0; i < bootinfo_ptr->region_cnt; ++i) {
        const auto &reg = bootinfo_regions(bootinfo_ptr)[i];
        if (reg.status != MemRegion::MemoryStatus::FREE &&
            reg.status != MemRegion::MemoryStatus::BOOT_RECLAIMABLE)
        {
            continue;
        }

        size_t pages = reg.area.size() / PAGESIZE;
        if (pages == 0) {
            continue;
        }
        loggers::SUSTCORE::INFO("建立 [%p, %p) 区域的 KPA 映射",
                                reg.area.begin.addr(), reg.area.end.addr());
        map_kpa_region(kernelman, reg.area);
    }
#if defined(__ARCH_loongarch64__)
    // 额外加入 Early Serial 区域
    PhyArea area(PhyAddr(0x1F00'0000), PhyAddr(0x2000'0000));
    loggers::SUSTCORE::INFO("建立 [%p, %p) 区域的 KPA 映射(Early Serial)",
                            area.begin.addr(), area.end.addr());
    map_kpa_region(kernelman, area);
#endif

    loggers::SUSTCORE::INFO("切换到新内核页表");
    kernelman.switch_root();
    kernelman.flush_tlb();

    // 初始化 Allocator 与内存池
    loggers::SUSTCORE::INFO("初始化分配器和内核对象池");
    Allocator::init();
    init_kop();

    void *copied_fdt = Allocator::malloc(static_cast<size_t>(dtb_size));
    if (copied_fdt == nullptr) {
        panic("无法复制 FDT");
    }
    loggers::SUSTCORE::INFO("复制 FDT : %p 到 %p 管理的内存", dtb_ptr,
                            copied_fdt);

    char *__ptr = reinterpret_cast<char *>(copied_fdt);
    loggers::SUSTCORE::INFO("__ptr = %p", __ptr);
    *__ptr = 'c';
    loggers::SUSTCORE::INFO("*__ptr = %c", *__ptr);

    memcpy(copied_fdt, dtb_ptr, static_cast<size_t>(dtb_size));
    *bootinfo_fdt_pa(bootinfo_ptr) = convert_pointer(copied_fdt);
    dtb_ptr                        = copied_fdt;
    if (FDTHelper::fdt_init(dtb_ptr) == nullptr) {
        panic("复制后 FDT 初始化失败");
    }
    loggers::SUSTCORE::INFO("FDT 已迁移到 [%p, %p)", copied_fdt,
                            static_cast<byte *>(copied_fdt) + dtb_size);

    loggers::SUSTCORE::INFO("回收 BootInfo 与 SBI reclaimable 区");
    for (size_t i = 0; i < bootinfo_ptr->region_cnt; ++i) {
        const auto &reg = bootinfo_regions(bootinfo_ptr)[i];
        if (reg.status != MemRegion::MemoryStatus::BOOT_RECLAIMABLE) {
            continue;
        }
        size_t pages = reg.area.size() / PAGESIZE;
        if (pages == 0) {
            continue;
        }
        loggers::SUSTCORE::INFO(
            "回收 BOOT_RECLAIMABLE 内存区域 [%p, %p), 共 %u 页",
            reg.area.begin.addr(), reg.area.end.addr(),
            static_cast<unsigned>(pages));
        RawGFPImpl::put_page(reg.area.begin, pages);
    }

    loggers::SUSTCORE::INFO("桥接代码完成! 进入 post-init 阶段");

    post_init();
}

void init_device_model() {
    loggers::SUSTCORE::INFO("构建设备模型");

    device::DeviceModel::init();
    driver::DriverModel::init();
    device::MMIOManager::init();

    auto &model = device::DeviceModel::inst();

    std::vector<MemRegion> boot_regions;
    auto *bootinfo = env::inst().bootinfo();
    assert(bootinfo != nullptr);
    boot_regions.reserve(bootinfo->region_cnt);
    for (size_t i = 0; i < bootinfo->region_cnt; ++i) {
        const auto &reg = bootinfo_regions(bootinfo)[i];
        boot_regions.push_back(MemRegion{
            .status = reg.status,
            .area   = reg.area,
        });
    }
    model.collect_memory_regions(&boot_regions);
    model.register_provider(
        util::owner<device::DeviceProvider *>(new fdt::FDTProvider(dtb_ptr)));
    model.register_provider(
        util::owner<device::DeviceProvider *>(new device::KernelProvider()));

    auto regions = device::DeviceModel::inst().memory_regions();
    for (size_t i = 0; i < regions.size(); ++i) {
        const auto &region = regions[i];
        loggers::SUSTCORE::INFO("设备模型内存区域 %u: [%p, %p) Status: %d", i,
                                region.area.begin.addr(),
                                region.area.end.addr(),
                                static_cast<int>(region.status));
    }

    const auto &cpus = model.cpus();
    unsigned long long cpu_freq_hz = 0;
#if defined(__ARCH_riscv64__)
    auto *platform = model.platform();
    if (platform != nullptr && platform->is<riscv::Riscv64Platform>()) {
        cpu_freq_hz = static_cast<unsigned long long>(
            platform->as<riscv::Riscv64Platform>()->timebase_frequency()
                .to_hz());
    }
#endif
    loggers::SUSTCORE::INFO("CPU 数量: %u, 频率: %llu Hz",
                            static_cast<unsigned>(cpus.cpus.size()),
                            cpu_freq_hz);
    // cpus.topology.print();

    // loggers::SUSTCORE::INFO("设备树详细信息:");
    // FDTHelper::print_device_tree_detailed();
}

extern "C" void post_init(void) {
    loggers::SUSTCORE::INFO("已进入 post-init 阶段");

    // 初始化 kernel object pool
    loggers::SUSTCORE::INFO("初始化能力系统");
    cap::CHolderManager::init();

    // 初始化中断处理程序
    loggers::SUSTCORE::INFO("初始化中断处理程序");
    Interrupt::init();

    loggers::SUSTCORE::INFO("初始化设备树配置");
    init_device_model();
    env::init_hart();

    Initialization::post_init();

    loggers::SUSTCORE::INFO("初始化任务管理器");
    task::TaskManager::init();

    loggers::SUSTCORE::INFO("初始化等待系统");
    wait::WaitReasonManager::init();

    loggers::SUSTCORE::INFO("初始化调度器");
    auto init_res = init_scheduler();
    if (!init_res.has_value()) {
        loggers::SUSTCORE::ERROR("初始化Scheduler失败! 错误码: %s",
                                 to_cstring(init_res.error()));
        while (true);
    }

    auto test_res = run_pre_bootstrap_tests();
    if (!test_res.has_value()) {
        loggers::SUSTCORE::ERROR("前置测试失败! 错误码: %s",
                                 to_cstring(test_res.error()));
        while (true);
    }

#ifdef __CONF_KERNEL_RUN_MODULES
    loggers::SUSTCORE::INFO("开始切入调度器");
    schd::Scheduler::inst().bootstrap_tasks();
#endif

    while (true);
}

extern "C" void redive(void);

void kernel_setup() {
    while (true);
}
