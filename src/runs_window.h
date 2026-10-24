#pragma once

#include "poller.h"
#include "table.h"

#include <set>

enum class RunFilter { All, Active, Failed, Success, Other };

// Номер фильтра по клавише 1..5 (или -1)
int filterKey(const TEvent &ev);
// Совпадение категории с фильтром
bool filterMatch(RunFilter f, RunCategory cat);
// Текст статуса со значком: «✔ успех»
Cell statusCell(const StatusLook &look);
// Порядок статусов при сортировке: активные, упавшие, прочие, успешные
int statusRank(const Run &r);

class RunsWindow : public TableWindow
{
public:
    explicit RunsWindow(const TRect &r);

    void updateData(Poller::State st);

    int dataCount() const override { return (int) st_.runs.size(); }
    bool accept(int idx) const override;
    Cell cell(int idx, int col) const override;
    int compare(int a, int b, int col) const override;
    int64_t rowId(int idx) const override { return st_.runs[idx].id; }
    void postFilter(std::vector<int> &rows) const override;
    std::string leadText(int idx, uchar &fore) const override;
    bool marked(int idx) const override { return marked_.count(st_.runs[idx].id) > 0; }
    std::string emptyText() const override { return L("No runs to show", "Нет запусков для отображения"); }
    std::string infoText() const override;
    std::string rowUrl(int idx) const override { return st_.runs[idx].url; }

    void openRow(int idx) override;
    bool rowKey(TEvent &ev) override;
    bool command(ushort cmd) override;

private:
    void setFilter(RunFilter f);
    void doAction(RunAction a);
    std::vector<Run> targets() const;

    Poller::State st_;
    std::set<int64_t> marked_;
    RunFilter filter_ = RunFilter::All;
    bool latestOnly_ = false;
    bool multiOwner_ = false;
};
