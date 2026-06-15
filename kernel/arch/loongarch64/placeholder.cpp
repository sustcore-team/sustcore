#include <arch/loongarch64/placeholder.h>
#include <arch/loongarch64/trait.h>

void Loongarch64EarlySerial::serial_write_char(char ch) {
    (void)ch;
}

void Loongarch64EarlySerial::serial_write_string(size_t len, const char *str) {
    (void)len;
    (void)str;
}

void Loongarch64Initialization::pre_init(void) {}

void Loongarch64Initialization::post_init(void) {}

void Loongarch64Interrupt::init(void) {}

void Loongarch64Interrupt::sti(void) {}

void Loongarch64Interrupt::cli(void) {}

bool Loongarch64Interrupt::enabled() {
    return false;
}

void Loongarch64Idle::idle() {}

void Loongarch64PageMan::set_cow(PTE *, bool) {}

void Loongarch64PageMan::set_paddr(PTE *, PhyAddr) {}

PhyAddr Loongarch64PageMan::read_root() {
    return PhyAddr::null;
}

void Loongarch64PageMan::init() {}

void Loongarch64PageMan::make_root(PhyAddr root) {
    (void)root;
}

void Loongarch64PageMan::__switch_root(PhyAddr root) {
    (void)root;
}

void Loongarch64PageMan::flush_tlb() {}

Result<Loongarch64PageMan::QueryResult> Loongarch64PageMan::query_page(VirAddr) {
    unexpect_return(ErrCode::NOT_SUPPORTED);
}

Result<void> Loongarch64PageMan::clone_mapping_from(Loongarch64PageMan &,
                                                    VirAddr) noexcept {
    unexpect_return(ErrCode::NOT_SUPPORTED);
}

Result<void> Loongarch64PageMan::merge_from(Loongarch64PageMan &) noexcept {
    unexpect_return(ErrCode::NOT_SUPPORTED);
}
