#include <tay/lock.h>

int main() {
#if defined(TAY_TEST_UNIQUE_LOCK_NO_MUTEX)
    tay::unique_lock<tay::spinlock> lock;
    lock.lock();
#elif defined(TAY_TEST_UNIQUE_LOCK_ALREADY_OWNS)
    tay::spinlock mutex;
    tay::unique_lock lock{mutex};
    lock.lock();
#elif defined(TAY_TEST_UNIQUE_LOCK_NOT_OWNED)
    tay::spinlock mutex;
    tay::unique_lock lock{mutex, tay::defer_lock};
    lock.unlock();
#else
#error "unique_lock panic scenario is not selected"
#endif
}
