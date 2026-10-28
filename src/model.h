#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

struct Run
{
    int64_t id = 0;
    int64_t workflowId = 0;
    std::string repo;        // owner/name
    std::string workflow;    // имя workflow
    std::string title;       // display_title (обычно сообщение коммита)
    std::string branch;
    std::string headSha;
    std::string event;
    std::string status;      // queued, in_progress, completed, waiting, ...
    std::string conclusion;  // success, failure, cancelled, ...
    std::string actor;
    std::string url;
    int runNumber = 0;
    int attempt = 1;
    time_t createdAt = 0;
    time_t updatedAt = 0;
    time_t startedAt = 0;
};

struct PullRequest
{
    int64_t id = 0;
    int number = 0;
    std::string repo;
    std::string title;
    std::string author;
    std::string headRef;
    std::string baseRef;
    std::string headSha;
    std::string url;
    bool draft = false;
    time_t createdAt = 0;
    time_t updatedAt = 0;
};

struct Issue
{
    int64_t id = 0;
    int number = 0;
    std::string repo;
    std::string title;
    std::string author;
    std::string url;
    std::vector<std::string> labels;
    int comments = 0;
    bool foreign = false;  // в чужом репозитории (создан пользователем)
    time_t createdAt = 0;
    time_t updatedAt = 0;
};

// Описание и комментарии issue или PR
struct Comment
{
    std::string author;
    std::string body;
    time_t createdAt = 0;
};

struct Discussion
{
    std::string title;
    std::string author;
    std::string state;
    std::string body;
    std::vector<std::string> labels;
    time_t createdAt = 0;
    std::vector<Comment> comments;
};

// Подробности PR, которых нет в списке
struct PullDetails
{
    std::string body;
    std::string mergeableState;
    int comments = 0;
    int reviewComments = 0;
    int commits = 0;
    int additions = 0;
    int deletions = 0;
    int changedFiles = 0;
};

// Статус развёртывания (GitHub Deployments)
struct DeploymentStatus
{
    std::string state;  // success, failure, error, inactive, in_progress, queued, pending
    std::string description;
    std::string creator;
    std::string logUrl;
    std::string environmentUrl;
    time_t createdAt = 0;
};

struct Deployment
{
    int64_t id = 0;
    std::string repo;
    std::string environment;
    std::string ref;
    std::string sha;
    std::string task;
    std::string description;
    std::string creator;
    time_t createdAt = 0;
    time_t updatedAt = 0;
    DeploymentStatus last;  // последний статус; state пустой, если статусов нет
};

struct Step
{
    int number = 0;
    std::string name;
    std::string status;
    std::string conclusion;
    time_t startedAt = 0;
    time_t completedAt = 0;
};

struct Job
{
    int64_t id = 0;
    std::string name;
    std::string status;
    std::string conclusion;
    std::string runner;
    time_t startedAt = 0;
    time_t completedAt = 0;
    std::vector<Step> steps;
};

struct Annotation
{
    std::string level;   // failure, warning, notice
    std::string path;
    int line = 0;
    std::string title;
    std::string message;
};

// Категория запуска для фильтров
enum class RunCategory { Active, Failed, Success, Other };

inline bool isFailure(const std::string &conclusion)
{
    return conclusion == "failure" || conclusion == "timed_out" || conclusion == "startup_failure";
}

inline RunCategory categoryOf(const Run &r)
{
    if (r.status != "completed")
        return RunCategory::Active;
    if (isFailure(r.conclusion))
        return RunCategory::Failed;
    if (r.conclusion == "success")
        return RunCategory::Success;
    return RunCategory::Other;
}

// Категория развёртывания по последнему статусу
inline RunCategory categoryOf(const Deployment &d)
{
    const std::string &st = d.last.state;
    if (st == "in_progress" || st == "queued" || st == "pending")
        return RunCategory::Active;
    if (st == "failure" || st == "error")
        return RunCategory::Failed;
    if (st == "success")
        return RunCategory::Success;
    return RunCategory::Other;
}
