#pragma once

#include "ui.h"

#include <map>

class RunWindow;

struct JobRow
{
    enum Kind { JobKind, StepKind, AnnKind, MsgKind } kind = MsgKind;
    int job = -1;
    int step = -1;
    int ann = -1;
    std::string text;
};

class RunInfoView : public TView
{
public:
    RunInfoView(const TRect &r, RunWindow *w);
    void draw() override;

private:
    RunWindow *w_;
};

class JobsList : public TListViewer
{
public:
    JobsList(const TRect &r, TScrollBar *vs, RunWindow *w);
    void draw() override;
    void handleEvent(TEvent &ev) override;
    void selectItem(short item) override;
    void getText(char *dest, short item, short maxLen) override;

private:
    RunWindow *w_;
};

class RunWindow : public TWindow
{
public:
    RunWindow(const TRect &r, const Run &run);

    void handleEvent(TEvent &ev) override;
    void shutDown() override;

    const Run &run() const { return run_; }
    const std::vector<JobRow> &rows() const { return rows_; }
    const std::vector<Job> &jobs() const { return jobs_; }
    const Annotation *annotation(int job, int ann) const;
    bool loading() const { return loading_; }

    void openRow(int i);

private:
    void reload();
    void rebuildRows();
    void action(RunAction a);

    Run run_;
    std::vector<Job> jobs_;
    std::map<int64_t, std::vector<Annotation>> ann_;
    std::vector<JobRow> rows_;
    std::string error_;
    bool loading_ = false;

    RunInfoView *info_;
    JobsList *list_;
    std::shared_ptr<bool> alive_;
    TTimerId timer_ = 0;
};
