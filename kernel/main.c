/**
 * @file main.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核Main函数
 * @version alpha-1.0.0
 * @date 2025-11-17
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <basec/logger.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <sbi/sbi.h>

int kputchar(int ch) {
    sbi_dbcn_console_write_byte((char)ch);
    return ch;
}

int kputs(const char *str) {
    int len = strlen(str);
    sbi_dbcn_console_write((umb_t)len, (const void*)str);
    return len;
}

/**
 * @brief 内核主函数
 * 
 * @return int 
 */
int main(void) {
    log_info("Hello RISCV World!");
    return 0;
}

/**
 * @brief 初始化
 * 
 */
void init(void) {
    kputs("\n");
    init_logger(kputs, "SUSTCore");
}

/**
 * @brief 收尾工作
 * 
 */
void terminate(void) {
    SBIRet ret;
    ret = sbi_shutdown();
    if (ret.error) {
        log_error("关机失败!");
    } 
    
    log_error("何意味?关机成功了?!你又是怎么溜达到这来的?!!");
    while(true);
}

/**
 * @brief 内核启动函数
 *
 */
void c_setup(void) {
    init();

    main();

    terminate();
}