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
#include <object/intobj.h>
#include <perm/intobj.h>
#include <perm/perm.h>
#include <test/cap.h>

namespace test::cap {
    namespace kcap = ::cap;

    struct CountingPayload : public kcap::_PayloadHelper<PayloadType::INTOBJ> {
        static size_t destruct_count;
        int value;

        explicit CountingPayload(int v) : value(v) {}

        void destruct() override {
            destruct_count++;
            delete this;
        }
    };

    size_t CountingPayload::destruct_count = 0;

    static Result<kcap::CHolder *> new_holder() {
        auto *chman = ::env::inst().chman();
        if (chman == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        auto holder_res = chman->create_holder();
        propagate(holder_res);
        return holder_res.value();
    }

    class CaseCreateObject : public TestCase {
    public:
        CaseCreateObject() : TestCase("创建对象能力并验证读写") {}

        void _run(void *env [[maybe_unused]]) const noexcept override {
            auto holder_res = new_holder();
            tassert(holder_res.has_value(), "创建 CHolder");
            auto *holder = holder_res.value();

            auto idx_res = holder->internal_lookup_freeslot();
            tassert(idx_res.has_value(), "分配槽位");
            CapIdx idx = idx_res.value();

            auto create_res =
                holder->internal_create<kcap::IntPayload>(idx, 12345);
            tassert(create_res.has_value(), "创建 IntPayload 能力");

            auto cap_res = holder->internal_lookup(idx);
            tassert(cap_res.has_value(), "取回能力");
            kcap::IntObj op(util::nnullforce(cap_res.value()));

            auto read_res = op.read();
            tassert(read_res.has_value() && read_res.value() == 12345,
                    "初始读值正确");

            auto write_res = op.write(54321);
            tassert(write_res.has_value(), "写入成功");
            auto read_after_write = op.read();
            tassert(read_after_write.has_value() &&
                        read_after_write.value() == 54321,
                    "写入后读值正确");
        }
    };

    class CaseSharedPayloadClone : public TestCase {
    public:
        CaseSharedPayloadClone() : TestCase("clone 默认共享 payload") {}

        void _run(void *env [[maybe_unused]]) const noexcept override {
            auto holder_res = new_holder();
            tassert(holder_res.has_value(), "创建 CHolder");
            auto *holder = holder_res.value();

            auto src_res = holder->internal_lookup_freeslot();
            tassert(src_res.has_value(), "分配源槽位");
            CapIdx src      = src_res.value();
            auto create_res = holder->internal_create<kcap::IntPayload>(src, 7);
            tassert(create_res.has_value(), "创建源能力");

            auto dst_res = holder->internal_lookup_freeslot();
            tassert(dst_res.has_value(), "分配 clone 槽位");
            CapIdx dst     = dst_res.value();
            auto clone_res = holder->internal_clone(dst, src);
            tassert(clone_res.has_value(), "clone 成功");

            kcap::IntObj src_op(
                util::nnullforce(holder->internal_lookup(src).value()));
            kcap::IntObj dst_op(
                util::nnullforce(holder->internal_lookup(dst).value()));

            auto write_res = src_op.write(9);
            tassert(write_res.has_value(), "源能力写入成功");
            auto cloned_read = dst_op.read();
            tassert(cloned_read.has_value() && cloned_read.value() == 9,
                    "clone 能力观察到同一 payload");
        }
    };

    class CaseDowngradeAndDerive : public TestCase {
    public:
        CaseDowngradeAndDerive() : TestCase("downgrade 与 derive 权限降级") {}

        void _run(void *env [[maybe_unused]]) const noexcept override {
            auto holder_res = new_holder();
            tassert(holder_res.has_value(), "创建 CHolder");
            auto *holder = holder_res.value();

            auto src_res = holder->internal_lookup_freeslot();
            tassert(src_res.has_value(), "分配源槽位");
            CapIdx src = src_res.value();
            auto create_res =
                holder->internal_create<kcap::IntPayload>(src, 100);
            tassert(create_res.has_value(), "创建源能力");

            auto dst_res = holder->internal_lookup_freeslot();
            tassert(dst_res.has_value(), "分配派生槽位");
            CapIdx dst = dst_res.value();

            b64 read_only   = perm::intobj::READ;
            auto derive_res = holder->internal_derive(dst, src, read_only);
            tassert(derive_res.has_value(), "derive READ-only 成功");

            kcap::IntObj derived(
                util::nnullforce(holder->internal_lookup(dst).value()));
            auto read_res = derived.read();
            tassert(read_res.has_value() && read_res.value() == 100,
                    "派生能力可读");

            auto write_res = derived.write(200);
            tassert(!write_res.has_value() &&
                        write_res.error() == ErrCode::INSUFFICIENT_PERMISSIONS,
                    "派生能力不可写");
        }
    };

    class CaseMigrate : public TestCase {
    public:
        CaseMigrate() : TestCase("migrate 仅在同一 CHolder 内移动") {}

        void _run(void *env [[maybe_unused]]) const noexcept override {
            auto holder_res = new_holder();
            tassert(holder_res.has_value(), "创建 CHolder");
            auto *holder = holder_res.value();

            auto src_res = holder->internal_lookup_freeslot();
            tassert(src_res.has_value(), "分配源槽位");
            CapIdx src = src_res.value();
            auto create_res =
                holder->internal_create<kcap::IntPayload>(src, 11);
            tassert(create_res.has_value(), "创建源能力");

            auto dst_res = holder->internal_lookup_freeslot();
            tassert(dst_res.has_value(), "分配目标槽位");
            CapIdx dst       = dst_res.value();
            auto migrate_res = holder->internal_migrate(dst, src);
            tassert(migrate_res.has_value(), "migrate 成功");

            auto src_lookup = holder->internal_lookup(src);
            tassert(!src_lookup.has_value() &&
                        src_lookup.error() == ErrCode::OUT_OF_BOUNDARY,
                    "源槽位已清空");

            kcap::IntObj dst_op(
                util::nnullforce(holder->internal_lookup(dst).value()));
            auto read_res = dst_op.read();
            tassert(read_res.has_value() && read_res.value() == 11,
                    "目标槽位读值正确");
        }
    };

    class CasePayloadDestruct : public TestCase {
    public:
        CasePayloadDestruct() : TestCase("payload 最后引用释放时 destruct") {}

        void _run(void *env [[maybe_unused]]) const noexcept override {
            CountingPayload::destruct_count = 0;

            auto holder_res = new_holder();
            tassert(holder_res.has_value(), "创建 CHolder");
            auto *holder = holder_res.value();

            auto src_res = holder->internal_lookup_freeslot();
            tassert(src_res.has_value(), "分配源槽位");
            CapIdx src      = src_res.value();
            auto create_res = holder->internal_create<CountingPayload>(src, 1);
            tassert(create_res.has_value(), "创建计数 payload");

            auto dst_res = holder->internal_lookup_freeslot();
            tassert(dst_res.has_value(), "分配 clone 槽位");
            CapIdx dst     = dst_res.value();
            auto clone_res = holder->internal_clone(dst, src);
            tassert(clone_res.has_value(), "clone 成功");

            auto remove_src = holder->internal_remove(src);
            tassert(remove_src.has_value(), "删除源能力");
            ttest(CountingPayload::destruct_count == 0);

            auto remove_dst = holder->internal_remove(dst);
            tassert(remove_dst.has_value(), "删除最后一个能力");
            ttest(CountingPayload::destruct_count == 1);
        }
    };

    void collect_tests(TestFramework &framework) {
        auto cases = util::ArrayList<TestCase *>();
        cases.push_back(new CaseCreateObject());
        cases.push_back(new CaseSharedPayloadClone());
        cases.push_back(new CaseDowngradeAndDerive());
        cases.push_back(new CaseMigrate());
        cases.push_back(new CasePayloadDestruct());

        framework.add_category(
            new TestCategory("capability", std::move(cases)));
    }
}  // namespace test::cap
