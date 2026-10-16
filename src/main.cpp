#define Uses_TObject
#include "github.h"
#include "log_window.h"
#include "poller.h"
#include "run_window.h"
#include "runs_window.h"
#include "ui.h"
#include "util.h"

#include <curl/curl.h>

#include <cstdlib>
#include <deque>
#include <mutex>
#include <thread>

namespace
{

const char *actionName(RunAction a)
{
    switch (a)
    {
    case RunAction::Rerun:
        return "Перезапустить";
    case RunAction::RerunFailed:
        return "Перезапустить упавшие job в";
    case RunAction::Cancel:
        return "Отменить";
    case RunAction::Delete:
        return "Удалить";
    case RunAction::DeleteLogs:
        return "Удалить логи";
    }
    return "";
}

bool eligible(RunAction a, const Run &r)
{
    bool done = r.status == "completed";
    switch (a)
    {
    case RunAction::Rerun:
    case RunAction::Delete:
    case RunAction::DeleteLogs:
        return done;
    case RunAction::RerunFailed:
        return done && (isFailure(r.conclusion) || r.conclusion == "cancelled");
    case RunAction::Cancel:
        return !done;
    }
    return false;
}

// 1 запуск, 2 запуска, 5 запусков
std::string runsWord(size_t n)
{
    size_t n100 = n % 100, n10 = n % 10;
    const char *w = (n100 >= 11 && n100 <= 14) ? "запусков" : n10 == 1 ? "запуск" : (n10 >= 2 && n10 <= 4) ? "запуска" : "запусков";
    return std::to_string(n) + " " + w;
}

} // namespace

class GhApp : public TApplication, public App
{
public:
    GhApp(GitHub &gh, Poller &poller, std::string startupError);

    static TMenuBar *initMenuBar(TRect r);
    static TStatusLine *initStatusLine(TRect r);

    void idle() override;
    void handleEvent(TEvent &ev) override;

    GitHub &gh() override { return gh_; }
    Poller &poller() override { return poller_; }
    void post(std::function<void()> fn) override;
    void async(std::function<void()> fn) override;
    void openRun(const Run &run) override;
    void openFirstLog(const Run &run) override;
    void openLog(const std::string &repo, int64_t jobId, const std::string &title, const std::string &jump) override;
    void runAction(RunAction a, std::vector<Run> runs, std::function<void(bool)> onDone) override;

private:
    GitHub &gh_;
    Poller &poller_;
    RunsWindow *runs_;
    std::mutex qm_;
    std::deque<std::function<void()>> queue_;
    uint64_t seenVersion_ = 0;
    time_t lastTick_ = 0;
};

GhApp::GhApp(GitHub &gh, Poller &poller, std::string startupError) :
    TProgInit(&GhApp::initStatusLine, &GhApp::initMenuBar, &TApplication::initDeskTop),
    gh_(gh),
    poller_(poller)
{
    // Окна обращаются к gApp уже при вставке (отрисовка)
    gApp = this;
    runs_ = new RunsWindow(deskTop->getExtent());
    deskTop->insert(runs_);
    if (!startupError.empty())
        post([startupError] { msgBox(startupError, mfError | mfOKButton); });
}

TMenuBar *GhApp::initMenuBar(TRect r)
{
    r.b.y = r.a.y + 1;
    return new TMenuBar(r,
        *new TSubMenu("~Ф~айл", kbAltF) +
            *new TMenuItem("~О~бновить всё", cmRefresh, kbCtrlR, hcNoContext, "Ctrl-R") +
            newLine() +
            *new TMenuItem("~В~ыход", cmQuit, kbAltX, hcNoContext, "Alt-X") +
        *new TSubMenu("~В~ид", kbAltV) +
            *new TMenuItem("~В~се", cmFilterAll, kbNoKey, hcNoContext, "1") +
            *new TMenuItem("~А~ктивные", cmFilterActive, kbNoKey, hcNoContext, "2") +
            *new TMenuItem("~У~павшие", cmFilterFailed, kbNoKey, hcNoContext, "3") +
            *new TMenuItem("У~с~пешные", cmFilterSuccess, kbNoKey, hcNoContext, "4") +
            *new TMenuItem("~П~рочие", cmFilterOther, kbNoKey, hcNoContext, "5") +
            newLine() +
            *new TMenuItem("~П~оиск…", cmSearch, kbCtrlF, hcNoContext, "/") +
            *new TMenuItem("~Т~олько последние", cmLatestOnly, kbNoKey, hcNoContext, "L") +
            *new TMenuItem("С~о~ртировка", cmSortToggle, kbNoKey, hcNoContext, "S") +
        *new TSubMenu("~З~апуск", kbAltR) +
            *new TMenuItem("~О~ткрыть", cmOpenRun, kbNoKey, hcNoContext, "Enter") +
            *new TMenuItem("~Л~ог", cmShowLog, kbF3, hcNoContext, "F3") +
            newLine() +
            *new TMenuItem("~П~ерезапустить", cmRerun, kbF5, hcNoContext, "F5") +
            *new TMenuItem("Перезапустить ~у~павшие", cmRerunFailed, kbF6, hcNoContext, "F6") +
            *new TMenuItem("~О~тменить", cmCancelRun, kbF7, hcNoContext, "F7") +
            *new TMenuItem("~У~далить", cmDeleteRun, kbF8, hcNoContext, "F8") +
            *new TMenuItem("Удалить ~л~оги", cmDeleteLogs, kbShiftF8, hcNoContext, "Shift-F8") +
            newLine() +
            *new TMenuItem("Отметить ~в~се", cmMarkAll, kbNoKey, hcNoContext, "+") +
            *new TMenuItem("~С~нять отметки", cmUnmarkAll, kbNoKey, hcNoContext, "-") +
        *new TSubMenu("~?~", kbAltH) +
            *new TMenuItem("~К~лавиши", cmKeysHelp, kbF1, hcNoContext, "F1") +
            *new TMenuItem("~О~ программе", cmAbout, kbNoKey));
}

