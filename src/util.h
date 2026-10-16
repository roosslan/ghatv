#pragma once

#include <ctime>
#include <string>
#include <vector>

namespace util
{

// ISO 8601 (2026-10-13T10:00:00Z) -> time_t, 0 для пустой строки
time_t parseIso(const std::string &s);
// "5 с", "3 мин", "2 ч", "4 дн"
std::string ago(time_t t, time_t now);
// "0:42", "12:05", "1:02:03"
std::string duration(long secs);
// "13.10 10:32"
std::string localTime(time_t t);
std::string lower(std::string s);
bool containsNoCase(const std::string &haystack, const std::string &needleLower);
std::string env(const char *name, const std::string &def = "");
int envInt(const char *name, int def);
bool envBool(const char *name, bool def);
std::vector<std::string> splitList(const std::string &s);

} // namespace util
