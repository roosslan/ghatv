#include "runs_window.h"
#include "github.h"
#include "util.h"

#include <map>

namespace
{

enum Col { ColWhen, ColStatus, ColRepo, ColWorkflow, ColBranch, ColEvent, ColTitle, ColDur };

std::vector<ColumnDef> runColumns()
{
    std::vector<ColumnDef> c(8);
    c[ColWhen] = {"Когда", 14, 0, 0, false, 0, true};
    c[ColStatus] = {"Статус", 14};
    c[ColRepo] = {"Репозиторий", 0, 26, 30};
    c[ColWorkflow] = {"Workflow", 0, 22, 26};
    c[ColBranch] = {"Ветка", 0, 14, 20};
    c[ColEvent] = {"Событие", 12, 0, 0, false, 130};
    c[ColTitle] = {"Заголовок"};
    c[ColDur] = {"Длит.", 8, 0, 0, true, 0, true};
    return c;
}

long runDuration(const Run &r, time_t now)
{
    time_t start = r.startedAt ? r.startedAt : r.createdAt;
    time_t end = r.status == "completed" ? r.updatedAt : now;
    return (long) (end - start);
}

} // namespace

int filterKey(const TEvent &ev)
{
    for (int i = 0; i < 5; ++i)
    {
        char k[2] = {(char) ('1' + i), 0};
        if (keyIs(ev, k, nullptr))
            return i;
    }
    return -1;
}

bool filterMatch(RunFilter f, RunCategory cat)
{
    switch (f)
    {
    case RunFilter::Active:
        return cat == RunCategory::Active;
    case RunFilter::Failed:
        return cat == RunCategory::Failed;
    case RunFilter::Success:
        return cat == RunCategory::Success;
    case RunFilter::Other:
        return cat == RunCategory::Other;
    default:
        return true;
    }
}

Cell statusCell(const StatusLook &look)
{
    return {look.glyph + " " + look.label, look.color};
}

int statusRank(const Run &r)
{
    switch (categoryOf(r))
    {
    case RunCategory::Active:
        return 0;
    case RunCategory::Failed:
        return 1;
    case RunCategory::Other:
        return 2;
    default:
        return 3;
    }
}

RunsWindow::RunsWindow(const TRect &r) :
    TWindowInit(&TWindow::initFrame),
    TableWindow(r, "Запуски", 1, runColumns(), 1)
{
}

void RunsWindow::updateData(Poller::State st)
{
    st_ = std::move(st);
    std::set<std::string> owners;
    std::set<int64_t> ids;
    for (auto &r : st_.runs)
    {
        owners.insert(r.repo.substr(0, r.repo.find('/')));
        ids.insert(r.id);
    }
    multiOwner_ = owners.size() > 1;
    for (auto it = marked_.begin(); it != marked_.end();)
        it = ids.count(*it) ? std::next(it) : marked_.erase(it);
    refresh();
}

bool RunsWindow::accept(int idx) const
{
    return filterMatch(filter_, categoryOf(st_.runs[idx]));
}

Cell RunsWindow::cell(int idx, int col) const
{
    const Run &r = st_.runs[idx];
    switch (col)
    {
    case ColWhen:
        return {util::localTime(r.createdAt)};
    case ColStatus:
        return statusCell(lookOf(r.status, r.conclusion));
    case ColRepo:
        return {repoLabel(r.repo, multiOwner_)};
    case ColWorkflow:
        return {r.workflow};
    case ColBranch:
        return {r.branch};
    case ColEvent:
        return {r.event};
    case ColTitle:
        return {"#" + std::to_string(r.runNumber) + " " + r.title};
    case ColDur:
        return {util::duration(runDuration(r, time(nullptr)))};
    }
    return {};
}

int RunsWindow::compare(int a, int b, int col) const
{
    const Run &x = st_.runs[a], &y = st_.runs[b];
    switch (col)
    {
    case ColWhen:
        return cmpNum(x.createdAt, y.createdAt);
    case ColStatus:
    {
        int c = cmpNum(statusRank(x), statusRank(y));
        return c ? c : cmpText(lookOf(x.status, x.conclusion).label, lookOf(y.status, y.conclusion).label);
    }
    case ColRepo:
        return cmpText(x.repo, y.repo);
    case ColWorkflow:
        return cmpText(x.workflow, y.workflow);
    case ColBranch:
        return cmpText(x.branch, y.branch);
    case ColEvent:
        return cmpText(x.event, y.event);
    case ColTitle:
        return cmpText(x.title, y.title);
    case ColDur:
    {
        time_t now = time(nullptr);
        return cmpNum(runDuration(x, now), runDuration(y, now));
    }
    }
    return 0;
}

void RunsWindow::postFilter(std::vector<int> &rows) const
{
    if (!latestOnly_)
        return;
    // Оставить самый свежий запуск каждой пары workflow + ветка, не меняя порядок сортировки
    auto key = [this](int i) {
        const Run &r = st_.runs[i];
        return r.repo + "\n" + std::to_string(r.workflowId) + "\n" + r.branch;
    };
    std::map<std::string, int> latest;
    for (int i : rows)
    {
        auto it = latest.find(key(i));
        if (it == latest.end() || st_.runs[it->second].createdAt < st_.runs[i].createdAt)
            latest[key(i)] = i;
    }
    std::vector<int> out;
    for (int i : rows)
        if (latest[key(i)] == i)
            out.push_back(i);
    rows.swap(out);
}

