#pragma once
#include <string>
#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#pragma comment(lib, "ole32.lib")
#else
#include <fstream>
#include <cstdint>
#endif

namespace utils {

    inline std::string generateGuid()
    {

#ifdef _WIN32
        GUID guid;
        CoCreateGuid(&guid);
        char buf[37];
        snprintf(buf, sizeof(buf),
            "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            guid.Data1, guid.Data2, guid.Data3,
            guid.Data4[0], guid.Data4[1],
            guid.Data4[2], guid.Data4[3], guid.Data4[4],
            guid.Data4[5], guid.Data4[6], guid.Data4[7]);
        return buf;
#else
        // Read 16 random bytes from the OS
        std::ifstream rng("/dev/urandom", std::ios::binary);
        uint8_t bytes[16]{};
        rng.read(reinterpret_cast<char*>(bytes), sizeof(bytes));

        // Set version 4 and variant bits
        bytes[6] = (bytes[6] & 0x0f) | 0x40;
        bytes[8] = (bytes[8] & 0x3f) | 0x80;

        char buf[37];
        snprintf(buf, sizeof(buf),
            "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            bytes[0], bytes[1], bytes[2], bytes[3],
            bytes[4], bytes[5], bytes[6], bytes[7],
            bytes[8], bytes[9], bytes[10], bytes[11],
            bytes[12], bytes[13], bytes[14], bytes[15]);
        return buf;
#endif


    }

} // namespace utils