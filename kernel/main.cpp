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

#include <arch/riscv64/csr.h>
#include <arch/riscv64/description.h>
#include <arch/riscv64/device/fdt_helper.h>
#include <arch/riscv64/mem/sv39.h>
#include <arch/trait.h>
#include <device/fdt.h>
#include <device/int.h>
#include <device/model.h>
#include <device/resource.h>
#include <driver/int/clint.h>
#include <env.h>
#include <kinit.h>
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
#include <task/scheduler.h>
#include <task/task.h>
#include <task/wait.h>
#include <test/framework.h>

#include <cassert>
#include <cstddef>
#include <cstdio>

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

    Result<void> init_clock() {
        [[maybe_unused]] auto &device_model = device::DeviceModel::inst();
        [[maybe_unused]] auto &irqman       = device_model.interrupt();
        [[maybe_unused]] auto &cpus         = device_model.cpus();
        [[maybe_unused]] auto *ctx          = hart_ctx;
        assert(ctx != nullptr);

        // 初始化 CLINT 定时器, 供调度器 tick 使用.
        loggers::SUSTCORE::INFO("初始化 hart Clint Timer%u",
                                static_cast<unsigned>(hart_ctx->hart_id()));
        auto *clock_source = cpus._clock_source;
        if (clock_source == nullptr) {
            loggers::SUSTCORE::ERROR("全局 ClockSource 不可用!");
            unexpect_return(ErrCode::NULLPTR);
        }

        // 获得时钟中断 virq
        auto clock_virq = device_model.clock_virq();
        if (clock_virq == 0) {
            loggers::SUSTCORE::ERROR("DeviceModel 未提供有效 clock_virq");
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        ctx->alarm()       = new device::ClintAlarm(clock_source, clock_virq);
        ctx->time_keeper() = new device::TimeKeeper(clock_source, ctx->alarm());
        auto enable_timer_res = irqman.enable_irq(clock_virq);
        if (!enable_timer_res.has_value()) {
            loggers::SUSTCORE::ERROR("启用 CLINT timer 中断失败!");
            propagate_return(enable_timer_res);
        }
        loggers::SUSTCORE::INFO("hart %u 已初始化 ClintAlarm",
                                static_cast<unsigned>(ctx->hart_id()));
        void_return();
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

        auto ret = init_clock();
        if (!ret.has_value()) {
            loggers::SUSTCORE::FATAL("初始化时钟失败: %s",
                                     to_cstring(ret.error()));
            panic("初始化时钟失败");
        }
    }
}  // namespace env

namespace {
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
    struct main : public tmm, main_kernel_pgd, meminfo {
    public:
        main() = default;
    };
}  // namespace key

void kernel_paging_setup() {
    [[maybe_unused]]
    constexpr KernelStage STAGE = KernelStage::PRE_INIT;
    auto &e                     = env::inst();
    // 创建内核页表管理器
    auto gfp_res                = GFP::get_free_page<STAGE>(1);
    if (!gfp_res.has_value()) {
        loggers::SUSTCORE::ERROR("无法为内核页表分配物理页");
        while (true);
    }

    PhyAddr pgd                               = gfp_res.value();
    e.main_kernel_pgd(key::main_kernel_pgd()) = pgd;
    EarlyPageMan::make_root(pgd);
    EarlyPageMan kernelman(pgd);

    ker_paddr::init();
    ker_paddr::mapping_kernel_areas(kernelman);

    // 对[0, uppm)进行恒等映射
    size_t sz = e.meminfo().uppm - e.meminfo().lowpm;
    kernelman.map_range<true>(e.meminfo().lowvm, e.meminfo().lowpm, sz,
                              EarlyPageMan::rwx(true, true, true), false, true);

    kernelman.switch_root();
    kernelman.flush_tlb();
}

