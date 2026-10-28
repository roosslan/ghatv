#include "deployments_window.h"
#include "github.h"
#include "util.h"

#include <set>

namespace
{

enum Col { ColWhen, ColStatus, ColRepo, ColEnv, ColRef, ColCreator, ColDescr };

std::vector<ColumnDef> deploymentColumns()
{
    std::vector<ColumnDef> c(7);
    c[ColWhen] = {"When|Когда", 14, 0, 0, false, 0, true};
    c[ColStatus] = {"Status|Статус", 14};
    c[ColRepo] = {"Repository|Репозиторий", 0, 26, 30};
    c[ColEnv] = {"Environment|Окружение", 0, 18, 24};
    c[ColRef] = {"Ref|Ref", 0, 16, 24};
    c[ColCreator] = {"Creator|Автор", 14, 0, 0, false, 110};
    c[ColDescr] = {"Description|Описание"};
    return c;
}

int rank(const Deployment &d)
{
    switch (categoryOf(d))
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

std::string shortSha(const std::string &sha)
{
    return sha.substr(0, 7);
}

} // namespace

StatusLook deploymentLook(const std::string &state)
{
    if (state == "success")
        return lookOf("completed", "success");
    if (state == "failure")
        return lookOf("completed", "failure");
    if (state == "error")
        return {"✘", L("error", "ошибка"), 0xC};
    if (state == "inactive")
        return {"○", L("inactive", "неактивно"), 0x7};
    if (state == "in_progress")
        return lookOf("in_progress", "");
    if (state == "queued")
        return lookOf("queued", "");
    if (state == "pending")
        return lookOf("pending", "");
    if (state.empty())
        return {"·", L("no status", "нет статуса"), 0x7};
    return {"?", state, 0x7};
}

std::string deploymentUrl(const Deployment &d)
{
    if (!d.last.logUrl.empty())
        return d.last.logUrl;
    if (!d.last.environmentUrl.empty())
        return d.last.environmentUrl;
    return "https://github.com/" + d.repo + "/deployments";
}

// ---------------------------------------------------------------- DeploymentsWindow

DeploymentsWindow::DeploymentsWindow(const TRect &r) :
    TWindowInit(&TWindow::initFrame),
    TableWindow(r, "Deployments|Развёртывания", 4, deploymentColumns())
{
}

void DeploymentsWindow::updateData(Poller::State st)
{
    st_ = std::move(st);
    std::set<std::string> owners;
    for (auto &d : st_.deployments)
        owners.insert(d.repo.substr(0, d.repo.find('/')));
    multiOwner_ = owners.size() > 1;
    refresh();
}

bool DeploymentsWindow::accept(int idx) const
{
    return filterMatch(filter_, categoryOf(st_.deployments[idx]));
}

Cell DeploymentsWindow::cell(int idx, int col) const
{
    const Deployment &d = st_.deployments[idx];
    switch (col)
    {
    case ColWhen:
        return {util::localTime(d.createdAt)};
    case ColStatus:
        return statusCell(deploymentLook(d.last.state));
    case ColRepo:
        return {repoLabel(d.repo, multiOwner_)};
    case ColEnv:
        return {d.environment};
    case ColRef:
        return {d.ref == d.sha ? shortSha(d.sha) : d.ref};
    case ColCreator:
        return {d.creator};
    case ColDescr:
        return {!d.last.description.empty() ? d.last.description : d.description};
    }
    return {};
}

int DeploymentsWindow::compare(int a, int b, int col) const
{
    const Deployment &x = st_.deployments[a], &y = st_.deployments[b];
    switch (col)
    {
    case ColWhen:
        return cmpNum(x.createdAt, y.createdAt);
    case ColStatus:
        return cmpNum(rank(x), rank(y));
    case ColRepo:
        return cmpText(x.repo, y.repo);
    case ColEnv:
        return cmpText(x.environment, y.environment);
    case ColRef:
        return cmpText(x.ref, y.ref);
    case ColCreator:
        return cmpText(x.creator, y.creator);
    case ColDescr:
        return cmpText(x.description, y.description);
    }
    return 0;
}

std::string DeploymentsWindow::infoText() const
{
    const char *names[] = {L("all", "все"), L("active", "активные"), L("failed", "упавшие"),
                           L("successful", "успешные"), L("other", "прочие")};
    std::string s;
    if (!st_.reposLoaded && st_.error.empty())
        s = L("Loading repository list…", "Загрузка списка репозиториев…");
    else
        s = L("Deployments: ", "Развёртываний: ") + std::to_string(st_.deployments.size()) + L("  shown: ", "  показано: ") +
            std::to_string(rowCount());
    s += L(" │ filter: ", " │ фильтр: ");
    s += names[(int) filter_];
    s += " │ " + viewState();
    s += L(" │ Enter — status history, o — browser", " │ Enter — история статусов, o — браузер");
    if (!st_.error.empty())
        s += " │ ⚠ " + st_.error;
    return s;
}

void DeploymentsWindow::openRow(int idx)
{
    gApp->openDeployment(st_.deployments[idx]);
}

bool DeploymentsWindow::rowKey(TEvent &ev)
{
    int f = filterKey(ev);
    if (f < 0)
        return false;
    filter_ = (RunFilter) f;
    refresh();
    return true;
}

bool DeploymentsWindow::command(ushort cmd)
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
    case cmFilterSuccess:
    case cmFilterOther:
        filter_ = (RunFilter) (cmd - cmFilterAll);
        refresh();
        return true;
    default:
        return false;
    }
}

