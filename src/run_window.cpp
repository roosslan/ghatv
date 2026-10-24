#include "run_window.h"
#include "github.h"
#include "poller.h"
#include "util.h"

#include <cstring>

namespace
{

std::string stepDuration(time_t start, time_t end, const std::string &status)
{
    if (!start)
        return "";
    if (status != "completed" || !end)
        end = time(nullptr);
    return util::duration((long) (end - start));
}

} // namespace

// ---------------------------------------------------------------- RunInfoView

RunInfoView::RunInfoView(const TRect &r, RunWindow *w) :
    TView(r),
    w_(w)
{
    growMode = gfGrowHiX;
}

void RunInfoView::draw()
{
    const Run &r = w_->run();
    StatusLook look = lookOf(r.status, r.conclusion);
    time_t start = r.startedAt ? r.startedAt : r.createdAt;
    time_t end = r.status == "completed" ? r.updatedAt : time(nullptr);

    TDrawBuffer b;
    TColorAttr n(clr::Normal), h(clr::Header);

    b.moveChar(0, ' ', n, size.x);
    int x = 1;
    x += b.moveStr(x, look.glyph + " " + look.label, withFore(clr::Normal, look.color));
    std::string head = "   " + r.workflow + " #" + std::to_string(r.runNumber);
    if (r.attempt > 1)
        head += L(" (attempt ", " (попытка ") + std::to_string(r.attempt) + ")";
    head += "   " + r.repo;
    b.moveStr(x, head, h, size.x - x);
    writeLine(0, 0, size.x, 1, b);

    putCell(b, 0, size.x, " " + r.title, n);
    writeLine(0, 1, size.x, 1, b);

    std::string meta = L(" branch ", " ветка ") + r.branch + " · " + r.event + L(" · author ", " · автор ") + r.actor + L(" · started ", " · начат ") +
                       util::localTime(start) + L(" · duration ", " · длительность ") + util::duration((long) (end - start));
    if (w_->loading())
        meta += " · ⟳";
    putCell(b, 0, size.x, meta, n);
    writeLine(0, 2, size.x, 1, b);

    putCell(b, 0, size.x, " " + r.url, TColorAttr(clr::Dim));
    writeLine(0, 3, size.x, 1, b);

    b.moveChar(0, ' ', TColorAttr(clr::Separator), size.x);
    b.moveChar(0, '\xC4', TColorAttr(clr::Separator), size.x);
    writeLine(0, 4, size.x, 1, b);
}

// ---------------------------------------------------------------- JobsList

JobsList::JobsList(const TRect &r, TScrollBar *vs, RunWindow *w) :
    TListViewer(r, 1, nullptr, vs),
    w_(w)
{
    growMode = gfGrowHiX | gfGrowHiY;
    setRange(0);
}

void JobsList::getText(char *dest, short item, short maxLen)
{
    strncpy(dest, item < (short) w_->rows().size() ? w_->rows()[item].text.c_str() : "", maxLen);
    dest[maxLen - 1] = '\0';
}

void JobsList::draw()
{
    bool active = (state & (sfSelected | sfActive)) == (sfSelected | sfActive);
    auto &rows = w_->rows();
    auto &jobs = w_->jobs();
    const int labelW = 12, durW = 9;

    for (int y = 0; y < size.y; ++y)
    {
        TDrawBuffer b;
        int item = topItem + y;
        bool isFocused = item == focused && item < range;
        uchar base = isFocused ? (active ? clr::Focus : clr::FocusDim) : clr::Normal;
        b.moveChar(0, ' ', TColorAttr(base), size.x);

        if (item < (int) rows.size())
        {
            const JobRow &row = rows[item];
            int nameW = size.x - 3 - labelW - durW;
            if (row.kind == JobRow::JobKind || row.kind == JobRow::StepKind)
            {
                const Job &job = jobs[row.job];
                bool isStep = row.kind == JobRow::StepKind;
                const std::string &status = isStep ? job.steps[row.step].status : job.status;
                const std::string &concl = isStep ? job.steps[row.step].conclusion : job.conclusion;
                StatusLook look = lookOf(status, concl);
                TColorAttr st = isFocused ? TColorAttr(base) : withFore(base, look.color);
                TColorAttr txt = isFocused ? TColorAttr(base)
                                           : (isStep ? TColorAttr(base) : withFore(base, 0xF));
                std::string name, dur;
                if (isStep)
                {
                    auto &s = job.steps[row.step];
                    name = "    " + std::to_string(s.number) + ". " + s.name;
                    dur = stepDuration(s.startedAt, s.completedAt, s.status);
                }
                else
                {
                    name = "▸ " + job.name + (job.runner.empty() ? "" : "  [" + job.runner + "]");
                    dur = stepDuration(job.startedAt, job.completedAt, job.status);
                }
                b.moveStr(1, look.glyph, st);
                putCell(b, 3, labelW, look.label, st);
                putCell(b, 3 + labelW, nameW, name, txt);
                putCell(b, 3 + labelW + nameW, durW, dur, txt, true);
            }
            else if (row.kind == JobRow::AnnKind)
            {
                const Annotation *a = w_->annotation(row.job, row.ann);
                uchar color = (a && a->level == "failure") ? 0xC : (a && a->level == "warning") ? 0xE : 0xB;
                TColorAttr at = isFocused ? TColorAttr(base) : withFore(base, color);
                std::string loc;
                if (a && !a->path.empty() && a->path != ".github")
                    loc = a->path + (a->line ? ":" + std::to_string(a->line) : "") + "  ";
                std::string msg = a ? a->message : "";
                for (auto &c : msg)
                    if (c == '\n' || c == '\r')
                        c = ' ';
                putCell(b, 3 + labelW, size.x - 3 - labelW, "    ! " + loc + msg, at);
            }
            else
                putCell(b, 1, size.x - 1, row.text, TColorAttr(isFocused ? base : clr::Dim));
        }
        writeLine(0, y, size.x, 1, b);
    }
}