Result<void> init_scheduler() {
    auto kernel_res = task::TaskManager::inst().create_kernel_task();
    if (!kernel_res.has_value()) {
        loggers::SUSTCORE::ERROR("创建KERNEL进程失败! 错误码: %s",
                                 to_cstring(kernel_res.error()));
        propagate_return(kernel_res);
    }

    auto idle_res = task::TaskManager::inst().create_idle_thread();
    if (!idle_res.has_value()) {
        loggers::SUSTCORE::ERROR("创建idle内核线程失败! 错误码: %s",
                                 to_cstring(idle_res.error()));
        propagate_return(idle_res);
    }

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

void init_device_model() {
    loggers::SUSTCORE::INFO("构建设备模型");

    device::DeviceModel::init();
    driver::DriverModel::init();
    device::MMIOManager::init();

    auto &model = device::DeviceModel::inst();

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
    loggers::SUSTCORE::INFO("CPU 数量: %u, 频率: %llu Hz",
                            static_cast<unsigned>(cpus.cpus.size()),
                            static_cast<unsigned long long>(cpus.freq.to_hz()));
    // cpus.topology.print();

    // loggers::SUSTCORE::INFO("设备树详细信息:");
    // FDTHelper::print_device_tree_detailed();
}

extern "C" void post_init(void) {
    loggers::SUSTCORE::INFO("已进入 post-init 阶段");
    auto &e = env::inst();

    // 将 pre-init 阶段中初始化的子系统再次初始化, 以适应内核虚拟地址空间
    GFP::post_init();
    PageMan::init();
    Allocator::init();

    // 将 tp 寄存器中的指针更新为内核虚拟地址空间中的环境实例
    PhyAddr old_tp = convert_pointer(env::hart_ctx);
    env::hart_ctx  = convert<KvaAddr>(old_tp).as<env::HartContext>();

    // 将低端内存设置为用户态
    loggers::SUSTCORE::INFO("将低端内存设置为用户态");
    auto &meminfo = e.meminfo();
    PageMan kernelman(env::inst().main_kernel_pgd());
    kernelman.modify_range_flags<PageMan::ModifyMask::U>(
        meminfo.lowvm, meminfo.uppm - meminfo.lowpm, PageMan::RWX::NONE, true,
        false);
    Initialization::promote_dtb_to_kpa();

    // 初始化 kernel object pool
    loggers::SUSTCORE::INFO("初始化内核对象池");
    init_kop();

    loggers::SUSTCORE::INFO("初始化权限系统");
    cap::CHolderManager::init();

    // 初始化中断处理程序
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

    loggers::SUSTCORE::INFO("测试输出ErrCode::BUSY", to_cstring(ErrCode::BUSY));

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

void pre_init() {
    [[maybe_unused]]
    constexpr KernelStage STAGE = KernelStage::PRE_INIT;
    // construct the env
    env::construct();

    Initialization::pre_init();

    auto &e = env::inst();

    auto detect_res = MemoryLayout::detect();
    if (!detect_res.has_value()) {
        loggers::SUSTCORE::FATAL("探测内存区域失败!错误码: %s",
                                 to_cstring(detect_res.error()));
        while (true);
    }

    PhyAddr upper_bound = PhyAddr::null;
    for (int i = 0; i < e.meminfo().region_cnt; i++) {
        const auto &reg = e.meminfo().regions[i];
        PhyAddr start   = reg.ptr;
        PhyAddr end     = start + reg.size;

        loggers::SUSTCORE::INFO("探测到内存区域 %d: [%p, %p) Status: %d", i,
                                start.addr(), end.addr(),
                                static_cast<int>(reg.status));
        if (upper_bound < end) {
            upper_bound = end;
        }
    }
    e.meminfo(key::main()).uppm = upper_bound;

    loggers::SUSTCORE::INFO("初始化GFP");
    GFP::pre_init();

    loggers::SUSTCORE::INFO("初始化内核地址空间管理器");
    EarlyPageMan::init();

    kernel_paging_setup();

    // FDTHelper::print_device_tree_detailed();

    // 进入 post-init 阶段
    // 此阶段内, 内核的所有代码和数据均已映射到内核虚拟地址空间
    // 将 redive() 函数定位到KvaAddr
    typedef void (*RediveFuncType)(void);
    auto redive_paddr  = (PhyAddr)(void *)redive;
    auto redive_kvaddr = convert<KvaAddr>(redive_paddr);
    loggers::SUSTCORE::DEBUG("redive函数物理地址: %p, 内核虚拟地址: %p",
                             redive_paddr.addr(), redive_kvaddr.addr());
    auto redive_func = (RediveFuncType)redive_kvaddr.addr();
    loggers::SUSTCORE::DEBUG("跳转到内核虚拟地址空间中的redive函数: %p",
                             redive_func);
    redive_func();
    loggers::SUSTCORE::ERROR("redive函数返回了, 这不应该发生!");
    while (true);
}

void kernel_setup() {
    pre_init();
    while (true);
}
