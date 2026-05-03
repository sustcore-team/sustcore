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
#include <mem/kaddr.h>
#include <sus/owner.h>
#include <sustcore/addr.h>
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

    void write_serial(const UBuffer &str, size_t len) {
        kwrites(str.kbuf(), len);
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
                UBuffer buf((VirAddr)arg0, (size_t)arg1);
                buf.sync_from_user();
                write_serial(buf, arg1);
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