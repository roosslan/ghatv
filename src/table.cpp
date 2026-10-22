#include "table.h"
#include "util.h"

#include <algorithm>
#include <cstring>

int cmpText(const std::string &a, const std::string &b)
{
    std::string la = util::lower(a), lb = util::lower(b);
    return la < lb ? -1 : la > lb ? 1 : 0;
}

std::string repoLabel(const std::string &repo, bool full)
{
    return full ? repo : repo.substr(repo.find('/') + 1);
}

// ---------------------------------------------------------------- TableHeader

TableHeader::TableHeader(const TRect &r, TableWindow *w) :
    TView(r),
    w_(w)
{
    growMode = gfGrowHiX;
}

void TableHeader::draw()
{
    TDrawBuffer b;
    TColorAttr a(clr::Header), sep(clr::Separator);
    b.moveChar(0, ' ', a, size.x);
    auto &cols = w_->columns();
    auto widths = w_->layout(size.x);
    int x = w_->lead();
    bool first = true;
    for (size_t i = 0; i < cols.size(); ++i)
    {
        if (widths[i] <= 0)
            continue;
        if (!first)
            b.moveStr(x++, "│", sep);
        first = false;
        std::string t = cols[i].title;
        if ((int) i == w_->sortCol())
            t = cols[i].right ? std::string(w_->sortDesc() ? "▼" : "▲") + t
                              : t + (w_->sortDesc() ? "▼" : "▲");
        putCell(b, x, widths[i], t, a, cols[i].right);
        x += widths[i];
    }
    writeLine(0, 0, size.x, 1, b);
}

void TableHeader::handleEvent(TEvent &ev)
{
    TView::handleEvent(ev);
    if (ev.what != evMouseDown)
        return;
    TPoint p = makeLocal(ev.mouse.where);
    auto widths = w_->layout(size.x);
    int x = w_->lead();
    bool first = true;
    for (size_t i = 0; i < widths.size(); ++i)
    {
        if (widths[i] <= 0)
            continue;
        if (!first)
            ++x;
        first = false;
        // Щелчок по разделителю относится к столбцу справа
        if (p.x >= x - 1 && p.x < x + widths[i])
        {
            w_->sortBy((int) i);
            break;
        }
        x += widths[i];
    }
    clearEvent(ev);
}

// ---------------------------------------------------------------- TableInfo

TableInfo::TableInfo(const TRect &r, TableWindow *w) :
    TView(r),
    w_(w)
{
    growMode = gfGrowLoY | gfGrowHiX | gfGrowHiY;
}

void TableInfo::draw()
{
    TDrawBuffer b;
    putCell(b, 0, size.x, " " + w_->infoText(), TColorAttr(clr::Info));
    writeLine(0, 0, size.x, 1, b);
}

// ---------------------------------------------------------------- TableList

TableList::TableList(const TRect &r, TScrollBar *vs, TableWindow *w) :
    TListViewer(r, 1, nullptr, vs),
    w_(w)
{
    growMode = gfGrowHiX | gfGrowHiY;
    setRange(0);
}

void TableList::getText(char *dest, short item, short maxLen)
{
    int idx = w_->dataAt(item);
    std::string t = idx >= 0 ? w_->cell(idx, 0).text : "";
    strncpy(dest, t.c_str(), maxLen);
    dest[maxLen - 1] = '\0';
}

