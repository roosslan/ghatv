#include "poller.h"
#include "util.h"

#include <algorithm>

Poller::Poller(GitHub &gh, PollerConfig cfg, std::function<void()> notify) :
    gh_(gh),
    cfg_(std::move(cfg)),
    notify_(std::move(notify))
{
}

Poller::~Poller()
{
    stop();
}

void Poller::start()
{
    thread_ = std::thread([this] { loop(); });
}

void Poller::stop()
{
    {
        std::lock_guard<std::mutex> lock(m_);
        stop_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable())
        thread_.join();
}

void Poller::bump()
{
    ++version_;
    if (notify_)
        notify_();
}

Poller::State Poller::snapshot() const
{
    std::lock_guard<std::mutex> lock(m_);
    State s;
    for (auto &kv : runs_)
        s.runs.insert(s.runs.end(), kv.second.begin(), kv.second.end());
    s.repoCount = (int) repos_.size();
    s.reposLoaded = reposLoaded_;
    s.busy = busy_;
    s.lastSync = lastSync_;
    if (!reposError_.empty())
        s.error = reposError_;
    else if (!errors_.empty())
    {
        s.error = errors_.begin()->first + ": " + errors_.begin()->second;
        if (errors_.size() > 1)
            s.error += " (+" + std::to_string(errors_.size() - 1) + ")";
    }
    return s;
}

void Poller::refreshAll()
{
    {
        std::lock_guard<std::mutex> lock(m_);
        forceAll_ = true;
        reposAt_ = 0;  // заодно перечитать список репозиториев
    }
    cv_.notify_all();
}

void Poller::refreshRepo(const std::string &repo)
{
    gh_.invalidate(repo);
    {
        std::lock_guard<std::mutex> lock(m_);
        forced_.insert(repo);
    }
    cv_.notify_all();
}

void Poller::removeRuns(const std::vector<int64_t> &ids)
{
    {
        std::lock_guard<std::mutex> lock(m_);
        for (auto &kv : runs_)
        {
            auto &v = kv.second;
            v.erase(std::remove_if(v.begin(), v.end(),
                                   [&](const Run &r) { return std::find(ids.begin(), ids.end(), r.id) != ids.end(); }),
                    v.end());
        }
    }
    bump();
}

bool Poller::repoAllowed(const RepoInfo &r) const
{
    auto has = [](const std::vector<std::string> &list, const std::string &v) {
        for (auto &x : list)
            if (util::lower(x) == util::lower(v))
                return true;
        return false;
    };
    std::string name = r.fullName.substr(r.fullName.find('/') + 1);
    if (!cfg_.include.empty())
        return has(cfg_.include, r.fullName) || has(cfg_.include, name);
    if (r.archived && !cfg_.includeArchived)
        return false;
    if (r.fork && !cfg_.includeForks)
        return false;
    if (!cfg_.owners.empty() && !has(cfg_.owners, r.owner))
        return false;
    if (has(cfg_.exclude, r.fullName) || has(cfg_.exclude, name))
        return false;
    return true;
}

void Poller::loadRepos()
{
    std::vector<RepoInfo> list;
    std::string err;
    bool ok = gh_.listRepos(cfg_.affiliation, list, err);
    {
        std::lock_guard<std::mutex> lock(m_);
        reposAt_ = time(nullptr);
        if (!ok)
        {
            reposError_ = "список репозиториев: " + err;
        }
        else
        {
            reposError_.clear();
            std::vector<std::string> names;
            for (auto &r : list)
                if (repoAllowed(r))
                    names.push_back(r.fullName);
            repos_ = names;
            reposLoaded_ = true;
            // Убрать данные исчезнувших репозиториев
            for (auto it = runs_.begin(); it != runs_.end();)
            {
                if (std::find(names.begin(), names.end(), it->first) == names.end())
                    it = runs_.erase(it);
                else
                    ++it;
            }
        }
    }
    bump();
}

void Poller::pollRepo(const std::string &repo)
{
    std::vector<Run> out;
    bool changed = false;
    std::string err;
    bool ok = gh_.listRuns(repo, cfg_.runsPerRepo, out, changed, err);
    bool notify = false;
    {
        std::lock_guard<std::mutex> lock(m_);
        lastPoll_[repo] = time(nullptr);
        if (ok)
        {
            notify = errors_.erase(repo) > 0;
            if (changed)
            {
                auto &cur = runs_[repo];
                if (!(cur.empty() && out.empty()))
                    notify = true;
                cur = std::move(out);
            }
        }
        else
        {
            errors_[repo] = err;
            notify = true;
        }
    }
    if (notify)
        bump();
}

void Poller::loop()
{
    for (;;)
    {
        {
            std::lock_guard<std::mutex> lock(m_);
            if (stop_)
                return;
        }
        time_t now = time(nullptr);
        bool needRepos;
        {
            std::lock_guard<std::mutex> lock(m_);
            needRepos = !reposLoaded_ ? (now - reposAt_ >= 15) : (now - reposAt_ >= 600);
        }
        if (needRepos)
            loadRepos();

        std::vector<std::string> due;
        {
            std::lock_guard<std::mutex> lock(m_);
            for (auto &repo : repos_)
            {
                bool active = false;
                auto it = runs_.find(repo);
                if (it != runs_.end())
                    for (auto &r : it->second)
                        if (r.status != "completed")
                        {
                            active = true;
                            break;
                        }
                int iv = active ? cfg_.activeInterval : cfg_.interval;
                auto lp = lastPoll_.find(repo);
                if (forceAll_ || forced_.count(repo) || lp == lastPoll_.end() || now - lp->second >= iv)
                    due.push_back(repo);
            }
            forceAll_ = false;
            forced_.clear();
            if (!due.empty())
                busy_ = true;
        }

        if (!due.empty())
        {
            bump();
            std::atomic<size_t> next{0};
            int n = std::max(1, std::min<int>(cfg_.threads, (int) due.size()));
            std::vector<std::thread> workers;
            for (int i = 0; i < n; ++i)
                workers.emplace_back([&] {
                    for (size_t k; (k = next++) < due.size();)
                    {
                        {
                            std::lock_guard<std::mutex> lock(m_);
                            if (stop_)
                                return;
                        }
                        pollRepo(due[k]);
                    }
                });
            for (auto &w : workers)
                w.join();
            {
                std::lock_guard<std::mutex> lock(m_);
                busy_ = false;
                lastSync_ = time(nullptr);
            }
            bump();
        }

        std::unique_lock<std::mutex> lock(m_);
        cv_.wait_for(lock, std::chrono::seconds(1), [this] { return stop_ || forceAll_ || !forced_.empty(); });
    }
}
