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
#include <cap/cspace.h>
#include <cap/cholder.h>
#include <object/csa.h>
#include <object/testobj.h>
#include <perm/csa.h>
#include <perm/testobj.h>
#include <kio.h>
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
            tassert(cap_csa0_opt.present() && cap_csa1_opt.present(), "获取 CHolder 的 CSA 能力");
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
            CSAOperation op0(cap_csa0_opt.value());

            auto idx_obj0_opt = op0.alloc_slot();
            tassert(idx_obj0_opt.present(), "分配槽位 idx_obj0");
            CapIdx idx_obj0 = idx_obj0_opt.value();

            expect("在 holder0 创建 TestObject 能力, 初始值应为 12345");
            CapErrCode err = op0.create<TestObject>(idx_obj0, 12345);
            tassert(err == CapErrCode::SUCCESS, "创建 TestObject");

            check("从 CSpace 取回新建能力并执行 read() 校验初始值");
            auto cap_obj0_opt = holder0.space().get(idx_obj0);
            tassert(cap_obj0_opt.present(), "获取初始对象能力");
            
            TestObjectOperation op_obj0(cap_obj0_opt.value());
            auto read_obj0 = op_obj0.read();
            tassert(read_obj0.present() && read_obj0.value() == 12345, "初始对象读值校验");
        }
    };

    class CaseClone : public TestCase {
    public:
        CaseClone() : TestCase("Clone 测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            CHolder holder0;
            CSAOperation op0(holder0.csa().value());
            auto idx_obj0 = op0.alloc_slot().value();
            op0.create<TestObject>(idx_obj0, 12345);

            auto idx_obj0_clone_opt = op0.alloc_slot();
            tassert(idx_obj0_clone_opt.present(), "分配槽位 idx_obj0_clone");
            CapIdx idx_obj0_clone = idx_obj0_clone_opt.value();

            expect("执行 clone, 两者应指向同一 payload");
            CapErrCode err = op0.clone(idx_obj0_clone, &holder0.space(), idx_obj0);
            tassert(err == CapErrCode::SUCCESS, "执行能力 clone");

            check("读取 clone 能力, 期望值仍为 12345");
            auto cap_clone_opt = holder0.space().get(idx_obj0_clone);
            tassert(cap_clone_opt.present());
            TestObjectOperation op_clone(cap_clone_opt.value());
            tassert(op_clone.read().value() == 12345, "clone 对象读值校验");
        }
    };

    class CaseMigrate : public TestCase {
    public:
        CaseMigrate() : TestCase("Migrate 跨 CSpace 测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            CHolder holder0, holder1;
            CSAOperation op0(holder0.csa().value());
            CSAOperation op1(holder1.csa().value());
            auto idx_obj0 = op0.alloc_slot().value();
            op0.create<TestObject>(idx_obj0, 12345);

            auto idx_obj1 = op1.alloc_slot().value();
            expect("执行迁移, 源槽位应被清空");
            CapErrCode err = op1.migrate(idx_obj1, &holder0.space(), idx_obj0);
            tassert(err == CapErrCode::SUCCESS, "执行能力 migrate");

            check("校验源槽位是否已清空");
            ttest(!holder0.space().get(idx_obj0).present());

            check("校验目标槽位读值正确");
            auto cap_migrated_opt = holder1.space().get(idx_obj1);
            tassert(cap_migrated_opt.present());
            TestObjectOperation op_migrated(cap_migrated_opt.value());
            tassert(op_migrated.read().value() == 12345, "migrate 后对象读值校验");
        }
    };

    class CaseDowngrade : public TestCase {
    public:
        CaseDowngrade() : TestCase("降权测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            CHolder holder0;
            CSAOperation op0(holder0.csa().value());
            auto idx = op0.alloc_slot().value();
            op0.create<TestObject>(idx, 12345);
            Capability* cap = holder0.space().get(idx).value();

            expect("降权到 READ-only, 写操作应被拒绝");
            PermissionBits read_only(perm::testobj::READ, PayloadType::TEST_OBJECT);
            tassert(cap->downgrade(read_only) == CapErrCode::SUCCESS, "执行降权至 READ");

            TestObjectOperation op(cap);
            op.increase(); // 应该静默失败或日志报警，逻辑上值不应变
            tassert(op.read().value() == 12345, "验证 READ-only 下写操作无效");

            expect("降权到 NONE, 读操作应被拒绝");
            PermissionBits no_perm(0, PayloadType::TEST_OBJECT);
            tassert(cap->downgrade(no_perm) == CapErrCode::SUCCESS, "执行降权至 NONE");
            auto res = op.read();
            tassert(!res.present() && res.error() == CapErrCode::INSUFFICIENT_PERMISSIONS, "验证 NONE 权限下读操作被拒绝");
        }
    };

    class CaseRecvSpace : public TestCase {
    public:
        CaseRecvSpace() : TestCase("RecvSpace 接收测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            CHolder holder0, holder1;
            CSAOperation op0(holder0.csa().value());
            auto idx_src = op0.alloc_slot().value();
            op0.create<TestObject>(idx_src, 24680);
            Capability* cap_src = holder0.space().get(idx_src).value();

            CapIdx idx_dst(SpaceType::RECV, idx_src.group, 0);

            check("未设置 sender 时接收应失败");
            ttest(holder1.recv_space().migrate(idx_dst, cap_src) == CapErrCode::INVALID_INDEX);

            check("设置错误 sender 时接收应失败");
            holder1.recv_space().set_sender(idx_dst.group, holder1.cholder_id);
            ttest(holder1.recv_space().migrate(idx_dst, cap_src) == CapErrCode::INVALID_INDEX);

            check("设置正确 sender 后接收应成功");
            holder1.recv_space().set_sender(idx_dst.group, holder0.cholder_id);
            tassert(holder1.recv_space().migrate(idx_dst, cap_src) == CapErrCode::SUCCESS, "通过 RecvSpace 进行迁移");

            check("通过 CHolder::access 访问应成功");
            auto cap_dst_opt = holder1.access(idx_dst);
            tassert(cap_dst_opt.present(), "通过 CHolder::access 访问能力");
            TestObjectOperation op_dst(cap_dst_opt.value());
            tassert(op_dst.read().value() == 24680, "RecvSpace 目标能力读值校验");
        }
    };

    class CaseRevoke : public TestCase {
    public:
        CaseRevoke() : TestCase("Revoke 子树清理测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            CHolder holder0, holder1;
            CSAOperation op0(holder0.csa().value());
            CSAOperation op1(holder1.csa().value());

            auto idx_root = op0.alloc_slot().value();
            op0.create<TestObject>(idx_root, 999);

            auto idx_l1_keep = op0.alloc_slot().value();
            op0.clone(idx_l1_keep, &holder0.space(), idx_root);

            auto idx_l1_revoke = op0.alloc_slot().value();
            op0.clone(idx_l1_revoke, &holder0.space(), idx_root);

            auto idx_l2_revoke = op0.alloc_slot().value();
            op0.clone(idx_l2_revoke, &holder0.space(), idx_l1_revoke);

            auto idx_l3_revoke = op1.alloc_slot().value();
            op1.clone(idx_l3_revoke, &holder0.space(), idx_l2_revoke);

            action("删除待撤销分支根节点, 触发子树清理");
            tassert(op0.remove(idx_l1_revoke) == CapErrCode::SUCCESS, "执行能力 remove(revoke)");

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
        cases.push_back(new CaseClone());
        cases.push_back(new CaseMigrate());
        cases.push_back(new CaseDowngrade());
        cases.push_back(new CaseRecvSpace());
        cases.push_back(new CaseRevoke());

        framework.add_category(new TestCategory("capability", std::move(cases)));
    }
}
