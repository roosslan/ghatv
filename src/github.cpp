#include "github.h"
#include "util.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace
{

std::string str(const json &j, const char *key)
{
    auto it = j.find(key);
    if (it == j.end() || !it->is_string())
        return "";
    return it->get<std::string>();
}

int64_t num(const json &j, const char *key)
{
    auto it = j.find(key);
    if (it == j.end() || !it->is_number())
        return 0;
    return it->get<int64_t>();
}

Run parseRun(const json &j, const std::string &repo)
{
    Run r;
    r.id = num(j, "id");
    r.workflowId = num(j, "workflow_id");
    r.repo = repo;
    r.workflow = str(j, "name");
    r.title = str(j, "display_title");
    r.branch = str(j, "head_branch");
    r.headSha = str(j, "head_sha");
    r.event = str(j, "event");
    r.status = str(j, "status");
    r.conclusion = str(j, "conclusion");
    r.url = str(j, "html_url");
    r.runNumber = (int) num(j, "run_number");
    r.attempt = (int) num(j, "run_attempt");
    r.createdAt = util::parseIso(str(j, "created_at"));
    r.updatedAt = util::parseIso(str(j, "updated_at"));
    r.startedAt = util::parseIso(str(j, "run_started_at"));
    if (j.contains("actor") && j["actor"].is_object())
        r.actor = str(j["actor"], "login");
    // Заголовок может быть многострочным: берём первую строку
    auto nl = r.title.find('\n');
    if (nl != std::string::npos)
        r.title.resize(nl);
    return r;
}

std::vector<std::string> labelNames(const json &j)
{
    std::vector<std::string> out;
    if (j.contains("labels") && j["labels"].is_array())
        for (auto &l : j["labels"])
            if (l.is_object())
                out.push_back(str(l, "name"));
    return out;
}

Issue parseIssue(const json &j, const std::string &repo)
{
    Issue is;
    is.id = num(j, "id");
    is.number = (int) num(j, "number");
    is.repo = repo;
    is.title = str(j, "title");
    is.url = str(j, "html_url");
    is.comments = (int) num(j, "comments");
    is.createdAt = util::parseIso(str(j, "created_at"));
    is.updatedAt = util::parseIso(str(j, "updated_at"));
    is.labels = labelNames(j);
    if (j.contains("user") && j["user"].is_object())
        is.author = str(j["user"], "login");
    return is;
}

PullRequest parsePull(const json &p, const std::string &repo)
{
    PullRequest pr;
    pr.id = num(p, "id");
    pr.number = (int) num(p, "number");
    pr.repo = repo;
    pr.title = str(p, "title");
    pr.url = str(p, "html_url");
    pr.draft = p.value("draft", false);
    pr.createdAt = util::parseIso(str(p, "created_at"));
    pr.updatedAt = util::parseIso(str(p, "updated_at"));
    if (p.contains("user") && p["user"].is_object())
        pr.author = str(p["user"], "login");
    if (p.contains("head") && p["head"].is_object())
    {
        pr.headRef = str(p["head"], "ref");
        pr.headSha = str(p["head"], "sha");
    }
    if (p.contains("base") && p["base"].is_object())
        pr.baseRef = str(p["base"], "ref");
    return pr;
}

size_t writeCb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    static_cast<std::string *>(userdata)->append(ptr, size * nmemb);
    return size * nmemb;
}

struct HeaderState
{
    std::map<std::string, std::string> headers;
};

size_t headerCb(char *buffer, size_t size, size_t nitems, void *userdata)
{
    auto *st = static_cast<HeaderState *>(userdata);
    std::string line(buffer, size * nitems);
    // При редиректах заголовки приходят от нескольких ответов: начинаем заново
    if (line.rfind("HTTP/", 0) == 0)
    {
        st->headers.clear();
        return size * nitems;
    }
    auto colon = line.find(':');
    if (colon != std::string::npos)
    {
        std::string key = util::lower(line.substr(0, colon));
        std::string value = line.substr(colon + 1);
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
            value.erase(value.begin());
        while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' '))
            value.pop_back();
        st->headers[key] = value;
    }
    return size * nitems;
}

// Один CURL-хэндл на поток: соединения с api.github.com переиспользуются
CURL *threadHandle()
{
    struct Holder
    {
        CURL *h = curl_easy_init();
        ~Holder() { curl_easy_cleanup(h); }
    };
    thread_local Holder holder;
    return holder.h;
}

} // namespace

