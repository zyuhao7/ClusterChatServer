#include "appconfig.hpp"
#include "db_fixture.hpp"
#include "messagehistorymodal.hpp"
#include "password.hpp"
#include "usermodal.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
DeterministicDbFixture &dbFixture()
{
    static DeterministicDbFixture fixture;
    return fixture;
}

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
    out << "server.offline_message_limit = 88\n";
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
    assert(cfg.server().offline_message_limit == 88);
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

void test_user_modal_uses_isolated_schema()
{
    dbFixture().resetTables();

    UserModal modal;
    User user;
    user.SetName("fixture-user");
    user.SetPwd("secret");
    user.SetState("online");

    assert(modal.Insert(user));
    assert(user.GetId() == 1);

    User loaded = modal.query(user.GetId());
    assert(loaded.GetName() == "fixture-user");
    assert(loaded.GetPwd() == "secret");
    assert(loaded.GetState() == "online");
}

void test_fixture_reset_leaves_zero_residual_rows()
{
    dbFixture().resetTables();

    UserModal modal;
    User alice;
    alice.SetName("alice");
    alice.SetPwd("pw-a");
    assert(modal.Insert(alice));

    User bob;
    bob.SetName("bob");
    bob.SetPwd("pw-b");
    assert(modal.Insert(bob));

    MessageHistoryModal history;
    assert(history.insertDirect("req-1", alice.GetId(), bob.GetId(), "hello") > 0);
    assert(dbFixture().rowCount("user") == 2);
    assert(dbFixture().rowCount("message_history") == 1);

    dbFixture().resetTables();

    assert(dbFixture().rowCount("message_history") == 0);
    assert(dbFixture().rowCount("friend") == 0);
    assert(dbFixture().rowCount("user") == 0);
}
} // namespace

int main()
{
    test_app_config_loads_values();
    test_password_hash_and_verify();
    test_password_legacy_plaintext_compat();
    dbFixture().bootstrapSuite();
    test_user_modal_uses_isolated_schema();
    test_fixture_reset_leaves_zero_residual_rows();
    std::cout << "cluster_chat_unit passed" << std::endl;
    return 0;
}
