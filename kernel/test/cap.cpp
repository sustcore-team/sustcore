/**
 * @file cap.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 使用 IntegerObject 验证 Capability Phase 1 基础设施。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <obj/cspace.h>
#include <obj/mem_seg.h>
#include <sustcore/addr.h>
#include <sustcore/capability.h>
#include <test/cases.h>
#include <test/int_obj.h>

#include <type_traits>

namespace kernel::test::cases {
    namespace {
        using fixtures::IntegerObject;

        template <typename Alternative, typename T>
        void require_error(const T &result, const char *message) noexcept {
            kernel::test::require(!result && result.error().template is<Alternative>(), message);
        }

        cap::CToken install_int(cap::CSpace &space, i64_t value, u64_t rights,
                                u8_t cnode = cap::ROOT_CNODE_INDEX) noexcept {
            auto object = IntegerObject::create(value);
            kernel::test::require(object.has_value(), "无法创建 IntegerObject");
            auto token = space.install(**object, rights, 0, cnode);
            if (!token) {
                delete *object;
                kernel::test::fail("无法安装 IntegerObject");
            }
            return *token;
        }
    }  // namespace

    void run_capability(Context &) noexcept {
        static_assert(sizeof(cap::CError) <= 32);
        static_assert(std::is_nothrow_move_constructible_v<cap::CError>);
        static_assert(sizeof(cap::Capability) == cap::CAPABILITY_SIZE);
        static_assert(alignof(cap::Capability) == cap::CAPABILITY_ALIGNMENT);
        static_assert(sizeof(cap::CNodeCell) == cap::CNODE_CELL_SIZE);
        static_assert(sizeof(cap::CToken) == cap::CAP_TOKEN_SIZE);
        static_assert(cap::SMALL_CNODE_SIZE ==
                      (cap::SMALL_CNODE_CAPACITY + 1) * cap::CNODE_CELL_SIZE);

        auto segment = memory::MemSeg::create(PAGE_SIZE * 2 + 17);
        kernel::test::require(segment.has_value(), "无法创建 MemSeg");
        kernel::test::require((*segment)->allocated_size() == 0, "MemSeg 创建时提前分配物理页");
        const std::byte payload[PAGE_SIZE + 3]{};
        auto written = (*segment)->write(PAGE_SIZE - 1, payload, sizeof(payload));
        kernel::test::require(written && *written == sizeof(payload), "MemSeg 跨页写入失败");
        kernel::test::require((*segment)->allocated_size() == PAGE_SIZE * 3,
                              "MemSeg 懒分配页数错误");
        kernel::test::require((*segment)->lookup_page(0).has_value(), "MemSeg 未记录首个物理页");
        segment->reset();

        constexpr auto encoded = cap::encode_token(0x1357, 0x2468, 3, 9);
        constexpr auto decoded = cap::decode_token(encoded);
        static_assert(decoded.valid && decoded.cspace_cookie == 0x1357 &&
                      decoded.generation == 0x2468 && decoded.cnode_index == 3 &&
                      decoded.slot_index == 9);
        static_assert(!cap::decode_token(cap::CToken{}).valid);

        kernel::test::require(IntegerObject::live_count() == 0, "IntegerObject 初始计数非零");
        auto created = cap::CSpace::create();
        kernel::test::require(created.has_value(), "无法创建 CSpace");
        auto *space = *created;

        constexpr u64_t ROOT_RIGHTS = cap::RIGHT_READ | cap::RIGHT_WRITE | cap::RIGHT_COPY |
                                      cap::RIGHT_MINT | cap::RIGHT_REVOKE | cap::RIGHT_GRANT;
        const auto root = install_int(*space, 42, ROOT_RIGHTS);

        auto resolved = space->resolve<IntegerObject>(root, cap::RIGHT_READ);
        kernel::test::require(resolved && resolved->object()->value() == 42,
                              "typed resolve 返回错误对象");
        kernel::test::require(resolved->allows(cap::RIGHT_WRITE), "typed resolve 丢失权限快照");

        auto type_mismatch = space->resolve(root, cap::ObjectType::THREAD);
        require_error<cap::CError::TypeMismatch>(type_mismatch, "未拒绝错误对象类型");
        type_mismatch.error().visit([&](const auto &error) noexcept {
            using Error = std::remove_cvref_t<decltype(error)>;
            if constexpr (std::is_same_v<Error, cap::CError::TypeMismatch>) {
                kernel::test::require(error.token == root &&
                                          error.expected == cap::ObjectType::THREAD &&
                                          error.actual == cap::ObjectType::INTEGER,
                                      "类型错误未保留 token 或类型上下文");
            }
        });
        const auto fields = cap::decode_token(root);
        const auto foreign =
            cap::encode_token(static_cast<u16_t>(space->cookie() + 1), fields.generation,
                              fields.cnode_index, fields.slot_index);
        auto invalid_token = space->resolve(foreign);
        require_error<cap::CError::InvalidToken>(invalid_token, "未拒绝错误 CSpace cookie");
        auto missing_node = space->resolve(
            cap::encode_token(space->cookie(), fields.generation, 255, fields.slot_index));
        require_error<cap::CError::MissingCNode>(missing_node, "未拒绝缺失 CNode");
        auto invalid_slot = space->resolve(
            cap::encode_token(space->cookie(), fields.generation, fields.cnode_index, 0xffff));
        require_error<cap::CError::InvalidSlot>(invalid_slot, "未拒绝非法 slot");

        auto copied = space->copy(root);
        kernel::test::require(copied.has_value(), "copy 失败");
        auto copied_view = space->resolve<IntegerObject>(*copied, ROOT_RIGHTS);
        kernel::test::require(copied_view && copied_view->object() == resolved->object(),
                              "copy 未共享同一个对象");

        auto limited = space->mint(root, cap::RIGHT_MINT | cap::RIGHT_READ, 0x55aa);
        kernel::test::require(limited.has_value(), "mint 降权失败");
        auto limited_view = space->resolve<IntegerObject>(*limited, cap::RIGHT_READ);
        kernel::test::require(limited_view && limited_view->badge() == 0x55aa,
                              "mint badge 或只读权限错误");
        auto denied_write = space->resolve<IntegerObject>(*limited, cap::RIGHT_WRITE);
        require_error<cap::CError::InsufficientRights>(denied_write,
                                                       "只读 capability 通过写权限检查");
        denied_write.error().visit([&](const auto &error) noexcept {
            using Error = std::remove_cvref_t<decltype(error)>;
            if constexpr (std::is_same_v<Error, cap::CError::InsufficientRights>) {
                kernel::test::require(error.token == *limited &&
                                          error.required == cap::RIGHT_WRITE &&
                                          (error.available & cap::RIGHT_WRITE) == 0,
                                      "权限错误未保留请求和可用权限");
            }
        });
        require_error<cap::CError::InsufficientRights>(
            space->mint(*limited, cap::RIGHT_READ | cap::RIGHT_WRITE), "mint 允许 capability 扩权");

        const auto copied_fields = cap::decode_token(*copied);
        kernel::test::require(space->delete_cap(*copied).has_value(), "删除 copy 失败");
        const auto replacement        = install_int(*space, 7, cap::RIGHT_READ);
        const auto replacement_fields = cap::decode_token(replacement);
        kernel::test::require(replacement_fields.slot_index == copied_fields.slot_index,
                              "free list 未优先复用刚释放的 slot");
        kernel::test::require(replacement_fields.generation != copied_fields.generation,
                              "slot 复用未更新 generation");
        auto stale = space->resolve(*copied);
        require_error<cap::CError::StaleToken>(stale, "slot 复用后旧 token 重新生效");
        stale.error().visit([&](const auto &error) noexcept {
            using Error = std::remove_cvref_t<decltype(error)>;
            if constexpr (std::is_same_v<Error, cap::CError::StaleToken>) {
                kernel::test::require(error.token == *copied && error.observed_generation ==
                                                                    replacement_fields.generation,
                                      "stale token 未保留观察到的 generation");
            }
        });
        kernel::test::require(space->delete_cap(replacement).has_value(),
                              "删除替换 IntegerObject 失败");

        const auto pinned_token = install_int(*space, 99, cap::RIGHT_READ);
        auto pin = space->resolve(pinned_token, cap::ObjectType::INTEGER, cap::RIGHT_READ);
        kernel::test::require(pin.has_value(), "无法取得 kernel pin");
        const auto live_before_delete = IntegerObject::live_count();
        kernel::test::require(space->delete_cap(pinned_token).has_value(), "持有 pin 时删除失败");
        kernel::test::require(IntegerObject::live_count() == live_before_delete,
                              "删除最后 capability 时提前销毁 pinned 对象");
        kernel::test::require(static_cast<IntegerObject *>(pin->get())->value() == 99,
                              "capability 删除后 pin 无法继续访问对象");
        pin->reset();
        kernel::test::require(IntegerObject::live_count() + 1 == live_before_delete,
                              "释放最后 pin 后对象未销毁");

        auto child = space->copy(root);
        kernel::test::require(child.has_value(), "创建 CDT child 失败");
        auto grandchild = space->mint(*child, cap::RIGHT_READ, 3);
        kernel::test::require(grandchild.has_value(), "创建 CDT grandchild 失败");
        kernel::test::require(space->revoke(root).has_value(), "revoke 派生树失败");
        require_error<cap::CError::InvalidSlot>(space->resolve(*child), "revoke 后 child 仍可解析");
        require_error<cap::CError::InvalidSlot>(space->resolve(*grandchild),
                                                "revoke 后 grandchild 仍可解析");
        kernel::test::require(space->resolve<IntegerObject>(root, cap::RIGHT_READ).has_value(),
                              "revoke 错误删除 source capability");

        auto small = cap::CNode::create_small();
        kernel::test::require(small.has_value(), "无法创建 SmallCNode");
        auto attached = space->attach(**small);
        kernel::test::require(attached && *attached != cap::ROOT_CNODE_INDEX,
                              "无法 attach SmallCNode");
        const auto small_token = install_int(*space, -1, cap::RIGHT_READ, *attached);
        require_error<cap::CError::Busy>(space->detach(*attached), "非空 CNode 被错误 detach");
        kernel::test::require(space->delete_cap(small_token).has_value(), "无法清空 SmallCNode");
        auto detached = space->detach(*attached);
        kernel::test::require(detached && *detached == *small, "detach 未返回原 CNode");
        delete *detached;

        limited_view->reset();
        copied_view->reset();
        resolved->reset();
        kernel::test::require(space->delete_cap(root).has_value(), "删除 root capability 失败");
        delete space;
        kernel::test::require(IntegerObject::live_count() == 0,
                              "Capability 测试结束后 IntegerObject 泄漏");
    }
}  // namespace kernel::test::cases
