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
#include <cap/cspace.h>
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
            CHolder holder0;
            CHolder holder1;

            auto cap_csa0_opt = holder0.csa();
            auto cap_csa1_opt = holder1.csa();
            tassert(cap_csa0_opt.present() && cap_csa1_opt.present(),
                    "获取 CHolder 的 CSA 能力");
            check("两个 CHolder 的 CSA 能力均已就绪");
        }
    };

    class CaseCreateObject : public TestCase {
    public:
        CaseCreateObject() : TestCase("创建对象能力并验证初始读值") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            CHolder holder0;
            auto cap_csa0_opt = holder0.csa();
            tassert(cap_csa0_opt.present());
            CSAOp op0(cap_csa0_opt.value());

            auto idx_obj0_opt = op0.get_free_slot();
            tassert(idx_obj0_opt.present(), "分配槽位 idx_obj0");
            CapIdx idx_obj0 = idx_obj0_opt.value();

            expect("在 holder0 创建 IntObj 能力, 初始值应为 12345");
            CapErrCode err = op0.create<IntObj>(idx_obj0, 12345);
            tassert(err == CapErrCode::SUCCESS, "创建 IntObj");

            check("从 CSpace 取回新建能力并执行 read() 校验初始值");
            auto cap_obj0_opt = holder0.space().get(idx_obj0);
            tassert(cap_obj0_opt.present(), "获取初始对象能力");

            IntOp op_obj0(cap_obj0_opt.value());
            auto read_obj0 = op_obj0.read();
            tassert(read_obj0.present() && read_obj0.value() == 12345,
                    "初始对象读值校验");
        }
    };

    class CaseSharedObject : public TestCase {
    public:
        CaseSharedObject() : TestCase("共享对象 SIntObj split 生命周期测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            CHolder holder0;
            auto cap_csa0_opt = holder0.csa();
            tassert(cap_csa0_opt.present());
            CSAOp op0(cap_csa0_opt.value());

            SIntManager manager;

            auto idx_root_opt = op0.get_free_slot();
            tassert(idx_root_opt.present(), "分配槽位 idx_sint_root0");
            CapIdx idx_root = idx_root_opt.value();

            expect(
                "在 holder0 创建共享对象 SIntObj 访问器能力, 初始值应为 31415");
            CapErrCode err =
                op0.create<SIntAcc>(idx_root, manager.create(31415));
            tassert(err == CapErrCode::SUCCESS, "创建根能力 #0");
            ttest(manager.object_count() == 1);

            auto cap_root_opt = holder0.space().get(idx_root);
            tassert(cap_root_opt.present(), "获取共享对象根能力 #0");
            SIntOp root_op(cap_root_opt.value());
            auto root_read0 = root_op.read();
            tassert(root_read0.present() && root_read0.value() == 31415,
                    "共享对象初始读值校验");

            expect(
                "使用 split 创建两个新根能力, 三个根能力应同时持有同一 "
                "SIntObj");

            auto idx_root1_opt = op0.get_free_slot();
            tassert(idx_root1_opt.present(), "分配槽位 idx_sint_root1");
            CapIdx idx_root1 = idx_root1_opt.value();
            tassert(op0.split<SIntAcc>(idx_root1, &holder0.space(), idx_root) ==
                        CapErrCode::SUCCESS,
                    "split 创建根能力 #1");

            auto idx_root2_opt = op0.get_free_slot();
            tassert(idx_root2_opt.present(), "分配槽位 idx_sint_root2");
            CapIdx idx_root2 = idx_root2_opt.value();
            tassert(op0.split<SIntAcc>(idx_root2, &holder0.space(), idx_root) ==
                        CapErrCode::SUCCESS,
                    "split 创建根能力 #2");

            auto cap_root1_opt = holder0.space().get(idx_root1);
            auto cap_root2_opt = holder0.space().get(idx_root2);
            tassert(cap_root1_opt.present() && cap_root2_opt.present(),
                    "获取 split 后根能力");
            SIntOp root1_op(cap_root1_opt.value());
            SIntOp root2_op(cap_root2_opt.value());

            action("通过根能力 #1 写入后, 其余根能力应观察到同一结果");
            root1_op.increase();
            auto read0 = root_op.read();
            auto read1 = root1_op.read();
            auto read2 = root2_op.read();
            tassert(read0.present() && read0.value() == 31416,
                    "根能力 #0 读值校验");
            tassert(read1.present() && read1.value() == 31416,
                    "根能力 #1 读值校验");
            tassert(read2.present() && read2.value() == 31416,
                    "根能力 #2 读值校验");

            action("删除根能力 #1, 其余根能力不应受影响");
            tassert(op0.remove(idx_root1) == CapErrCode::SUCCESS,
                    "删除根能力 #1");
            ttest(!holder0.space().get(idx_root1).present());
            ttest(holder0.space().get(idx_root).present());
            ttest(holder0.space().get(idx_root2).present());

            action("中间执行 GC, 因仍有根能力存活, 对象不应被回收");
            manager.GC();
            ttest(manager.object_count() == 1);

            action("删除根能力 #2, 根能力 #0 仍应可读");
            tassert(op0.remove(idx_root2) == CapErrCode::SUCCESS,
                    "删除根能力 #2");
            ttest(!holder0.space().get(idx_root2).present());
            auto read_after_remove = root_op.read();
            tassert(read_after_remove.present() &&
                        read_after_remove.value() == 31416,
                    "根能力 #0 在其他根释放后仍可访问");

            action("最后删除根能力 #0, 再执行 GC 应回收对象");
            tassert(op0.remove(idx_root) == CapErrCode::SUCCESS,
                    "删除根能力 #0");
            ttest(!holder0.space().get(idx_root).present());
            manager.GC();
            ttest(manager.object_count() == 0);
        }
    };

    class CaseSplitPermission : public TestCase {
    public:
        CaseSplitPermission() : TestCase("SIntObj split 权限边界测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            CHolder holder0;
            auto cap_csa0_opt = holder0.csa();
            tassert(cap_csa0_opt.present());
            CSAOp op0(cap_csa0_opt.value());
            SIntManager manager;

            auto idx_no_split_src_opt = op0.get_free_slot();
            tassert(idx_no_split_src_opt.present(),
                    "分配槽位 idx_no_split_src");
            CapIdx idx_no_split_src = idx_no_split_src_opt.value();
            tassert(
                op0.create<SIntAcc>(idx_no_split_src, manager.create(27182)) ==
                    CapErrCode::SUCCESS,
                "创建无 SPLIT 负向场景源能力");

            auto cap_no_split_opt = holder0.space().get(idx_no_split_src);
            tassert(cap_no_split_opt.present(), "获取负向场景源能力");
            PermissionBits no_split_perm(perm::sintobj::READ,
                                         PayloadType::SINTOBJ);
            tassert(cap_no_split_opt.value()->downgrade(no_split_perm) ==
                        CapErrCode::SUCCESS,
                    "降权为无 SPLIT 权限");

            auto idx_no_split_dst_opt = op0.get_free_slot();
            tassert(idx_no_split_dst_opt.present(),
                    "分配槽位 idx_no_split_dst");
            CapIdx idx_no_split_dst = idx_no_split_dst_opt.value();

            expect("去除 SPLIT 权限后执行 split 应失败");
            tassert(op0.split<SIntAcc>(idx_no_split_dst, &holder0.space(),
                                       idx_no_split_src) ==
                        CapErrCode::INSUFFICIENT_PERMISSIONS,
                    "验证无 SPLIT 权限时 split 失败");
            ttest(!holder0.space().get(idx_no_split_dst).present());

            auto idx_split_only_src_opt = op0.get_free_slot();
            tassert(idx_split_only_src_opt.present(),
                    "分配槽位 idx_split_only_src");
            CapIdx idx_split_only_src = idx_split_only_src_opt.value();
            tassert(op0.create<SIntAcc>(idx_split_only_src,
                                        manager.create(16180)) ==
                        CapErrCode::SUCCESS,
                    "创建 SPLIT-only 场景源能力");

            auto cap_split_only_opt = holder0.space().get(idx_split_only_src);
            tassert(cap_split_only_opt.present(), "获取 SPLIT-only 场景源能力");

                auto idx_split_observer_opt = op0.get_free_slot();
                tassert(idx_split_observer_opt.present(),
                    "分配槽位 idx_split_observer");
                CapIdx idx_split_observer = idx_split_observer_opt.value();
                tassert(op0.split<SIntAcc>(idx_split_observer, &holder0.space(),
                               idx_split_only_src) ==
                    CapErrCode::SUCCESS,
                    "创建观测能力(完整权限)");

                auto cap_split_observer_opt = holder0.space().get(idx_split_observer);
                tassert(cap_split_observer_opt.present(), "获取观测能力");
                SIntOp split_observer_op(cap_split_observer_opt.value());
                auto observer_read0 = split_observer_op.read();
                tassert(observer_read0.present() && observer_read0.value() == 16180,
                    "观测能力初始读值校验");

            PermissionBits split_only_perm(perm::basic::SPLIT,
                                           PayloadType::SINTOBJ);
            tassert(cap_split_only_opt.value()->downgrade(split_only_perm) ==
                        CapErrCode::SUCCESS,
                    "仅保留 SPLIT 权限");

            SIntOp split_only_src_op(cap_split_only_opt.value());
            auto src_read = split_only_src_op.read();
            tassert(
                !src_read.present() &&
                    src_read.error() == CapErrCode::INSUFFICIENT_PERMISSIONS,
                "SPLIT-only 源能力读操作应被拒绝");

            action("SPLIT-only 下 write/increase/decrease 操作应被拒绝");
            split_only_src_op.write(42424);
            split_only_src_op.increase();
            split_only_src_op.decrease();
            auto observer_read1 = split_observer_op.read();
            tassert(observer_read1.present() && observer_read1.value() == 16180,
                    "SPLIT-only 下写操作未生效");

            auto idx_split_chain_l1_opt = op0.get_free_slot();
            tassert(idx_split_chain_l1_opt.present(),
                    "分配槽位 idx_split_chain_l1");
            CapIdx idx_split_chain_l1 = idx_split_chain_l1_opt.value();

            expect("保留 SPLIT 且移除其它权限后, split 仍应成功");
            tassert(
                op0.split<SIntAcc>(idx_split_chain_l1, &holder0.space(),
                                   idx_split_only_src) == CapErrCode::SUCCESS,
                "验证 SPLIT-only 权限下 split 成功");

            auto idx_split_chain_l2_opt = op0.get_free_slot();
            tassert(idx_split_chain_l2_opt.present(),
                    "分配槽位 idx_split_chain_l2");
            CapIdx idx_split_chain_l2 = idx_split_chain_l2_opt.value();
            tassert(op0.split<SIntAcc>(idx_split_chain_l2, &holder0.space(),
                                       idx_split_chain_l1) ==
                        CapErrCode::SUCCESS,
                    "验证链式 split 成功(l1 -> l2)");

            auto cap_split_chain_l1_opt = holder0.space().get(idx_split_chain_l1);
            auto cap_split_chain_l2_opt = holder0.space().get(idx_split_chain_l2);
            tassert(cap_split_chain_l1_opt.present() && cap_split_chain_l2_opt.present(),
                    "获取链式 split 结果能力");
            SIntOp split_chain_l1_op(cap_split_chain_l1_opt.value());
            SIntOp split_chain_l2_op(cap_split_chain_l2_opt.value());
            auto l1_read = split_chain_l1_op.read();
            auto l2_read = split_chain_l2_op.read();
            tassert(
                !l1_read.present() &&
                    l1_read.error() == CapErrCode::INSUFFICIENT_PERMISSIONS,
                "链式 split l1 能力应继承 SPLIT-only 权限");
            tassert(
                !l2_read.present() &&
                    l2_read.error() == CapErrCode::INSUFFICIENT_PERMISSIONS,
                "链式 split l2 能力应继承 SPLIT-only 权限");

            action("清理能力并执行 GC");
            tassert(op0.remove(idx_no_split_src) == CapErrCode::SUCCESS,
                    "清理负向场景源能力");
            tassert(op0.remove(idx_split_only_src) == CapErrCode::SUCCESS,
                    "清理 SPLIT-only 场景源能力");
            tassert(op0.remove(idx_split_observer) == CapErrCode::SUCCESS,
                    "清理观测能力");
            tassert(op0.remove(idx_split_chain_l1) == CapErrCode::SUCCESS,
                    "清理链式 split l1 能力");
            tassert(op0.remove(idx_split_chain_l2) == CapErrCode::SUCCESS,
                    "清理链式 split l2 能力");
            manager.GC();
            ttest(manager.object_count() == 0);
        }
    };

    class CaseClone : public TestCase {
    public:
        CaseClone() : TestCase("Clone 测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            CHolder holder0;
            CSAOp op0(holder0.csa().value());
            auto idx_obj0 = op0.get_free_slot().value();
            op0.create<IntObj>(idx_obj0, 12345);

            auto idx_obj0_clone_opt = op0.get_free_slot();
            tassert(idx_obj0_clone_opt.present(), "分配槽位 idx_obj0_clone");
            CapIdx idx_obj0_clone = idx_obj0_clone_opt.value();

            expect("执行 clone, 两者应指向同一 payload");
            CapErrCode err =
                op0.clone(idx_obj0_clone, &holder0.space(), idx_obj0);
            tassert(err == CapErrCode::SUCCESS, "执行能力 clone");

            check("读取 clone 能力, 期望值仍为 12345");
            auto cap_clone_opt = holder0.space().get(idx_obj0_clone);
            tassert(cap_clone_opt.present());
            IntOp op_clone(cap_clone_opt.value());
            tassert(op_clone.read().value() == 12345, "clone 对象读值校验");
        }
    };

    class CaseMigrate : public TestCase {
    public:
        CaseMigrate() : TestCase("Migrate 跨 CSpace 测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            CHolder holder0, holder1;
            CSAOp op0(holder0.csa().value());
            CSAOp op1(holder1.csa().value());
            auto idx_obj0 = op0.get_free_slot().value();
            op0.create<IntObj>(idx_obj0, 12345);

            auto idx_obj1 = op1.get_free_slot().value();
            expect("执行迁移, 源槽位应被清空");
            CapErrCode err = op1.migrate(idx_obj1, &holder0.space(), idx_obj0);
            tassert(err == CapErrCode::SUCCESS, "执行能力 migrate");

            check("校验源槽位是否已清空");
            ttest(!holder0.space().get(idx_obj0).present());

            check("校验目标槽位读值正确");
            auto cap_migrated_opt = holder1.space().get(idx_obj1);
            tassert(cap_migrated_opt.present());
            IntOp op_migrated(cap_migrated_opt.value());
            tassert(op_migrated.read().value() == 12345,
                    "migrate 后对象读值校验");
        }
    };

    class CaseDowngrade : public TestCase {
    public:
        CaseDowngrade() : TestCase("降权测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            CHolder holder0;
            CSAOp op0(holder0.csa().value());
            auto idx = op0.get_free_slot().value();
            op0.create<IntObj>(idx, 12345);
            Capability* cap = holder0.space().get(idx).value();

            expect("降权到 READ-only, 写操作应被拒绝");
            PermissionBits read_only(perm::intobj::READ, PayloadType::INTOBJ);
            tassert(cap->downgrade(read_only) == CapErrCode::SUCCESS,
                    "执行降权至 READ");

            IntOp op(cap);
            op.increase();  // 应该静默失败或日志报警，逻辑上值不应变
            tassert(op.read().value() == 12345, "验证 READ-only 下写操作无效");

            expect("降权到 NONE, 读操作应被拒绝");
            PermissionBits no_perm(0, PayloadType::INTOBJ);
            tassert(cap->downgrade(no_perm) == CapErrCode::SUCCESS,
                    "执行降权至 NONE");
            auto res = op.read();
            tassert(!res.present() &&
                        res.error() == CapErrCode::INSUFFICIENT_PERMISSIONS,
                    "验证 NONE 权限下读操作被拒绝");
        }
    };

    class CaseRecvSpace : public TestCase {
    public:
        CaseRecvSpace() : TestCase("RecvSpace 接收测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            CHolder holder0, holder1;
            CSAOp op0(holder0.csa().value());
            auto idx_src = op0.get_free_slot().value();
            op0.create<IntObj>(idx_src, 24680);
            Capability* cap_src = holder0.space().get(idx_src).value();

            CapIdx idx_dst(SpaceType::RECV, idx_src.group, 0);

            check("未设置 sender 时接收应失败");
            ttest(holder1.recv_space().migrate(idx_dst, cap_src) ==
                  CapErrCode::INVALID_INDEX);

            check("设置错误 sender 时接收应失败");
            holder1.recv_space().set_sender(idx_dst.group, holder1.cholder_id);
            ttest(holder1.recv_space().migrate(idx_dst, cap_src) ==
                  CapErrCode::INVALID_INDEX);

            check("设置正确 sender 后接收应成功");
            holder1.recv_space().set_sender(idx_dst.group, holder0.cholder_id);
            tassert(holder1.recv_space().migrate(idx_dst, cap_src) ==
                        CapErrCode::SUCCESS,
                    "通过 RecvSpace 进行迁移");

            check("通过 CHolder::access 访问应成功");
            auto cap_dst_opt = holder1.access(idx_dst);
            tassert(cap_dst_opt.present(), "通过 CHolder::access 访问能力");
            IntOp op_dst(cap_dst_opt.value());
            tassert(op_dst.read().value() == 24680,
                    "RecvSpace 目标能力读值校验");
        }
    };

    class CaseRevoke : public TestCase {
    public:
        CaseRevoke() : TestCase("Revoke 子树清理测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            CHolder holder0, holder1;
            CSAOp op0(holder0.csa().value());
            CSAOp op1(holder1.csa().value());

            auto idx_root = op0.get_free_slot().value();
            op0.create<IntObj>(idx_root, 999);

            auto idx_l1_keep = op0.get_free_slot().value();
            op0.clone(idx_l1_keep, &holder0.space(), idx_root);

            auto idx_l1_revoke = op0.get_free_slot().value();
            op0.clone(idx_l1_revoke, &holder0.space(), idx_root);

            auto idx_l2_revoke = op0.get_free_slot().value();
            op0.clone(idx_l2_revoke, &holder0.space(), idx_l1_revoke);

            auto idx_l3_revoke = op1.get_free_slot().value();
            op1.clone(idx_l3_revoke, &holder0.space(), idx_l2_revoke);

            action("删除待撤销分支根节点, 触发子树清理");
            tassert(op0.remove(idx_l1_revoke) == CapErrCode::SUCCESS,
                    "执行能力 remove(revoke)");

            check("校验后代是否已删除, 根与兄弟是否保留");
            ttest(!holder0.space().get(idx_l2_revoke).present());
            ttest(!holder1.space().get(idx_l3_revoke).present());
            ttest(holder0.space().get(idx_root).present());
            ttest(holder0.space().get(idx_l1_keep).present());
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
        cases.push_back(new CaseDowngrade());
        cases.push_back(new CaseRecvSpace());
        cases.push_back(new CaseRevoke());

        framework.add_category(
            new TestCategory("capability", std::move(cases)));
    }
}  // namespace test::cap
