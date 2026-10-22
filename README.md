# ghatv

[![CI](https://github.com/roosslan/ghatv/actions/workflows/ci.yml/badge.svg)](https://github.com/roosslan/ghatv/actions/workflows/ci.yml)
[![Релиз](https://img.shields.io/github/v/release/roosslan/ghatv)](https://github.com/roosslan/ghatv/releases)

Текстовая панель GitHub Actions по всем репозиториям аккаунта в стиле файлового менеджера. Интерфейс построен на [Turbo Vision](https://github.com/magiblot/tvision) и открывается в браузере через [ttyd](https://github.com/tsl0922/ttyd). Всё работает в одном Docker-контейнере.

```
браузер ──ws──> ttyd ──pty──> ghatv (C++/tvision) ──HTTPS──> api.github.com
```

![Запуски, запуск, лог, pull request'ы, карточка PR, issues](docs/screenshot.gif)

## Возможности

- Три основных окна: запуски workflow, открытые pull request'ы и открытые issues по всем репозиториям.
- Список запусков: время запуска (дата и время), статус, репозиторий, workflow, ветка, событие, заголовок, длительность.
- Сортировка по любому столбцу щелчком мыши по заголовку; повторный щелчок меняет направление.
- Фильтры: все, активные (выполняются, в очереди, ожидают), упавшие, успешные, прочие. Поиск по тексту, режим «только последний запуск» для каждой пары workflow и ветки.
- Окно запуска: jobs, steps и annotations (короткие сообщения об ошибках). Пока запуск не завершён, окно обновляется само.
- Просмотр лога job: подсветка групп, ошибок и предупреждений, переход к ошибке, поиск.
- Действия: перезапуск, перезапуск только упавших job, отмена, удаление запуска, удаление только логов. Действия применяются к текущей строке или ко всем отмеченным.
- Окно открытых pull request'ов. Статус CI определяется по уже загруженным запускам для последнего коммита PR, без дополнительных запросов. Enter или двойной щелчок открывает карточку PR: ветки, число коммитов и файлов, состояние слияния и все запуски CI для последнего коммита; из карточки открываются описание и комментарии.
- Окно открытых issues: issues в своих репозиториях и issues, созданные пользователем в чужих репозиториях (через поиск GitHub). Enter открывает описание и комментарии.
- Опрос с ETag. Ответы 304 не расходуют лимит API. Репозитории с активными запусками опрашиваются чаще.

## Токен

Нужен токен GitHub с доступом к Actions:

- classic PAT или токен `gh` со scope `repo` (для публичных репозиториев достаточно `public_repo`);
- fine-grained PAT с правами **Actions: Read and write**, **Metadata: Read**, **Pull requests: Read** и **Issues: Read**. Для одного только просмотра хватит Actions: Read.

Issues, созданные в чужих репозиториях, ищутся через поиск GitHub и видны только в пределах прав токена.

Если установлен GitHub CLI, можно взять его токен: `gh auth token`.

## Запуск

Через docker compose (порт доступен только с этого компьютера):

```bash
GH_TOKEN=$(gh auth token) docker compose up -d --build
```

Затем открыть http://localhost:7681.

Готовый образ из GitHub Container Registry (собирается CI из ветки `trunk` и тегов релизов):

```bash
docker run -d --name ghatv -p 127.0.0.1:7681:7681 -e GH_TOKEN=$(gh auth token) -e TZ=Europe/Moscow ghcr.io/roosslan/ghatv:latest
```

Сборка образа вручную, без compose:

```bash
docker build -t ghatv .
```

```bash
docker run -d --name ghatv -p 127.0.0.1:7681:7681 -e GH_TOKEN=$(gh auth token) -e TZ=Europe/Moscow ghatv
```

Токен можно положить в файл `.env` рядом с `docker-compose.yml` (строка `GH_TOKEN=...`). Файл исключён из git.

Каждая вкладка браузера запускает отдельный экземпляр ghatv со своим опросом API.

## Настройка

| Переменная | По умолчанию | Назначение |
|---|---|---|
| `GH_TOKEN` | — | токен GitHub (также читается `GITHUB_TOKEN`) |
| `GHATV_AFFILIATION` | `owner` | какие репозитории брать: `owner`, `collaborator`, `organization_member` или их список через запятую |
| `GHATV_OWNERS` | — | оставить только этих владельцев (через запятую) |
| `GHATV_REPOS` | — | явный список репозиториев `owner/name` или `name`; отключает остальные фильтры |
| `GHATV_EXCLUDE` | — | исключить репозитории |
| `GHATV_INCLUDE_FORKS` | `false` | показывать форки |
| `GHATV_INCLUDE_ARCHIVED` | `false` | показывать архивные репозитории |
| `GHATV_RUNS_PER_REPO` | `20` | сколько последних запусков брать из каждого репозитория (до 100) |
| `GHATV_INTERVAL` | `60` | интервал опроса репозитория без активных запусков, секунд |
| `GHATV_ACTIVE_INTERVAL` | `10` | интервал опроса репозитория с активными запусками, секунд |
| `GHATV_PULLS` | `true` | опрашивать открытые pull request'ы (с интервалом `GHATV_INTERVAL`) |
| `GHATV_ISSUES` | `true` | опрашивать открытые issues (с интервалом `GHATV_INTERVAL`) |
| `GHATV_THREADS` | `6` | число параллельных запросов при опросе |
| `GITHUB_API_URL` | `https://api.github.com` | адрес API |
| `TZ` | — | часовой пояс для отображения времени |

Список репозиториев перечитывается раз в 10 минут или по Ctrl+R.

## Клавиши

| Клавиша | Действие |
|---|---|
| Enter, двойной щелчок | открыть строку: запуск, карточку PR, issue; в окне запуска — лог job или шага |
| Esc, Backspace | назад: закрыть текущее окно (в диалогах Backspace стирает текст) |
| F3 | лог упавшего job |
| F5, r | перезапустить |
| F6, f | перезапустить упавшие job |
| F7, c | отменить |
| F8, Del | удалить запуск |
| Shift+F8 | удалить только логи |
| Ins | отметить строку; `+` отметить все видимые, `-` снять отметки |
| 1…5 | фильтр: все / активные / упавшие / успешные / прочие |
| /, Ctrl+F | поиск |
| l | только последний запуск workflow и ветки |
| щелчок по заголовку столбца | сортировка по столбцу; повторный щелчок — обратный порядок |
| s | сортировка по следующему столбцу |
| F2 | следующее основное окно: запуски → PR → issues |
| Alt+1, Alt+2, Alt+3 | запуски / pull request'ы / issues |
| Ctrl+R | обновить всё |
| F10, F9 | меню |
| Alt+X | выход |

В окне pull request'ов: Enter — карточка PR, F3 — лог упавшего запуска PR, F5/F6/F7 — перезапуск / перезапуск упавших / отмена запусков PR, `1…5` — фильтр по статусу CI, `d` — скрыть черновики, `/` — поиск, `s` — сортировка. Удаление запусков из этого окна не выполняется.

В карточке PR: Enter — запуск, `d` или щелчок по строке «Описание и комментарии» — обсуждение, F3 — лог, F5/F6/F7 — действия над запуском.

В окне issues: Enter — описание и комментарии, `1` — все, `2` — в своих репозиториях, `3` — созданные пользователем в чужих.

Статус CI в списке PR берётся из последних `GHATV_RUNS_PER_REPO` запусков репозитория: у давно не обновлявшегося PR он может отображаться как «нет CI».

В окне лога: `/` поиск, `n`/`N` следующее/предыдущее совпадение, `e`/`E` следующая/предыдущая ошибка, `g`/`G` начало/конец.

Буквенные клавиши работают и в русской раскладке.

## Сборка без Docker

Нужны компилятор C++17, CMake, libcurl, nlohmann-json и ncursesw. Turbo Vision подтягивается через CMake FetchContent.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build -j
```

Бинарник для Linux x86_64 (без ttyd, запускается прямо в терминале) прикладывается к каждому [релизу](https://github.com/roosslan/ghatv/releases). Он собран в Debian 12 и требует glibc 2.36 или новее, libcurl4 и libncursesw6.

## Тесты

Модульные тесты проверяют разбор ответов GitHub API, разбор логов, перенос текста, сводный статус CI и вспомогательные функции. Сеть для них не нужна.

```bash
cmake -S . -B build -DGHATV_TESTS=ON
```

```bash
cmake --build build -j
```

```bash
ctest --test-dir build --output-on-failure
```

## CI и релизы

- `.github/workflows/ci.yml` — на каждый push и pull request: сборка, тесты, сборка Docker-образа. Из ветки `trunk` и тегов образ публикуется в `ghcr.io/roosslan/ghatv`.
- `.github/workflows/release.yml` — на тег `v*`: сборка в Debian 12, тесты, архив `ghatv-<версия>-linux-x86_64.tar.gz` с контрольной суммой и релиз GitHub с автоматическим списком изменений.

Для выпуска релиза необходимо поднять версию в `project(ghatv VERSION ...)` в `CMakeLists.txt` и создать тег с той же версией, например `v0.2.0`. Если тег не совпадает с версией в `CMakeLists.txt`, релиз не создаётся.

## Скриншот

`docs/screenshot.gif` создаётся скриптом `tools/screenshot.py`: ghatv запускается в виртуальном терминале (pyte), после каждого набора нажатий снимается кадр, кадры собираются в GIF (для имени файла `.png` сохраняется один снимок). Для обновления анимации необходимо собрать образ и выполнить:

```bash
docker run --rm -e GH_TOKEN=$(gh auth token) -e TZ=Europe/Moscow -v "$PWD:/work" --entrypoint sh ghatv:latest -c "apt-get update -qq && apt-get install -y -qq python3-pyte python3-pil python3-fonttools fonts-dejavu-core && python3 /work/tools/screenshot.py --out /work/docs/screenshot.gif --wait 25 --key-wait 5 --keys '\r' --keys '\x1b[B|\r' --keys '\x1b|\x1b|\x1b2' --keys '\x1b[B|\r' --keys '\x1b|\x1b3'"
```

Каждый `--keys` — нажатия перед очередным кадром, разделённые `|`: `\r` — Enter, `\x1b` — Esc, `\x1b[B` — стрелка вниз, `\x1b2` — Alt+2.

## Структура

- `src/github.*` — клиент GitHub REST API (libcurl, кеш ETag, учёт лимитов) и разбор ответов;
- `src/poller.*` — фоновый опрос запусков, PR и issues;
- `src/table.*` — общее окно-таблица: столбцы, сортировка по щелчку, поиск;
- `src/runs_window.*` — окно со списком запусков;
- `src/pulls_window.*` — окно открытых pull request'ов;
- `src/pull_window.*` — карточка PR;
- `src/issues_window.*` — окно открытых issues;
- `src/run_window.*` — окно запуска: jobs, steps, annotations;
- `src/log_window.*` — просмотр лога job, описания и комментариев;
- `src/main.cpp` — приложение, меню, строка состояния, действия над запусками;
- `tests/` — модульные тесты;
- `tools/screenshot.py` — создание анимации для README;
- `.github/workflows/` — CI и релизы.

## Лицензия

[MIT](LICENSE).
