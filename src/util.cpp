#include "util.h"
#include "i18n.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

Lang gLang = Lang::En;

std::string trPair(const std::string &s)
{
    auto bar = s.find('|');
    if (bar == std::string::npos)
        return s;
    return gLang == Lang::Ru ? s.substr(bar + 1) : s.substr(0, bar);
}

namespace util
{

time_t parseIso(const std::string &s)
{
    if (s.empty())
        return 0;
    struct tm tm {};
    if (sscanf(s.c_str(), "%d-%d-%dT%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6)
        return 0;
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    return timegm(&tm);
}

std::string ago(time_t t, time_t now)
{
    if (t == 0)
        return "";
    long d = (long) (now - t);
    if (d < 0)
        d = 0;
    char buf[32];
    if (d < 60)
        snprintf(buf, sizeof(buf), L("%lds", "%ld с"), d);
    else if (d < 3600)
        snprintf(buf, sizeof(buf), L("%ld min", "%ld мин"), d / 60);
    else if (d < 86400)
        snprintf(buf, sizeof(buf), L("%ld h", "%ld ч"), d / 3600);
    else
        snprintf(buf, sizeof(buf), L("%ld d", "%ld дн"), d / 86400);
    return buf;
}

std::string duration(long secs)
{
    if (secs < 0)
        return "";
    char buf[32];
    if (secs >= 3600)
        snprintf(buf, sizeof(buf), "%ld:%02ld:%02ld", secs / 3600, secs / 60 % 60, secs % 60);
    else
        snprintf(buf, sizeof(buf), "%ld:%02ld", secs / 60, secs % 60);
    return buf;
}

std::string localTime(time_t t)
{
    if (t == 0)
        return "";
    struct tm tm {};
    localtime_r(&t, &tm);
    char buf[32];
    strftime(buf, sizeof(buf), L("%y-%m-%d %H:%M", "%d.%m.%y %H:%M"), &tm);
    return buf;
}

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char) std::tolower(c); });
    return s;
}

bool containsNoCase(const std::string &haystack, const std::string &needleLower)
{
    return lower(haystack).find(needleLower) != std::string::npos;
}

std::string env(const char *name, const std::string &def)
{
    const char *v = getenv(name);
    return (v && *v) ? std::string(v) : def;
}

int envInt(const char *name, int def)
{
    std::string v = env(name);
    if (v.empty())
        return def;
    return atoi(v.c_str());
}

bool envBool(const char *name, bool def)
{
    std::string v = lower(env(name));
    if (v.empty())
        return def;
    return v == "1" || v == "true" || v == "yes" || v == "on";
}

std::vector<std::string> splitList(const std::string &s)
{
    std::vector<std::string> out;
    std::string cur;
    for (char c : s + ",")
    {
        if (c == ',' || c == ' ' || c == ';')
        {
            if (!cur.empty())
                out.push_back(cur);
            cur.clear();
        }
        else
            cur += c;
    }
    return out;
}

} // namespace util
