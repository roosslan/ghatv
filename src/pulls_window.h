#pragma once

#include "poller.h"
#include "runs_window.h"
#include "table.h"

class PullsWindow : public TableWindow
{
public:
    explicit PullsWindow(const TRect &r);

    void updateData(Poller::State st);

    int dataCount() const override { return (int) st_.pulls.size(); }
    bool accept(int idx) const override;
    Cell cell(int idx, int col) const override;
    int compare(int a, int b, int col) const override;
    int64_t rowId(int idx) const override { return st_.pulls[idx].id; }
    bool dimmed(int idx) const override { return st_.pulls[idx].draft; }
    std::string emptyText() const override { return L("No open pull requests", "Открытых pull request'ов нет"); }
    std::string infoText() const override;
    std::string rowUrl(int idx) const override { return st_.pulls[idx].url; }

    void openRow(int idx) override;
    bool rowKey(TEvent &ev) override;
    bool command(ushort cmd) override;

private:
    void action(RunAction a);

    Poller::State st_;
    std::vector<std::vector<Run>> ci_;  // запуски CI для каждого PR из st_.pulls
    RunFilter filter_ = RunFilter::All;
    bool hideDrafts_ = false;
    bool multiOwner_ = false;
};
