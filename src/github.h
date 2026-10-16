#pragma once

#include "model.h"

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <vector>

struct HttpResponse
{
    long status = 0;
    std::string body;
    std::string error;
    std::string link;
    bool fromCache = false;  // ответ 304, тело взято из кеша ETag
};

struct RepoInfo
{
    std::string fullName;
    std::string owner;
    bool archived = false;
    bool fork = false;
};

// Клиент GitHub REST API. Потокобезопасен: у каждого потока свой CURL-хэндл.
class GitHub
{
public:
    GitHub(std::string token, std::string apiBase);

    bool listRepos(const std::string &affiliation, std::vector<RepoInfo> &out, std::string &err);
    // changed = false, если данные не изменились с прошлого запроса (304)
    bool listRuns(const std::string &repo, int perPage, std::vector<Run> &out, bool &changed, std::string &err);
    bool getRun(const std::string &repo, int64_t runId, Run &out, std::string &err);
    bool listJobs(const std::string &repo, int64_t runId, std::vector<Job> &out, std::string &err);
    bool annotations(const std::string &repo, int64_t jobId, std::vector<Annotation> &out, std::string &err);
    bool jobLog(const std::string &repo, int64_t jobId, std::string &out, std::string &err);

    bool deleteRun(const std::string &repo, int64_t runId, std::string &err);
    bool deleteLogs(const std::string &repo, int64_t runId, std::string &err);
    bool cancelRun(const std::string &repo, int64_t runId, std::string &err);
    bool rerun(const std::string &repo, int64_t runId, std::string &err);
    bool rerunFailed(const std::string &repo, int64_t runId, std::string &err);

    // Сбросить кеш ETag для репозитория, чтобы следующий опрос получил свежие данные
    void invalidate(const std::string &repo);

    int rateRemaining() const { return rateRemaining_.load(); }
    int rateLimit() const { return rateLimit_.load(); }
    time_t rateReset() const { return rateReset_.load(); }

private:
    HttpResponse request(const std::string &method, const std::string &path, bool useEtag = false);
    bool simpleAction(const std::string &method, const std::string &path, std::string &err);

    std::string token_;
    std::string apiBase_;

    std::mutex cacheMutex_;
    std::map<std::string, std::pair<std::string, std::string>> etagCache_;  // url -> (etag, body)

    std::atomic<int> rateRemaining_{-1};
    std::atomic<int> rateLimit_{-1};
    std::atomic<time_t> rateReset_{0};
};
