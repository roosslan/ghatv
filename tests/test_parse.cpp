#include "check.h"
#include "github.h"

TEST("ghparse::runs: поля запуска")
{
    const char *body = R"({
      "total_count": 2,
      "workflow_runs": [
        {
          "id": 101, "workflow_id": 7, "name": "CI", "display_title": "Fix build\nsecond line",
          "head_branch": "trunk", "head_sha": "abc123", "event": "push",
          "status": "completed", "conclusion": "failure", "html_url": "https://github.com/o/r/actions/runs/101",
          "run_number": 12, "run_attempt": 2,
          "created_at": "2026-10-13T10:00:00Z", "updated_at": "2026-10-13T10:02:30Z",
          "run_started_at": "2026-10-13T10:00:10Z",
          "actor": {"login": "octocat"}
        },
        {
          "id": 102, "name": "Release", "status": "in_progress", "conclusion": null,
          "created_at": "2026-10-13T11:00:00Z"
        }
      ]
    })";
    auto runs = ghparse::runs(body, "o/r");
    CHECK_EQ(runs.size(), (size_t) 2);
    if (runs.size() != 2)
        return;
    const Run &r = runs[0];
    CHECK_EQ(r.id, (int64_t) 101);
    CHECK_EQ(r.workflowId, (int64_t) 7);
    CHECK_EQ(r.repo, std::string("o/r"));
    CHECK_EQ(r.title, std::string("Fix build"));  // только первая строка
    CHECK_EQ(r.headSha, std::string("abc123"));
    CHECK_EQ(r.conclusion, std::string("failure"));
    CHECK_EQ(r.runNumber, 12);
    CHECK_EQ(r.attempt, 2);
    CHECK_EQ(r.actor, std::string("octocat"));
    CHECK_EQ(r.updatedAt - r.createdAt, (time_t) 150);
    CHECK_EQ(r.startedAt - r.createdAt, (time_t) 10);
    // null и отсутствующие поля дают пустые значения
    CHECK_EQ(runs[1].conclusion, std::string(""));
    CHECK_EQ(runs[1].actor, std::string(""));
    CHECK_EQ(runs[1].startedAt, (time_t) 0);
}

TEST("ghparse::runs: пустой ответ и неверный JSON")
{
    CHECK(ghparse::runs("{}", "o/r").empty());
    bool thrown = false;
    try
    {
        ghparse::runs("не json", "o/r");
    }
    catch (const std::exception &)
    {
        thrown = true;
    }
    CHECK(thrown);
}

TEST("ghparse::pulls: ветки, sha, черновик")
{
    const char *body = R"([
      {
        "id": 5, "number": 3, "title": "Docker", "html_url": "https://github.com/o/r/pull/3",
        "draft": true, "user": {"login": "me"},
        "head": {"ref": "feature/x", "sha": "deadbeef"}, "base": {"ref": "trunk"},
        "created_at": "2026-10-13T10:00:00Z", "updated_at": "2026-10-13T12:00:00Z"
      }
    ])";
    auto prs = ghparse::pulls(body, "o/r");
    CHECK_EQ(prs.size(), (size_t) 1);
    if (prs.empty())
        return;
    CHECK_EQ(prs[0].number, 3);
    CHECK(prs[0].draft);
    CHECK_EQ(prs[0].author, std::string("me"));
    CHECK_EQ(prs[0].headRef, std::string("feature/x"));
    CHECK_EQ(prs[0].headSha, std::string("deadbeef"));
    CHECK_EQ(prs[0].baseRef, std::string("trunk"));
}

TEST("ghparse::issues: PR в списке issues пропускаются")
{
    const char *body = R"([
      {"id": 1, "number": 10, "title": "Bug", "comments": 4, "user": {"login": "a"},
       "labels": [{"name": "bug"}, {"name": "help wanted"}]},
      {"id": 2, "number": 11, "title": "PR", "pull_request": {"url": "x"}}
    ])";
    auto issues = ghparse::issues(body, "o/r");
    CHECK_EQ(issues.size(), (size_t) 1);
    if (issues.empty())
        return;
    CHECK_EQ(issues[0].number, 10);
    CHECK_EQ(issues[0].comments, 4);
    CHECK_EQ(issues[0].labels.size(), (size_t) 2);
    CHECK_EQ(issues[0].labels[1], std::string("help wanted"));
    CHECK(!issues[0].foreign);
}

TEST("ghparse::searchIssues: репозиторий из repository_url")
{
    const char *body = R"({"total_count": 1, "items": [
      {"id": 9, "number": 379, "title": "Seg fault",
       "repository_url": "https://api.github.com/repos/ghostbsd/issues"}
    ]})";
    auto issues = ghparse::searchIssues(body);
    CHECK_EQ(issues.size(), (size_t) 1);
    if (!issues.empty())
        CHECK_EQ(issues[0].repo, std::string("ghostbsd/issues"));
    CHECK_EQ(ghparse::repoFromUrl("https://ghe.example.com/api/v3/repos/a/b"), std::string("a/b"));
}
