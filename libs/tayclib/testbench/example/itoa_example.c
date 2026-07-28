/**
 * @file itoa_example.c
 * @brief Demonstrate integer conversion with tayclib.
 * @version 0.1.0-dev.1
 * @date 2026-07-28
 */

#include <stdio.h>

#include <tay/itoa.h>

int main(void) {
    char decimal[32];
    char hexadecimal[32];
    char binary[65];

    itoa_s(-2026, decimal, sizeof(decimal), 10);
    utoa_s(0xcafeu, hexadecimal, sizeof(hexadecimal), 16);
    ulltoa_s(42ull, binary, sizeof(binary), 2);

    printf("decimal: %s\n", decimal);
    printf("hexadecimal: %s\n", hexadecimal);
    printf("binary: %s\n", binary);
    return 0;
}
