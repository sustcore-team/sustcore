#include <tay/owner.h>

#include <cassert>

int main() {
    int value = 9;
    tay::owner owned{&value};
    assert(owned && *owned == 9 && owned.get() == &value);

    return 0;
}