namespace ghparse
{

std::vector<Run> runs(const std::string &body, const std::string &repo)
{
    std::vector<Run> out;
    auto j = json::parse(body);
    if (j.contains("workflow_runs") && j["workflow_runs"].is_array())
        for (auto &r : j["workflow_runs"])
            out.push_back(parseRun(r, repo));
    return out;
}

std::vector<PullRequest> pulls(const std::string &body, const std::string &repo)
{
    std::vector<PullRequest> out;
    auto j = json::parse(body);
    if (j.is_array())
        for (auto &p : j)
            out.push_back(parsePull(p, repo));
    return out;
}

std::vector<Issue> issues(const std::string &body, const std::string &repo)
{
    std::vector<Issue> out;
    auto j = json::parse(body);
    if (j.is_array())
        for (auto &i : j)
            if (!i.contains("pull_request"))  // список issues включает и PR
                out.push_back(parseIssue(i, repo));
    return out;
}

std::vector<Issue> searchIssues(const std::string &body)
{
    std::vector<Issue> out;
    auto j = json::parse(body);
    if (j.contains("items") && j["items"].is_array())
        for (auto &i : j["items"])
            if (!i.contains("pull_request"))
                out.push_back(parseIssue(i, repoFromUrl(str(i, "repository_url"))));
    return out;
}

std::string repoFromUrl(const std::string &url)
{
    const std::string prefix = "/repos/";
    auto pos = url.find(prefix);
    return pos == std::string::npos ? url : url.substr(pos + prefix.size());
}

} // namespace ghparse

GitHub::GitHub(std::string token, std::string apiBase) :
    token_(std::move(token)),
    apiBase_(std::move(apiBase))
{
    while (!apiBase_.empty() && apiBase_.back() == '/')
        apiBase_.pop_back();
}

HttpResponse GitHub::request(const std::string &method, const std::string &path, bool useEtag)
{
    HttpResponse res;
    std::string url = path.rfind("http", 0) == 0 ? path : apiBase_ + path;

    CURL *h = threadHandle();
    if (!h)
    {
        res.error = "curl_easy_init failed";
        return res;
    }
    curl_easy_reset(h);

    std::string cachedEtag, cachedBody;
    if (useEtag)
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = etagCache_.find(url);
        if (it != etagCache_.end())
        {
            cachedEtag = it->second.first;
            cachedBody = it->second.second;
        }
    }

    curl_slist *hdrs = nullptr;
    hdrs = curl_slist_append(hdrs, "Accept: application/vnd.github+json");
    hdrs = curl_slist_append(hdrs, "X-GitHub-Api-Version: 2022-11-28");
    hdrs = curl_slist_append(hdrs, ("Authorization: Bearer " + token_).c_str());
    if (!cachedEtag.empty())
        hdrs = curl_slist_append(hdrs, ("If-None-Match: " + cachedEtag).c_str());

    char errbuf[CURL_ERROR_SIZE] = {0};
    HeaderState hs;
    curl_easy_setopt(h, CURLOPT_URL, url.c_str());
    curl_easy_setopt(h, CURLOPT_HTTPHEADER, hdrs);
    curl_easy_setopt(h, CURLOPT_USERAGENT, "ghatv");
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(h, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(h, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(h, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, &res.body);
    curl_easy_setopt(h, CURLOPT_HEADERFUNCTION, headerCb);
    curl_easy_setopt(h, CURLOPT_HEADERDATA, &hs);
    if (method == "POST")
    {
        curl_easy_setopt(h, CURLOPT_POST, 1L);
        curl_easy_setopt(h, CURLOPT_POSTFIELDS, "");
        curl_easy_setopt(h, CURLOPT_POSTFIELDSIZE, 0L);
    }
    else if (method != "GET")
        curl_easy_setopt(h, CURLOPT_CUSTOMREQUEST, method.c_str());

    CURLcode rc = curl_easy_perform(h);
    curl_slist_free_all(hdrs);

    if (rc != CURLE_OK)
    {
        res.error = errbuf[0] ? errbuf : curl_easy_strerror(rc);
        return res;
    }
    curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &res.status);

    auto &H = hs.headers;
    if (H.count("x-ratelimit-remaining"))
        rateRemaining_ = atoi(H["x-ratelimit-remaining"].c_str());
    if (H.count("x-ratelimit-limit"))
        rateLimit_ = atoi(H["x-ratelimit-limit"].c_str());
    if (H.count("x-ratelimit-reset"))
        rateReset_ = (time_t) atoll(H["x-ratelimit-reset"].c_str());
    if (H.count("link"))
        res.link = H["link"];

    if (res.status == 304 && !cachedEtag.empty())
    {
        res.status = 200;
        res.body = cachedBody;
        res.fromCache = true;
    }
    else if (res.status == 200 && useEtag && H.count("etag"))
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        etagCache_[url] = {H["etag"], res.body};
    }

    if (res.status >= 400)
    {
        std::string msg;
        try
        {
            msg = str(json::parse(res.body), "message");
        }
        catch (...)
        {
        }
        res.error = "HTTP " + std::to_string(res.status) + (msg.empty() ? "" : ": " + msg);
    }
    return res;
}

