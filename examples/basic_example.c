/**
 * @file basic_example.c
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2022
 *
 * @brief Basic example for dynostatic-buffer.
 * @version 0.1
 * @date 2022-06-16
 *
 */

#include <stdio.h>

#include "dynostatic-buffer.h"

void example_logger(const char *message, size_t len)
{
    if (len == 0) {
        return;
    }
    printf("%s", message);
}

int main(void)
{
    ds_initialize_allocation(example_logger);
    printf("TODO: end this example.");
}
