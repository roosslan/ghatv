#pragma once

#include "table.h"

class PullWindow;

class PullInfoView : public TView
{
public:
    PullInfoView(const TRect &r, PullWindow *w);
    void draw() override;
    void handleEvent(TEvent &ev) override;

private:
    PullWindow *w_;
};

// Карточка PR: сведения, ссылка на обсуждение и все запуски CI для последнего коммита
class PullWindow : public TableWindow
{
public:
    PullWindow(const TRect &r, const PullRequest &pr);

    void shutDown() override;
    void handleEvent(TEvent &ev) override;

    int dataCount() const override { return (int) runs_.size(); }
    Cell cell(int idx, int col) const override;
    int compare(int a, int b, int col) const override;
    int64_t rowId(int idx) const override { return runs_[idx].id; }
    std::string emptyText() const override;
    std::string infoText() const override;
    std::string rowUrl(int idx) const override { return runs_[idx].url; }
    // Клавиша o в карточке открывает сам PR
    std::string currentUrl() const override { return pr_.url; }

    void openRow(int idx) override;
    bool rowKey(TEvent &ev) override;
    bool command(ushort cmd) override;

    const PullRequest &pr() const { return pr_; }
    const PullDetails &details() const { return details_; }
    bool hasDetails() const { return hasDetails_; }
    bool loading() const { return loading_; }
    void openDiscussion();

private:
    void reload();
    void action(RunAction a);

    PullRequest pr_;
    PullDetails details_;
    bool hasDetails_ = false;
    std::vector<Run> runs_;
    std::string error_;
    bool loading_ = false;
    PullInfoView *infoView_;
    std::shared_ptr<bool> alive_;
    TTimerId timer_ = 0;
};
