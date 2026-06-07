# Markdown Table Editor для Notepad++

Markdown Table Editor превращает Notepad++ в удобный редактор Markdown-таблиц.
Пишите таблицу как обычный текст, а плагин выровняет колонки, сохранит Markdown-разметку и поможет быстро переставлять строки, колонки и данные.

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

1. Скачайте `MarkdownTableEditor-0.4.0-x64.zip` из последнего релиза: https://github.com/krotname/NppMarkdownTableEditor/releases/latest
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

## Команды

| Команда | Что делает |
| --- | --- |
| `Tab: align table or indent` | Выравнивает таблицу под курсором; вне таблицы работает как обычный `Tab` |
| `Align table` | Выравнивает текущую Markdown-таблицу |
| `Next cell` / `Previous cell` | Перемещает курсор между ячейками |
| `Insert row below` / `Delete row` | Добавляет или удаляет строку |
| `Insert column right` / `Delete column` | Добавляет или удаляет колонку |
| `Move row up` / `Move row down` | Перемещает текущую строку |
| `Move column left` / `Move column right` | Перемещает текущую колонку |
| `Sort rows ascending` / `Sort rows descending` | Сортирует строки по текущей колонке |
| `Convert CSV/TSV selection to table` | Превращает выделенный CSV/TSV в Markdown-таблицу |
| `Insert table...` | Вставляет новую таблицу заданного размера |

Горячие клавиши по умолчанию:

| Команда | Сочетание |
| --- | --- |
| `Tab: align table or indent` | `Tab` |
| `Align table` | `Ctrl+Alt+A` |
| `Next cell` | `Ctrl+Alt+Right` |
| `Previous cell` | `Ctrl+Alt+Left` |
| `Insert row below` | `Ctrl+Alt+Down` |
| `Delete row` | `Ctrl+Alt+Up` |
| `Insert column right` | `Ctrl+Alt+Shift+Right` |
| `Delete column` | `Ctrl+Alt+Shift+Left` |

## Сборка и тесты

Нужны Visual Studio 2022 Build Tools.

```cmd
MSBuild.exe vs.proj\NppPluginTemplate.vcxproj /p:Configuration=Release /p:Platform=x64
```

DLL будет создана здесь:

```text
bin64\MarkdownTableEditor.dll
```

Запуск smoke-тестов ядра:

```cmd
MSBuild.exe tests\CoreSmoke.vcxproj /p:Configuration=Debug /p:Platform=x64
tests\CoreSmoke.exe
```
