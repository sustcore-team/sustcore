/**
 * @file syscall.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 系统调用
 * @version alpha-1.0.0
 * @date 2026-05-04
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <kio.h>
#include <env.h>
#include <mem/kaddr.h>
#include <schd/schdbase.h>
#include <sus/owner.h>
#include <sustcore/addr.h>
#include <sustcore/errcode.h>
#include <sustcore/syscall.h>
#include <syscall/syscall.h>

namespace syscall {
    // 将用户空间中的数据读取到内核空间中
    class UBuffer {
    private:
        VirAddr _uaddr;             // 用户空间地址
        util::owner<char *> _kbuf;  // 内核空间缓冲区地址
        size_t _len;                // 数据长度
    public:
        UBuffer(VirAddr uaddr, size_t len) : _uaddr(uaddr), _len(len) {
            // 分配内核空间缓冲区
            _kbuf = util::owner(new char[_len]);
        }

        ~UBuffer() {
            loggers::SYSCALL::DEBUG("释放内核缓冲区: %p", _kbuf.get());
            delete[] _kbuf;
        }

        UBuffer &sync_from_user() {
            // 从用户空间复制数据到内核空间
            // 打开guard
            ker_paddr::SumGuard guard;
            guard.open();
            memcpy(_kbuf, _uaddr.addr(), _len);
            return *this;
        }

        UBuffer &sync_to_user() {
            // 从内核空间复制数据到用户空间
            ker_paddr::SumGuard guard;
            guard.open();
            memcpy(_uaddr.addr(), _kbuf, _len);
            return *this;
        }

        [[nodiscard]] char *kbuf() const {
            return _kbuf;
        }

        [[nodiscard]] size_t len() const {
            return _len;
        }

        [[nodiscard]] VirAddr uaddr() const {
            return _uaddr;
        }
    };

    // 用户空间字符串(readonly)
    class UString {
    private:
        UBuffer _ubuf;
        int _len;
    public:
        UString(VirAddr uaddr, size_t maxlen) : _ubuf(uaddr, maxlen) {
            sync_from_user();
        }

        ~UString() {
        }

        UString &sync_from_user() {
            _ubuf.sync_from_user();
            _len = strnlen(kbuf(), maxlen());
            return *this;
        }

        [[nodiscard]] char *kbuf() const {
            return _ubuf.kbuf();
        }

        [[nodiscard]] size_t len() const {
            return _len;
        }

        [[nodiscard]] size_t maxlen() const {
            return _ubuf.len();
        }

        [[nodiscard]] VirAddr uaddr() const {
            return _ubuf.uaddr();
        }
    };

    void write_serial(const UString &str, size_t len) {
        kwrites(str.kbuf(), len);
    }

    constexpr size_t MAX_SYSCALL_PATH = 256;

    void create_process(const UString &path) {
        auto load_res =
            env::inst().tm()->load_elf(path.kbuf(), schd::ClassType::FCFS);
        if (!load_res.has_value()) {
            loggers::SYSCALL::ERROR("创建进程失败: path=%s, 错误码: %s",
                                    path.kbuf(), to_cstring(load_res.error()));
            return;
        }

        loggers::SYSCALL::INFO("创建进程成功: path=%s, pid=%d", path.kbuf(),
                               load_res.value()->pid);
    }

    RetPack entrance(const ArgPack &args) {
        b64 arg0 = args.args[0];
        b64 arg1 = args.args[1];
        b64 arg2 = args.args[2];
        b64 arg3 = args.args[3];
        b64 arg4 = args.args[4];

        b64 sysno  = args.syscall_number;
        b64 capidx = args.capidx;

        b64 ret0 = 0, ret1 = 0;

        bool processed = true;

        // 根据syscall number分发
        switch (sysno) {
            case SYS_WRITE_SERIAL: {
                write_serial(UString((VirAddr)arg0, arg1), arg1);
                ret0 = ret1 = 0;
                break;
            }
            case SYS_CREATE_PROCESS: {
                create_process(UString((VirAddr)arg0, MAX_SYSCALL_PATH));
                ret0 = ret1 = 0;
                break;
            }
            default: {
                processed = false;
                loggers::SYSCALL::ERROR("未知的系统调用号: %d", sysno);
                ret0 = ret1 = 0;
                break;
            }
        }

        return RetPack{processed, ret0, ret1};
    };
}  // namespace syscall
