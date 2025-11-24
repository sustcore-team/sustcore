extern int kmod_main(void);

void _start(void) {
    int ret = kmod_main();
    if (ret != 0) {
        // 错误退出
        // exit(ret);
    }
    // 正常退出
    // exit (0);
    // unreachable
    while (true);
}