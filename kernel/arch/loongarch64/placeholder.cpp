#include <arch/loongarch64/placeholder.h>
#include <arch/loongarch64/trait.h>

using namespace la64;

void EarlySerial::serial_write_char(char ch) {
    (void)ch;
}

void EarlySerial::serial_write_string(size_t len, const char *str) {
    (void)len;
    (void)str;
}

void Initialization::pre_init(void) {}

void Initialization::post_init(void) {}

void Interrupt::init(void) {}

void Interrupt::sti(void) {}

void Interrupt::cli(void) {}

bool Interrupt::enabled() {
    return false;
}

void Idle::idle() {}

void PageMan::set_cow(PTE *, bool) {}

void PageMan::set_paddr(PTE *, PhyAddr) {}

PhyAddr PageMan::read_root() {
    return PhyAddr::null;
}

void PageMan::init() {}

void PageMan::make_root(PhyAddr root) {
    (void)root;
}

void PageMan::__switch_root(PhyAddr root) {
    (void)root;
}

void PageMan::flush_tlb() {}

Result<PageMan::QueryResult> PageMan::query_page(VirAddr) {
    unexpect_return(ErrCode::NOT_SUPPORTED);
}

Result<void> PageMan::clone_mapping_from(PageMan &,
                                               VirAddr) noexcept {
    unexpect_return(ErrCode::NOT_SUPPORTED);
}

Result<void> PageMan::merge_from(PageMan &) noexcept {
    unexpect_return(ErrCode::NOT_SUPPORTED);
}
