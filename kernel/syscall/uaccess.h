/**
 * @file uaccess.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 用户态缓冲区访问封装
 * @version alpha-1.0.0
 * @date 2026-05-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <env.h>
#include <logger.h>
#include <mem/vma.h>
#include <object/memory.h>
#include <task/scheduler.h>
#include <sus/owner.h>
#include <sus/range.h>
#include <sustcore/addr.h>
#include <syscall/syscall.h>

#include <cstddef>
#include <cstring>

namespace syscall {
    /**
     * @brief 将用户空间缓冲区映射为内核可读写缓存.
     */
    class UBuffer {
    private:
        VirAddr _uaddr;             // 用户空间地址
        util::owner<char *> _kbuf;  // 内核空间缓冲区地址
        size_t _len;                // 数据长度
        TaskMemoryManager *_tmm;    // 用户空间内存管理器
        cap::MemoryPayload *_memory;
        size_t _mem_offset;
        bool _resolved;

        /**
         * @brief 解析当前系统调用对应的用户地址空间.
         *
         * @return Result<TaskMemoryManager*> 成功时返回用户地址空间管理器.
         */
        [[nodiscard]]
        Result<TaskMemoryManager *> resolve_tmm() noexcept {
            if (_tmm != nullptr) {
                return _tmm;
            }

            auto *current = schd::Scheduler::inst().current_tcb();
            if (current != nullptr && current->task != nullptr) {
                _tmm = current->task->tmm.get();
            } else {
                _tmm = env::inst().tmm();
            }

            if (_tmm == nullptr) {
                loggers::SYSCALL::ERROR("UBuffer: 用户地址空间为空");
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            return _tmm;
        }

        /**
         * @brief 解析用户缓冲区对应的唯一 MemoryPayload.
         *
         * 要求整个缓冲区都落在同一个 VMA 中; 否则返回错误.
         *
         * @return Result<void> 成功返回空结果.
         */
        [[nodiscard]]
        Result<void> resolve_payload() {
            if (_resolved || _len == 0) {
                _resolved = true;
                void_return();
            }

            auto tmm_res = resolve_tmm();
            propagate(tmm_res);
            auto locate_res = tmm_res.value()->locate(_uaddr);
            propagate(locate_res);
            VMA *vma = locate_res.value();
            auto *memory = vma != nullptr ? vma->memory_payload() : nullptr;
            if (memory == nullptr) {
                loggers::SYSCALL::ERROR("UBuffer: 用户缓冲区无有效 MemoryPayload");
                unexpect_return(ErrCode::INVALID_PARAM);
            }

            VirAddr end = _uaddr + _len;
            if (!within(vma->varea, _uaddr) || end > vma->varea.end) {
                loggers::SYSCALL::ERROR(
                    "UBuffer: 用户缓冲区跨越多个VMA或超出VMA范围: [%p, %p)",
                    _uaddr.addr(), end.addr());
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }

            _memory     = memory;
            _mem_offset = vma->mem_offset + (_uaddr - vma->varea.begin);
            _resolved   = true;
            auto *current = schd::Scheduler::inst().current_tcb();
            loggers::SYSCALL::DEBUG(
                "UBuffer: 解析成功 pid=%lu uaddr=%p len=%lu vma=[%p,%p) "
                "type=%d mem=%p vma_mem_off=%lu mem_off=%lu",
                current != nullptr && current->task != nullptr
                    ? current->task->pid
                    : 0,
                _uaddr.addr(), _len, vma->varea.begin.addr(),
                vma->varea.end.addr(), static_cast<int>(vma->type), _memory,
                vma->mem_offset, _mem_offset);
            void_return();
        }

    public:
        /**
         * @brief 构造一个用户缓冲区代理.
         *
         * @param uaddr 用户空间地址.
         * @param len 缓冲区长度.
         */
        UBuffer(VirAddr uaddr, size_t len)
            : _uaddr(uaddr),
              _kbuf(util::owner(new char[len])),
              _len(len),
              _tmm(nullptr),
              _memory(nullptr),
              _mem_offset(0),
              _resolved(false) {
        }

        UBuffer(UBuffer &)            = delete;
        UBuffer &operator=(UBuffer &) = delete;

        /**
         * @brief 移动构造用户缓冲区代理.
         *
         * @param other 被移动对象.
         */
        UBuffer(UBuffer &&other) noexcept
            : _uaddr(other._uaddr),
              _kbuf(std::move(other._kbuf)),
              _len(other._len),
              _tmm(other._tmm),
              _memory(other._memory),
              _mem_offset(other._mem_offset),
              _resolved(other._resolved) {
            other._uaddr = VirAddr::null;
            other._kbuf  = util::owner<char *>(nullptr);
            other._len   = 0;
            other._tmm   = nullptr;
            other._memory = nullptr;
            other._mem_offset = 0;
            other._resolved = false;
        }

        /**
         * @brief 释放内核缓冲区资源.
         */
        void cleanup() noexcept {
            if (_kbuf != nullptr) {
                loggers::SYSCALL::DEBUG("释放内核缓冲区: %p", _kbuf.get());
                delete[] _kbuf;
                _kbuf = util::owner<char *>(nullptr);
            }
        }

        /**
         * @brief 移动赋值用户缓冲区代理.
         *
         * @param other 被移动对象.
         * @return UBuffer& 当前对象.
         */
        UBuffer &operator=(UBuffer &&other) noexcept {
            if (this != &other) {
                cleanup();
                _uaddr = other._uaddr;
                _kbuf  = std::move(other._kbuf);
                _len   = other._len;
                _tmm   = other._tmm;
                _memory = other._memory;
                _mem_offset = other._mem_offset;
                _resolved = other._resolved;

                other._kbuf  = util::owner<char *>(nullptr);
                other._uaddr = VirAddr::null;
                other._len   = 0;
                other._tmm   = nullptr;
                other._memory = nullptr;
                other._mem_offset = 0;
                other._resolved = false;
            }
            return *this;
        }

        /**
         * @brief 析构并释放内部缓冲.
         */
        ~UBuffer() noexcept {
            cleanup();
        }

        /**
         * @brief 将用户空间数据复制到内核缓冲区.
         *
         * @return Result<void> 成功返回空结果.
         */
        [[nodiscard]]
        Result<void> sync_from_user() {
            if (_kbuf == nullptr || _len == 0) {
                void_return();
            }

            auto resolve_res = resolve_payload();
            propagate(resolve_res);
            auto read_res = _memory->read(_mem_offset, _kbuf, _len);
            if (!read_res.has_value()) {
                loggers::SYSCALL::ERROR(
                    "UBuffer: 从用户空间同步失败: %s",
                    to_cstring(read_res.error()));
                propagate_return(read_res);
            }
            void_return();
        }

        /**
         * @brief 将内核缓冲区内容复制回用户空间.
         *
         * @return Result<void> 成功返回空结果, 失败返回错误码.
         */
        [[nodiscard]]
        Result<void> commit_to_user(size_t len) noexcept {
            if (len > _len) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            if (_kbuf == nullptr || len == 0) {
                void_return();
            }

            auto resolve_res = resolve_payload();
            propagate(resolve_res);
            auto first_page_res = _memory->lookup_page(_mem_offset);
            if (first_page_res.has_value()) {
                loggers::SYSCALL::DEBUG(
                    "UBuffer: commit pid=%lu uaddr=%p len=%lu mem=%p mem_off=%lu "
                    "first_paddr=%p",
                    schd::Scheduler::inst().current_tcb() != nullptr &&
                            schd::Scheduler::inst().current_tcb()->task != nullptr
                        ? schd::Scheduler::inst().current_tcb()->task->pid
                        : 0,
                    _uaddr.addr(), len, _memory, _mem_offset,
                    first_page_res.value().addr());
            }
            
            auto write_res = _memory->write(_mem_offset, _kbuf, len);
            if (!write_res.has_value()) {
                loggers::SYSCALL::ERROR(
                    "UBuffer: 提交到用户空间失败: %s",
                    to_cstring(write_res.error()));
                propagate_return(write_res);
            }
            
            void_return();
        }

        [[nodiscard]]
        Result<void> commit_to_user() noexcept {
            return commit_to_user(_len);
        }

        /**
         * @brief 返回内核缓冲区地址.
         */
        [[nodiscard]] char *kbuf() const {
            return _kbuf;
        }

        /**
         * @brief 返回缓冲区长度.
         */
        [[nodiscard]] size_t len() const {
            return _len;
        }

        /**
         * @brief 返回用户空间原始地址.
         */
        [[nodiscard]] VirAddr uaddr() const {
            return _uaddr;
        }
    };

    /**
     * @brief 用户空间只读字符串代理.
     */
    class UString {
    private:
        UBuffer _ubuf;
        int _len;

        [[nodiscard]]
        Result<char> read_user_char(size_t offset) noexcept {
            TaskMemoryManager *tmm = nullptr;
            auto *current = schd::Scheduler::inst().current_tcb();
            if (current != nullptr && current->task != nullptr) {
                tmm = current->task->tmm.get();
            } else {
                tmm = env::inst().tmm();
            }
            if (tmm == nullptr) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            VirAddr addr = uaddr() + offset;
            auto locate_res = tmm->locate(addr);
            propagate(locate_res);
            VMA *vma = locate_res.value();
            auto *memory = vma != nullptr ? vma->memory_payload() : nullptr;
            if (memory == nullptr || !within(vma->varea, addr))
            {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }

            size_t mem_offset = vma->mem_offset + (addr - vma->varea.begin);
            char ch           = '\0';
            auto read_res     = memory->read(mem_offset, &ch, 1);
            propagate(read_res);
            if (read_res.value() != 1) {
                unexpect_return(ErrCode::IO_ERROR);
            }
            return ch;
        }

    public:
        /**
         * @brief 从用户空间构造字符串代理并立即同步.
         *
         * @param uaddr 用户空间地址.
         * @param maxlen 最大读取长度.
         */
        UString(VirAddr uaddr, size_t maxlen) : _ubuf(uaddr, maxlen) {
            auto sync_res = sync_from_user();
            if (!sync_res.has_value()) {
                _len = 0;
            }
        }

        /**
         * @brief 复制构造字符串代理.
         *
         * @param other 被复制对象.
         */
        UString(const UString &other) : _ubuf(other.uaddr(), other.maxlen()) {
            memcpy(_ubuf.kbuf(), other.kbuf(), other.maxlen());
            _len = other._len;
        }

        /**
         * @brief 移动构造字符串代理.
         *
         * @param other 被移动对象.
         */
        UString(UString &&other) noexcept
            : _ubuf(std::move(other._ubuf)), _len(other._len) {
            other._len = 0;
        }

        /**
         * @brief 复制赋值字符串代理.
         *
         * @param other 被复制对象.
         * @return UString& 当前对象.
         */
        UString &operator=(const UString &other) {
            if (this != &other) {
                UString copied(other);
                *this = std::move(copied);
            }
            return *this;
        }

        /**
         * @brief 移动赋值字符串代理.
         *
         * @param other 被移动对象.
         * @return UString& 当前对象.
         */
        UString &operator=(UString &&other) noexcept {
            if (this != &other) {
                _ubuf     = std::move(other._ubuf);
                _len      = other._len;
                other._len = 0;
            }
            return *this;
        }

        /**
         * @brief 析构字符串代理.
         */
        ~UString() noexcept = default;

        /**
         * @brief 从用户空间重新同步字符串数据.
         *
         * @return Result<void> 成功返回空结果.
         */
        [[nodiscard]]
        Result<void> sync_from_user() noexcept {
            if (_ubuf.kbuf() == nullptr || maxlen() == 0) {
                _len = 0;
                void_return();
            }

            memset(kbuf(), 0, maxlen());
            size_t copied = 0;
            for (; copied + 1 < maxlen(); ++copied) {
                auto ch_res = read_user_char(copied);
                propagate(ch_res);
                kbuf()[copied] = ch_res.value();
                if (ch_res.value() == '\0') {
                    _len = static_cast<int>(copied);
                    void_return();
                }
            }

            auto last_ch_res = read_user_char(copied);
            propagate(last_ch_res);
            kbuf()[copied] = last_ch_res.value();
            if (last_ch_res.value() == '\0') {
                _len = static_cast<int>(copied);
                void_return();
            }
            kbuf()[maxlen() - 1] = '\0';
            _len = static_cast<int>(strnlen(kbuf(), maxlen()));
            void_return();
        }

        /**
         * @brief 返回内核字符串缓冲区.
         */
        [[nodiscard]] char *kbuf() const {
            return _ubuf.kbuf();
        }

        /**
         * @brief 返回当前字符串长度.
         */
        [[nodiscard]] size_t len() const {
            return _len;
        }

        /**
         * @brief 返回最大可读长度.
         */
        [[nodiscard]] size_t maxlen() const {
            return _ubuf.len();
        }

        /**
         * @brief 返回对应用户空间地址.
         */
        [[nodiscard]] VirAddr uaddr() const {
            return _ubuf.uaddr();
        }
    };
}  // namespace syscall
