#include "runs_window.h"
#include "github.h"
#include "util.h"

#include <algorithm>
#include <cstring>

namespace
{

int clampi(int v, int lo, int hi)
{
    return std::max(lo, std::min(v, hi));
}

const int kLead = 3;  // отметка + значок статуса + пробел

// Пишет набор столбцов с разделителями, возвращает x после последнего
struct RowWriter
{
    TDrawBuffer &b;
    int x = 0;
    TColorAttr sep;
    void col(int width, const std::string &text, TColorAttr attr, bool right = false)
    {
        if (width <= 0)
            return;
        putCell(b, x, width, text, attr, right);
        x += width;
        b.moveStr(x, "│", sep);
        x += 1;
    }
};

} // namespace

RunCols RunCols::compute(int width)
{
    RunCols c{};
    c.status = 11;
    c.dur = 8;
    c.age = 7;
    c.event = width >= 120 ? 12 : 0;
    int seps = c.event ? 7 : 6;
    int avail = width - kLead - c.status - c.dur - c.age - c.event - seps;
    if (avail < 12)
        avail = 12;
    c.repo = clampi(avail * 26 / 100, 6, 30);
    c.workflow = clampi(avail * 22 / 100, 6, 26);
    c.branch = clampi(avail * 14 / 100, 4, 20);
    c.title = std::max(0, avail - c.repo - c.workflow - c.branch);
    return c;
}

// ---------------------------------------------------------------- RunsHeader

RunsHeader::RunsHeader(const TRect &r) :
    TView(r)
{
    growMode = gfGrowHiX;
}

void RunsHeader::draw()
{
    TDrawBuffer b;
    b.moveChar(0, ' ', TColorAttr(clr::Header), size.x);
    RunCols c = RunCols::compute(size.x);
    RowWriter w{b, kLead, TColorAttr(clr::Separator)};
    TColorAttr a(clr::Header);
    w.col(c.status, "Статус", a);
    w.col(c.repo, "Репозиторий", a);
    w.col(c.workflow, "Workflow", a);
    w.col(c.branch, "Ветка", a);
    w.col(c.event, "Событие", a);
    w.col(c.title, "Заголовок", a);
    w.col(c.dur, "Длит.", a, true);
    putCell(b, w.x, c.age, "Когда", a, true);
    writeLine(0, 0, size.x, 1, b);
}

// ---------------------------------------------------------------- RunsInfo

RunsInfo::RunsInfo(const TRect &r, RunsWindow *w) :
    TView(r),
    w_(w)
{
    growMode = gfGrowLoY | gfGrowHiX | gfGrowHiY;
}

void RunsInfo::draw()
{
    TDrawBuffer b;
    putCell(b, 0, size.x, " " + w_->infoText(), TColorAttr(clr::Info));
    writeLine(0, 0, size.x, 1, b);
}

// ---------------------------------------------------------------- RunsList

RunsList::RunsList(const TRect &r, TScrollBar *vs, RunsWindow *w) :
    TListViewer(r, 1, nullptr, vs),
    w_(w)
{
    growMode = gfGrowHiX | gfGrowHiY;
    setRange(0);
}

void RunsList::getText(char *dest, short item, short maxLen)
{
    const Run *r = w_->runAt(item);
    strncpy(dest, r ? r->workflow.c_str() : "", maxLen);
    dest[maxLen - 1] = '\0';
}

void RunsList::draw()
{
    time_t now = time(nullptr);
    RunCols c = RunCols::compute(size.x);
    bool active = (state & (sfSelected | sfActive)) == (sfSelected | sfActive);

    for (int y = 0; y < size.y; ++y)
    {
        TDrawBuffer b;
        int item = topItem + y;
        const Run *r = item < range ? w_->runAt(item) : nullptr;
        if (!r)
        {
            b.moveChar(0, ' ', TColorAttr(clr::Normal), size.x);
            if (range == 0 && y == size.y / 2)
            {
                std::string msg = "Нет запусков для отображения";
                putCell(b, 0, size.x, std::string((size.x - (int) TText::width(msg)) / 2, ' ') + msg,
                        TColorAttr(clr::Dim));
            }
            writeLine(0, y, size.x, 1, b);
            continue;
        }

        bool isFocused = item == focused;
        bool marked = w_->isMarked(r->id);
        uchar base = isFocused ? (active ? clr::Focus : clr::FocusDim) : clr::Normal;
        TColorAttr text = marked ? withFore(base, clr::Mark) : TColorAttr(base);
        TColorAttr sep = isFocused ? TColorAttr(base) : TColorAttr(clr::Separator);
        StatusLook look = lookOf(r->status, r->conclusion);
        TColorAttr stAttr = isFocused ? text : withFore(base, look.color);

        b.moveChar(0, ' ', TColorAttr(base), size.x);
        b.moveStr(0, marked ? "√" : " ", text);
        b.moveStr(1, look.glyph, stAttr);

        std::string repo = w_->multiOwner() ? r->repo : r->repo.substr(r->repo.find('/') + 1);
        time_t start = r->startedAt ? r->startedAt : r->createdAt;
        time_t end = r->status == "completed" ? r->updatedAt : now;
        std::string title = "#" + std::to_string(r->runNumber) + " " + r->title;

        RowWriter w{b, kLead, sep};
        w.col(c.status, look.label, stAttr);
        w.col(c.repo, repo, text);
        w.col(c.workflow, r->workflow, text);
        w.col(c.branch, r->branch, text);
        w.col(c.event, r->event, text);
        w.col(c.title, title, text);
        w.col(c.dur, util::duration((long) (end - start)), text, true);
        putCell(b, w.x, c.age, util::ago(r->createdAt, now), text, true);
        writeLine(0, y, size.x, 1, b);
    }
}

