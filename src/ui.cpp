#include "ui.h"

#include <algorithm>
#include <cstring>
#include <ctime>

App *gApp = nullptr;

StatusLook lookOf(const std::string &status, const std::string &conclusion)
{
    if (status == "completed")
    {
        if (conclusion == "success")
            return {"✔", L("success", "успех"), 0xA};
        if (conclusion == "failure")
            return {"✘", L("failure", "ошибка"), 0xC};
        if (conclusion == "timed_out")
            return {"✘", L("timed out", "таймаут"), 0xC};
        if (conclusion == "startup_failure")
            return {"✘", L("startup failure", "сбой старта"), 0xC};
        if (conclusion == "cancelled")
            return {"⊘", L("cancelled", "отменён"), 0x7};
        if (conclusion == "skipped")
            return {"»", L("skipped", "пропущен"), 0x7};
        if (conclusion == "neutral")
            return {"○", L("neutral", "нейтрально"), 0x7};
        if (conclusion == "action_required")
            return {"!", L("action required", "нужно действие"), 0xD};
        if (conclusion == "stale")
            return {"~", L("stale", "устарел"), 0x7};
        return {"?", conclusion.empty() ? L("completed", "завершён") : conclusion, 0x7};
    }
    if (status == "in_progress")
    {
        static const char *spin[] = {"◐", "◓", "◑", "◒"};
        return {spin[time(nullptr) % 4], L("in progress", "выполняется"), 0xE};
    }
    if (status == "queued")
        return {"◌", L("queued", "в очереди"), 0xD};
    if (status == "waiting")
        return {"◌", L("waiting", "ожидает"), 0xD};
    if (status == "pending")
        return {"◌", L("waiting", "ожидает"), 0xD};
    if (status == "requested")
        return {"◌", L("requested", "запрошен"), 0xD};
    return {"?", status, 0x7};
}

std::vector<Run> runsOfPull(const std::vector<Run> &runs, const PullRequest &pr)
{
    std::vector<Run> out;
    for (auto &r : runs)
    {
        if (r.repo != pr.repo || r.headSha != pr.headSha || r.headSha.empty())
            continue;
        bool replaced = false;
        for (auto &o : out)
            if (o.workflowId == r.workflowId)
            {
                if (r.createdAt > o.createdAt)
                    o = r;
                replaced = true;
                break;
            }
        if (!replaced)
            out.push_back(r);
    }
    return out;
}

RunCategory ciCategory(const std::vector<Run> &prRuns)
{
    bool failed = false, other = false;
    for (auto &r : prRuns)
        switch (categoryOf(r))
        {
        case RunCategory::Active:
            return RunCategory::Active;
        case RunCategory::Failed:
            failed = true;
            break;
        case RunCategory::Other:
            other = true;
            break;
        default:
            break;
        }
    if (failed)
        return RunCategory::Failed;
    if (other || prRuns.empty())
        return RunCategory::Other;
    return RunCategory::Success;
}

StatusLook ciLook(const std::vector<Run> &prRuns)
{
    if (prRuns.empty())
        return {"·", L("no CI", "нет CI"), 0x7};
    // Показать самый «важный» запуск: активный, затем упавший, затем прочие
    const Run *pick = &prRuns.front();
    RunCategory cat = ciCategory(prRuns);
    for (auto &r : prRuns)
        if (categoryOf(r) == cat)
        {
            pick = &r;
            break;
        }
    return lookOf(pick->status, pick->conclusion);
}

TColorAttr withFore(uchar base, uchar fore)
{
    return TColorAttr((base & 0xF0) | (fore & 0x0F));
}

bool keyIs(const TEvent &ev, const char *latin, const char *cyrillic)
{
    if (ev.what != evKeyDown)
        return false;
    TStringView t = ev.keyDown.getText();
    if (t.empty())
        return false;
    std::string s(t.data(), t.size());
    return s == latin || (cyrillic && s == cyrillic);
}

void putCell(TDrawBuffer &b, int x, int width, const std::string &text, TColorAttr attr, bool right)
{
    if (width <= 0)
        return;
    b.moveChar(x, ' ', attr, width);
    int w = (int) TText::width(text);
    int off = (right && w < width) ? width - w : 0;
    b.moveStr(x + off, text, attr, width - off);
    // Если текст не поместился, показать многоточие в последней ячейке
    if (w > width && width > 1)
        b.moveStr(x + width - 1, "…", attr, 1);
}

