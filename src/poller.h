#pragma once

#include "github.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

struct PollerConfig
{
    std::string affiliation = "owner";
    int runsPerRepo = 20;
    int interval = 60;        // секунд между опросами «тихого» репозитория
    int activeInterval = 10;  // секунд между опросами репозитория с активными запусками
    int threads = 6;
    bool includeForks = false;
    bool includeArchived = false;
    std::vector<std::string> owners;   // если не пусто: только эти владельцы
    std::vector<std::string> include;  // если не пусто: только эти репозитории (owner/name)
    std::vector<std::string> exclude;  // исключить репозитории (owner/name или name)
    bool pulls = true;                 // опрашивать открытые pull request'ы
    bool issues = true;                // опрашивать открытые issues
};

// Фоновый опрос запусков по всем репозиториям
class Poller
{
public:
    struct State
    {
        std::vector<Run> runs;
        std::vector<PullRequest> pulls;
        std::vector<Issue> issues;
        std::string login;
        int repoCount = 0;
        bool reposLoaded = false;
        bool busy = false;
        time_t lastSync = 0;
        std::string error;
    };

    Poller(GitHub &gh, PollerConfig cfg, std::function<void()> notify);
    ~Poller();

    void start();
    void stop();

    uint64_t version() const { return version_.load(); }
    State snapshot() const;

    void refreshAll();
    void refreshRepo(const std::string &repo);
    void removeRuns(const std::vector<int64_t> &ids);

private:
    void loop();
    void loadRepos();
    void pollRepo(const std::string &repo, bool force);
    bool repoAllowed(const RepoInfo &r) const;
    void bump();
    void pollForeignIssues();

    GitHub &gh_;
    PollerConfig cfg_;
    std::function<void()> notify_;

    mutable std::mutex m_;
    std::condition_variable cv_;
    std::vector<std::string> repos_;
    std::map<std::string, std::vector<Run>> runs_;
    std::map<std::string, time_t> lastPoll_;
    std::map<std::string, std::vector<PullRequest>> pulls_;
    std::map<std::string, time_t> lastPullPoll_;
    std::map<std::string, std::vector<Issue>> issues_;
    std::vector<Issue> foreignIssues_;
    time_t lastSearch_ = 0;
    std::string login_;
    std::set<std::string> forced_;
    std::map<std::string, std::string> errors_;
    bool forceAll_ = false;
    bool stop_ = false;
    bool reposLoaded_ = false;
    bool busy_ = false;
    time_t reposAt_ = 0;
    time_t lastSync_ = 0;
    std::string reposError_;

    std::atomic<uint64_t> version_{1};
    std::thread thread_;
};