TStatusLine *GhApp::initStatusLine(TRect r)
{
    r.a.y = r.b.y - 1;
    return new TStatusLine(r,
        *new TStatusDef(0, 0xFFFF) +
            *new TStatusItem("~F1~ Справка", kbF1, cmKeysHelp) +
            *new TStatusItem("~F3~ Лог", kbF3, cmShowLog) +
            *new TStatusItem("~F5~ Перезапуск", kbF5, cmRerun) +
            *new TStatusItem("~F6~ Упавшие", kbF6, cmRerunFailed) +
            *new TStatusItem("~F7~ Отмена", kbF7, cmCancelRun) +
            *new TStatusItem("~F8~ Удалить", kbF8, cmDeleteRun) +
            *new TStatusItem("~F10~ Меню", kbF10, cmMenu) +
            *new TStatusItem("~Alt-X~ Выход", kbAltX, cmQuit) +
            *new TStatusItem(0, kbShiftF8, cmDeleteLogs) +
            *new TStatusItem(0, kbF9, cmMenu) +
            *new TStatusItem(0, kbCtrlR, cmRefresh) +
            *new TStatusItem(0, kbCtrlF, cmSearch));
}

void GhApp::post(std::function<void()> fn)
{
    {
        std::lock_guard<std::mutex> lock(qm_);
        queue_.push_back(std::move(fn));
    }
    TEventQueue::wakeUp();
}

void GhApp::async(std::function<void()> fn)
{
    std::thread(std::move(fn)).detach();
}

void GhApp::idle()
{
    TApplication::idle();

    std::deque<std::function<void()>> q;
    {
        std::lock_guard<std::mutex> lock(qm_);
        q.swap(queue_);
    }
    for (auto &f : q)
        f();

    uint64_t v = poller_.version();
    if (v != seenVersion_)
    {
        seenVersion_ = v;
        runs_->updateData(poller_.snapshot());
    }

    // Раз в секунду перерисовать относительное время и индикаторы
    time_t now = time(nullptr);
    if (now != lastTick_)
    {
        lastTick_ = now;
        message(deskTop, evBroadcast, cmTick, nullptr);
    }
}

void GhApp::handleEvent(TEvent &ev)
{
    TApplication::handleEvent(ev);
    if (ev.what != evCommand)
        return;
    switch (ev.message.command)
    {
    case cmKeysHelp:
        showHelp();
        break;
    case cmRefresh:
        poller_.refreshAll();
        break;
    case cmAbout:
        msgBox("ghatv — панель GitHub Actions\nна Turbo Vision (magiblot/tvision)", mfInformation | mfOKButton);
        break;
    default:
        return;
    }
    clearEvent(ev);
}

void GhApp::openRun(const Run &run)
{
    deskTop->insert(new RunWindow(deskTop->getExtent(), run));
}

void GhApp::openLog(const std::string &repo, int64_t jobId, const std::string &title, const std::string &jump)
{
    deskTop->insert(new LogWindow(deskTop->getExtent(), repo, jobId, title, jump));
}

void GhApp::openFirstLog(const Run &run)
{
    async([this, run] {
        std::vector<Job> jobs;
        std::string err;
        bool ok = gh_.listJobs(run.repo, run.id, jobs, err);
        post([this, run, ok, jobs, err] {
            if (!ok)
            {
                msgBox("Не удалось получить список job: " + err, mfError | mfOKButton);
                return;
            }
            if (jobs.empty())
            {
                msgBox("В запуске нет job", mfInformation | mfOKButton);
                return;
            }
            const Job *job = &jobs.front();
            for (auto &j : jobs)
                if (isFailure(j.conclusion))
                {
                    job = &j;
                    break;
                }
            std::string title = run.repo.substr(run.repo.find('/') + 1) + " · " + job->name;
            openLog(run.repo, job->id, title, isFailure(job->conclusion) ? "#error" : "");
        });
    });
}

