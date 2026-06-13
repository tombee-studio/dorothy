/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#pragma once

#include <iostream>
#include <vector>
#include <string>

template <typename... Args>
std::string format(const std::string& fmt, Args... args) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
    size_t len = std::snprintf(nullptr, 0, fmt.c_str(), args...);
    std::vector<char> buf(len + 1);
    std::snprintf(&buf[0], len + 1, fmt.c_str(), args...);
#pragma GCC diagnostic pop
    return std::string(&buf[0], &buf[0] + len);
}
