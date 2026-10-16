#pragma once

#define Uses_TApplication
#define Uses_TProgram
#define Uses_TWindow
#define Uses_TView
#define Uses_TGroup
#define Uses_TListViewer
#define Uses_TScroller
#define Uses_TScrollBar
#define Uses_TDrawBuffer
#define Uses_TEvent
#define Uses_TEventQueue
#define Uses_TKeys
#define Uses_TRect
#define Uses_TMenuBar
#define Uses_TSubMenu
#define Uses_TMenuItem
#define Uses_TStatusLine
#define Uses_TStatusItem
#define Uses_TStatusDef
#define Uses_TDeskTop
#define Uses_MsgBox
#define Uses_TText
#define Uses_TDialog
#define Uses_TStaticText
#define Uses_TButton
#include <tvision/tv.h>

#include "model.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class GitHub;
class Poller;

enum : ushort
{
    cmOpenRun = 1000,
    cmShowLog,
    cmRerun,
    cmRerunFailed,
    cmCancelRun,
    cmDeleteRun,
    cmDeleteLogs,
    cmRefresh,
    cmFilterAll,
    cmFilterActive,
    cmFilterFailed,
    cmFilterSuccess,
    cmFilterOther,
    cmSearch,
    cmLatestOnly,
    cmSortToggle,
    cmMarkAll,
    cmUnmarkAll,
    cmKeysHelp,
    cmAbout,
    cmTick,  // широковещательно раз в секунду
};

// Цвета (BIOS: старшая тетрада — фон, младшая — текст)
namespace clr
{
constexpr uchar Normal = 0x1B;     // светло-голубой на синем
constexpr uchar Header = 0x1E;     // жёлтый на синем
constexpr uchar Separator = 0x13;  // бирюзовый на синем
constexpr uchar Focus = 0x30;      // чёрный на бирюзовом
constexpr uchar FocusDim = 0x70;   // курсор в неактивном окне
constexpr uchar Info = 0x30;
constexpr uchar Dim = 0x17;
constexpr uchar Mark = 0x0E;       // цвет текста отмеченной строки
} // namespace clr

struct StatusLook
{
    std::string glyph;
    std::string label;
    uchar color;  // цвет текста (0..F)
};

StatusLook lookOf(const std::string &status, const std::string &conclusion);

// Цвет с заменой только цвета текста
TColorAttr withFore(uchar base, uchar fore);

// Нажатая клавиша соответствует символу (латиница или кириллица в той же позиции)
bool keyIs(const TEvent &ev, const char *latin, const char *cyrillic);

// Вывод строки шириной ровно width ячеек (обрезка или дополнение пробелами)
void putCell(TDrawBuffer &b, int x, int width, const std::string &text, TColorAttr attr, bool right = false);

enum class RunAction { Rerun, RerunFailed, Cancel, Delete, DeleteLogs };

// Интерфейс приложения для окон
class App
{
public:
    virtual ~App() = default;
    virtual GitHub &gh() = 0;
    virtual Poller &poller() = 0;
    // Выполнить функцию в UI-потоке (можно вызывать из любого потока)
    virtual void post(std::function<void()> fn) = 0;
    // Выполнить функцию в фоновом потоке
    virtual void async(std::function<void()> fn) = 0;
    virtual void openRun(const Run &run) = 0;
    // Открыть лог первого упавшего job (или первого job, если упавших нет)
    virtual void openFirstLog(const Run &run) = 0;
    virtual void openLog(const std::string &repo, int64_t jobId, const std::string &title,
                         const std::string &jumpStep) = 0;
    // Действие над запусками с подтверждением; onDone вызывается в UI-потоке
    virtual void runAction(RunAction a, std::vector<Run> runs, std::function<void(bool)> onDone = {}) = 0;
};

extern App *gApp;

void showHelp();

// Окно сообщения с размером по тексту
ushort msgBox(const std::string &text, ushort options);