void JobsList::selectItem(short item)
{
    w_->openRow(item);
}

void JobsList::handleEvent(TEvent &ev)
{
    if (ev.what == evKeyDown && ev.keyDown.keyCode == kbEnter)
    {
        w_->openRow(focused);
        clearEvent(ev);
        return;
    }
    TListViewer::handleEvent(ev);
}

// ---------------------------------------------------------------- RunWindow

RunWindow::RunWindow(const TRect &r, const Run &run) :
    TWindowInit(&TWindow::initFrame),
    TWindow(r, (run.repo.substr(run.repo.find('/') + 1) + " · " + run.workflow + " #" +
                std::to_string(run.runNumber)).c_str(), wnNoNumber),
    run_(run),
    alive_(std::make_shared<bool>(true))
{
    flags = wfMove | wfGrow | wfZoom | wfClose;
    growMode = gfGrowHiX | gfGrowHiY;

    TRect e = getExtent();
    e.grow(-1, -1);
    info_ = new RunInfoView(TRect(e.a.x, e.a.y, e.b.x, e.a.y + 5), this);
    insert(info_);
    TScrollBar *vs = standardScrollBar(sbVertical);
    list_ = new JobsList(TRect(e.a.x, e.a.y + 5, e.b.x, e.b.y), vs, this);
    insert(list_);
    list_->select();

    reload();
    rebuildRows();
    timer_ = setTimer(5000, 5000);
}

void RunWindow::shutDown()
{
    *alive_ = false;
    if (timer_)
        killTimer(timer_);
    timer_ = 0;
    info_ = nullptr;
    list_ = nullptr;
    TWindow::shutDown();
}

const Annotation *RunWindow::annotation(int job, int ann) const
{
    if (job < 0 || job >= (int) jobs_.size())
        return nullptr;
    auto it = ann_.find(jobs_[job].id);
    if (it == ann_.end() || ann < 0 || ann >= (int) it->second.size())
        return nullptr;
    return &it->second[ann];
}

void RunWindow::reload()
{
    if (loading_)
        return;
    loading_ = true;
    if (info_)
        info_->drawView();
    auto alive = alive_;
    Run run = run_;
    gApp->async([this, alive, run] {
        GitHub &gh = gApp->gh();
        Run fresh;
        std::string errRun, errJobs;
        bool okRun = gh.getRun(run.repo, run.id, fresh, errRun);
        std::vector<Job> jobs;
        bool okJobs = gh.listJobs(run.repo, run.id, jobs, errJobs);
        std::map<int64_t, std::vector<Annotation>> ann;
        if (okJobs)
        {
            int n = 0;
            for (auto &j : jobs)
                if (j.status == "completed" && j.conclusion != "success" && j.conclusion != "skipped" && n++ < 30)
                {
                    std::vector<Annotation> a;
                    std::string e;
                    if (gh.annotations(run.repo, j.id, a, e))
                        ann[j.id] = std::move(a);
                }
        }
        gApp->post([this, alive, okRun, fresh, okJobs, jobs, ann, errRun, errJobs] {
            if (!*alive)
                return;
            loading_ = false;
            if (okRun)
                run_ = fresh;
            if (okJobs)
            {
                jobs_ = jobs;
                ann_ = ann;
                error_.clear();
            }
            else
                error_ = errJobs.empty() ? errRun : errJobs;
            rebuildRows();
            redraw();
        });
    });
}

