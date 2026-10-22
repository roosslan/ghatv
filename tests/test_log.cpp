#include "check.h"
#include "log_window.h"

TEST("parseLog: отметки времени, группы, ошибки, ANSI")
{
    std::string raw =
        "\xEF\xBB\xBF"
        "2026-10-13T10:00:00.1234567Z ##[group]Run cmake --build build\r\n"
        "2026-10-13T10:00:01.0000000Z [command]/usr/bin/cmake --build build\n"
        "2026-10-13T10:00:02.0000000Z \x1b[31mred text\x1b[0m\n"
        "2026-10-13T10:00:03.0000000Z ##[endgroup]\n"
        "2026-10-13T10:00:04.0000000Z ##[error]Process completed with exit code 1.\n"
        "2026-10-13T10:00:05.0000000Z ##[warning]Node.js 20 is deprecated\n"
        "Error: something failed\n"
        "\ttab\n"
        "\n\n";
    int maxW = 0;
    auto lines = parseLog(raw, maxW);
    CHECK_EQ(lines.size(), (size_t) 7);  // endgroup и хвостовые пустые строки удаляются
    if (lines.size() != 7)
        return;
    CHECK(lines[0].group);
    CHECK_EQ(lines[0].text, std::string("▸ Run cmake --build build"));
    CHECK_EQ(lines[1].text, std::string("$ /usr/bin/cmake --build build"));
    CHECK_EQ(lines[2].text, std::string("red text"));
    CHECK(lines[3].error);
    CHECK_EQ(lines[3].text, std::string("✘ Process completed with exit code 1."));
    CHECK(!lines[4].error);
    CHECK(lines[5].error);  // «Error:» без маркера тоже считается ошибкой
    CHECK_EQ(lines[6].text, std::string("        tab"));
    CHECK(maxW >= 37);
}

TEST("wrapText: перенос по словам с сохранением отступа")
{
    auto v = wrapText("  one two three four five six", 12);
    CHECK_EQ(v.size(), (size_t) 3);
    if (v.size() == 3)
    {
        CHECK_EQ(v[0], std::string("  one two"));
        CHECK_EQ(v[1], std::string("  three four"));
        CHECK_EQ(v[2], std::string("  five six"));
    }
}

TEST("wrapText: кириллица считается по ширине, а не по байтам")
{
    auto v = wrapText("слово слово слово", 11);
    CHECK_EQ(v.size(), (size_t) 2);
    if (v.size() == 2)
        CHECK_EQ(v[0], std::string("слово слово"));
}

TEST("wrapText: пустая строка и длинное слово")
{
    auto e = wrapText("", 20);
    CHECK_EQ(e.size(), (size_t) 1);
    auto l = wrapText("https://example.com/very/long/url/that/does/not/fit", 20);
    CHECK_EQ(l.size(), (size_t) 1);  // слово не режется, остаётся горизонтальная прокрутка
}
