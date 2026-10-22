#include "check.h"
#include "util.h"

#include <cstdlib>

TEST("parseIso: дата в UTC")
{
    CHECK_EQ(util::parseIso("1970-01-01T00:00:00Z"), (time_t) 0);
    CHECK_EQ(util::parseIso("2000-03-01T00:00:00Z"), (time_t) 951868800);
    CHECK_EQ(util::parseIso("2024-02-29T23:59:59Z"), (time_t) 1709251199);
    CHECK_EQ(util::parseIso("2026-10-13T10:00:00Z"), (time_t) 1791885600);
}

TEST("parseIso: пустая и некорректная строка")
{
    CHECK_EQ(util::parseIso(""), (time_t) 0);
    CHECK_EQ(util::parseIso("вчера"), (time_t) 0);
}

TEST("duration: минуты и часы")
{
    CHECK_EQ(util::duration(0), std::string("0:00"));
    CHECK_EQ(util::duration(42), std::string("0:42"));
    CHECK_EQ(util::duration(605), std::string("10:05"));
    CHECK_EQ(util::duration(3723), std::string("1:02:03"));
    CHECK_EQ(util::duration(-1), std::string(""));
}

TEST("ago: единицы измерения")
{
    time_t now = 1791885600;
    CHECK_EQ(util::ago(now - 5, now), std::string("5 с"));
    CHECK_EQ(util::ago(now - 180, now), std::string("3 мин"));
    CHECK_EQ(util::ago(now - 7200, now), std::string("2 ч"));
    CHECK_EQ(util::ago(now - 4 * 86400, now), std::string("4 дн"));
    CHECK_EQ(util::ago(now + 30, now), std::string("0 с"));
    CHECK_EQ(util::ago(0, now), std::string(""));
}

TEST("localTime: дата и время")
{
    CHECK_EQ(util::localTime(1791885600), std::string("13.10.26 10:00"));
    CHECK_EQ(util::localTime(0), std::string(""));
}

TEST("splitList: запятые, пробелы, точки с запятой")
{
    auto v = util::splitList("a, b;c  d,,");
    CHECK_EQ(v.size(), (size_t) 4);
    if (v.size() == 4)
    {
        CHECK_EQ(v[0], std::string("a"));
        CHECK_EQ(v[3], std::string("d"));
    }
    CHECK(util::splitList("").empty());
}

TEST("envBool и envInt")
{
    setenv("GHATV_TEST_FLAG", "Yes", 1);
    CHECK(util::envBool("GHATV_TEST_FLAG", false));
    setenv("GHATV_TEST_FLAG", "off", 1);
    CHECK(!util::envBool("GHATV_TEST_FLAG", true));
    unsetenv("GHATV_TEST_FLAG");
    CHECK(util::envBool("GHATV_TEST_FLAG", true));
    setenv("GHATV_TEST_NUM", "42", 1);
    CHECK_EQ(util::envInt("GHATV_TEST_NUM", 0), 42);
    CHECK_EQ(util::envInt("GHATV_TEST_MISSING", 7), 7);
}

TEST("lower и containsNoCase")
{
    CHECK_EQ(util::lower("AbC"), std::string("abc"));
    CHECK(util::containsNoCase("Build And Release", "release"));
    CHECK(!util::containsNoCase("Build", "test"));
}
