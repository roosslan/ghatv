#pragma once

#include "poller.h"
#include "ui.h"

#include <set>

class RunsWindow;

// Раскладка столбцов главной таблицы
struct RunCols
{
    int status, repo, workflow, branch, event, title, dur, age;
    static RunCols compute(int width);
};

class RunsHeader : public TView
{
public:
    explicit RunsHeader(const TRect &r);
    void draw() override;
};

class RunsInfo : public TView
{
public:
    RunsInfo(const TRect &r, RunsWindow *w);
    void draw() override;

private:
    RunsWindow *w_;
};

class RunsList : public TListViewer
{
public:
    RunsList(const TRect &r, TScrollBar *vs, RunsWindow *w);
    void draw() override;
    void handleEvent(TEvent &ev) override;
    void selectItem(short item) override;
    void getText(char *dest, short item, short maxLen) override;

private:
    RunsWindow *w_;
};

enum class RunFilter { All, Active, Failed, Success, Other };

class RunsWindow : public TWindow
{
public:
    explicit RunsWindow(const TRect &r);

    void updateData(Poller::State st);
    void tick();
    void handleEvent(TEvent &ev) override;

    int count() const { return (int) visible_.size(); }
    const Run *runAt(int i) const;
    const Run *current() const;
    bool isMarked(int64_t id) const { return marked_.count(id) > 0; }
    bool multiOwner() const { return multiOwner_; }
    std::string infoText() const;

    void toggleMark(int i);
    void setFilter(RunFilter f);
    void doAction(RunAction a);

private:
    void rebuild();
    std::vector<Run> targets() const;

    RunsHeader *header_;
    RunsList *list_;
    RunsInfo *info_;

    Poller::State st_;
    std::vector<int> visible_;
    std::set<int64_t> marked_;
    RunFilter filter_ = RunFilter::All;
    std::string search_;
    bool latestOnly_ = false;
    bool sortByRepo_ = false;
    bool multiOwner_ = false;
};
