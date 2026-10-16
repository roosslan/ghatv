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
