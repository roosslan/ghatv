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
            return {"✔", "успех", 0xA};
        if (conclusion == "failure")
            return {"✘", "ошибка", 0xC};
        if (conclusion == "timed_out")
            return {"✘", "таймаут", 0xC};
        if (conclusion == "startup_failure")
            return {"✘", "сбой старта", 0xC};
        if (conclusion == "cancelled")
            return {"⊘", "отменён", 0x7};
        if (conclusion == "skipped")
            return {"»", "пропущен", 0x7};
        if (conclusion == "neutral")
            return {"○", "нейтрально", 0x7};
        if (conclusion == "action_required")
            return {"!", "нужно действие", 0xD};
        if (conclusion == "stale")
            return {"~", "устарел", 0x7};
        return {"?", conclusion.empty() ? "завершён" : conclusion, 0x7};
    }
    if (status == "in_progress")
    {
        static const char *spin[] = {"◐", "◓", "◑", "◒"};
        return {spin[time(nullptr) % 4], "выполняется", 0xE};
    }
    if (status == "queued")
        return {"◌", "в очереди", 0xD};
    if (status == "waiting")
        return {"◌", "ожидает", 0xD};
    if (status == "pending")
        return {"◌", "ожидает", 0xD};
    if (status == "requested")
        return {"◌", "запрошен", 0xD};
    return {"?", status, 0x7};
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
    const char *text =
        "Enter  открыть запуск / лог\n"
        "F3     лог упавшего job\n"
        "F5 r   перезапустить\n"
        "F6 f   перезапустить упавшие job\n"
        "F7 c   отменить\n"
        "F8 Del удалить запуск\n"
        "Shift+F8 удалить только логи\n"
        "Ins    отметить, + все, - снять\n"
        "1..5   фильтр: все/активные/упавшие/успешные/прочие\n"
        "/      поиск (Ctrl+F)\n"
        "l      только последний запуск workflow+ветка\n"
        "s      сортировка: время / репозиторий\n"
        "Ctrl+R обновить всё\n"
        "Esc    закрыть окно";
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
    static const char *titles[] = {"Внимание", "Ошибка", "Информация", "Подтверждение"};
    static const char *names[] = {"Да", "Нет", "OK", "Отмена"};
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