// ---------------------------------------------------------------- DeploymentWindow

namespace
{

void addLine(std::vector<LogLine> &out, const std::string &text, uchar color)
{
    LogLine l;
    l.text = text;
    l.color = color;
    out.push_back(std::move(l));
}

std::vector<LogLine> renderHistory(const Deployment &d, const std::vector<DeploymentStatus> &sts, int &maxW)
{
    std::vector<LogLine> out;
    addLine(out, d.repo + " · " + d.environment, 0x0F);
    addLine(out,
            std::string("ref ") + d.ref + " (" + shortSha(d.sha) + ") · " + L("task ", "задача ") + d.task + " · " +
                L("by ", "автор ") + d.creator + " · " + util::localTime(d.createdAt),
            0x0B);
    if (!d.description.empty())
        addLine(out, d.description, 0x07);
    addLine(out, "", 0x07);
    if (sts.empty())
        addLine(out, L("No statuses", "Статусов нет"), 0x08);
    for (auto &s : sts)
    {
        StatusLook look = deploymentLook(s.state);
        std::string line = util::localTime(s.createdAt) + "  " + look.glyph + " " + look.label;
        if (!s.creator.empty())
            line += "  · " + s.creator;
        if (!s.description.empty())
            line += "  · " + s.description;
        addLine(out, line, look.color);
        if (!s.logUrl.empty())
            addLine(out, std::string("    ") + L("log: ", "лог: ") + s.logUrl, 0x08);
        if (!s.environmentUrl.empty())
            addLine(out, std::string("    ") + L("environment: ", "окружение: ") + s.environmentUrl, 0x08);
    }
    maxW = 0;
    for (auto &l : out)
        maxW = std::max(maxW, (int) TText::width(l.text));
    return out;
}

} // namespace

DeploymentWindow::DeploymentWindow(const TRect &r, const Deployment &d) :
    TWindowInit(&TWindow::initFrame),
    TWindow(r, (d.repo.substr(d.repo.find('/') + 1) + " · " + d.environment).c_str(), wnNoNumber),
    dep_(d),
    alive_(std::make_shared<bool>(true))
{
    flags = wfMove | wfGrow | wfZoom | wfClose;
    growMode = gfGrowHiX | gfGrowHiY;
    palette = wpGrayWindow;

    TRect e = getExtent();
    e.grow(-1, -1);
    TScrollBar *hs = standardScrollBar(sbHorizontal | sbHandleKeyboard);
    TScrollBar *vs = standardScrollBar(sbVertical | sbHandleKeyboard);
    view_ = new LogView(TRect(e.a.x, e.a.y, e.b.x, e.b.y - 1), hs, vs);
    view_->setHint(L("/ search  o browser  Esc/Backspace — back", "/ поиск  o браузер  Esc/Backspace — назад"));
    insert(view_);
    info_ = new LogInfo(TRect(e.a.x, e.b.y - 1, e.b.x, e.b.y), view_);
    insert(info_);
    view_->select();
    load();
}

void DeploymentWindow::shutDown()
{
    *alive_ = false;
    view_ = nullptr;
    info_ = nullptr;
    TWindow::shutDown();
}

void DeploymentWindow::load()
{
    auto alive = alive_;
    Deployment d = dep_;
    view_->setMessage(L("Loading…", "Загрузка…"));
    gApp->async([this, alive, d] {
        std::vector<DeploymentStatus> sts;
        std::string err;
        bool ok = gApp->gh().deploymentStatuses(d.repo, d.id, 100, sts, err);
        int maxW = 0;
        auto lines = ok ? std::make_shared<std::vector<LogLine>>(renderHistory(d, sts, maxW)) : nullptr;
        gApp->post([this, alive, ok, lines, maxW, err] {
            if (!*alive || !view_)
                return;
            if (!ok)
                view_->setMessage(L("Failed to load: ", "Не удалось загрузить: ") + err);
            else
                view_->setLines(std::move(*lines), maxW, "");
            redraw();
        });
    });
}

void DeploymentWindow::handleEvent(TEvent &ev)
{
    TWindow::handleEvent(ev);
    if (ev.what == evKeyDown && ev.keyDown.keyCode == kbEsc)
    {
        close();
        clearEvent(ev);
        return;
    }
    if (ev.what == evCommand)
    {
        if (ev.message.command == cmSearch && view_)
            view_->search();
        else if (ev.message.command == cmRefresh)
            load();
        else if (ev.message.command == cmOpenBrowser)
            gApp->openUrl(deploymentUrl(dep_));
        else
            return;
        clearEvent(ev);
        return;
    }
    if (info_)
        info_->drawView();
}
