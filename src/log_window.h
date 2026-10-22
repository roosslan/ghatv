#pragma once

#include "ui.h"

struct LogLine
{
    std::string text;
    uchar color = 0x07;
    bool error = false;
    bool group = false;
};

std::vector<LogLine> parseLog(const std::string &raw, int &maxWidth);
// Перенос строки по словам на ширину width ячеек
std::vector<std::string> wrapText(const std::string &line, int width);

class LogView : public TScroller
{
public:
    LogView(const TRect &r, TScrollBar *hs, TScrollBar *vs);

    void setLines(std::vector<LogLine> lines, int maxWidth, const std::string &jump);
    void setMessage(const std::string &msg);
    void draw() override;
    void handleEvent(TEvent &ev) override;

    void search();
    std::string infoText() const;
    // Подсказка в строке состояния вместо подсказки для лога
    void setHint(const std::string &hint) { hint_ = hint; }

private:
    void gotoLine(int line);
    void findNext(int dir);
    void nextError(int dir);

    std::vector<LogLine> lines_;
    std::string message_;
    std::string search_;
    int cur_ = -1;
    int errors_ = 0;
    std::string hint_;
};

class LogInfo : public TView
{
public:
    LogInfo(const TRect &r, LogView *v);
    void draw() override;

private:
    LogView *v_;
};

class LogWindow : public TWindow
{
public:
    LogWindow(const TRect &r, const std::string &repo, int64_t jobId, const std::string &title,
              const std::string &jump);

    void handleEvent(TEvent &ev) override;
    void shutDown() override;

private:
    void load();

    std::string repo_;
    int64_t jobId_;
    std::string jump_;
    LogView *view_;
    LogInfo *info_;
    std::shared_ptr<bool> alive_;
};

// Описание и комментарии issue или PR
class DiscussionWindow : public TWindow
{
public:
    DiscussionWindow(const TRect &r, const std::string &repo, int number, const std::string &title);

    void handleEvent(TEvent &ev) override;
    void shutDown() override;

private:
    void load();

    std::string repo_;
    int number_;
    LogView *view_;
    LogInfo *info_;
    std::shared_ptr<bool> alive_;
};
