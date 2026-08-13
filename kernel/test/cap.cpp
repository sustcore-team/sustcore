/**
 * @file cap.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 使用 IntegerObject 验证 Capability Phase 1 基础设施。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <log.h>
#include <obj/cspace.h>
#include <obj/integer.h>
#include <obj/memory_segment.h>
#include <sustcore/addr.h>
#include <sustcore/capability.h>

namespace test {
    namespace {
        void require(bool condition, const char *message) noexcept {
            if (!condition)
                kernel::log::panic("Capability selftest 失败: {}", message);
        }

        template <typename T>
        void require_error(const T &result, cap::CapError expected, const char *message) noexcept {
            require(!result && result.error() == expected, message);
        }

        cap::CapToken install_int(cap::CSpace &space, i64_t value, u64_t rights,
                                  u8_t cnode = cap::ROOT_CNODE_INDEX) noexcept {
            auto object = cap::IntegerObject::create(value);
            require(object.has_value(), "无法创建 IntegerObject");
            auto token = space.install(**object, rights, 0, cnode);
            if (!token) {
                delete *object;
                kernel::log::panic("Capability selftest 失败: 无法安装 IntegerObject");
            }
            return *token;
        }
    }  // namespace

    void run_capability_selftest() noexcept {
        static_assert(sizeof(cap::Capability) == cap::CAPABILITY_SIZE);
        static_assert(alignof(cap::Capability) == cap::CAPABILITY_ALIGNMENT);
        static_assert(sizeof(cap::CNodeCell) == cap::CNODE_CELL_SIZE);
        static_assert(sizeof(cap::CapToken) == cap::CAP_TOKEN_SIZE);
        static_assert(cap::SMALL_CNODE_SIZE ==
                      (cap::SMALL_CNODE_CAPACITY + 1) * cap::CNODE_CELL_SIZE);

        auto segment = memory::MemorySegment::create(PAGE_SIZE * 2 + 17);
        require(segment.has_value(), "无法创建 MemorySegment");
        require((*segment)->allocated_size() == 0, "MemorySegment 创建时提前分配物理页");
        const std::byte payload[PAGE_SIZE + 3]{};
        auto written = (*segment)->write(PAGE_SIZE - 1, payload, sizeof(payload));
        require(written && *written == sizeof(payload), "MemorySegment 跨页写入失败");
        require((*segment)->allocated_size() == PAGE_SIZE * 3, "MemorySegment 懒分配页数错误");
        require((*segment)->lookup_page(0).has_value(), "MemorySegment 未记录首个物理页");
        segment->reset();

        constexpr auto encoded = cap::encode_token(0x1357, 0x2468, 3, 9);
        constexpr auto decoded = cap::decode_token(encoded);
        static_assert(decoded.valid && decoded.cspace_cookie == 0x1357 &&
                      decoded.generation == 0x2468 && decoded.cnode_index == 3 &&
                      decoded.slot_index == 9);
        static_assert(!cap::decode_token(cap::CapToken{}).valid);

        require(cap::IntegerObject::live_count() == 0, "IntegerObject 初始计数非零");
        auto created = cap::CSpace::create();
        require(created.has_value(), "无法创建 CSpace");
        auto *space = *created;

        constexpr u64_t ROOT_RIGHTS = cap::RIGHT_READ | cap::RIGHT_WRITE | cap::RIGHT_COPY |
                                      cap::RIGHT_MINT | cap::RIGHT_REVOKE | cap::RIGHT_GRANT;
        const auto root = install_int(*space, 42, ROOT_RIGHTS);

        auto resolved = space->resolve<cap::IntegerObject>(root, cap::RIGHT_READ);
        require(resolved && resolved->object()->value() == 42, "typed resolve 返回错误对象");
        require(resolved->allows(cap::RIGHT_WRITE), "typed resolve 丢失权限快照");

        require_error(space->resolve(root, cap::ObjectType::THREAD), cap::CapError::TYPE_MISMATCH,
                      "未拒绝错误对象类型");
        const auto fields = cap::decode_token(root);
        require_error(space->resolve(cap::encode_token(static_cast<u16_t>(space->cookie() + 1),
                                                       fields.generation, fields.cnode_index,
                                                       fields.slot_index)),
                      cap::CapError::INVALID_TOKEN, "未拒绝错误 CSpace cookie");
        require_error(space->resolve(cap::encode_token(space->cookie(), fields.generation, 255,
                                                       fields.slot_index)),
                      cap::CapError::MISSING_CNODE, "未拒绝缺失 CNode");
        require_error(space->resolve(cap::encode_token(space->cookie(), fields.generation,
                                                       fields.cnode_index, 0xffff)),
                      cap::CapError::INVALID_SLOT, "未拒绝非法 slot");

        auto copied = space->copy(root);
        require(copied.has_value(), "copy 失败");
        auto copied_view = space->resolve<cap::IntegerObject>(*copied, ROOT_RIGHTS);
        require(copied_view && copied_view->object() == resolved->object(),
                "copy 未共享同一个对象");

        auto limited = space->mint(root, cap::RIGHT_MINT | cap::RIGHT_READ, 0x55aa);
        require(limited.has_value(), "mint 降权失败");
        auto limited_view = space->resolve<cap::IntegerObject>(*limited, cap::RIGHT_READ);
        require(limited_view && limited_view->badge() == 0x55aa, "mint badge 或只读权限错误");
        require_error(space->resolve<cap::IntegerObject>(*limited, cap::RIGHT_WRITE),
                      cap::CapError::INSUFFICIENT_RIGHTS, "只读 capability 通过写权限检查");
        require_error(space->mint(*limited, cap::RIGHT_READ | cap::RIGHT_WRITE),
                      cap::CapError::INSUFFICIENT_RIGHTS, "mint 允许 capability 扩权");

        const auto copied_fields = cap::decode_token(*copied);
        require(space->delete_cap(*copied).has_value(), "删除 copy 失败");
        const auto replacement        = install_int(*space, 7, cap::RIGHT_READ);
        const auto replacement_fields = cap::decode_token(replacement);
        require(replacement_fields.slot_index == copied_fields.slot_index,
                "free list 未优先复用刚释放的 slot");
        require(replacement_fields.generation != copied_fields.generation,
                "slot 复用未更新 generation");
        require_error(space->resolve(*copied), cap::CapError::STALE_TOKEN,
                      "slot 复用后旧 token 重新生效");
        require(space->delete_cap(replacement).has_value(), "删除替换 IntegerObject 失败");

        const auto pinned_token = install_int(*space, 99, cap::RIGHT_READ);
        auto pin = space->resolve(pinned_token, cap::ObjectType::INTEGER, cap::RIGHT_READ);
        require(pin.has_value(), "无法取得 kernel pin");
        const auto live_before_delete = cap::IntegerObject::live_count();
        require(space->delete_cap(pinned_token).has_value(), "持有 pin 时删除失败");
        require(cap::IntegerObject::live_count() == live_before_delete,
                "删除最后 capability 时提前销毁 pinned 对象");
        require(static_cast<cap::IntegerObject *>(pin->get())->value() == 99,
                "capability 删除后 pin 无法继续访问对象");
        pin->reset();
        require(cap::IntegerObject::live_count() + 1 == live_before_delete,
                "释放最后 pin 后对象未销毁");

        auto child = space->copy(root);
        require(child.has_value(), "创建 CDT child 失败");
        auto grandchild = space->mint(*child, cap::RIGHT_READ, 3);
        require(grandchild.has_value(), "创建 CDT grandchild 失败");
        require(space->revoke(root).has_value(), "revoke 派生树失败");
        require_error(space->resolve(*child), cap::CapError::INVALID_SLOT,
                      "revoke 后 child 仍可解析");
        require_error(space->resolve(*grandchild), cap::CapError::INVALID_SLOT,
                      "revoke 后 grandchild 仍可解析");
        require(space->resolve<cap::IntegerObject>(root, cap::RIGHT_READ).has_value(),
                "revoke 错误删除 source capability");

        auto small = cap::CNode::create_small();
        require(small.has_value(), "无法创建 SmallCNode");
        auto attached = space->attach(**small);
        require(attached && *attached != cap::ROOT_CNODE_INDEX, "无法 attach SmallCNode");
        const auto small_token = install_int(*space, -1, cap::RIGHT_READ, *attached);
        require_error(space->detach(*attached), cap::CapError::BUSY, "非空 CNode 被错误 detach");
        require(space->delete_cap(small_token).has_value(), "无法清空 SmallCNode");
        auto detached = space->detach(*attached);
        require(detached && *detached == *small, "detach 未返回原 CNode");
        delete *detached;

        limited_view->reset();
        copied_view->reset();
        resolved->reset();
        require(space->delete_cap(root).has_value(), "删除 root capability 失败");
        delete space;
        require(cap::IntegerObject::live_count() == 0, "Capability 测试结束后 IntegerObject 泄漏");
        kernel::log::info("Capability Phase 1 selftest 通过");
    }
}  // namespace test
