#include "password.hpp"

#include <crypt.h>
#include <cstring>
#include <random>
#include <vector>

std::string PasswordSecurity::hashBcrypt(const std::string &plain, int cost)
{
    if (plain.empty())
    {
        return "";
    }

    if (cost < 4)
    {
        cost = 4;
    }
    if (cost > 16)
    {
        cost = 16;
    }

    std::vector<char> random_bytes(16);
    std::random_device rd;
    for (size_t i = 0; i < random_bytes.size(); ++i)
    {
        random_bytes[i] = static_cast<char>(rd());
    }

    char *setting = crypt_gensalt_ra("$2b$", static_cast<unsigned long>(cost),
                                     reinterpret_cast<const char *>(random_bytes.data()),
                                     static_cast<int>(random_bytes.size()));
    if (setting == nullptr)
    {
        return "";
    }

    void *data = nullptr;
    int size = 0;
    char *hashed = crypt_ra(plain.c_str(), setting, &data, &size);
    std::string result = hashed != nullptr ? hashed : "";

    free(setting);
    if (data != nullptr)
    {
        std::memset(data, 0, static_cast<size_t>(size));
        free(data);
    }
    return result;
}

bool PasswordSecurity::verify(const std::string &plain, const std::string &stored)
{
    if (plain.empty() || stored.empty())
    {
        return false;
    }

    if (!isBcryptHash(stored))
    {
        // Backward compatibility for legacy plaintext records.
        return plain == stored;
    }

    void *data = nullptr;
    int size = 0;
    char *hashed = crypt_ra(plain.c_str(), stored.c_str(), &data, &size);
    bool ok = hashed != nullptr && stored == hashed;
    if (data != nullptr)
    {
        std::memset(data, 0, static_cast<size_t>(size));
        free(data);
    }
    return ok;
}

bool PasswordSecurity::isBcryptHash(const std::string &stored)
{
    return stored.rfind("$2a$", 0) == 0 ||
           stored.rfind("$2b$", 0) == 0 ||
           stored.rfind("$2y$", 0) == 0;
}
