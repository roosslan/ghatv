#include "log_window.h"
#include "github.h"
#include "util.h"

#include <cctype>
#include <cstring>

namespace
{

bool startsWith(const std::string &s, const char *p)
{
    return s.compare(0, strlen(p), p) == 0;
}

// Убрать ANSI escape-последовательности и развернуть табуляции
std::string cleanLine(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    int col = 0;
    for (size_t i = 0; i < s.size(); ++i)
    {
        unsigned char c = s[i];
        if (c == 0x1B)
        {
            if (i + 1 < s.size() && s[i + 1] == '[')
            {
                i += 2;
                while (i < s.size() && !(s[i] >= 0x40 && s[i] <= 0x7E))
                    ++i;
            }
            continue;
        }
        if (c == '\t')
        {
            int n = 8 - col % 8;
            out.append(n, ' ');
            col += n;
            continue;
        }
        if (c < 0x20 || c == 0x7F)
            continue;
        out += (char) c;
        if ((c & 0xC0) != 0x80)
            ++col;
    }
    return out;
}

} // namespace

std::vector<LogLine> parseLog(const std::string &raw, int &maxWidth)
{
    std::vector<LogLine> out;
    maxWidth = 0;
    size_t pos = 0;
    // BOM в начале лога
    if (raw.compare(0, 3, "\xEF\xBB\xBF") == 0)
        pos = 3;
    while (pos <= raw.size())
    {
        size_t nl = raw.find('\n', pos);
        if (nl == std::string::npos)
            nl = raw.size();
        std::string line = raw.substr(pos, nl - pos);
        pos = nl + 1;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // Отметка времени в начале строки: 2026-10-13T10:00:00.1234567Z
        if (line.size() > 20 && isdigit((unsigned char) line[0]) && line[4] == '-' && line[10] == 'T')
        {
            size_t sp = line.find(' ');
            if (sp != std::string::npos && sp < 40)
                line = line.substr(sp + 1);
            else if (sp == std::string::npos)
                line.clear();
        }
        line = cleanLine(line);

        LogLine l;
        if (startsWith(line, "##[endgroup]"))
            continue;
        if (startsWith(line, "##[group]"))
        {
            l.text = "▸ " + line.substr(9);
            l.color = 0x0F;
            l.group = true;
        }
        else if (startsWith(line, "##[error]"))
        {
            l.text = "✘ " + line.substr(9);
            l.color = 0x0C;
            l.error = true;
        }
        else if (startsWith(line, "##[warning]"))
        {
            l.text = "⚠ " + line.substr(11);
            l.color = 0x0E;
        }
        else if (startsWith(line, "##[notice]"))
        {
            l.text = "ℹ " + line.substr(10);
            l.color = 0x0B;
        }
        else if (startsWith(line, "##[debug]"))
        {
            l.text = line.substr(9);
            l.color = 0x08;
        }
        else if (startsWith(line, "[command]"))
        {
            l.text = "$ " + line.substr(9);
            l.color = 0x0A;
        }
        else
        {
            l.text = line;
            if (startsWith(line, "Error:") || startsWith(line, "error:") || startsWith(line, "ERROR"))
            {
                l.color = 0x0C;
                l.error = true;
            }
        }
        int w = (int) TText::width(l.text);
        if (w > maxWidth)
            maxWidth = w;
        out.push_back(std::move(l));
        if (nl == raw.size())
            break;
    }
    while (!out.empty() && out.back().text.empty())
        out.pop_back();
    return out;
}

std::vector<std::string> wrapText(const std::string &line, int width)
{
    std::vector<std::string> out;
    if (width < 10)
        width = 10;
    // Отступ первой строки сохраняется у продолжений
    size_t ind = line.find_first_not_of(' ');
    std::string indent = ind == std::string::npos ? "" : line.substr(0, std::min<size_t>(ind, 16));
    std::string cur = indent;
    int curW = (int) indent.size();
    size_t i = indent.size();
    bool startOfLine = true;
    while (i <= line.size())
    {
        size_t j = line.find(' ', i);
        if (j == std::string::npos)
            j = line.size();
        std::string word = line.substr(i, j - i);
        int ww = (int) TText::width(word);
        if (startOfLine || curW + 1 + ww <= width)
        {
            if (!startOfLine)
            {
                cur += ' ';
                ++curW;
            }
            cur += word;
            curW += ww;
            startOfLine = false;
        }
        else
        {
            out.push_back(cur);
            cur = indent + word;
            curW = (int) indent.size() + ww;
        }
        i = j + 1;
    }
    out.push_back(cur);
    return out;
}