void TableList::draw()
{
    bool active = (state & (sfSelected | sfActive)) == (sfSelected | sfActive);
    auto &cols = w_->columns();
    auto widths = w_->layout(size.x);

    for (int y = 0; y < size.y; ++y)
    {
        TDrawBuffer b;
        int item = topItem + y;
        int idx = item < range ? w_->dataAt(item) : -1;
        if (idx < 0)
        {
            b.moveChar(0, ' ', TColorAttr(clr::Normal), size.x);
            if (range == 0 && y == size.y / 2)
            {
                std::string msg = w_->emptyText();
                putCell(b, 0, size.x, std::string(std::max(0, (size.x - (int) TText::width(msg)) / 2), ' ') + msg,
                        TColorAttr(clr::Dim));
            }
            writeLine(0, y, size.x, 1, b);
            continue;
        }

        bool isFocused = item == focused;
        uchar base = isFocused ? (active ? clr::Focus : clr::FocusDim) : clr::Normal;
        TColorAttr text(base);
        if (w_->marked(idx))
            text = withFore(base, clr::Mark);
        else if (w_->dimmed(idx) && !isFocused)
            text = withFore(base, 0x7);
        TColorAttr sep = isFocused ? TColorAttr(base) : TColorAttr(clr::Separator);

        b.moveChar(0, ' ', TColorAttr(base), size.x);
        if (w_->lead() > 0)
        {
            uchar fore = 0;
            std::string lt = w_->leadText(idx, fore);
            putCell(b, 0, w_->lead(), lt, fore && !isFocused ? withFore(base, fore) : text);
        }
        int x = w_->lead();
        bool first = true;
        for (size_t i = 0; i < cols.size(); ++i)
        {
            if (widths[i] <= 0)
                continue;
            if (!first)
                b.moveStr(x++, "│", sep);
            first = false;
            Cell c = w_->cell(idx, (int) i);
            TColorAttr a = (c.fore && !isFocused && !w_->marked(idx)) ? withFore(base, c.fore) : text;
            putCell(b, x, widths[i], c.text, a, cols[i].right);
            x += widths[i];
        }
        writeLine(0, y, size.x, 1, b);
    }
}

void TableList::selectItem(short item)
{
    int idx = w_->dataAt(item);
    if (idx >= 0)
        w_->openRow(idx);
}

void TableList::handleEvent(TEvent &ev)
{
    if (ev.what == evKeyDown)
    {
        bool handled = true;
        if (ev.keyDown.keyCode == kbEnter)
        {
            int idx = w_->focusedData();
            if (idx >= 0)
                w_->openRow(idx);
        }
        else if (w_->rowKey(ev))
            ;
        else if (keyIs(ev, "/", "."))
            w_->search();
        else if (keyIs(ev, "s", "ы"))
            w_->nextSort();
        else
            handled = false;
        if (handled)
        {
            clearEvent(ev);
            return;
        }
    }
    TListViewer::handleEvent(ev);
}

// ---------------------------------------------------------------- TableWindow

TableWindow::TableWindow(const TRect &r, const std::string &title, short number, std::vector<ColumnDef> cols,
                         int lead, int topLines) :
    TWindowInit(&TWindow::initFrame),
    TWindow(r, title.c_str(), number),
    cols_(std::move(cols)),
    lead_(lead)
{
    flags = wfMove | wfGrow | wfZoom;
    growMode = gfGrowHiX | gfGrowHiY;
    sortDesc_ = !cols_.empty() && cols_[0].descFirst;

    TRect e = getExtent();
    e.grow(-1, -1);
    topY_ = e.a.y;
    int y = e.a.y + topLines;
    insert(new TableHeader(TRect(e.a.x, y, e.b.x, y + 1), this));
    TScrollBar *vs = standardScrollBar(sbVertical);
    list_ = new TableList(TRect(e.a.x, y + 1, e.b.x, e.b.y - 1), vs, this);
    insert(list_);
    info_ = new TableInfo(TRect(e.a.x, e.b.y - 1, e.b.x, e.b.y), this);
    insert(info_);
    list_->select();
}

std::vector<int> TableWindow::layout(int width) const
{
    std::vector<int> w(cols_.size(), 0);
    int fixed = 0, shown = 0, rest = -1;
    for (size_t i = 0; i < cols_.size(); ++i)
    {
        if (cols_[i].minTable && width < cols_[i].minTable)
            continue;
        ++shown;
        if (cols_[i].width)
        {
            w[i] = cols_[i].width;
            fixed += w[i];
        }
        else if (!cols_[i].maxWidth && rest < 0)
            rest = (int) i;
    }
    int avail = std::max(12, width - lead_ - fixed - std::max(0, shown - 1));
    int used = 0;
    for (size_t i = 0; i < cols_.size(); ++i)
        if (!cols_[i].width && cols_[i].maxWidth && !(cols_[i].minTable && width < cols_[i].minTable))
        {
            w[i] = std::max(4, std::min(avail * cols_[i].share / 100, cols_[i].maxWidth));
            used += w[i];
        }
    if (rest >= 0)
        w[rest] = std::max(1, avail - used);
    return w;
}