void RunWindow::rebuildRows()
{
    rows_.clear();
    for (int j = 0; j < (int) jobs_.size(); ++j)
    {
        JobRow jr;
        jr.kind = JobRow::JobKind;
        jr.job = j;
        rows_.push_back(jr);
        auto it = ann_.find(jobs_[j].id);
        if (it != ann_.end())
            for (int a = 0; a < (int) it->second.size(); ++a)
            {
                JobRow ar;
                ar.kind = JobRow::AnnKind;
                ar.job = j;
                ar.ann = a;
                rows_.push_back(ar);
            }
        for (int s = 0; s < (int) jobs_[j].steps.size(); ++s)
        {
            JobRow sr;
            sr.kind = JobRow::StepKind;
            sr.job = j;
            sr.step = s;
            rows_.push_back(sr);
        }
    }
    if (rows_.empty() || !error_.empty())
    {
        JobRow m;
        m.text = !error_.empty() ? L("Error: ", "Ошибка: ") + error_ : loading_ ? L("Loading…", "Загрузка…") : L("No jobs", "Нет job");
        rows_.insert(rows_.begin(), m);
    }
    if (list_)
    {
        list_->setRange((short) rows_.size());
        if (list_->focused >= list_->range)
            list_->focusItemNum(std::max(0, list_->range - 1));
    }
}

void RunWindow::openRow(int i)
{
    if (i < 0 || i >= (int) rows_.size())
        return;
    const JobRow &row = rows_[i];
    if (row.job < 0)
        return;
    const Job &job = jobs_[row.job];
    std::string jump;
    if (row.kind == JobRow::StepKind)
        jump = job.steps[row.step].name;
    else if (row.kind == JobRow::AnnKind || isFailure(job.conclusion))
        jump = "#error";
    std::string title = run_.repo.substr(run_.repo.find('/') + 1) + " · " + job.name;
    gApp->openLog(run_.repo, job.id, title, jump);
}

void RunWindow::action(RunAction a)
{
    auto alive = alive_;
    gApp->runAction(a, {run_}, [this, alive, a](bool ok) {
        if (!*alive || !ok)
            return;
        if (a == RunAction::Delete)
            close();
        else
            reload();
    });
}

void RunWindow::handleEvent(TEvent &ev)
{
    TWindow::handleEvent(ev);
    if (ev.what == evBroadcast && ev.message.command == cmTick)
    {
        if (info_)
            info_->drawView();
        if (list_)
            list_->drawView();
        return;
    }
    if (ev.what == evBroadcast && ev.message.command == cmTimerExpired && timer_ &&
        ev.message.infoPtr == (void *) timer_)
    {
        bool active = run_.status != "completed";
        for (auto &j : jobs_)
            if (j.status != "completed")
                active = true;
        if (active)
            reload();
        return;
    }
    if (ev.what == evKeyDown)
    {
        if (ev.keyDown.keyCode == kbEsc)
            close();
        else if (ev.keyDown.keyCode == kbDel)
            action(RunAction::Delete);
        else if (keyIs(ev, "r", "к"))
            action(RunAction::Rerun);
        else if (keyIs(ev, "f", "а"))
            action(RunAction::RerunFailed);
        else if (keyIs(ev, "c", "с"))
            action(RunAction::Cancel);
        else if (keyIs(ev, "o", "щ"))
            gApp->openUrl(run_.url);
        else
            return;
        clearEvent(ev);
        return;
    }
    if (ev.what != evCommand)
        return;
    switch (ev.message.command)
    {
    case cmOpenRun:
    case cmShowLog:
        if (list_)
        {
            int f = list_->focused;
            if (f >= 0 && f < (int) rows_.size() && rows_[f].job >= 0)
                openRow(f);
            else
                gApp->openFirstLog(run_);
        }
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
    case cmDeleteRun:
        action(RunAction::Delete);
        break;
    case cmDeleteLogs:
        action(RunAction::DeleteLogs);
        break;
    case cmRefresh:
        reload();
        break;
    case cmOpenBrowser:
        gApp->openUrl(run_.url);
        break;
    default:
        return;
    }
    clearEvent(ev);
}