void RunsList::selectItem(short item)
{
    if (const Run *r = w_->runAt(item))
        gApp->openRun(*r);
}

void RunsList::handleEvent(TEvent &ev)
{
    if (ev.what == evKeyDown)
    {
        bool handled = true;
        switch (ev.keyDown.keyCode)
        {
        case kbEnter:
            if (const Run *r = w_->current())
                gApp->openRun(*r);
            break;
        case kbIns:
            w_->toggleMark(focused);
            if (focused + 1 < range)
                focusItemNum(focused + 1);
            drawView();
            break;
        case kbDel:
            w_->doAction(RunAction::Delete);
            break;
        default:
            handled = false;
        }
        if (!handled)
        {
            handled = true;
            if (keyIs(ev, "1", nullptr))
                w_->setFilter(RunFilter::All);
            else if (keyIs(ev, "2", nullptr))
                w_->setFilter(RunFilter::Active);
            else if (keyIs(ev, "3", nullptr))
                w_->setFilter(RunFilter::Failed);
            else if (keyIs(ev, "4", nullptr))
                w_->setFilter(RunFilter::Success);
            else if (keyIs(ev, "5", nullptr))
                w_->setFilter(RunFilter::Other);
            else if (keyIs(ev, "r", "к"))
                w_->doAction(RunAction::Rerun);
            else if (keyIs(ev, "f", "а"))
                w_->doAction(RunAction::RerunFailed);
            else if (keyIs(ev, "c", "с"))
                w_->doAction(RunAction::Cancel);
            else if (keyIs(ev, "/", "."))
                message(owner, evCommand, cmSearch, nullptr);
            else if (keyIs(ev, "l", "д"))
                message(owner, evCommand, cmLatestOnly, nullptr);
            else if (keyIs(ev, "s", "ы"))
                message(owner, evCommand, cmSortToggle, nullptr);
            else if (keyIs(ev, "+", nullptr))
                message(owner, evCommand, cmMarkAll, nullptr);
            else if (keyIs(ev, "-", nullptr))
                message(owner, evCommand, cmUnmarkAll, nullptr);
            else
                handled = false;
        }
        if (handled)
        {
            clearEvent(ev);
            return;
        }
    }
    TListViewer::handleEvent(ev);
}

// ---------------------------------------------------------------- RunsWindow

RunsWindow::RunsWindow(const TRect &r) :
    TWindowInit(&TWindow::initFrame),
    TWindow(r, "GitHub Actions", wnNoNumber)
{
    flags = wfMove | wfGrow | wfZoom;
    growMode = gfGrowHiX | gfGrowHiY;

    TRect e = getExtent();
    e.grow(-1, -1);
    header_ = new RunsHeader(TRect(e.a.x, e.a.y, e.b.x, e.a.y + 1));
    insert(header_);
    TScrollBar *vs = standardScrollBar(sbVertical);
    list_ = new RunsList(TRect(e.a.x, e.a.y + 1, e.b.x, e.b.y - 1), vs, this);
    insert(list_);
    info_ = new RunsInfo(TRect(e.a.x, e.b.y - 1, e.b.x, e.b.y), this);
    insert(info_);
    list_->select();
}

const Run *RunsWindow::runAt(int i) const
{
    if (i < 0 || i >= (int) visible_.size())
        return nullptr;
    return &st_.runs[visible_[i]];
}

const Run *RunsWindow::current() const
{
    return runAt(list_->focused);
}

void RunsWindow::updateData(Poller::State st)
{
    int64_t curId = 0;
    if (const Run *r = current())
        curId = r->id;
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

    rebuild();
    if (curId)
        for (int i = 0; i < count(); ++i)
            if (runAt(i)->id == curId)
            {
                list_->focusItemNum(i);
                break;
            }
    redraw();
}

