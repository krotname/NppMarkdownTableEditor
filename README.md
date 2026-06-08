# Markdown Table Editor для Notepad++

[![CI](https://github.com/krotname/NppMarkdownTableEditor/actions/workflows/CI_build.yml/badge.svg)](https://github.com/krotname/NppMarkdownTableEditor/actions/workflows/CI_build.yml)
[![codecov](https://codecov.io/gh/krotname/NppMarkdownTableEditor/branch/master/graph/badge.svg)](https://codecov.io/gh/krotname/NppMarkdownTableEditor)
[![Release](https://img.shields.io/github/v/release/krotname/NppMarkdownTableEditor?label=release)](https://github.com/krotname/NppMarkdownTableEditor/releases/latest)
[![License](https://img.shields.io/github/license/krotname/NppMarkdownTableEditor)](LICENSE)

Markdown Table Editor превращает Notepad++ в удобный редактор Markdown-таблиц.
Берёте чужую косую таблицу или сгенерированную ИИ, жмете ТАБ, а плагин выровняет колонки, сохранит Markdown-разметку
и поможет быстро переставлять строки, колонки и данные.

## Другие версии

- Для JetBrains IDEs: [IdeaMarkdownTableEditor](https://github.com/krotname/IdeaMarkdownTableEditor)

## Демо

![Пример выравнивания Markdown-таблицы в Notepad++](docs/demo.gif)

GIF собран из реальных скриншотов Notepad++ под Windows: открыт обычный `.md` файл, команда `Align table` вызвана из меню плагина.

## Зачем он нужен

- Не нужно уходить из Notepad++ в отдельный Markdown-редактор только ради таблиц. 
- Большие pipe-таблицы остаются читаемыми в plain text.
- `Tab`, сортировка и операции со строками/колонками экономят ручное выравнивание.
- CSV/TSV можно быстро превратить в аккуратную Markdown-таблицу.
- UTF-8 и CJK-символы учитываются при расчете ширины колонок.

## Возможности

- `Tab` внутри Markdown-таблицы выравнивает таблицу.
- Выравнивание таблицы вокруг курсора.
- Переход к следующей или предыдущей ячейке.
- Вставка, удаление и перемещение строк.
- Вставка, удаление и перемещение колонок.
- Сортировка строк по текущей колонке по возрастанию или убыванию.
- Конвертация выделенного CSV/TSV-текста в Markdown-таблицу.
- Вставка новой таблицы с выбранным числом колонок и строк.
- Сохранение Markdown-маркеров выравнивания: `---`, `:---`, `---:`, `:---:`.
- Корректная обработка escaped pipes: `\|`.

## Установка

1. Скачайте ZIP-архив из последнего релиза: https://github.com/krotname/NppMarkdownTableEditor/releases/latest
2. Распакуйте архив.
3. Скопируйте папку `MarkdownTableEditor` в каталог плагинов Notepad++.
4. Перезапустите Notepad++.
5. Откройте меню `Plugins > Markdown Table Editor`.

Обычный путь для 64-битного Notepad++:

```text
C:\Program Files\Notepad++\plugins\MarkdownTableEditor\MarkdownTableEditor.dll
```

Если Windows не дает писать в `Program Files`, установите плагин в пользовательский каталог:

```text
%LOCALAPPDATA%\Notepad++\plugins\MarkdownTableEditor\MarkdownTableEditor.dll
```

## Публикация

- PR в официальный Notepad++ Plugin List: https://github.com/notepad-plus-plus/nppPluginList/pull/1111

## Совместимость

| Архитектура Notepad++ | Минимальная версия |
| --------------------- | ------------------ |
| x86                   | 7.5.9              |
| x64                   | 8.3.1              |
| ARM64                 | 8.3.1              |

На x64 Notepad++ 7.5.9-8.2.1 плагин загружается и пункт меню появляется, но команды редактирования таблиц не меняют документ. Notepad++ 7.5.9 не выпускался в ARM64-сборке.

## Команды

| Команда                                        | Что делает                                                               |
| ---------------------------------------------- | ------------------------------------------------------------------------ |
| `Tab: align table or indent`                   | Выравнивает таблицу под курсором; вне таблицы работает как обычный `Tab` |
| `Align table`                                  | Выравнивает текущую Markdown-таблицу                                     |
| `Next cell` / `Previous cell`                  | Перемещает курсор между ячейками                                         |
| `Insert row below` / `Delete row`              | Добавляет или удаляет строку                                             |
| `Insert column right` / `Delete column`        | Добавляет или удаляет колонку                                            |
| `Move row up` / `Move row down`                | Перемещает текущую строку                                                |
| `Move column left` / `Move column right`       | Перемещает текущую колонку                                               |
| `Sort rows ascending` / `Sort rows descending` | Сортирует строки по текущей колонке                                      |
| `Convert CSV/TSV selection to table`           | Превращает выделенный CSV/TSV в Markdown-таблицу                         |
| `Insert table...`                              | Вставляет новую таблицу заданного размера                                |

Например, выделите `Name,Score` и следующую строку `Anna,10`.
Выполните `Plugins > Markdown Table Editor > Convert CSV/TSV selection to table`.
Получится Markdown-таблица с колонками `Name` и `Score`.

Горячие клавиши по умолчанию:

| Команда                      | Сочетание              |
| ---------------------------- | ---------------------- |
| `Tab: align table or indent` | `Tab`                  |
| `Align table`                | `Ctrl+Alt+A`           |
| `Next cell`                  | `Ctrl+Alt+Right`       |
| `Previous cell`              | `Ctrl+Alt+Left`        |
| `Insert row below`           | `Ctrl+Alt+Down`        |
| `Delete row`                 | `Ctrl+Alt+Up`          |
| `Insert column right`        | `Ctrl+Alt+Shift+Right` |
| `Delete column`              | `Ctrl+Alt+Shift+Left`  |

## Сборка и тесты

Нужны Visual Studio 2022 Build Tools. Команды ниже запускайте из Developer Command Prompt или после добавления MSBuild в `PATH`.

```cmd
msbuild Package.proj /t:Package /p:Configuration=Release /p:Platform=x64
```

Версия плагина задается только в `Version.props`. `Package.proj`, DLL version resource и release ZIP-имена читают это значение; `/p:Version` не является источником версии и при несовпадении с `Version.props` считается ошибкой.

Готовые ZIP-архивы появятся в папке `build`.

Ручная сборка через MSBuild:

```cmd
msbuild vs.proj\MarkdownTableEditor.vcxproj /p:Configuration=Release /p:Platform=x64
```

DLL будет создана здесь:

```text
bin64\MarkdownTableEditor.dll
```

Запуск smoke-тестов ядра:

```cmd
msbuild Package.proj /t:RunCoreSmokeTests
```

Отчет покрытия C++ core:

```cmd
msbuild Package.proj /t:Coverage /p:Configuration=Debug /p:Platform=x64
```

Cobertura XML появится в `build/reports/coverage/coverage.cobertura.xml`.