// ---------------------------------------------------------------- LogView

LogView::LogView(const TRect &r, TScrollBar *hs, TScrollBar *vs) :
    TScroller(r, hs, vs)
{
    growMode = gfGrowHiX | gfGrowHiY;
    setLimit(0, 0);
}

void LogView::setLines(std::vector<LogLine> lines, int maxWidth, const std::string &jump)
{
    lines_ = std::move(lines);
    message_.clear();
    errors_ = 0;
    for (auto &l : lines_)
        if (l.error)
            ++errors_;
    setLimit(maxWidth + 1, (int) lines_.size());

    int target = -1;
    if (jump == "#error")
    {
        for (int i = 0; i < (int) lines_.size(); ++i)
            if (lines_[i].error)
            {
                target = i;
                break;
            }
    }
    else if (!jump.empty())
    {
        std::string q = util::lower(jump);
        for (int i = 0; i < (int) lines_.size(); ++i)
            if (lines_[i].group && util::containsNoCase(lines_[i].text, q))
            {
                target = i;
                break;
            }
        // Имя шага может не совпадать с заголовком группы: тогда первая ошибка
        if (target < 0)
            for (int i = 0; i < (int) lines_.size(); ++i)
                if (lines_[i].error)
                {
                    target = i;
                    break;
                }
    }
    if (target >= 0)
        gotoLine(target);
    else
        drawView();
}

void LogView::setMessage(const std::string &msg)
{
    message_ = msg;
    drawView();
}

void LogView::gotoLine(int line)
{
    cur_ = line;
    int top = std::max(0, line - size.y / 3);
    scrollTo(0, std::min(top, std::max(0, limit.y - size.y)));
    drawView();
}

void LogView::draw()
{
    TColorAttr bg(0x07);
    for (int y = 0; y < size.y; ++y)
    {
        TDrawBuffer b;
        b.moveChar(0, ' ', bg, size.x);
        int i = delta.y + y;
        if (i < (int) lines_.size())
        {
            const LogLine &l = lines_[i];
            TColorAttr a = i == cur_ ? TColorAttr(0x30) : TColorAttr(l.color);
            if (i == cur_)
                b.moveChar(0, ' ', a, size.x);
            b.moveStr(0, l.text, a, size.x, delta.x);
        }
        else if (lines_.empty() && y == size.y / 2)
        {
            std::string msg = message_.empty() ? "Загрузка лога…" : message_;
            int w = (int) TText::width(msg);
            b.moveStr(std::max(0, (size.x - w) / 2), msg, TColorAttr(0x0E), size.x);
        }
        writeLine(0, y, size.x, 1, b);
    }
}

void LogView::findNext(int dir)
{
    if (search_.empty() || lines_.empty())
        return;
    std::string q = util::lower(search_);
    int n = (int) lines_.size();
    int start = cur_ >= 0 ? cur_ : (dir > 0 ? delta.y - 1 : delta.y + size.y);
    for (int k = 1; k <= n; ++k)
    {
        int i = ((start + dir * k) % n + n) % n;
        if (util::containsNoCase(lines_[i].text, q))
        {
            gotoLine(i);
            return;
        }
    }
    msgBox("Не найдено: " + search_, mfInformation | mfOKButton);
}

void LogView::nextError(int dir)
{
    int n = (int) lines_.size();
    int start = cur_ >= 0 ? cur_ : (dir > 0 ? delta.y - 1 : delta.y + size.y);
    for (int i = start + dir; i >= 0 && i < n; i += dir)
        if (lines_[i].error)
        {
            gotoLine(i);
            return;
        }
}