void GitHub::invalidate(const std::string &repo)
{
    std::lock_guard<std::mutex> lock(cacheMutex_);
    std::string key = "/repos/" + repo + "/";
    for (auto it = etagCache_.begin(); it != etagCache_.end();)
    {
        if (it->first.find(key) != std::string::npos)
            it = etagCache_.erase(it);
        else
            ++it;
    }
}

bool GitHub::listRepos(const std::string &affiliation, std::vector<RepoInfo> &out, std::string &err)
{
    out.clear();
    for (int page = 1; page < 100; ++page)
    {
        auto res = request("GET", "/user/repos?per_page=100&sort=pushed&affiliation=" + affiliation +
                                      "&page=" + std::to_string(page));
        if (!res.error.empty())
        {
            err = res.error;
            return false;
        }
        json j;
        try
        {
            j = json::parse(res.body);
        }
        catch (const std::exception &e)
        {
            err = e.what();
            return false;
        }
        if (!j.is_array())
            break;
        for (auto &r : j)
        {
            RepoInfo info;
            info.fullName = str(r, "full_name");
            info.archived = r.value("archived", false);
            info.fork = r.value("fork", false);
            if (r.contains("owner") && r["owner"].is_object())
                info.owner = str(r["owner"], "login");
            out.push_back(info);
        }
        if (j.size() < 100)
            break;
    }
    return true;
}

bool GitHub::listRuns(const std::string &repo, int perPage, std::vector<Run> &out, bool &changed, std::string &err)
{
    changed = false;
    auto res = request("GET", "/repos/" + repo + "/actions/runs?per_page=" + std::to_string(perPage), true);
    // 404: в репозитории нет Actions или к ним нет доступа
    if (res.status == 404)
    {
        changed = true;
        out.clear();
        return true;
    }
    if (!res.error.empty())
    {
        err = res.error;
        return false;
    }
    if (res.fromCache)
        return true;
    try
    {
        out = ghparse::runs(res.body, repo);
    }
    catch (const std::exception &e)
    {
        err = e.what();
        return false;
    }
    changed = true;
    return true;
}

bool GitHub::listPulls(const std::string &repo, std::vector<PullRequest> &out, bool &changed, std::string &err)
{
    changed = false;
    auto res = request("GET", "/repos/" + repo + "/pulls?state=open&per_page=100", true);
    if (res.status == 404)
    {
        changed = true;
        out.clear();
        return true;
    }
    if (!res.error.empty())
    {
        err = res.error;
        return false;
    }
    if (res.fromCache)
        return true;
    try
    {
        out = ghparse::pulls(res.body, repo);
    }
    catch (const std::exception &e)
    {
        err = e.what();
        return false;
    }
    changed = true;
    return true;
}

bool GitHub::currentUser(std::string &login, std::string &err)
{
    auto res = request("GET", "/user");
    if (!res.error.empty())
    {
        err = res.error;
        return false;
    }
    try
    {
        login = str(json::parse(res.body), "login");
    }
    catch (const std::exception &e)
    {
        err = e.what();
        return false;
    }
    return !login.empty();
}