std::string RunsWindow::leadText(int idx, uchar &fore) const
{
    fore = clr::Mark;
    return marked(idx) ? "√" : "";
}

std::string RunsWindow::infoText() const
{
    static const char *names[] = {"все", "активные", "упавшие", "успешные", "прочие"};
    std::string s;
    if (!st_.reposLoaded && st_.error.empty())
        s = "Загрузка списка репозиториев…";
    else
        s = "Репозиториев: " + std::to_string(st_.repoCount) + "  запусков: " + std::to_string(st_.runs.size()) +
            "  показано: " + std::to_string(rowCount());
    s += " │ фильтр: ";
    s += names[(int) filter_];
    if (latestOnly_)
        s += ", последние";
    s += " │ " + viewState();
    if (!marked_.empty())
        s += " │ отмечено: " + std::to_string(marked_.size());
    int rem = gApp->gh().rateRemaining(), lim = gApp->gh().rateLimit();
    if (rem >= 0)
        s += " │ API: " + std::to_string(rem) + "/" + std::to_string(lim);
    if (st_.busy)
        s += " │ ⟳ обновление";
    else if (st_.lastSync)
        s += " │ обновлено " + util::ago(st_.lastSync, time(nullptr)) + " назад";
    if (!st_.error.empty())
        s += " │ ⚠ " + st_.error;
    return s;
}

void RunsWindow::openRow(int idx)
{
    gApp->openRun(st_.runs[idx]);
}

void RunsWindow::setFilter(RunFilter f)
{
    filter_ = f;
    refresh();
}

std::vector<Run> RunsWindow::targets() const
{
    std::vector<Run> out;
    if (!marked_.empty())
    {
        for (auto &r : st_.runs)
            if (marked_.count(r.id))
                out.push_back(r);
    }
    else if (int idx = focusedData(); idx >= 0)
        out.push_back(st_.runs[idx]);
    return out;
}

void RunsWindow::doAction(RunAction a)
{
    auto t = targets();
    if (t.empty())
        return;
    gApp->runAction(a, std::move(t), [this, a](bool ok) {
        if (ok && (a == RunAction::Delete || a == RunAction::DeleteLogs))
        {
            marked_.clear();
            redraw();
        }
    });
}

bool RunsWindow::rowKey(TEvent &ev)
{
    int f = filterKey(ev);
    if (f >= 0)
        setFilter((RunFilter) f);
    else if (ev.keyDown.keyCode == kbIns)
    {
        int row = focusedRow();
        if (int idx = dataAt(row); idx >= 0)
        {
            if (!marked_.erase(st_.runs[idx].id))
                marked_.insert(st_.runs[idx].id);
            if (row + 1 < rowCount())
                list_->focusItemNum(row + 1);
            redraw();
        }
    }
    else if (ev.keyDown.keyCode == kbDel)
        doAction(RunAction::Delete);
    else if (keyIs(ev, "r", "к"))
        doAction(RunAction::Rerun);
    else if (keyIs(ev, "f", "а"))
        doAction(RunAction::RerunFailed);
    else if (keyIs(ev, "c", "с"))
        doAction(RunAction::Cancel);
    else if (keyIs(ev, "l", "д"))
        command(cmLatestOnly);
    else if (keyIs(ev, "+", nullptr))
        command(cmMarkAll);
    else if (keyIs(ev, "-", nullptr))
        command(cmUnmarkAll);
    else
        return false;
    return true;
}

bool RunsWindow::command(ushort cmd)
{
    int idx = focusedData();
    switch (cmd)
    {
    case cmOpenRun:
        if (idx >= 0)
            openRow(idx);
        break;
    case cmShowLog:
        if (idx >= 0)
            gApp->openFirstLog(st_.runs[idx]);
        break;
    case cmRerun:
        doAction(RunAction::Rerun);
        break;
    case cmRerunFailed:
        doAction(RunAction::RerunFailed);
        break;
    case cmCancelRun:
        doAction(RunAction::Cancel);
        break;
    case cmDeleteRun:
        doAction(RunAction::Delete);
        break;
    case cmDeleteLogs:
        doAction(RunAction::DeleteLogs);
        break;
    case cmFilterAll:
    case cmFilterActive:
    case cmFilterFailed:
    case cmFilterSuccess:
    case cmFilterOther:
        setFilter((RunFilter) (cmd - cmFilterAll));
        break;
    case cmLatestOnly:
        latestOnly_ = !latestOnly_;
        refresh();
        break;
    case cmMarkAll:
        for (int r = 0; r < rowCount(); ++r)
            marked_.insert(st_.runs[dataAt(r)].id);
        redraw();
        break;
    case cmUnmarkAll:
        marked_.clear();
        redraw();
        break;
    default:
        return false;
    }
    return true;
}
