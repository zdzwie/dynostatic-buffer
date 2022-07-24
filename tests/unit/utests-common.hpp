/**
 * @file utests-common.hpp
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2022
 *
 * @brief Function using for all unit tests files.
 * @version 0.1
 * @date 2022-07-17
 *
 */

#pragma once

constexpr uint8_t max_memory_usage_dev = 2; /**< Maximal deviation between calculated and read memory usage. */

/**
 * @brief Simple test logger overriding standard output logger function.
 *
 * @param[in] p_message Message to print in standard output.
 * @param[in] len Length of message to print.
 */
void utests_stdout_logger(const char *p_message, size_t len);