bool GitHub::listIssues(const std::string &repo, std::vector<Issue> &out, bool &changed, std::string &err)
{
    changed = false;
    auto res = request("GET", "/repos/" + repo + "/issues?state=open&per_page=100", true);
    // 410: issues в репозитории отключены
    if (res.status == 404 || res.status == 410)
    {
        changed = true;
        out.clear();
        return true;
    }
    if (!res.error.empty())
    {
        err = res.error;
        return false;
    }
    if (res.fromCache)
        return true;
    try
    {
        out = ghparse::issues(res.body, repo);
    }
    catch (const std::exception &e)
    {
        err = e.what();
        return false;
    }
    changed = true;
    return true;
}

bool GitHub::searchIssues(const std::string &q, std::vector<Issue> &out, bool &changed, std::string &err)
{
    changed = false;
    std::string enc;
    for (char c : q)
        enc += c == ' ' ? std::string("+") : std::string(1, c);
    auto res = request("GET", "/search/issues?per_page=100&sort=updated&q=" + enc, true);
    if (!res.error.empty())
    {
        err = res.error;
        return false;
    }
    if (res.fromCache)
        return true;
    try
    {
        out = ghparse::searchIssues(res.body);
    }
    catch (const std::exception &e)
    {
        err = e.what();
        return false;
    }
    changed = true;
    return true;
}

bool GitHub::discussion(const std::string &repo, int number, Discussion &out, std::string &err)
{
    std::string base = "/repos/" + repo + "/issues/" + std::to_string(number);
    auto res = request("GET", base);
    if (!res.error.empty())
    {
        err = res.error;
        return false;
    }
    try
    {
        auto j = json::parse(res.body);
        out.title = str(j, "title");
        out.state = str(j, "state");
        out.body = str(j, "body");
        out.labels = labelNames(j);
        out.createdAt = util::parseIso(str(j, "created_at"));
        if (j.contains("user") && j["user"].is_object())
            out.author = str(j["user"], "login");
    }
    catch (const std::exception &e)
    {
        err = e.what();
        return false;
    }
    out.comments.clear();
    for (int page = 1; page < 10; ++page)
    {
        auto cr = request("GET", base + "/comments?per_page=100&page=" + std::to_string(page));
        if (!cr.error.empty())
        {
            err = cr.error;
            return false;
        }
        size_t got = 0;
        try
        {
            for (auto &c : json::parse(cr.body))
            {
                ++got;
                Comment cm;
                cm.body = str(c, "body");
                cm.createdAt = util::parseIso(str(c, "created_at"));
                if (c.contains("user") && c["user"].is_object())
                    cm.author = str(c["user"], "login");
                out.comments.push_back(std::move(cm));
            }
        }
        catch (const std::exception &e)
        {
            err = e.what();
            return false;
        }
        if (got < 100)
            break;
    }
    return true;
}

bool GitHub::pullDetails(const std::string &repo, int number, PullDetails &out, std::string &err)
{
    auto res = request("GET", "/repos/" + repo + "/pulls/" + std::to_string(number));
    if (!res.error.empty())
    {
        err = res.error;
        return false;
    }
    try
    {
        auto j = json::parse(res.body);
        out.body = str(j, "body");
        out.mergeableState = str(j, "mergeable_state");
        out.comments = (int) num(j, "comments");
        out.reviewComments = (int) num(j, "review_comments");
        out.commits = (int) num(j, "commits");
        out.additions = (int) num(j, "additions");
        out.deletions = (int) num(j, "deletions");
        out.changedFiles = (int) num(j, "changed_files");
    }
    catch (const std::exception &e)
    {
        err = e.what();
        return false;
    }
    return true;
}

bool GitHub::runsForSha(const std::string &repo, const std::string &sha, std::vector<Run> &out, std::string &err)
{
    out.clear();
    auto res = request("GET", "/repos/" + repo + "/actions/runs?per_page=100&head_sha=" + sha);
    if (res.status == 404)
        return true;
    if (!res.error.empty())
    {
        err = res.error;
        return false;
    }
    try
    {
        out = ghparse::runs(res.body, repo);
    }
    catch (const std::exception &e)
    {
        err = e.what();
        return false;
    }
    return true;
}

bool GitHub::getRun(const std::string &repo, int64_t runId, Run &out, std::string &err)
{
    auto res = request("GET", "/repos/" + repo + "/actions/runs/" + std::to_string(runId));
    if (!res.error.empty())
    {
        err = res.error;
        return false;
    }
    try
    {
        out = parseRun(json::parse(res.body), repo);
    }
    catch (const std::exception &e)
    {
        err = e.what();
        return false;
    }
    return true;
}

