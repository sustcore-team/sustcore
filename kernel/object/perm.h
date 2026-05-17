/**
 * @file perm.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 权限定义
 * @version alpha-1.0.0
 * @date 2026-02-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/types.h>

namespace perm::basic {
    // permissions
    constexpr b64 CLONE        = 0x0002;
    constexpr b64 MIGRATE      = 0x0004;
    /**
     * @brief 一次性迁移权限.
     *
     * 允许执行一次MIGRATE语义的转移; 转移完成后目标cap会自动清除此位,
     * 避免被接收方继续二次分发.
     */
    constexpr b64 MIGRATE_ONCE = 0x0008;
}  // namespace perm::basic

namespace perm::endpoint {
    constexpr b64 GRANT = 0x01'0000;
    constexpr b64 READ  = 0x02'0000;
    constexpr b64 WRITE = 0x04'0000;
}  // namespace perm::endpoint

namespace perm::reply {
    /// 允许从ReplyObject读取回复消息.
    constexpr b64 CALLER  = 0x01'0000;
    /// 允许向ReplyObject写入回复消息.
    constexpr b64 REPLIER = 0x02'0000;
}  // namespace perm::reply

namespace perm::intobj {
    // IntObjectect的权限定义
    // 该对象仅用于测试能力系统, 因此权限非常简单
    constexpr b64 READ     = 0x1'0000;
    constexpr b64 WRITE    = 0x2'0000;
    constexpr b64 INCREASE = 0x4'0000;
    constexpr b64 DECREASE = 0x8'0000;
}  // namespace perm::intobj

namespace perm::memory {
    constexpr b64 MAP      = 0x01'0000;
    constexpr b64 READ     = 0x02'0000;
    constexpr b64 WRITE    = 0x04'0000;
    constexpr b64 EXEC     = 0x08'0000;
    constexpr b64 RESIZE   = 0x10'0000;
    constexpr b64 QUERY    = 0x20'0000;
    constexpr b64 FLEXUP   = 0x40'0000;
    constexpr b64 FLEXDOWN = 0x80'0000;
}  // namespace perm::memory

namespace perm::mutex {
    // Mutex的权限定义
    // 使用互斥锁( lock() / unlock() )的权限
    constexpr b64 USE     = 0x01'0000;
    // 查询锁状态的权限
    constexpr b64 QUERY   = 0x02'0000;
    // 销毁锁的权限
    constexpr b64 DESTROY = 0x04'0000;
}  // namespace perm::mutex

namespace perm::notif {
    // 发送信号/取消信号的权限
    constexpr b8 SIGNAL   = 0b01;
    // 查询信号/等待信号的权限
    constexpr b8 QUERY    = 0b10;
    // 一个信号所需的权限位数
    constexpr size_t BITS = 2;
    constexpr size_t STARTBITS =
        16;  // 从第16位开始分配信号权限(即从0x0001'0000开始)
    constexpr size_t MASK = 0b11;  // 每个信号占用的权限位掩码
    // 总共支持的信号数量
    constexpr size_t MAX_SIGNALS = (sizeof(b64) * 8 - STARTBITS) / BITS;

    // 计算第idx个信号的权限位偏移
    constexpr size_t bitoffset(size_t idx) {
        return STARTBITS + idx * BITS;
    }

    // 获取第idx个信号的权限值
    constexpr b8 perm(size_t permbits, size_t idx) {
        return (permbits >> bitoffset(idx)) & MASK;
    }
}  // namespace perm::notif

namespace perm::pcb {
    constexpr b64 GETPID    = 0x01'0000;
    constexpr b64 KILL      = 0x02'0000;
    constexpr b64 VMCONTEXT = 0x04'0000;
    constexpr b64 NEW_THREAD  = 0x08'0000;
    constexpr b64 NEW_PROCESS = 0x10'0000;
    constexpr b64 EXECUTE     = 0x20'0000;
}  // namespace perm::pcb

namespace perm::sintobj {
    // SharedIntObjectect的权限定义
    // 该对象仅用于测试能力系统, 因此权限非常简单
    constexpr b64 READ     = 0x1'0000;
    constexpr b64 WRITE    = 0x2'0000;
    constexpr b64 INCREASE = 0x4'0000;
    constexpr b64 DECREASE = 0x8'0000;
}  // namespace perm::sintobj

namespace perm::vfile {
    // VFile的权限定义
    // basic permissions
    constexpr b64 READ  = 0x01'0000;
    constexpr b64 WRITE = 0x02'0000;
    constexpr b64 EXEC  = 0x04'0000;
}  // namespace perm::vfile
