#pragma once

#include "poller.h"
#include "table.h"

enum class IssueFilter { All, Mine, Foreign };

class IssuesWindow : public TableWindow
{
public:
    explicit IssuesWindow(const TRect &r);

    void updateData(Poller::State st);

    int dataCount() const override { return (int) st_.issues.size(); }
    bool accept(int idx) const override;
    Cell cell(int idx, int col) const override;
    int compare(int a, int b, int col) const override;
    int64_t rowId(int idx) const override { return st_.issues[idx].id; }
    std::string emptyText() const override { return L("No open issues", "Открытых issues нет"); }
    std::string infoText() const override;
    std::string rowUrl(int idx) const override { return st_.issues[idx].url; }

    void openRow(int idx) override;
    bool rowKey(TEvent &ev) override;
    bool command(ushort cmd) override;

private:
    Poller::State st_;
    IssueFilter filter_ = IssueFilter::All;
    bool multiOwner_ = false;
};
