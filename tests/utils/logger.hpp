/**
 * @file logger.hpp
 * @author Jakub Brzezowski
 * @copyright Copyright (c) 2022
 *
 * @brief Implementation of additional logger for unit tests.
 * @version 1.0
 * @date 2022-07-17
 *
 */

 #pragma once

#include <iostream>
#include <sstream>

/**
 * @class Logger
 * @brief Additional logger for testing framework.
 */
class Logger {
private:
    static constexpr const char *RESET = "\033[0m";
    static constexpr const char *RED = "\033[31m";
    static constexpr const char *GREEN = "\033[32m";
    static constexpr const char *YELLOW = "\033[33m";
    static constexpr const char *BLUE = "\033[34m";
    static constexpr const char *MAGENTA = "\033[35m";
public:
    /**
     * @brief Construct a new LOGOUT object.
     */
    Logger()
    {
    }

    /**
     * @brief Convert argument to string.
     *
     * @tparam T Type of argument.
     * @param[in] value Argument to convert.
     *
     * @return std::string String representation of argument.
     */
    template<typename T>
    static std::string to_stirng(const T &value)
    {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    }

    /**
     * @brief Print message using standard output.
     *
     * @tparam First Type of first argument.
     * @tparam Args Type of rest arguments.
     * @param[in] first First argument.
     * @param[in] args Rest arguments.
     *
     * @return std::string Formatted message.
     */
    template<typename First, typename ... Args>
    static std::string format(const First &first, const Args &... args)
    {
        std::ostringstream oss;
        oss << first;
        return oss.str() + " " + format(args ...);
    }

    /**
     * @brief Print message using standard output.
     *
     * @return std::string Empty string.
     */
    static std::string format()
    {
        return "";
    }

    /**
     * @brief Print message using standard output.
     *
     * @tparam Args Type of arguments.
     * @param[in] args Arguments.
     */
    template<typename ... Args>
    static void info(Args... args)
    {
        std::cout << GREEN << "[INFO] " << format(args ...) <<  RESET << std::endl;
    }

    /**
     * @brief Print warning message using standard output.
     *
     * @tparam Args Type of arguments.
     * @param[in] args Arguments.
     */
    template<typename ... Args>
    static void warn(Args... args)
    {
        std::cout << YELLOW << "[WARN] " << format(args ...) <<  RESET << std::endl;
    }

    /**
     * @brief Print error message using standard output.
     *
     * @tparam Args Type of arguments.
     * @param[in] args Arguments.
     */
    template<typename ... Args>
    static void error(Args... args)
    {
        std::cout << RED << "[ERR] " << format(args ...) <<  RESET << std::endl;
    }

    /**
     * @brief Print debug message using standard output.
     *
     * @tparam Args Type of arguments.
     * @param[in] args Arguments.
     */
    template<typename ... Args>
    static void debug(Args... args)
    {
        std::cout << BLUE << "[DEBUG] " << format(args ...) <<  RESET << std::endl;
    }

    /**
     * @brief Print fail message using standard output.
     * @warning Terminate the program with error status.
     *
     * @tparam Args Type of arguments.
     * @param[in] args Arguments.
     */
    template<typename ... Args>
    static void fail(Args... args)
    {
        std::cout << MAGENTA << "[FAIL] " << format(args ...) <<  RESET << std::endl;
        exit(EXIT_FAILURE); // Terminate the program with error status
    }

    /**
     * @brief Print warning message using standard output.
     */
    static std::ostream&  warn()
    {
        std::cout << "[WARN] ";
        return std::cout;
    }

    /**
     * @brief Print error message using standard output.
     */
    static std::ostream&  err()
    {
        std::cout << "[ERR] ";
        return std::cout;
    }
};
