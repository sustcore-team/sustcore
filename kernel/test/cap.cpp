/**
 * @file cap.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 能力系统测试实现
 * @version alpha-1.0.0
 * @date 2026-03-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/capability.h>
#include <cap/cholder.h>
#include <env.h>
#include <kio.h>
#include <object/csa.h>
#include <object/intobj.h>
#include <perm/csa.h>
#include <perm/intobj.h>
#include <test/cap.h>

namespace test::cap {
    class CaseInitCHolder : public TestCase {
    public:
        CaseInitCHolder() : TestCase("初始化 CHolder 与 CSA") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            expect("创建两个 CHolder, 分别作为源/目标 CSpace");
            auto* chman = ::env::inst().chman();
            tassert(chman != nullptr, "应能从 env 获取 CHolderManager");
            auto holder0_res = chman->create_holder();
            auto holder1_res = chman->create_holder();
            tassert(holder0_res.has_value() && holder1_res.has_value(),
                "创建 CHolder 实例");
            CHolder* holder0 = holder0_res.value();
            CHolder* holder1 = holder1_res.value();

            auto csa_result0 = holder0->csa();
            auto csa_result1 = holder1->csa();
            tassert(csa_result0.has_value() && csa_result1.has_value(),
                    "获取 CHolder 的 CSA 能力");
            check("两个 CHolder 的 CSA 能力均已就绪");
        }
    };

    class CaseCreateObject : public TestCase {
    public:
        CaseCreateObject() : TestCase("创建对象能力并验证初始读值") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            auto* chman = ::env::inst().chman();
            tassert(chman != nullptr, "应能从 env 获取 CHolderManager");
            auto holder0_res = chman->create_holder();
            tassert(holder0_res.has_value(), "创建 CHolder");
            CHolder* holder0 = holder0_res.value();

            auto csa_result = holder0->csa();
            tassert(csa_result.has_value());
            CSAOperator op0(csa_result.value());

            auto gfs_result = op0.get_free_slot();
            tassert(gfs_result.has_value(), "分配槽位 idx_obj0");
            CapIdx idx_obj0 = gfs_result.value();

            expect("在 holder0 创建 IntObject 能力, 初始值应为 12345");
            auto create_result = op0.create<IntObject>(idx_obj0, 12345);
            tassert(create_result.has_value(), "创建 IntObject");

            check("从 CSpace 取回新建能力并执行 read() 校验初始值");
            auto get_result = holder0->space().get(idx_obj0);
            tassert(get_result.has_value(), "获取初始对象能力");

            IntObjOperator op_obj0(get_result.value());
            auto read_obj0 = op_obj0.read();
            tassert(read_obj0.has_value() && read_obj0.value() == 12345,
                    "初始对象读值校验");
        }
    };

    class CaseSharedObject : public TestCase {
    public:
        CaseSharedObject() : TestCase("共享对象 SharedIntObject split 生命周期测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            auto* chman = ::env::inst().chman();
            tassert(chman != nullptr, "应能从 env 获取 CHolderManager");
            auto holder0_res = chman->create_holder();
            tassert(holder0_res.has_value(), "创建 CHolder");
            CHolder* holder0 = holder0_res.value();

            auto csa_result = holder0->csa();
            tassert(csa_result.has_value());
            CSAOperator op0(csa_result.value());

            SharedIntObjectManager manager;

            auto gfs_result_root = op0.get_free_slot();
            tassert(gfs_result_root.has_value(), "分配槽位 idx_sint_root0");
            CapIdx idx_root = gfs_result_root.value();

            expect(
                "在 holder0 创建共享对象 SharedIntObject 访问器能力, 初始值应为 31415");
            auto create_result =
                op0.create<SharedIntObjectAccessor>(idx_root, manager.create(31415));
            tassert(create_result.has_value(), "创建根能力 #0");
            ttest(manager.object_count() == 1);

            auto get_root_result = holder0->space().get(idx_root);
            tassert(get_root_result.has_value(), "获取共享对象根能力 #0");
            SharedIntObjOperator root_op(get_root_result.value());
            auto root_read0 = root_op.read();
            tassert(root_read0.has_value() && root_read0.value() == 31415,
                    "共享对象初始读值校验");

            expect(
                "使用 split 创建两个新根能力, 三个根能力应同时持有同一 "
                "SharedIntObject");

            auto gfs_result_root1 = op0.get_free_slot();
            tassert(gfs_result_root1.has_value(), "分配槽位 idx_sint_root1");
            CapIdx idx_root1 = gfs_result_root1.value();
            auto split_root1_result =
                op0.split<SharedIntObjectAccessor>(idx_root1, &holder0->space(), idx_root);
            tassert(split_root1_result.has_value(), "split 创建根能力 #1");

            auto gfs_result_root2 = op0.get_free_slot();
            tassert(gfs_result_root2.has_value(), "分配槽位 idx_sint_root2");
            CapIdx idx_root2 = gfs_result_root2.value();
            auto split_root2_result =
                op0.split<SharedIntObjectAccessor>(idx_root2, &holder0->space(), idx_root);
            tassert(split_root2_result.has_value(), "split 创建根能力 #2");

            auto get_root1_result = holder0->space().get(idx_root1);
            auto get_root2_result = holder0->space().get(idx_root2);
            tassert(get_root1_result.has_value() && get_root2_result.has_value(),
                    "获取 split 后根能力");
            SharedIntObjOperator root1_op(get_root1_result.value());
            SharedIntObjOperator root2_op(get_root2_result.value());

            action("通过根能力 #1 写入后, 其余根能力应观察到同一结果");
            auto increase_root1_result = root1_op.increase();
            tassert(increase_root1_result.has_value(), "根能力 #1 increase 成功");
            auto read0 = root_op.read();
            auto read1 = root1_op.read();
            auto read2 = root2_op.read();
            tassert(read0.has_value() && read0.value() == 31416,
                    "根能力 #0 读值校验");
            tassert(read1.has_value() && read1.value() == 31416,
                    "根能力 #1 读值校验");
            tassert(read2.has_value() && read2.value() == 31416,
                    "根能力 #2 读值校验");

            action("删除根能力 #1, 其余根能力不应受影响");
                auto remove_root1_result = op0.remove(idx_root1);
                tassert(remove_root1_result.has_value(), "删除根能力 #1");
            ttest(!holder0->space().get(idx_root1).has_value());
            ttest(holder0->space().get(idx_root).has_value());
            ttest(holder0->space().get(idx_root2).has_value());

            action("中间执行 GC, 因仍有根能力存活, 对象不应被回收");
            manager.GC();
            ttest(manager.object_count() == 1);

            action("删除根能力 #2, 根能力 #0 仍应可读");
                auto remove_root2_result = op0.remove(idx_root2);
                tassert(remove_root2_result.has_value(), "删除根能力 #2");
            ttest(!holder0->space().get(idx_root2).has_value());
            auto read_after_remove = root_op.read();
            tassert(read_after_remove.has_value() &&
                        read_after_remove.value() == 31416,
                    "根能力 #0 在其他根释放后仍可访问");

            action("最后删除根能力 #0, 再执行 GC 应回收对象");
                    auto remove_root_result = op0.remove(idx_root);
                    tassert(remove_root_result.has_value(), "删除根能力 #0");
            ttest(!holder0->space().get(idx_root).has_value());
            manager.GC();
            ttest(manager.object_count() == 0);
        }
    };

    class CaseSplitPermission : public TestCase {
    public:
        CaseSplitPermission() : TestCase("SharedIntObject split 权限边界测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            auto* chman = ::env::inst().chman();
            tassert(chman != nullptr, "应能从 env 获取 CHolderManager");
            auto holder0_res = chman->create_holder();
            tassert(holder0_res.has_value(), "创建 CHolder");
            CHolder* holder0 = holder0_res.value();

            auto csa_result = holder0->csa();
            tassert(csa_result.has_value());
            CSAOperator op0(csa_result.value());
            SharedIntObjectManager manager;

            auto gfs_result_no_split_src = op0.get_free_slot();
            tassert(gfs_result_no_split_src.has_value(),
                    "分配槽位 idx_no_split_src");
            CapIdx idx_no_split_src = gfs_result_no_split_src.value();
            auto create_no_split_result =
                op0.create<SharedIntObjectAccessor>(idx_no_split_src, manager.create(27182));
            tassert(create_no_split_result.has_value(),
                    "创建无 SPLIT 负向场景源能力");

            auto get_no_split_result = holder0->space().get(idx_no_split_src);
            tassert(get_no_split_result.has_value(), "获取负向场景源能力");
            PermissionBits no_split_perm(perm::sintobj::READ,
                                         PayloadType::SINTOBJ);
            auto downgrade_no_split_result =
                get_no_split_result.value()->downgrade(no_split_perm);
            tassert(downgrade_no_split_result.has_value(),
                    "降权为无 SPLIT 权限");

            auto gfs_result_no_split_dst = op0.get_free_slot();
            tassert(gfs_result_no_split_dst.has_value(),
                    "分配槽位 idx_no_split_dst");
            CapIdx idx_no_split_dst = gfs_result_no_split_dst.value();

            expect("去除 SPLIT 权限后执行 split 应失败");
            auto split_no_split_result = op0.split<SharedIntObjectAccessor>(
                idx_no_split_dst, &holder0->space(), idx_no_split_src);
            tassert(!split_no_split_result.has_value() &&
                        split_no_split_result.error() ==
                            ErrCode::INSUFFICIENT_PERMISSIONS,
                    "验证无 SPLIT 权限时 split 失败");
            ttest(!holder0->space().get(idx_no_split_dst).has_value());

            auto gfs_result_split_only_src = op0.get_free_slot();
            tassert(gfs_result_split_only_src.has_value(),
                    "分配槽位 idx_split_only_src");
            CapIdx idx_split_only_src = gfs_result_split_only_src.value();
            auto create_split_only_result =
                op0.create<SharedIntObjectAccessor>(idx_split_only_src, manager.create(16180));
            tassert(create_split_only_result.has_value(),
                    "创建 SPLIT-only 场景源能力");

            auto get_split_only_result = holder0->space().get(idx_split_only_src);
            tassert(get_split_only_result.has_value(),
                    "获取 SPLIT-only 场景源能力");

            auto gfs_result_split_observer = op0.get_free_slot();
            tassert(gfs_result_split_observer.has_value(),
                    "分配槽位 idx_split_observer");
            CapIdx idx_split_observer = gfs_result_split_observer.value();
            auto split_observer_result = op0.split<SharedIntObjectAccessor>(
                idx_split_observer, &holder0->space(), idx_split_only_src);
            tassert(split_observer_result.has_value(), "创建观测能力(完整权限)");

            auto get_split_observer_result =
                holder0->space().get(idx_split_observer);
            tassert(get_split_observer_result.has_value(), "获取观测能力");
            SharedIntObjOperator split_observer_op(get_split_observer_result.value());
            auto observer_read0 = split_observer_op.read();
            tassert(
                observer_read0.has_value() && observer_read0.value() == 16180,
                "观测能力初始读值校验");

            PermissionBits split_only_perm(perm::basic::SPLIT,
                                           PayloadType::SINTOBJ);
            auto downgrade_split_only_result =
                get_split_only_result.value()->downgrade(split_only_perm);
            tassert(downgrade_split_only_result.has_value(),
                    "仅保留 SPLIT 权限");

            SharedIntObjOperator split_only_src_op(get_split_only_result.value());
            auto src_read = split_only_src_op.read();
            tassert(
                !src_read.has_value() &&
                    src_read.error() == ErrCode::INSUFFICIENT_PERMISSIONS,
                "SPLIT-only 源能力读操作应被拒绝");

            action("SPLIT-only 下 write/increase/decrease 操作应被拒绝");
            auto write_split_only_result = split_only_src_op.write(42424);
            tassert(!write_split_only_result.has_value() &&
                        write_split_only_result.error() ==
                            ErrCode::INSUFFICIENT_PERMISSIONS,
                    "SPLIT-only 下 write 应被拒绝");
            auto increase_split_only_result = split_only_src_op.increase();
            tassert(!increase_split_only_result.has_value() &&
                        increase_split_only_result.error() ==
                            ErrCode::INSUFFICIENT_PERMISSIONS,
                    "SPLIT-only 下 increase 应被拒绝");
            auto decrease_split_only_result = split_only_src_op.decrease();
            tassert(!decrease_split_only_result.has_value() &&
                        decrease_split_only_result.error() ==
                            ErrCode::INSUFFICIENT_PERMISSIONS,
                    "SPLIT-only 下 decrease 应被拒绝");
            auto observer_read1 = split_observer_op.read();
            tassert(
                observer_read1.has_value() && observer_read1.value() == 16180,
                "SPLIT-only 下写操作未生效");

            auto gfs_result_split_chain_l1 = op0.get_free_slot();
            tassert(gfs_result_split_chain_l1.has_value(),
                    "分配槽位 idx_split_chain_l1");
            CapIdx idx_split_chain_l1 = gfs_result_split_chain_l1.value();

            expect("保留 SPLIT 且移除其它权限后, split 仍应成功");
            auto split_chain_l1_result = op0.split<SharedIntObjectAccessor>(
                idx_split_chain_l1, &holder0->space(), idx_split_only_src);
            tassert(split_chain_l1_result.has_value(),
                    "验证 SPLIT-only 权限下 split 成功");

            auto gfs_result_split_chain_l2 = op0.get_free_slot();
            tassert(gfs_result_split_chain_l2.has_value(),
                    "分配槽位 idx_split_chain_l2");
            CapIdx idx_split_chain_l2 = gfs_result_split_chain_l2.value();
            auto split_chain_l2_result = op0.split<SharedIntObjectAccessor>(
                idx_split_chain_l2, &holder0->space(), idx_split_chain_l1);
            tassert(split_chain_l2_result.has_value(),
                    "验证链式 split 成功(l1 -> l2)");

            auto get_split_chain_l1_result =
                holder0->space().get(idx_split_chain_l1);
            auto get_split_chain_l2_result =
                holder0->space().get(idx_split_chain_l2);
            tassert(get_split_chain_l1_result.has_value() &&
                        get_split_chain_l2_result.has_value(),
                    "获取链式 split 结果能力");
            SharedIntObjOperator split_chain_l1_op(get_split_chain_l1_result.value());
            SharedIntObjOperator split_chain_l2_op(get_split_chain_l2_result.value());
            auto l1_read = split_chain_l1_op.read();
            auto l2_read = split_chain_l2_op.read();
            tassert(!l1_read.has_value() &&
                        l1_read.error() == ErrCode::INSUFFICIENT_PERMISSIONS,
                    "链式 split l1 能力应继承 SPLIT-only 权限");
            tassert(!l2_read.has_value() &&
                        l2_read.error() == ErrCode::INSUFFICIENT_PERMISSIONS,
                    "链式 split l2 能力应继承 SPLIT-only 权限");

            action("清理能力并执行 GC");
                    auto remove_no_split_src_result = op0.remove(idx_no_split_src);
                    tassert(remove_no_split_src_result.has_value(),
                        "清理负向场景源能力");
                    auto remove_split_only_src_result = op0.remove(idx_split_only_src);
                    tassert(remove_split_only_src_result.has_value(),
                        "清理 SPLIT-only 场景源能力");
                    auto remove_split_observer_result = op0.remove(idx_split_observer);
                    tassert(remove_split_observer_result.has_value(), "清理观测能力");
                    auto remove_split_chain_l1_result = op0.remove(idx_split_chain_l1);
                    tassert(remove_split_chain_l1_result.has_value(),
                        "清理链式 split l1 能力");
                    auto remove_split_chain_l2_result = op0.remove(idx_split_chain_l2);
                    tassert(remove_split_chain_l2_result.has_value(),
                        "清理链式 split l2 能力");
            manager.GC();
            ttest(manager.object_count() == 0);
        }
    };

    class CaseClone : public TestCase {
    public:
        CaseClone() : TestCase("Clone 测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            auto* chman = ::env::inst().chman();
            tassert(chman != nullptr, "应能从 env 获取 CHolderManager");
            auto holder0_res = chman->create_holder();
            tassert(holder0_res.has_value(), "创建 CHolder");
            CHolder* holder0 = holder0_res.value();

            auto csa_result = holder0->csa();
            tassert(csa_result.has_value());
            CSAOperator op0(csa_result.value());

            auto gfs_result_obj0 = op0.get_free_slot();
            tassert(gfs_result_obj0.has_value(), "分配槽位 idx_obj0");
            CapIdx idx_obj0 = gfs_result_obj0.value();

            auto create_result = op0.create<IntObject>(idx_obj0, 12345);
            tassert(create_result.has_value(), "创建 IntObject");

            auto gfs_result_obj0_clone = op0.get_free_slot();
            tassert(gfs_result_obj0_clone.has_value(),
                    "分配槽位 idx_obj0_clone");
            CapIdx idx_obj0_clone = gfs_result_obj0_clone.value();

            expect("执行 clone, 两者应指向同一 payload");
            auto clone_result =
                op0.clone(idx_obj0_clone, &holder0->space(), idx_obj0);
            tassert(clone_result.has_value(), "执行能力 clone");

            check("读取 clone 能力, 期望值仍为 12345");
            auto get_clone_result = holder0->space().get(idx_obj0_clone);
            tassert(get_clone_result.has_value());
            IntObjOperator op_clone(get_clone_result.value());
            auto read_clone_result = op_clone.read();
            tassert(read_clone_result.has_value() &&
                        read_clone_result.value() == 12345,
                    "clone 对象读值校验");
        }
    };

    class CaseMigrate : public TestCase {
    public:
        CaseMigrate() : TestCase("Migrate 跨 CSpace 测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            auto* chman = ::env::inst().chman();
            tassert(chman != nullptr, "应能从 env 获取 CHolderManager");
            auto holder0_res = chman->create_holder();
            auto holder1_res = chman->create_holder();
            tassert(holder0_res.has_value() && holder1_res.has_value(),
                "创建 CHolder 实例");
            CHolder* holder0 = holder0_res.value();
            CHolder* holder1 = holder1_res.value();

            auto csa_result0 = holder0->csa();
            auto csa_result1 = holder1->csa();
            tassert(csa_result0.has_value() && csa_result1.has_value(),
                    "获取源/目标 CHolder 的 CSA 能力");
            CSAOperator op0(csa_result0.value());
            CSAOperator op1(csa_result1.value());

            auto gfs_result_obj0 = op0.get_free_slot();
            tassert(gfs_result_obj0.has_value(), "分配槽位 idx_obj0");
            CapIdx idx_obj0 = gfs_result_obj0.value();
            auto create_result = op0.create<IntObject>(idx_obj0, 12345);
            tassert(create_result.has_value(), "创建 IntObject");

            auto gfs_result_obj1 = op1.get_free_slot();
            tassert(gfs_result_obj1.has_value(), "分配槽位 idx_obj1");
            CapIdx idx_obj1 = gfs_result_obj1.value();
            expect("执行迁移, 源槽位应被清空");
            auto migrate_result =
                op1.migrate(idx_obj1, &holder0->space(), idx_obj0);
            tassert(migrate_result.has_value(), "执行能力 migrate");

            check("校验源槽位是否已清空");
            ttest(!holder0->space().get(idx_obj0).has_value());

            check("校验目标槽位读值正确");
            auto get_migrated_result = holder1->space().get(idx_obj1);
            tassert(get_migrated_result.has_value());
            IntObjOperator op_migrated(get_migrated_result.value());
            auto read_migrated_result = op_migrated.read();
            tassert(read_migrated_result.has_value() &&
                        read_migrated_result.value() == 12345,
                    "migrate 后对象读值校验");
        }
    };

    class CaseSendReceive : public TestCase {
    public:
        CaseSendReceive() : TestCase("SendRecord 收发流程测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            auto* chman = ::env::inst().chman();
            tassert(chman != nullptr, "应能从 env 获取 CHolderManager");

            auto sender_res = chman->create_holder();
            auto receiver_res = chman->create_holder();
            tassert(sender_res.has_value() && receiver_res.has_value(),
                    "创建发送方与接收方 CHolder");
            CHolder* sender = sender_res.value();
            CHolder* receiver = receiver_res.value();

            auto sender_csa_res = sender->csa();
            auto receiver_csa_res = receiver->csa();
            tassert(sender_csa_res.has_value() && receiver_csa_res.has_value(),
                    "获取双方 CSA 能力");
            CSAOperator sender_op(sender_csa_res.value());
            CSAOperator receiver_op(receiver_csa_res.value());

            auto gfs_src_res = sender_op.get_free_slot();
            tassert(gfs_src_res.has_value(), "分配发送方槽位");
            CapIdx src_idx = gfs_src_res.value();
            auto create_res = sender_op.create<IntObject>(src_idx, 20260426);
            tassert(create_res.has_value(), "发送方创建待发送能力");

            auto send_res = sender_op.send(src_idx, receiver->id());
            tassert(send_res.has_value(), "生成 ReceiveToken");
            ReceiveToken token = send_res.value();

            auto gfs_dst_res = receiver_op.get_free_slot();
            tassert(gfs_dst_res.has_value(), "分配接收方槽位");
            CapIdx dst_idx = gfs_dst_res.value();

            auto receive_res = receiver_op.receive(dst_idx, token);
            tassert(receive_res.has_value(), "接收方完成 migrate");

            ttest(!sender->space().get(src_idx).has_value());

            auto get_received_res = receiver->space().get(dst_idx);
            tassert(get_received_res.has_value(), "接收方槽位应已装载能力");
            IntObjOperator received_op(get_received_res.value());
            auto read_res = received_op.read();
            tassert(read_res.has_value() && read_res.value() == 20260426,
                    "接收后的能力读值校验");
        }
    };

    class CaseDowngrade : public TestCase {
    public:
        CaseDowngrade() : TestCase("降权测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            auto* chman = env::inst().chman();
            tassert(chman != nullptr, "应能从 env 获取 CHolderManager");
            auto holder0_res = chman->create_holder();
            tassert(holder0_res.has_value(), "创建 CHolder");
            CHolder* holder0 = holder0_res.value();

            auto csa_result = holder0->csa();
            tassert(csa_result.has_value());
            CSAOperator op0(csa_result.value());
            auto gfs_result = op0.get_free_slot();
            tassert(gfs_result.has_value(), "分配槽位 idx");
            CapIdx idx = gfs_result.value();
            auto create_result = op0.create<IntObject>(idx, 12345);
            tassert(create_result.has_value(), "创建 IntObject");
            auto get_result = holder0->space().get(idx);
            tassert(get_result.has_value(), "获取 IntObject 能力");
            Capability* cap = get_result.value();

            expect("降权到 READ-only, 写操作应被拒绝");
            PermissionBits read_only(perm::intobj::READ, PayloadType::INTOBJ);
            auto downgrade_read_result = cap->downgrade(read_only);
            tassert(downgrade_read_result.has_value(), "执行降权至 READ");

            IntObjOperator op(cap);
            auto increase_result = op.increase();
            tassert(!increase_result.has_value() &&
                        increase_result.error() ==
                            ErrCode::INSUFFICIENT_PERMISSIONS,
                    "READ-only 下 increase 应被拒绝");
            auto read_after_increase = op.read();
            tassert(read_after_increase.has_value() &&
                        read_after_increase.value() == 12345,
                    "验证 READ-only 下写操作无效");

            expect("降权到 NONE, 读操作应被拒绝");
            PermissionBits no_perm(0, PayloadType::INTOBJ);
            auto downgrade_none_result = cap->downgrade(no_perm);
            tassert(downgrade_none_result.has_value(), "执行降权至 NONE");
            auto read_none_result = op.read();
            tassert(!read_none_result.has_value() &&
                        read_none_result.error() ==
                            ErrCode::INSUFFICIENT_PERMISSIONS,
                    "验证 NONE 权限下读操作被拒绝");
        }
    };

    class CaseRevoke : public TestCase {
    public:
        CaseRevoke() : TestCase("Revoke 子树清理测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            auto* chman = env::inst().chman();
            tassert(chman != nullptr, "应能从 env 获取 CHolderManager");
            auto holder0_res = chman->create_holder();
            auto holder1_res = chman->create_holder();
            tassert(holder0_res.has_value() && holder1_res.has_value(),
                "创建 CHolder 实例");
            CHolder* holder0 = holder0_res.value();
            CHolder* holder1 = holder1_res.value();

            auto csa_result0 = holder0->csa();
            auto csa_result1 = holder1->csa();
            tassert(csa_result0.has_value() && csa_result1.has_value(),
                    "获取源/目标 CHolder 的 CSA 能力");
            CSAOperator op0(csa_result0.value());
            CSAOperator op1(csa_result1.value());

            auto gfs_result_root = op0.get_free_slot();
            tassert(gfs_result_root.has_value(), "分配槽位 idx_root");
            CapIdx idx_root = gfs_result_root.value();
            auto create_result = op0.create<IntObject>(idx_root, 999);
            tassert(create_result.has_value(), "创建根能力");

            auto gfs_result_l1_keep = op0.get_free_slot();
            tassert(gfs_result_l1_keep.has_value(), "分配槽位 idx_l1_keep");
            CapIdx idx_l1_keep = gfs_result_l1_keep.value();
            auto clone_l1_keep_result =
                op0.clone(idx_l1_keep, &holder0->space(), idx_root);
            tassert(clone_l1_keep_result.has_value(), "创建保留分支 l1");

            auto gfs_result_l1_revoke = op0.get_free_slot();
            tassert(gfs_result_l1_revoke.has_value(), "分配槽位 idx_l1_revoke");
            CapIdx idx_l1_revoke = gfs_result_l1_revoke.value();
            auto clone_l1_revoke_result =
                op0.clone(idx_l1_revoke, &holder0->space(), idx_root);
            tassert(clone_l1_revoke_result.has_value(), "创建待撤销分支 l1");

            auto gfs_result_l2_revoke = op0.get_free_slot();
            tassert(gfs_result_l2_revoke.has_value(), "分配槽位 idx_l2_revoke");
            CapIdx idx_l2_revoke = gfs_result_l2_revoke.value();
            auto clone_l2_revoke_result =
                op0.clone(idx_l2_revoke, &holder0->space(), idx_l1_revoke);
            tassert(clone_l2_revoke_result.has_value(), "创建待撤销分支 l2");

            auto gfs_result_l3_revoke = op1.get_free_slot();
            tassert(gfs_result_l3_revoke.has_value(), "分配槽位 idx_l3_revoke");
            CapIdx idx_l3_revoke = gfs_result_l3_revoke.value();
            auto clone_l3_revoke_result =
                op1.clone(idx_l3_revoke, &holder0->space(), idx_l2_revoke);
            tassert(clone_l3_revoke_result.has_value(), "创建待撤销分支 l3");

            action("删除待撤销分支根节点, 触发子树清理");
            auto remove_result = op0.remove(idx_l1_revoke);
            tassert(remove_result.has_value(), "执行能力 remove(revoke)");

            check("校验后代是否已删除, 根与兄弟是否保留");
            ttest(!holder0->space().get(idx_l2_revoke).has_value());
            ttest(!holder1->space().get(idx_l3_revoke).has_value());
            ttest(holder0->space().get(idx_root).has_value());
            ttest(holder0->space().get(idx_l1_keep).has_value());
        }
    };

    void collect_tests(TestFramework& framework) {
        auto cases = util::ArrayList<TestCase*>();
        cases.push_back(new CaseInitCHolder());
        cases.push_back(new CaseCreateObject());
        cases.push_back(new CaseSharedObject());
        cases.push_back(new CaseSplitPermission());
        cases.push_back(new CaseClone());
        cases.push_back(new CaseMigrate());
        cases.push_back(new CaseSendReceive());
        cases.push_back(new CaseDowngrade());
        cases.push_back(new CaseRevoke());

        framework.add_category(
            new TestCategory("capability", std::move(cases)));
    }
}  // namespace test::cap