void showHelp()
{
    const char *text = L(
        "Enter    open row: run, PR, issue, log\n"
        "o        open in browser (Ctrl+Enter)\n"
        "F3       log of the failed job\n"
        "F5 r     rerun\n"
        "F6 f     rerun failed jobs\n"
        "F7 c     cancel\n"
        "F8 Del   delete run;  Shift+F8 delete logs only\n"
        "Ins      mark;  + mark all;  - unmark\n"
        "1..5     filter: all/active/failed/successful/other\n"
        "/        search (Ctrl+F)\n"
        "l        latest run per workflow and branch\n"
        "s        sort by next column; click a header to sort\n"
        "F2       next window: runs, PRs, issues, deployments (Alt+1..4)\n"
        "Ctrl+L   language: English / Русский\n"
        "Ctrl+R   refresh everything\n"
        "Esc, Backspace  back / close window",
        "Enter    открыть строку: запуск, PR, issue, лог\n"
        "o        открыть в браузере (Ctrl+Enter)\n"
        "F3       лог упавшего job\n"
        "F5 r     перезапустить\n"
        "F6 f     перезапустить упавшие job\n"
        "F7 c     отменить\n"
        "F8 Del   удалить запуск;  Shift+F8 только логи\n"
        "Ins      отметить;  + все;  - снять\n"
        "1..5     фильтр: все/активные/упавшие/успешные/прочие\n"
        "/        поиск (Ctrl+F)\n"
        "l        последний запуск workflow и ветки\n"
        "s        сортировка по следующему столбцу; щелчок по заголовку\n"
        "F2       следующее окно: запуски, PR, issues, развёртывания (Alt+1..4)\n"
        "Ctrl+L   язык: English / Русский\n"
        "Ctrl+R   обновить всё\n"
        "Esc, Backspace  назад / закрыть окно");
    msgBox(text, mfInformation | mfOKButton);
}

ushort msgBox(const std::string &text, ushort options)
{
    int w = 0, lines = 0;
    size_t pos = 0;
    while (pos <= text.size())
    {
        size_t nl = text.find("\n", pos);
        if (nl == std::string::npos)
            nl = text.size();
        w = std::max(w, (int) TText::width(TStringView(text.data() + pos, nl - pos)));
        ++lines;
        pos = nl + 1;
    }
    TPoint d = TProgram::deskTop->size;
    int bw = std::max(40, std::min(w + 6, d.x - 4));
    // Длинные строки переносятся: добавить запас по высоте
    int wrapped = lines + (w + 6 > bw ? (w / std::max(1, bw - 6)) : 0);
    int bh = std::min(wrapped + 6, d.y - 2);
    TRect r(0, 0, bw, bh);
    r.move((d.x - bw) / 2, (d.y - bh) / 2);

    // Свой диалог вместо messageBox: подписи tvision зашиты по-английски
    const char *titles[] = {L("Warning", "Внимание"), L("Error", "Ошибка"), L("Information", "Информация"),
                            L("Confirm", "Подтверждение")};
    const char *names[] = {L("Yes", "Да"), L("No", "Нет"), "OK", L("Cancel", "Отмена")};
    static const ushort cmds[] = {cmYes, cmNo, cmOK, cmCancel};

    TDialog *dlg = new TDialog(r, titles[options & 0x3]);
    dlg->insert(new TStaticText(TRect(3, 2, bw - 2, bh - 3), text));
    std::vector<int> btns;
    for (int i = 0; i < 4; ++i)
        if (options & (0x0100 << i))
            btns.push_back(i);
    const int btnW = 12, gap = 2;
    int total = (int) btns.size() * btnW + ((int) btns.size() - 1) * gap;
    int x = (bw - total) / 2;
    for (size_t k = 0; k < btns.size(); ++k)
    {
        int i = btns[k];
        dlg->insert(new TButton(TRect(x, bh - 3, x + btnW, bh - 1), names[i], cmds[i],
                                k == 0 ? bfDefault : bfNormal));
        x += btnW + gap;
    }
    dlg->selectNext(False);
    ushort res = TProgram::deskTop->execView(dlg);
    TObject::destroy(dlg);
    return res;
}
