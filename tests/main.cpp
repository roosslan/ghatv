#include "check.h"

#include <cstdlib>

int main()
{
    // Время в тестах сравнивается в UTC
    setenv("TZ", "UTC", 1);
    int failedTests = 0;
    for (auto &t : check::registry())
    {
        int before = check::failures();
        try
        {
            t.fn();
        }
        catch (const std::exception &e)
        {
            std::cerr << "исключение: " << e.what() << "\n";
            ++check::failures();
        }
        bool ok = check::failures() == before;
        failedTests += !ok;
        std::cout << (ok ? "[ OK ] " : "[FAIL] ") << t.name << "\n";
    }
    std::cout << "\nТестов: " << check::registry().size() << ", не пройдено: " << failedTests << "\n";
    return failedTests ? 1 : 0;
}
