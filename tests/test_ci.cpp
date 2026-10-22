#include "check.h"
#include "ui.h"

namespace
{

Run run(int64_t id, int64_t wf, const char *sha, const char *status, const char *conclusion, time_t created)
{
    Run r;
    r.id = id;
    r.workflowId = wf;
    r.repo = "o/r";
    r.headSha = sha;
    r.status = status;
    r.conclusion = conclusion;
    r.createdAt = created;
    return r;
}

} // namespace

TEST("categoryOf: статусы и результаты запусков")
{
    CHECK(categoryOf(run(1, 1, "", "queued", "", 0)) == RunCategory::Active);
    CHECK(categoryOf(run(1, 1, "", "waiting", "", 0)) == RunCategory::Active);
    CHECK(categoryOf(run(1, 1, "", "completed", "success", 0)) == RunCategory::Success);
    CHECK(categoryOf(run(1, 1, "", "completed", "failure", 0)) == RunCategory::Failed);
    CHECK(categoryOf(run(1, 1, "", "completed", "timed_out", 0)) == RunCategory::Failed);
    CHECK(categoryOf(run(1, 1, "", "completed", "startup_failure", 0)) == RunCategory::Failed);
    CHECK(categoryOf(run(1, 1, "", "completed", "cancelled", 0)) == RunCategory::Other);
    CHECK(categoryOf(run(1, 1, "", "completed", "skipped", 0)) == RunCategory::Other);
}

TEST("runsOfPull: последний запуск каждого workflow для sha PR")
{
    PullRequest pr;
    pr.repo = "o/r";
    pr.headSha = "aaa";
    std::vector<Run> runs = {
        run(1, 10, "aaa", "completed", "failure", 100),
        run(2, 10, "aaa", "completed", "success", 200),  // перезапуск того же workflow
        run(3, 20, "aaa", "completed", "success", 150),
        run(4, 10, "bbb", "completed", "failure", 300),  // другой коммит
    };
    auto pr_runs = runsOfPull(runs, pr);
    CHECK_EQ(pr_runs.size(), (size_t) 2);
    for (auto &r : pr_runs)
        if (r.workflowId == 10)
            CHECK_EQ(r.id, (int64_t) 2);
    CHECK(ciCategory(pr_runs) == RunCategory::Success);

    // Другой репозиторий с тем же sha не учитывается
    PullRequest other = pr;
    other.repo = "o/other";
    CHECK(runsOfPull(runs, other).empty());
}

TEST("ciCategory: приоритет активных и упавших")
{
    std::vector<Run> v = {run(1, 1, "a", "completed", "success", 0), run(2, 2, "a", "completed", "failure", 0)};
    CHECK(ciCategory(v) == RunCategory::Failed);
    v.push_back(run(3, 3, "a", "in_progress", "", 0));
    CHECK(ciCategory(v) == RunCategory::Active);
    CHECK(ciCategory({}) == RunCategory::Other);
    CHECK_EQ(ciLook({}).label, std::string("нет CI"));
    CHECK_EQ(ciLook(v).label, std::string("выполняется"));
}

TEST("lookOf: подписи статусов")
{
    CHECK_EQ(lookOf("completed", "success").label, std::string("успех"));
    CHECK_EQ(lookOf("completed", "failure").label, std::string("ошибка"));
    CHECK_EQ(lookOf("queued", "").label, std::string("в очереди"));
    CHECK_EQ(lookOf("completed", "cancelled").label, std::string("отменён"));
}
