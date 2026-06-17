/**
 * @file exception.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch64 trap 入口处理
 * @version alpha-1.0.0
 * @date 2026-06-18
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/loongarch64/csrnum.h>
#include <arch/loongarch64/trait.h>
#include <logger.h>
#include <sus/logger.h>

using namespace la64;

namespace {
    extern "C" void isr_entry();

    #define _STR(x) #x
    #define STR(x)  _STR(x)

    inline umb_t csr_read_estat() {
        umb_t value = 0;
        asm volatile("csrrd %0, " STR(CSR_ESTAT) : "=r"(value));
        return value;
    }

    inline umb_t csr_read_crmd() {
        umb_t value = 0;
        asm volatile("csrrd %0, " STR(CSR_CRMD) : "=r"(value));
        return value;
    }

    inline void csr_write_crmd(umb_t value) {
        asm volatile("csrwr %0, " STR(CSR_CRMD) ::"r"(value));
    }
}  // namespace

extern "C" void handle_trap(umb_t era, umb_t estat, Context *ctx) {
    loggers::INTERRUPT::INFO("LoongArch64 trap: era=0x%lx estat=0x%lx ctx=%p",
                             era, estat, ctx);
    if (ctx == nullptr) {
        loggers::INTERRUPT::INFO("ctx: null");
        return;
    }

    loggers::INTERRUPT::INFO("ctx: sp=0x%lx ra=0x%lx tp=0x%lx fp=0x%lx",
                             ctx->sp(), ctx->ra, ctx->tp, ctx->fp);
    loggers::INTERRUPT::INFO(
        "args: a0=0x%lx a1=0x%lx a2=0x%lx a3=0x%lx",
        ctx->a0, ctx->a1, ctx->a2, ctx->a3);
    loggers::INTERRUPT::INFO(
        "args: a4=0x%lx a5=0x%lx a6=0x%lx a7=0x%lx",
        ctx->a4, ctx->a5, ctx->a6, ctx->a7);
    loggers::INTERRUPT::INFO(
        "temp: t0=0x%lx t1=0x%lx t2=0x%lx t3=0x%lx t4=0x%lx",
        ctx->t0, ctx->t1, ctx->t2, ctx->t3, ctx->t4);
    loggers::INTERRUPT::INFO("temp: t5=0x%lx t6=0x%lx t7=0x%lx t8=0x%lx",
                             ctx->t5, ctx->t6, ctx->t7, ctx->t8);
    loggers::INTERRUPT::INFO("saved: u0=0x%lx s0=0x%lx s1=0x%lx s2=0x%lx",
                             ctx->u0, ctx->s0, ctx->s1, ctx->s2);
    loggers::INTERRUPT::INFO(
        "saved: s3=0x%lx s4=0x%lx s5=0x%lx s6=0x%lx s7=0x%lx s8=0x%lx",
        ctx->s3, ctx->s4, ctx->s5, ctx->s6, ctx->s7, ctx->s8);

    while (true);
}

void Interrupt::init(void) {
    auto isr_addr = reinterpret_cast<umb_t>(&isr_entry);
    asm volatile("csrwr %0, " STR(CSR_EENTRY) ::"r"(isr_addr));
    loggers::INTERRUPT::INFO("LoongArch64 isr_entry 已设置: 0x%lx", isr_addr);
}

void Interrupt::sti(void) {
    auto crmd = csr_read_crmd();
    crmd |= CRMD_IE;
    csr_write_crmd(crmd);
}

void Interrupt::cli(void) {
    auto crmd = csr_read_crmd();
    crmd &= ~CRMD_IE;
    csr_write_crmd(crmd);
}

bool Interrupt::enabled() {
    return (csr_read_crmd() & CRMD_IE) != 0;
}
