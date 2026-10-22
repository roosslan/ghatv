#include "pulls_window.h"
#include "util.h"

#include <set>

namespace
{

enum Col { ColUpdated, ColCi, ColRepo, ColNumber, ColTitle, ColBranch, ColAuthor };

std::vector<ColumnDef> pullColumns()
{
    std::vector<ColumnDef> c(7);
    c[ColUpdated] = {"Обновлён", 14, 0, 0, false, 0, true};
    c[ColCi] = {"CI", 14};
    c[ColRepo] = {"Репозиторий", 0, 24, 30};
    c[ColNumber] = {"#", 6, 0, 0, true, 0, true};
    c[ColTitle] = {"Заголовок"};
    c[ColBranch] = {"Ветка → база", 0, 26, 36};
    c[ColAuthor] = {"Автор", 14, 0, 0, false, 110};
    return c;
}

int ciRank(RunCategory c, bool none)
{
    if (none)
        return 4;
    switch (c)
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

} // namespace

PullsWindow::PullsWindow(const TRect &r) :
    TWindowInit(&TWindow::initFrame),
    TableWindow(r, "Pull request'ы", 2, pullColumns())
{
}

void PullsWindow::updateData(Poller::State st)
{
    st_ = std::move(st);
    std::set<std::string> owners;
    for (auto &p : st_.pulls)
        owners.insert(p.repo.substr(0, p.repo.find('/')));
    multiOwner_ = owners.size() > 1;
    ci_.clear();
    for (auto &p : st_.pulls)
        ci_.push_back(runsOfPull(st_.runs, p));
    refresh();
}

bool PullsWindow::accept(int idx) const
{
    if (hideDrafts_ && st_.pulls[idx].draft)
        return false;
    // «Прочие» — без CI или с отменёнными/пропущенными запусками
    return filterMatch(filter_, ciCategory(ci_[idx]));
}

Cell PullsWindow::cell(int idx, int col) const
{
    const PullRequest &p = st_.pulls[idx];
    switch (col)
    {
    case ColUpdated:
        return {util::localTime(p.updatedAt)};
    case ColCi:
        return statusCell(ciLook(ci_[idx]));
    case ColRepo:
        return {repoLabel(p.repo, multiOwner_)};
    case ColNumber:
        return {std::to_string(p.number)};
    case ColTitle:
        return {(p.draft ? "(черновик) " : "") + p.title};
    case ColBranch:
        return {p.headRef + " → " + p.baseRef};
    case ColAuthor:
        return {p.author};
    }
    return {};
}

int PullsWindow::compare(int a, int b, int col) const
{
    const PullRequest &x = st_.pulls[a], &y = st_.pulls[b];
    switch (col)
    {
    case ColUpdated:
        return cmpNum(x.updatedAt, y.updatedAt);
    case ColCi:
        return cmpNum(ciRank(ciCategory(ci_[a]), ci_[a].empty()), ciRank(ciCategory(ci_[b]), ci_[b].empty()));
    case ColRepo:
        return cmpText(x.repo, y.repo);
    case ColNumber:
        return cmpNum(x.number, y.number);
    case ColTitle:
        return cmpText(x.title, y.title);
    case ColBranch:
        return cmpText(x.headRef, y.headRef);
    case ColAuthor:
        return cmpText(x.author, y.author);
    }
    return 0;
}

std::string PullsWindow::infoText() const
{
    static const char *names[] = {"все", "CI идёт", "CI упал", "CI успешно", "без CI / прочие"};
    std::string s;
    if (!st_.reposLoaded && st_.error.empty())
        s = "Загрузка списка репозиториев…";
    else
        s = "Открытых PR: " + std::to_string(st_.pulls.size()) + "  показано: " + std::to_string(rowCount());
    s += " │ фильтр: ";
    s += names[(int) filter_];
    if (hideDrafts_)
        s += ", без черновиков";
    s += " │ " + viewState();
    s += " │ Enter — открыть PR, d — черновики";
    if (!st_.error.empty())
        s += " │ ⚠ " + st_.error;
    return s;
}

void PullsWindow::openRow(int idx)
{
    gApp->openPull(st_.pulls[idx]);
}

void PullsWindow::action(RunAction a)
{
    int idx = focusedData();
    if (idx < 0)
        return;
    if (ci_[idx].empty())
    {
        msgBox("У этого PR нет запусков workflow", mfInformation | mfOKButton);
        return;
    }
    gApp->runAction(a, ci_[idx]);
}

bool PullsWindow::rowKey(TEvent &ev)
{
    int f = filterKey(ev);
    if (f >= 0)
    {
        filter_ = (RunFilter) f;
        refresh();
    }
    else if (keyIs(ev, "r", "к"))
        action(RunAction::Rerun);
    else if (keyIs(ev, "f", "а"))
        action(RunAction::RerunFailed);
    else if (keyIs(ev, "c", "с"))
        action(RunAction::Cancel);
    else if (keyIs(ev, "d", "в"))
        command(cmHideDrafts);
    else
        return false;
    return true;
}

bool PullsWindow::command(ushort cmd)
{
    int idx = focusedData();
    switch (cmd)
    {
    case cmOpenRun:
        if (idx >= 0)
            openRow(idx);
        break;
    case cmShowLog:
    {
        // Лог упавшего запуска PR, иначе первого
        if (idx < 0 || ci_[idx].empty())
            break;
        const Run *pick = &ci_[idx].front();
        for (auto &r : ci_[idx])
            if (isFailure(r.conclusion))
            {
                pick = &r;
                break;
            }
        gApp->openFirstLog(*pick);
        break;
    }
    case cmRerun:
        action(RunAction::Rerun);
        break;
    case cmRerunFailed:
        action(RunAction::RerunFailed);
        break;
    case cmCancelRun:
        action(RunAction::Cancel);
        break;
    case cmFilterAll:
    case cmFilterActive:
    case cmFilterFailed:
    case cmFilterSuccess:
    case cmFilterOther:
        filter_ = (RunFilter) (cmd - cmFilterAll);
        refresh();
        break;
    case cmHideDrafts:
        hideDrafts_ = !hideDrafts_;
        refresh();
        break;
    default:
        // Удаление запусков из окна PR не выполняется
        return false;
    }
    return true;
}
