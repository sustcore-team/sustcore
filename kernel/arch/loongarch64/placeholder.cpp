#include <arch/loongarch64/placeholder.h>
#include <arch/loongarch64/trait.h>
#include <logger.h>
#include <sus/logger.h>

using namespace la64;

void Initialization::pre_init(void) {}

void Initialization::post_init(void) {}

Result<void> Initialization::init_clock() {
    loggers::SUSTCORE::ERROR("LoongArch64 时钟初始化尚未实现");
    unexpect_return(ErrCode::NOT_SUPPORTED);
}

void Idle::idle() {}