void LogView::search()
{
    char buf[128];
    strncpy(buf, search_.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    if (inputBox("Поиск в логе", "Текст:", buf, sizeof(buf) - 1) != cmCancel && buf[0])
    {
        search_ = buf;
        findNext(1);
    }
}

std::string LogView::infoText() const
{
    std::string s = "строка " + std::to_string(std::min<int>(delta.y + 1, (int) lines_.size())) + "/" +
                    std::to_string(lines_.size());
    if (hint_.empty())
        s += " │ ошибок: " + std::to_string(errors_);
    if (!search_.empty())
        s += " │ поиск: «" + search_ + "»";
    s += hint_.empty() ? " │ / поиск  n/N дальше/назад  e/E ошибка  g/G начало/конец  Esc/Backspace — назад"
                       : " │ " + hint_;
    return s;
}

void LogView::handleEvent(TEvent &ev)
{
    if (ev.what == evKeyDown)
    {
        bool handled = true;
        ushort k = ev.keyDown.keyCode;
        if (k == kbCtrlHome || k == kbCtrlPgUp)
            scrollTo(delta.x, 0);
        else if (k == kbCtrlEnd || k == kbCtrlPgDn)
            scrollTo(delta.x, std::max(0, limit.y - size.y));
        else if (keyIs(ev, "/", "."))
            search();
        else if (keyIs(ev, "n", "т"))
            findNext(1);
        else if (keyIs(ev, "N", "Т"))
            findNext(-1);
        else if (keyIs(ev, "e", "у"))
            nextError(1);
        else if (keyIs(ev, "E", "У"))
            nextError(-1);
        else if (keyIs(ev, "g", "п"))
            scrollTo(0, 0);
        else if (keyIs(ev, "G", "П"))
            scrollTo(0, std::max(0, limit.y - size.y));
        else
            handled = false;
        if (handled)
        {
            clearEvent(ev);
            drawView();
            return;
        }
    }
    TScroller::handleEvent(ev);
}

// ---------------------------------------------------------------- LogInfo

LogInfo::LogInfo(const TRect &r, LogView *v) :
    TView(r),
    v_(v)
{
    growMode = gfGrowLoY | gfGrowHiX | gfGrowHiY;
}

void LogInfo::draw()
{
    TDrawBuffer b;
    putCell(b, 0, size.x, " " + v_->infoText(), TColorAttr(clr::Info));
    writeLine(0, 0, size.x, 1, b);
}

// ---------------------------------------------------------------- LogWindow

LogWindow::LogWindow(const TRect &r, const std::string &repo, int64_t jobId, const std::string &title,
                     const std::string &jump) :
    TWindowInit(&TWindow::initFrame),
    TWindow(r, ("лог: " + title).c_str(), wnNoNumber),
    repo_(repo),
    jobId_(jobId),
    jump_(jump),
    alive_(std::make_shared<bool>(true))
{
    flags = wfMove | wfGrow | wfZoom | wfClose;
    growMode = gfGrowHiX | gfGrowHiY;
    palette = wpGrayWindow;

    TRect e = getExtent();
    e.grow(-1, -1);
    TScrollBar *hs = standardScrollBar(sbHorizontal | sbHandleKeyboard);
    TScrollBar *vs = standardScrollBar(sbVertical | sbHandleKeyboard);
    view_ = new LogView(TRect(e.a.x, e.a.y, e.b.x, e.b.y - 1), hs, vs);
    insert(view_);
    info_ = new LogInfo(TRect(e.a.x, e.b.y - 1, e.b.x, e.b.y), view_);
    insert(info_);
    view_->select();
    load();
}

void LogWindow::shutDown()
{
    *alive_ = false;
    view_ = nullptr;
    info_ = nullptr;
    TWindow::shutDown();
}

void LogWindow::load()
{
    auto alive = alive_;
    std::string repo = repo_, jump = jump_;
    int64_t id = jobId_;
    view_->setMessage("Загрузка лога…");
    gApp->async([this, alive, repo, id, jump] {
        std::string raw, err;
        bool ok = gApp->gh().jobLog(repo, id, raw, err);
        int maxW = 0;
        auto lines = ok ? std::make_shared<std::vector<LogLine>>(parseLog(raw, maxW)) : nullptr;
        gApp->post([this, alive, ok, lines, maxW, err, jump] {
            if (!*alive || !view_)
                return;
            if (!ok)
                view_->setMessage("Не удалось получить лог: " + err +
                                  (err.find("404") != std::string::npos ? " (лог появится после завершения job)" : ""));
            else if (lines->empty())
                view_->setMessage("Лог пуст");
            else
                view_->setLines(std::move(*lines), maxW, jump);
            redraw();
        });
    });
}

void LogWindow::handleEvent(TEvent &ev)
{
    TWindow::handleEvent(ev);
    if (ev.what == evKeyDown && ev.keyDown.keyCode == kbEsc)
    {
        close();
        clearEvent(ev);
        return;
    }
    if (ev.what == evCommand)
    {
        if (ev.message.command == cmSearch && view_)
            view_->search();
        else if (ev.message.command == cmRefresh)
            load();
        else
            return;
        clearEvent(ev);
        return;
    }
    // Обновить строку состояния после прокрутки
    if (info_)
        info_->drawView();
}

// ---------------------------------------------------------------- DiscussionWindow

namespace
{

void addText(std::vector<LogLine> &out, const std::string &text, int width, uchar color)
{
    size_t pos = 0;
    while (pos <= text.size())
    {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos)
            nl = text.size();
        std::string line = text.substr(pos, nl - pos);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        for (auto &part : wrapText(line, width))
        {
            LogLine l;
            l.text = part;
            l.color = color;
            out.push_back(std::move(l));
        }
        pos = nl + 1;
    }
}

void addLine(std::vector<LogLine> &out, const std::string &text, uchar color, bool group = false)
{
    LogLine l;
    l.text = text;
    l.color = color;
    l.group = group;
    out.push_back(std::move(l));
}

std::vector<LogLine> renderDiscussion(const Discussion &d, int number, int width, int &maxW)
{
    std::vector<LogLine> out;
    addLine(out, "#" + std::to_string(number) + "  " + d.title, 0x0F);
    std::string meta = "автор " + d.author + " · создан " + util::localTime(d.createdAt) + " · " +
                       (d.state == "open" ? "открыт" : d.state == "closed" ? "закрыт" : d.state);
    if (!d.labels.empty())
    {
        meta += " · метки:";
        for (auto &l : d.labels)
            meta += " [" + l + "]";
    }
    addLine(out, meta, 0x0B);
    addLine(out, "", 0x07);
    if (d.body.empty())
        addLine(out, "(без описания)", 0x08);
    else
        addText(out, d.body, width, 0x07);
    for (auto &c : d.comments)
    {
        addLine(out, "", 0x07);
        std::string head = "── " + c.author + " · " + util::localTime(c.createdAt) + " ";
        int hw = (int) TText::width(head);
        for (int i = hw; i < std::min(width, 100); ++i)
            head += "─";
        addLine(out, head, 0x0E, true);
        addText(out, c.body, width, 0x07);
    }
    if (d.comments.empty())
    {
        addLine(out, "", 0x07);
        addLine(out, "Комментариев нет", 0x08);
    }
    maxW = 0;
    for (auto &l : out)
        maxW = std::max(maxW, (int) TText::width(l.text));
    return out;
}

} // namespace

