/**
 * @file gtest_out.hpp
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2022
 *
 * @brief Implementation of additional logger for unit tests.
 * @version 0.1
 * @date 2022-07-17
 *
 */

#pragma once

#include "gtest/gtest.h"

/**
 * @class LOGOUT
 * @brief Declare standard output for unit test with gtest framework.
 */
static class LOGOUT {
public:
    /**
     * @brief Construct a new LOGOUT object.
     */
    LOGOUT()
    {
    }

    /**
     * @brief Print info message using standard output.
     */
    std::ostream&  info()
    {
        std::cout << "[INFO] ";
        return std::cout;
    }

    /**
     * @brief Print warning message using standard output.
     */
    std::ostream&  warn()
    {
        std::cout << "[WARN] ";
        return std::cout;
    }

    /**
     * @brief Print error message using standard output.
     */
    std::ostream&  err()
    {
        std::cout << "[ERR] ";
        return std::cout;
    }
} GOUT;
