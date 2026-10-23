#pragma once

#include <string>

// Язык интерфейса; по умолчанию английский
enum class Lang { En, Ru };
extern Lang gLang;

// Строка для текущего языка
inline const char *L(const char *en, const char *ru)
{
    return gLang == Lang::Ru ? ru : en;
}

// "English|Русский" -> часть для текущего языка; строка без «|» возвращается целиком.
// Используется там, где текст хранится и должен меняться при переключении языка (заголовки).
std::string trPair(const std::string &s);