DiscussionWindow::DiscussionWindow(const TRect &r, const std::string &repo, int number, const std::string &title) :
    TWindowInit(&TWindow::initFrame),
    TWindow(r, (repo.substr(repo.find('/') + 1) + "#" + std::to_string(number) + " · " + title).c_str(), wnNoNumber),
    repo_(repo),
    number_(number),
    alive_(std::make_shared<bool>(true))
{
    flags = wfMove | wfGrow | wfZoom | wfClose;
    growMode = gfGrowHiX | gfGrowHiY;
    palette = wpGrayWindow;

    TRect e = getExtent();
    e.grow(-1, -1);
    TScrollBar *hs = standardScrollBar(sbHorizontal | sbHandleKeyboard);
    TScrollBar *vs = standardScrollBar(sbVertical | sbHandleKeyboard);
    view_ = new LogView(TRect(e.a.x, e.a.y, e.b.x, e.b.y - 1), hs, vs);
    view_->setHint("/ поиск  n/N дальше/назад  g/G начало/конец  Esc/Backspace — назад");
    insert(view_);
    info_ = new LogInfo(TRect(e.a.x, e.b.y - 1, e.b.x, e.b.y), view_);
    insert(info_);
    view_->select();
    load();
}

void DiscussionWindow::shutDown()
{
    *alive_ = false;
    view_ = nullptr;
    info_ = nullptr;
    TWindow::shutDown();
}

void DiscussionWindow::load()
{
    auto alive = alive_;
    std::string repo = repo_;
    int number = number_;
    int width = std::max(40, view_->size.x - 1);
    view_->setMessage("Загрузка…");
    gApp->async([this, alive, repo, number, width] {
        Discussion d;
        std::string err;
        bool ok = gApp->gh().discussion(repo, number, d, err);
        int maxW = 0;
        auto lines = ok ? std::make_shared<std::vector<LogLine>>(renderDiscussion(d, number, width, maxW)) : nullptr;
        gApp->post([this, alive, ok, lines, maxW, err] {
            if (!*alive || !view_)
                return;
            if (!ok)
                view_->setMessage("Не удалось загрузить: " + err);
            else
                view_->setLines(std::move(*lines), maxW, "");
            redraw();
        });
    });
}

void DiscussionWindow::handleEvent(TEvent &ev)
{
    TWindow::handleEvent(ev);
    if (ev.what == evKeyDown && ev.keyDown.keyCode == kbEsc)
    {
        close();
        clearEvent(ev);
        return;
    }
    if (ev.what == evCommand)
    {
        if (ev.message.command == cmSearch && view_)
            view_->search();
        else if (ev.message.command == cmRefresh)
            load();
        else
            return;
        clearEvent(ev);
        return;
    }
    if (info_)
        info_->drawView();
}
