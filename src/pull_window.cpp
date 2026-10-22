#include "pull_window.h"
#include "github.h"
#include "runs_window.h"
#include "util.h"

namespace
{

const int kInfoLines = 5;

enum Col { ColWhen, ColStatus, ColWorkflow, ColEvent, ColAttempt, ColDur };

std::vector<ColumnDef> runColumns()
{
    std::vector<ColumnDef> c(6);
    c[ColWhen] = {"Когда", 14, 0, 0, false, 0, true};
    c[ColStatus] = {"Статус", 14};
    c[ColWorkflow] = {"Workflow"};
    c[ColEvent] = {"Событие", 14};
    c[ColAttempt] = {"Попытка", 8, 0, 0, true};
    c[ColDur] = {"Длит.", 8, 0, 0, true, 0, true};
    return c;
}

long runDuration(const Run &r)
{
    time_t start = r.startedAt ? r.startedAt : r.createdAt;
    time_t end = r.status == "completed" ? r.updatedAt : time(nullptr);
    return (long) (end - start);
}

std::string mergeText(const std::string &s)
{
    if (s == "clean")
        return "можно слить";
    if (s == "dirty")
        return "конфликты";
    if (s == "blocked")
        return "слияние заблокировано";
    if (s == "behind")
        return "отстаёт от базы";
    if (s == "unstable")
        return "проверки не пройдены";
    if (s == "draft")
        return "черновик";
    if (s == "has_hooks")
        return "можно слить (есть хуки)";
    return "состояние слияния вычисляется";
}

} // namespace

// ---------------------------------------------------------------- PullInfoView

PullInfoView::PullInfoView(const TRect &r, PullWindow *w) :
    TView(r),
    w_(w)
{
    growMode = gfGrowHiX;
}

void PullInfoView::draw()
{
    const PullRequest &p = w_->pr();
    TColorAttr n(clr::Normal), h(clr::Header), d(clr::Dim);
    TDrawBuffer b;

    putCell(b, 0, size.x, " #" + std::to_string(p.number) + "  " + p.title, h);
    writeLine(0, 0, size.x, 1, b);

    std::string l1 = " " + p.repo + " · автор " + p.author + " · " + p.headRef + " → " + p.baseRef;
    if (p.draft)
        l1 += " · черновик";
    putCell(b, 0, size.x, l1, n);
    writeLine(0, 1, size.x, 1, b);

    std::string l2 = " создан " + util::localTime(p.createdAt) + " · обновлён " + util::localTime(p.updatedAt);
    if (w_->hasDetails())
    {
        auto &dt = w_->details();
        l2 += " · коммитов " + std::to_string(dt.commits) + " · файлов " + std::to_string(dt.changedFiles) + " · +" +
              std::to_string(dt.additions) + " −" + std::to_string(dt.deletions) + " · " +
              mergeText(dt.mergeableState);
    }
    if (w_->loading())
        l2 += " · ⟳";
    putCell(b, 0, size.x, l2, n);
    writeLine(0, 2, size.x, 1, b);

    std::string l3 = " ▸ Описание и комментарии";
    if (w_->hasDetails())
        l3 += " (" + std::to_string(w_->details().comments) + " комм., " +
              std::to_string(w_->details().reviewComments) + " в ревью)";
    l3 += " — клавиша d или щелчок";
    putCell(b, 0, size.x, l3, withFore(clr::Normal, 0xF));
    writeLine(0, 3, size.x, 1, b);

    putCell(b, 0, size.x, " " + p.url, d);
    writeLine(0, 4, size.x, 1, b);
}

void PullInfoView::handleEvent(TEvent &ev)
{
    TView::handleEvent(ev);
    if (ev.what == evMouseDown)
    {
        TPoint p = makeLocal(ev.mouse.where);
        if (p.y == 3 || (ev.mouse.eventFlags & meDoubleClick))
            w_->openDiscussion();
        clearEvent(ev);
    }
}

// ---------------------------------------------------------------- PullWindow

PullWindow::PullWindow(const TRect &r, const PullRequest &pr) :
    TWindowInit(&TWindow::initFrame),
    TableWindow(r, "PR " + pr.repo.substr(pr.repo.find('/') + 1) + "#" + std::to_string(pr.number), wnNoNumber,
                runColumns(), 0, kInfoLines),
    pr_(pr),
    alive_(std::make_shared<bool>(true))
{
    flags |= wfClose;
    TRect e = getExtent();
    e.grow(-1, -1);
    infoView_ = new PullInfoView(TRect(e.a.x, topY_, e.b.x, topY_ + kInfoLines), this);
    insert(infoView_);
    list_->select();
    reload();
    timer_ = setTimer(10000, 10000);
}

