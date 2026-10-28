#pragma once

#include "log_window.h"
#include "poller.h"
#include "runs_window.h"
#include "table.h"

// Значок и подпись статуса развёртывания
StatusLook deploymentLook(const std::string &state);
// Адрес развёртывания: лог (обычно запуск Actions), окружение или страница развёртываний репозитория
std::string deploymentUrl(const Deployment &d);

class DeploymentsWindow : public TableWindow
{
public:
    explicit DeploymentsWindow(const TRect &r);

    void updateData(Poller::State st);

    int dataCount() const override { return (int) st_.deployments.size(); }
    bool accept(int idx) const override;
    Cell cell(int idx, int col) const override;
    int compare(int a, int b, int col) const override;
    int64_t rowId(int idx) const override { return st_.deployments[idx].id; }
    std::string emptyText() const override { return L("No deployments", "Развёртываний нет"); }
    std::string infoText() const override;
    std::string rowUrl(int idx) const override { return deploymentUrl(st_.deployments[idx]); }

    void openRow(int idx) override;
    bool rowKey(TEvent &ev) override;
    bool command(ushort cmd) override;

private:
    Poller::State st_;
    RunFilter filter_ = RunFilter::All;
    bool multiOwner_ = false;
};

// История статусов развёртывания
class DeploymentWindow : public TWindow
{
public:
    DeploymentWindow(const TRect &r, const Deployment &d);
    std::string url() const { return deploymentUrl(dep_); }

    void handleEvent(TEvent &ev) override;
    void shutDown() override;

private:
    void load();

    Deployment dep_;
    LogView *view_;
    LogInfo *info_;
    std::shared_ptr<bool> alive_;
};