void RunsWindow::rebuild()
{
    visible_.clear();
    std::string q = util::lower(search_);
    for (int i = 0; i < (int) st_.runs.size(); ++i)
    {
        const Run &r = st_.runs[i];
        RunCategory cat = categoryOf(r);
        switch (filter_)
        {
        case RunFilter::Active:
            if (cat != RunCategory::Active)
                continue;
            break;
        case RunFilter::Failed:
            if (cat != RunCategory::Failed)
                continue;
            break;
        case RunFilter::Success:
            if (cat != RunCategory::Success)
                continue;
            break;
        case RunFilter::Other:
            if (cat != RunCategory::Other)
                continue;
            break;
        default:
            break;
        }
        if (!q.empty())
        {
            std::string hay = r.repo + "\n" + r.workflow + "\n" + r.branch + "\n" + r.title + "\n" + r.actor +
                              "\n" + r.event + "\n#" + std::to_string(r.runNumber);
            if (!util::containsNoCase(hay, q))
                continue;
        }
        visible_.push_back(i);
    }

    auto &runs = st_.runs;
    std::stable_sort(visible_.begin(), visible_.end(), [&](int a, int b) {
        if (sortByRepo_ && runs[a].repo != runs[b].repo)
            return runs[a].repo < runs[b].repo;
        return runs[a].createdAt > runs[b].createdAt;
    });

    if (latestOnly_)
    {
        std::set<std::string> seen;
        std::vector<int> v;
        for (int i : visible_)
        {
            const Run &r = runs[i];
            std::string key = r.repo + "\n" + std::to_string(r.workflowId) + "\n" + r.branch;
            if (seen.insert(key).second)
                v.push_back(i);
        }
        visible_.swap(v);
    }

    list_->setRange((short) std::min<size_t>(visible_.size(), 32000));
    if (list_->focused >= list_->range)
        list_->focusItemNum(std::max(0, list_->range - 1));
}

std::string RunsWindow::infoText() const
{
    static const char *names[] = {"все", "активные", "упавшие", "успешные", "прочие"};
    std::string s;
    if (!st_.reposLoaded && st_.error.empty())
        s = "Загрузка списка репозиториев…";
    else
        s = "Репозиториев: " + std::to_string(st_.repoCount) + "  запусков: " + std::to_string(st_.runs.size()) +
            "  показано: " + std::to_string(visible_.size());
    s += " │ фильтр: ";
    s += names[(int) filter_];
    if (latestOnly_)
        s += ", последние";
    if (sortByRepo_)
        s += ", по репозиторию";
    if (!search_.empty())
        s += " │ поиск: «" + search_ + "»";
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

void RunsWindow::tick()
{
    list_->drawView();
    info_->drawView();
}

void RunsWindow::toggleMark(int i)
{
    if (const Run *r = runAt(i))
    {
        if (!marked_.erase(r->id))
            marked_.insert(r->id);
        info_->drawView();
    }
}

void RunsWindow::setFilter(RunFilter f)
{
    filter_ = f;
    rebuild();
    redraw();
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
    else if (const Run *r = current())
        out.push_back(*r);
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

void RunsWindow::handleEvent(TEvent &ev)
{
    TWindow::handleEvent(ev);
    if (ev.what == evBroadcast && ev.message.command == cmTick)
    {
        tick();
        return;
    }
    if (ev.what != evCommand)
        return;
    switch (ev.message.command)
    {
    case cmOpenRun:
        if (const Run *r = current())
            gApp->openRun(*r);
        break;
    case cmShowLog:
        if (const Run *r = current())
            gApp->openFirstLog(*r);
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
        setFilter(RunFilter::All);
        break;
    case cmFilterActive:
        setFilter(RunFilter::Active);
        break;
    case cmFilterFailed:
        setFilter(RunFilter::Failed);
        break;
    case cmFilterSuccess:
        setFilter(RunFilter::Success);
        break;
    case cmFilterOther:
        setFilter(RunFilter::Other);
        break;
    case cmLatestOnly:
        latestOnly_ = !latestOnly_;
        rebuild();
        redraw();
        break;
    case cmSortToggle:
        sortByRepo_ = !sortByRepo_;
        rebuild();
        redraw();
        break;
    case cmMarkAll:
        for (int i = 0; i < count(); ++i)
            marked_.insert(runAt(i)->id);
        redraw();
        break;
    case cmUnmarkAll:
        marked_.clear();
        redraw();
        break;
    case cmSearch:
    {
        char buf[128];
        strncpy(buf, search_.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        if (inputBox("Поиск", "Текст (пусто — сбросить):", buf, sizeof(buf) - 1) != cmCancel)
        {
            search_ = buf;
            rebuild();
            redraw();
        }
        break;
    }
    default:
        return;
    }
    clearEvent(ev);
}
