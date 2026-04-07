#ifndef PASSWORD_SECURITY_H
#define PASSWORD_SECURITY_H

#include <string>

class PasswordSecurity
{
public:
    static std::string hashBcrypt(const std::string &plain, int cost);
    static bool verify(const std::string &plain, const std::string &stored);
    static bool isBcryptHash(const std::string &stored);
};

#endif
