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

int main(void)
{
    dynostatic_buffer_t ds_buffer = { 0 };
    ds_initialize_allocation(&ds_buffer);
    printf("TODO: end this example.");
}
