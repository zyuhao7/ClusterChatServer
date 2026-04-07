#include "appconfig.hpp"
#include "password.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
void test_app_config_loads_values()
{
    const std::string path = "/tmp/cluster_chat_unit_test.conf";
    std::ofstream out(path.c_str());
    out << "# comment\n";
    out << "db.host = 10.0.0.8\n";
    out << "db.port = 3307\n";
    out << "db.user = chatuser\n";
    out << "db.password = chatpass\n";
    out << "db.name = chatdb\n";
    out << "redis.host = 10.0.0.9\n";
    out << "redis.port = 6380\n";
    out << "server.host = 0.0.0.0\n";
    out << "server.port = 9900\n";
    out << "server.thread_num = 8\n";
    out << "server.log_level = DEBUG\n";
    out << "security.bcrypt_cost = 12\n";
    out.close();

    AppConfig &cfg = AppConfig::instance();
    assert(cfg.loadFromFile(path));
    assert(cfg.db().host == "10.0.0.8");
    assert(cfg.db().port == 3307);
    assert(cfg.db().user == "chatuser");
    assert(cfg.db().password == "chatpass");
    assert(cfg.db().database == "chatdb");
    assert(cfg.redis().host == "10.0.0.9");
    assert(cfg.redis().port == 6380);
    assert(cfg.server().host == "0.0.0.0");
    assert(cfg.server().port == 9900);
    assert(cfg.server().thread_num == 8);
    assert(cfg.server().log_level == "DEBUG");
    assert(cfg.server().bcrypt_cost == 12);
}

void test_password_hash_and_verify()
{
    const std::string plain = "P@ssw0rd";
    const std::string hashed = PasswordSecurity::hashBcrypt(plain, 10);
    assert(!hashed.empty());
    assert(PasswordSecurity::isBcryptHash(hashed));
    assert(PasswordSecurity::verify(plain, hashed));
    assert(!PasswordSecurity::verify("wrong-password", hashed));
}

void test_password_legacy_plaintext_compat()
{
    assert(PasswordSecurity::verify("legacy", "legacy"));
    assert(!PasswordSecurity::verify("legacy", "legacy-other"));
    assert(!PasswordSecurity::isBcryptHash("legacy"));
}
} // namespace

int main()
{
    test_app_config_loads_values();
    test_password_hash_and_verify();
    test_password_legacy_plaintext_compat();
    std::cout << "cluster_chat_unit passed" << std::endl;
    return 0;
}