void PullWindow::shutDown()
{
    *alive_ = false;
    if (timer_)
        killTimer(timer_);
    timer_ = 0;
    infoView_ = nullptr;
    TableWindow::shutDown();
}

void PullWindow::reload()
{
    if (loading_)
        return;
    loading_ = true;
    if (infoView_)
        infoView_->drawView();
    auto alive = alive_;
    PullRequest pr = pr_;
    gApp->async([this, alive, pr] {
        PullDetails det;
        std::vector<Run> runs;
        std::string errDet, errRuns;
        bool okDet = gApp->gh().pullDetails(pr.repo, pr.number, det, errDet);
        bool okRuns = pr.headSha.empty() || gApp->gh().runsForSha(pr.repo, pr.headSha, runs, errRuns);
        gApp->post([this, alive, okDet, det, okRuns, runs, errDet, errRuns] {
            if (!*alive)
                return;
            loading_ = false;
            if (okDet)
            {
                details_ = det;
                hasDetails_ = true;
            }
            if (okRuns)
                runs_ = runs;
            error_ = !okRuns ? errRuns : !okDet ? errDet : "";
            refresh();
        });
    });
}

Cell PullWindow::cell(int idx, int col) const
{
    const Run &r = runs_[idx];
    switch (col)
    {
    case ColWhen:
        return {util::localTime(r.createdAt)};
    case ColStatus:
        return statusCell(lookOf(r.status, r.conclusion));
    case ColWorkflow:
        return {r.workflow + " #" + std::to_string(r.runNumber)};
    case ColEvent:
        return {r.event};
    case ColAttempt:
        return {std::to_string(r.attempt)};
    case ColDur:
        return {util::duration(runDuration(r))};
    }
    return {};
}

int PullWindow::compare(int a, int b, int col) const
{
    const Run &x = runs_[a], &y = runs_[b];
    switch (col)
    {
    case ColWhen:
        return cmpNum(x.createdAt, y.createdAt);
    case ColStatus:
        return cmpNum(statusRank(x), statusRank(y));
    case ColWorkflow:
        return cmpText(x.workflow, y.workflow);
    case ColEvent:
        return cmpText(x.event, y.event);
    case ColAttempt:
        return cmpNum(x.attempt, y.attempt);
    case ColDur:
        return cmpNum(runDuration(x), runDuration(y));
    }
    return 0;
}

std::string PullWindow::emptyText() const
{
    if (loading_)
        return "Загрузка…";
    if (!error_.empty())
        return "Ошибка: " + error_;
    return "Для последнего коммита PR нет запусков workflow";
}

std::string PullWindow::infoText() const
{
    std::string s = "Запусков для коммита " + pr_.headSha.substr(0, 7) + ": " + std::to_string(runs_.size());
    s += " │ " + viewState();
    s += " │ Enter — запуск, d — описание и комментарии, F3 — лог, Esc/Backspace — назад";
    if (!error_.empty())
        s += " │ ⚠ " + error_;
    return s;
}

void PullWindow::openRow(int idx)
{
    gApp->openRun(runs_[idx]);
}

void PullWindow::openDiscussion()
{
    gApp->openDiscussion(pr_.repo, pr_.number, pr_.title);
}

void PullWindow::action(RunAction a)
{
    int idx = focusedData();
    if (idx < 0)
        return;
    auto alive = alive_;
    gApp->runAction(a, {runs_[idx]}, [this, alive](bool ok) {
        if (*alive && ok)
            reload();
    });
}

bool PullWindow::rowKey(TEvent &ev)
{
    if (keyIs(ev, "d", "в"))
        openDiscussion();
    else if (keyIs(ev, "r", "к"))
        action(RunAction::Rerun);
    else if (keyIs(ev, "f", "а"))
        action(RunAction::RerunFailed);
    else if (keyIs(ev, "c", "с"))
        action(RunAction::Cancel);
    else
        return false;
    return true;
}

bool PullWindow::command(ushort cmd)
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
            gApp->openFirstLog(runs_[idx]);
        break;
    case cmRerun:
        action(RunAction::Rerun);
        break;
    case cmRerunFailed:
        action(RunAction::RerunFailed);
        break;
    case cmCancelRun:
        action(RunAction::Cancel);
        break;
    case cmRefresh:
        reload();
        break;
    default:
        return false;
    }
    return true;
}

void PullWindow::handleEvent(TEvent &ev)
{
    if (ev.what == evKeyDown && ev.keyDown.keyCode == kbEsc)
    {
        close();
        clearEvent(ev);
        return;
    }
    if (ev.what == evBroadcast && ev.message.command == cmTimerExpired && timer_ &&
        ev.message.infoPtr == (void *) timer_)
    {
        bool active = false;
        for (auto &r : runs_)
            if (r.status != "completed")
                active = true;
        if (active)
            reload();
        return;
    }
    TableWindow::handleEvent(ev);
}