void GhApp::runAction(RunAction a, std::vector<Run> runs, std::function<void(bool)> onDone)
{
    std::vector<Run> ok;
    for (auto &r : runs)
        if (eligible(a, r))
            ok.push_back(r);
    if (ok.empty())
    {
        msgBox("Действие не применимо к выбранным запускам", mfInformation | mfOKButton);
        return;
    }
    size_t skipped = runs.size() - ok.size();

    bool confirm = a == RunAction::Delete || a == RunAction::DeleteLogs || a == RunAction::Cancel || ok.size() > 1;
    if (confirm)
    {
        std::string text = std::string(actionName(a)) + " ";
        if (ok.size() == 1)
            text += "запуск\n" + ok[0].workflow + " #" + std::to_string(ok[0].runNumber) + "\n(" + ok[0].repo + ")?";
        else
            text += runsWord(ok.size()) + "?";
        if (skipped)
            text += "\nНеподходящих пропущено: " + std::to_string(skipped);
        if (msgBox(text, mfConfirmation | mfYesButton | mfNoButton) != cmYes)
            return;
    }

    async([this, a, ok, onDone] {
        std::vector<int64_t> done;
        std::vector<std::string> errors;
        for (auto &r : ok)
        {
            std::string err;
            bool res = false;
            switch (a)
            {
            case RunAction::Rerun:
                res = gh_.rerun(r.repo, r.id, err);
                break;
            case RunAction::RerunFailed:
                res = gh_.rerunFailed(r.repo, r.id, err);
                break;
            case RunAction::Cancel:
                res = gh_.cancelRun(r.repo, r.id, err);
                break;
            case RunAction::Delete:
                res = gh_.deleteRun(r.repo, r.id, err);
                break;
            case RunAction::DeleteLogs:
                res = gh_.deleteLogs(r.repo, r.id, err);
                break;
            }
            if (res)
                done.push_back(r.id);
            else
                errors.push_back(r.repo + " #" + std::to_string(r.runNumber) + ": " + err);
        }
        post([this, a, ok, done, errors, onDone] {
            if (a == RunAction::Delete && !done.empty())
                poller_.removeRuns(done);
            for (auto &r : ok)
                poller_.refreshRepo(r.repo);
            if (!errors.empty())
            {
                std::string text = "Ошибок: " + std::to_string(errors.size());
                for (size_t i = 0; i < errors.size() && i < 4; ++i)
                    text += "\n" + errors[i];
                msgBox(text, mfError | mfOKButton);
            }
            if (onDone)
                onDone(errors.empty());
        });
    });
}

int main()
{

    curl_global_init(CURL_GLOBAL_DEFAULT);

    std::string token = util::env("GH_TOKEN", util::env("GITHUB_TOKEN"));
    std::string startupError;
    if (token.empty())
        startupError = "Не задан токен.\nНеобходимо передать переменную окружения GH_TOKEN.";

    PollerConfig cfg;
    cfg.affiliation = util::env("GHATV_AFFILIATION", "owner");
    cfg.runsPerRepo = std::max(1, std::min(100, util::envInt("GHATV_RUNS_PER_REPO", 20)));
    cfg.interval = std::max(10, util::envInt("GHATV_INTERVAL", 60));
    cfg.activeInterval = std::max(3, util::envInt("GHATV_ACTIVE_INTERVAL", 10));
    cfg.threads = std::max(1, std::min(16, util::envInt("GHATV_THREADS", 6)));
    cfg.includeForks = util::envBool("GHATV_INCLUDE_FORKS", false);
    cfg.includeArchived = util::envBool("GHATV_INCLUDE_ARCHIVED", false);
    cfg.owners = util::splitList(util::env("GHATV_OWNERS"));
    cfg.include = util::splitList(util::env("GHATV_REPOS"));
    cfg.exclude = util::splitList(util::env("GHATV_EXCLUDE"));

    GitHub gh(token, util::env("GITHUB_API_URL", "https://api.github.com"));
    Poller poller(gh, cfg, [] { TEventQueue::wakeUp(); });

    auto *app = new GhApp(gh, poller, startupError);
    gApp = app;
    if (!token.empty())
        poller.start();
    app->run();
    TObject::destroy(app);

    // Фоновые потоки могут ждать ответа сети: не ждать их завершения
    std::_Exit(0);
}
