#ifndef TESTS_CPP_DB_FIXTURE_HPP
#define TESTS_CPP_DB_FIXTURE_HPP

#include <string>

class DeterministicDbFixture
{
public:
    DeterministicDbFixture();

    void bootstrapSuite();
    void resetTables();
    long long rowCount(const std::string &table_name);

private:
    struct FixtureConfig
    {
        std::string host;
        unsigned int port;
        std::string user;
        std::string password;
        std::string schema;
        std::string charset;
        std::string schema_template_path;
        std::string runtime_config_path;
    };

    FixtureConfig _cfg;
};

#endif
