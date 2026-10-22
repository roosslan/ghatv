#pragma once

// Минимальный набор для модульных тестов без внешних зависимостей

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace check
{

struct Test
{
    const char *name;
    std::function<void()> fn;
};

inline std::vector<Test> &registry()
{
    static std::vector<Test> tests;
    return tests;
}

inline int &failures()
{
    static int n = 0;
    return n;
}

struct Registrar
{
    Registrar(const char *name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

template <class A, class B>
void eq(const A &a, const B &b, const char *ea, const char *eb, const char *file, int line)
{
    if (a == b)
        return;
    std::ostringstream os;
    os << file << ":" << line << ": ожидалось " << ea << " == " << eb << "\n    слева:  " << a
       << "\n    справа: " << b;
    std::cerr << os.str() << "\n";
    ++failures();
}

} // namespace check

#define CHECK_CAT2(a, b) a##b
#define CHECK_CAT(a, b) CHECK_CAT2(a, b)

#define TEST(name)                                                                                   \
    static void CHECK_CAT(test_fn_, __LINE__)();                                                     \
    static check::Registrar CHECK_CAT(test_reg_, __LINE__)(name, CHECK_CAT(test_fn_, __LINE__));    \
    static void CHECK_CAT(test_fn_, __LINE__)()

#define CHECK(expr)                                                                                  \
    do                                                                                               \
    {                                                                                                \
        if (!(expr))                                                                                 \
        {                                                                                            \
            std::cerr << __FILE__ << ":" << __LINE__ << ": не выполнено " #expr "\n";               \
            ++check::failures();                                                                     \
        }                                                                                            \
    } while (0)

#define CHECK_EQ(a, b) check::eq((a), (b), #a, #b, __FILE__, __LINE__)
