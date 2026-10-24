#include "issues_window.h"
#include "runs_window.h"
#include "util.h"

#include <set>

namespace
{

enum Col { ColUpdated, ColRepo, ColNumber, ColTitle, ColLabels, ColAuthor, ColComments };

std::vector<ColumnDef> issueColumns()
{
    std::vector<ColumnDef> c(7);
    c[ColUpdated] = {"Updated|Обновлён", 14, 0, 0, false, 0, true};
    c[ColRepo] = {"Repository|Репозиторий", 0, 28, 36};
    c[ColNumber] = {"#", 6, 0, 0, true, 0, true};
    c[ColTitle] = {"Title|Заголовок"};
    c[ColLabels] = {"Labels|Метки", 0, 18, 28};
    c[ColAuthor] = {"Author|Автор", 14, 0, 0, false, 110};
    c[ColComments] = {"Comm.|Комм.", 6, 0, 0, true, 0, true};
    return c;
}

std::string joinLabels(const std::vector<std::string> &l)
{
    std::string s;
    for (auto &x : l)
        s += (s.empty() ? "" : ", ") + x;
    return s;
}

} // namespace

IssuesWindow::IssuesWindow(const TRect &r) :
    TWindowInit(&TWindow::initFrame),
    TableWindow(r, "Issues", 3, issueColumns())
{
}

void IssuesWindow::updateData(Poller::State st)
{
    st_ = std::move(st);
    std::set<std::string> owners;
    for (auto &i : st_.issues)
        if (!i.foreign)
            owners.insert(i.repo.substr(0, i.repo.find('/')));
    multiOwner_ = owners.size() > 1;
    refresh();
}

bool IssuesWindow::accept(int idx) const
{
    const Issue &i = st_.issues[idx];
    switch (filter_)
    {
    case IssueFilter::Mine:
        return !i.foreign;
    case IssueFilter::Foreign:
        return i.foreign;
    default:
        return true;
    }
}

Cell IssuesWindow::cell(int idx, int col) const
{
    const Issue &i = st_.issues[idx];
    switch (col)
    {
    case ColUpdated:
        return {util::localTime(i.updatedAt)};
    case ColRepo:
        // Чужие репозитории показываются с владельцем и выделяются цветом
        if (i.foreign)
            return {i.repo, 0xD};
        return {repoLabel(i.repo, multiOwner_)};
    case ColNumber:
        return {std::to_string(i.number)};
    case ColTitle:
        return {i.title};
    case ColLabels:
        return {joinLabels(i.labels), 0xE};
    case ColAuthor:
        return {i.author};
    case ColComments:
        return {i.comments ? std::to_string(i.comments) : ""};
    }
    return {};
}

int IssuesWindow::compare(int a, int b, int col) const
{
    const Issue &x = st_.issues[a], &y = st_.issues[b];
    switch (col)
    {
    case ColUpdated:
        return cmpNum(x.updatedAt, y.updatedAt);
    case ColRepo:
        return cmpText(x.repo, y.repo);
    case ColNumber:
        return cmpNum(x.number, y.number);
    case ColTitle:
        return cmpText(x.title, y.title);
    case ColLabels:
        return cmpText(joinLabels(x.labels), joinLabels(y.labels));
    case ColAuthor:
        return cmpText(x.author, y.author);
    case ColComments:
        return cmpNum(x.comments, y.comments);
    }
    return 0;
}

std::string IssuesWindow::infoText() const
{
    const char *names[] = {L("all", "все"), L("in my repositories", "в моих репозиториях"), L("mine elsewhere", "мои в чужих")};
    int foreign = 0;
    for (auto &i : st_.issues)
        foreign += i.foreign;
    std::string s;
    if (!st_.reposLoaded && st_.error.empty())
        s = L("Loading repository list…", "Загрузка списка репозиториев…");
    else
        s = "Issues: " + std::to_string(st_.issues.size()) + L(" (mine: ", " (в моих: ") +
            std::to_string(st_.issues.size() - foreign) + L(", elsewhere: ", ", в чужих: ") + std::to_string(foreign) +
            L(")  shown: ", ")  показано: ") + std::to_string(rowCount());
    s += L(" │ filter: ", " │ фильтр: ");
    s += names[(int) filter_];
    s += " │ " + viewState();
    s += L(" │ Enter — open, o — browser, 1/2/3 — filter", " │ Enter — открыть, 1/2/3 — фильтр");
    if (!st_.error.empty())
        s += " │ ⚠ " + st_.error;
    return s;
}

void IssuesWindow::openRow(int idx)
{
    const Issue &i = st_.issues[idx];
    gApp->openDiscussion(i.repo, i.number, i.title);
}

bool IssuesWindow::rowKey(TEvent &ev)
{
    int f = filterKey(ev);
    if (f < 0 || f > 2)
        return false;
    filter_ = (IssueFilter) f;
    refresh();
    return true;
}

bool IssuesWindow::command(ushort cmd)
{
    switch (cmd)
    {
    case cmOpenRun:
        if (int idx = focusedData(); idx >= 0)
            openRow(idx);
        return true;
    case cmFilterAll:
    case cmFilterActive:
    case cmFilterFailed:
        filter_ = (IssueFilter) (cmd - cmFilterAll);
        refresh();
        return true;
    default:
        return false;
    }
}
