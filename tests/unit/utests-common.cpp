/**
 * @file utests-common.cpp
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2022
 *
 * @brief Function using for all unit tests files.
 * @version 0.1
 * @date 2022-07-17
 *
 */

#include <sstream>
#include <iostream>
#include <iomanip>

void utests_stdout_logger(const char *p_message, size_t len)
{
    if (len == 0) {
        return;
    }

    std::cout << p_message << std::setw(len);
}