void TableWindow::sortBy(int col)
{
    if (col < 0 || col >= (int) cols_.size())
        return;
    if (col == sortCol_)
        sortDesc_ = !sortDesc_;
    else
    {
        sortCol_ = col;
        sortDesc_ = cols_[col].descFirst;
    }
    refresh();
}

void TableWindow::nextSort()
{
    auto widths = layout(list_->size.x);
    int n = (int) cols_.size();
    for (int k = 1; k <= n; ++k)
    {
        int c = (sortCol_ + k) % n;
        if (widths[c] > 0)
        {
            sortCol_ = c;
            sortDesc_ = cols_[c].descFirst;
            break;
        }
    }
    refresh();
}

int TableWindow::focusedRow() const
{
    return list_->focused < list_->range ? list_->focused : -1;
}

int TableWindow::focusedData() const
{
    return dataAt(focusedRow());
}

void TableWindow::rebuild()
{
    rows_.clear();
    std::string q = util::lower(search_);
    int n = dataCount();
    for (int i = 0; i < n; ++i)
    {
        if (!accept(i))
            continue;
        if (!q.empty())
        {
            bool found = false;
            for (size_t c = 0; c < cols_.size() && !found; ++c)
                found = util::containsNoCase(cell(i, (int) c).text, q);
            if (!found)
                continue;
        }
        rows_.push_back(i);
    }
    int col = sortCol_;
    bool desc = sortDesc_;
    bool defDesc = !cols_.empty() && cols_[0].descFirst;
    std::stable_sort(rows_.begin(), rows_.end(), [&](int a, int b) {
        int c = compare(a, b, col);
        if (c != 0)
            return desc ? c > 0 : c < 0;
        // При равенстве — порядок по первому столбцу (обычно время)
        c = compare(a, b, 0);
        return defDesc ? c > 0 : c < 0;
    });
    postFilter(rows_);
}

void TableWindow::refresh()
{
    int64_t cur = 0;
    bool had = false;
    // Курсор на первой строке остаётся наверху: новые строки появляются над ним
    if (int idx = focusedData(); idx >= 0 && list_->focused > 0)
    {
        cur = rowId(idx);
        had = true;
    }
    rebuild();
    list_->setRange((short) std::min<size_t>(rows_.size(), 32000));
    int pos = -1;
    if (had)
        for (int r = 0; r < rowCount(); ++r)
            if (rowId(rows_[r]) == cur)
            {
                pos = r;
                break;
            }
    if (pos >= 0)
        list_->focusItemNum(pos);
    else if (list_->focused >= list_->range)
        list_->focusItemNum(std::max(0, list_->range - 1));
    redraw();
}

void TableWindow::search()
{
    char buf[128];
    strncpy(buf, search_.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    if (inputBox("Поиск", "Текст (пусто — сбросить):", buf, sizeof(buf) - 1) != cmCancel)
    {
        search_ = buf;
        refresh();
    }
}

std::string TableWindow::viewState() const
{
    std::string s = "сортировка: " + cols_[sortCol_].title + (sortDesc_ ? " ▼" : " ▲");
    if (!search_.empty())
        s += " │ поиск: «" + search_ + "»";
    return s;
}

void TableWindow::handleEvent(TEvent &ev)
{
    TWindow::handleEvent(ev);
    if (ev.what == evBroadcast && ev.message.command == cmTick)
    {
        list_->drawView();
        info_->drawView();
        return;
    }
    if (ev.what != evCommand)
        return;
    switch (ev.message.command)
    {
    case cmSearch:
        search();
        break;
    case cmSortToggle:
        nextSort();
        break;
    default:
        if (!command(ev.message.command))
            return;
    }
    clearEvent(ev);
}