bool GitHub::listJobs(const std::string &repo, int64_t runId, std::vector<Job> &out, std::string &err)
{
    out.clear();
    for (int page = 1; page < 20; ++page)
    {
        auto res = request("GET", "/repos/" + repo + "/actions/runs/" + std::to_string(runId) +
                                      "/jobs?filter=latest&per_page=100&page=" + std::to_string(page));
        if (!res.error.empty())
        {
            err = res.error;
            return false;
        }
        size_t got = 0;
        try
        {
            auto j = json::parse(res.body);
            for (auto &jj : j["jobs"])
            {
                ++got;
                Job job;
                job.id = num(jj, "id");
                job.name = str(jj, "name");
                job.status = str(jj, "status");
                job.conclusion = str(jj, "conclusion");
                job.runner = str(jj, "runner_name");
                job.startedAt = util::parseIso(str(jj, "started_at"));
                job.completedAt = util::parseIso(str(jj, "completed_at"));
                if (jj.contains("steps") && jj["steps"].is_array())
                    for (auto &s : jj["steps"])
                    {
                        Step st;
                        st.number = (int) num(s, "number");
                        st.name = str(s, "name");
                        st.status = str(s, "status");
                        st.conclusion = str(s, "conclusion");
                        st.startedAt = util::parseIso(str(s, "started_at"));
                        st.completedAt = util::parseIso(str(s, "completed_at"));
                        job.steps.push_back(st);
                    }
                out.push_back(std::move(job));
            }
        }
        catch (const std::exception &e)
        {
            err = e.what();
            return false;
        }
        if (got < 100)
            break;
    }
    return true;
}

bool GitHub::annotations(const std::string &repo, int64_t jobId, std::vector<Annotation> &out, std::string &err)
{
    out.clear();
    auto res = request("GET", "/repos/" + repo + "/check-runs/" + std::to_string(jobId) + "/annotations?per_page=100");
    if (!res.error.empty())
    {
        err = res.error;
        return false;
    }
    try
    {
        auto j = json::parse(res.body);
        for (auto &a : j)
        {
            Annotation an;
            an.level = str(a, "annotation_level");
            an.path = str(a, "path");
            an.line = (int) num(a, "start_line");
            an.title = str(a, "title");
            an.message = str(a, "message");
            out.push_back(an);
        }
    }
    catch (const std::exception &e)
    {
        err = e.what();
        return false;
    }
    return true;
}

bool GitHub::jobLog(const std::string &repo, int64_t jobId, std::string &out, std::string &err)
{
    auto res = request("GET", "/repos/" + repo + "/actions/jobs/" + std::to_string(jobId) + "/logs");
    if (!res.error.empty())
    {
        err = res.error;
        return false;
    }
    out = std::move(res.body);
    return true;
}

bool GitHub::simpleAction(const std::string &method, const std::string &path, std::string &err)
{
    auto res = request(method, path);
    if (!res.error.empty())
    {
        err = res.error;
        return false;
    }
    return true;
}

bool GitHub::deleteRun(const std::string &repo, int64_t runId, std::string &err)
{
    return simpleAction("DELETE", "/repos/" + repo + "/actions/runs/" + std::to_string(runId), err);
}

bool GitHub::deleteLogs(const std::string &repo, int64_t runId, std::string &err)
{
    return simpleAction("DELETE", "/repos/" + repo + "/actions/runs/" + std::to_string(runId) + "/logs", err);
}

bool GitHub::cancelRun(const std::string &repo, int64_t runId, std::string &err)
{
    return simpleAction("POST", "/repos/" + repo + "/actions/runs/" + std::to_string(runId) + "/cancel", err);
}

bool GitHub::rerun(const std::string &repo, int64_t runId, std::string &err)
{
    return simpleAction("POST", "/repos/" + repo + "/actions/runs/" + std::to_string(runId) + "/rerun", err);
}

bool GitHub::rerunFailed(const std::string &repo, int64_t runId, std::string &err)
{
    return simpleAction("POST", "/repos/" + repo + "/actions/runs/" + std::to_string(runId) + "/rerun-failed-jobs", err);
}
