#pragma once

#include "ui.h"

// Описание столбца таблицы
struct ColumnDef
{
    std::string title;
    int width = 0;           // фиксированная ширина
    int share = 0;           // доля свободного места в процентах (при width == 0)
    int maxWidth = 0;        // предел для доли; 0 у столбца без width — забирает остаток
    bool right = false;      // выравнивание по правому краю
    int minTable = 0;        // скрыть столбец, если таблица уже
    bool descFirst = false;  // первый щелчок сортирует по убыванию
};

struct Cell
{
    std::string text;
    uchar fore = 0;  // цвет текста; 0 — цвет строки
};

class TableWindow;

class TableHeader : public TView
{
public:
    TableHeader(const TRect &r, TableWindow *w);
    void draw() override;
    void handleEvent(TEvent &ev) override;

private:
    TableWindow *w_;
};

class TableInfo : public TView
{
public:
    TableInfo(const TRect &r, TableWindow *w);
    void draw() override;

private:
    TableWindow *w_;
};

class TableList : public TListViewer
{
public:
    TableList(const TRect &r, TScrollBar *vs, TableWindow *w);
    void draw() override;
    void handleEvent(TEvent &ev) override;
    void selectItem(short item) override;
    void getText(char *dest, short item, short maxLen) override;

private:
    TableWindow *w_;
};

// Окно-таблица: заголовок с сортировкой по щелчку, список строк, строка состояния.
// Подкласс задаёт данные через виртуальные методы, работая с индексами своих данных.
class TableWindow : public TWindow
{
public:
    TableWindow(const TRect &r, const std::string &title, short number, std::vector<ColumnDef> cols,
                int lead = 0, int topLines = 0);

    // Данные
    virtual int dataCount() const = 0;
    virtual bool accept(int idx) const { (void) idx; return true; }
    virtual Cell cell(int idx, int col) const = 0;
    // Сравнение по столбцу: <0, 0, >0 в порядке возрастания
    virtual int compare(int a, int b, int col) const = 0;
    virtual int64_t rowId(int idx) const = 0;
    // Обработка списка после фильтрации и сортировки
    virtual void postFilter(std::vector<int> &rows) const { (void) rows; }
    // Текст в первых lead ячейках строки (например, отметка)
    virtual std::string leadText(int idx, uchar &fore) const { (void) idx; (void) fore; return ""; }
    virtual bool dimmed(int idx) const { (void) idx; return false; }
    virtual bool marked(int idx) const { (void) idx; return false; }
    virtual std::string emptyText() const = 0;
    virtual std::string infoText() const = 0;

    // Поведение
    virtual void openRow(int idx) = 0;
    virtual bool rowKey(TEvent &ev) { (void) ev; return false; }
    virtual bool command(ushort cmd) { (void) cmd; return false; }

    void handleEvent(TEvent &ev) override;

    std::vector<int> layout(int width) const;
    const std::vector<ColumnDef> &columns() const { return cols_; }
    int lead() const { return lead_; }
    int sortCol() const { return sortCol_; }
    bool sortDesc() const { return sortDesc_; }
    void sortBy(int col);
    void nextSort();

    int rowCount() const { return (int) rows_.size(); }
    int dataAt(int row) const { return row >= 0 && row < rowCount() ? rows_[row] : -1; }
    int focusedData() const;
    int focusedRow() const;

    // Пересчитать фильтр и сортировку, сохранив позицию курсора, и перерисовать
    void refresh();
    void search();

protected:
    // Строка с текущей сортировкой и поиском для infoText()
    std::string viewState() const;

    TableList *list_;
    TableInfo *info_;
    std::string search_;
    int topY_;  // первая строка области над таблицей (для подклассов)

private:
    void rebuild();

    std::vector<ColumnDef> cols_;
    std::vector<int> rows_;
    int lead_;
    int sortCol_ = 0;
    bool sortDesc_;
};

// Помощники сравнения
int cmpText(const std::string &a, const std::string &b);
template <class T> int cmpNum(T a, T b) { return a < b ? -1 : a > b ? 1 : 0; }

// Имя репозитория для таблиц: без владельца, если все репозитории одного владельца
std::string repoLabel(const std::string &repo, bool full);
