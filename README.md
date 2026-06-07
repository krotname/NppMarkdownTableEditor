# Markdown Table Editor для Notepad++

Markdown Table Editor - плагин Notepad++, который редактирует Markdown-таблицы с вертикальными чертами и выравнивает их как обычный текст.
Это практичный C++-порт рабочего процесса из пакета Sublime Text Table Editor: Markdown остается plain text, а плагин понимает строки и ячейки таблицы.

## Демо

![Пример выравнивания Markdown-таблицы в Notepad++](docs/demo.gif)

GIF собран из реальных скриншотов Notepad++ под Windows: временный Markdown-файл открыт в редакторе, команда `Align table` вызвана из меню плагина.

## Возможности

- Выравнивание Markdown-таблицы вокруг курсора.
- Переход к следующей или предыдущей ячейке.
- Вставка или удаление текущей строки.
- Вставка или удаление текущей колонки.
- Перемещение текущей строки вверх или вниз.
- Перемещение текущей колонки влево или вправо.
- Сортировка строк таблицы по текущей колонке по возрастанию или убыванию.
- Конвертация выделенного CSV/TSV-текста в выровненную Markdown-таблицу.
- Вставка новой Markdown-таблицы с заданным числом колонок и data-строк.
- `Tab` внутри Markdown-таблицы выравнивает таблицу.
- Сохранение Markdown-маркеров выравнивания: `---`, `:---`, `---:`, `:---:`.
- Подсчет ширины UTF-8: не-CJK символы считаются шириной 1, CJK символы - шириной 2.
- Поддержка таблиц как с внешними `|`, так и без них.
- Игнорирование escaped pipes (`\|`) при определении границ ячеек.

## Сравнение с Markdown Foundry

Markdown Foundry - это расширение для Visual Studio Code: https://marketplace.visualstudio.com/items?itemName=dvlprlife.mdfoundry

| Критерий                           | Markdown Table Editor                                                                      |
| ---------------------------------- | ------------------------------------------------------------------------------------------ |
| Редактор                           | Notepad++                                                                                  |
| Основной фокус                     | Легкое редактирование Markdown pipe-таблиц в Notepad++                                     |
| Установка                          | DLL из GitHub Release в каталог плагинов Notepad++                                         |
| Выравнивание таблицы               | Есть                                                                                       |
| Сохранение `:---`, `:---:`, `---:` | Есть                                                                                       |
| Escaped pipes `\|` внутри ячеек    | Поддерживаются                                                                             |
| Действие `Tab` внутри таблицы      | Выравнивает таблицу; вне таблицы передает обычный `Tab` в Scintilla                        |
| Вставка и удаление строк/колонок   | Есть                                                                                       |
| Перемещение строк/колонок          | Есть                                                                                       |
| Сортировка строк по колонке        | Есть, включая числовое сравнение при числовых значениях                                    |
| Конвертация CSV/TSV в таблицу      | Есть, по выделенному тексту                                                                |
| Вставка новой таблицы по размеру   | Есть, через диалог `Insert table...`                                                       |
| Учет ширины CJK-символов           | Есть                                                                                       |
| Лицензия                           | GPL-2.0                                                                                    |
| Лучше подходит для                 | Пользователей Notepad++, которым нужен быстрый table editor без перехода в другой редактор |
																																
## Команды                                                                                                                         

Команды доступны в меню `Plugins > Markdown Table Editor`.

| Команда в меню                     | Сочетание по умолчанию |
| ---------------------------------- | ---------------------- |
| Tab: align table or indent         | Tab                    |
| Align table                        | Ctrl+Alt+A             |
| Next cell                          | Ctrl+Alt+Right         |
| Previous cell                      | Ctrl+Alt+Left          |
| Insert row below                   | Ctrl+Alt+Down          |
| Delete row                         | Ctrl+Alt+Up            |
| Insert column right                | Ctrl+Alt+Shift+Right   |
| Delete column                      | Ctrl+Alt+Shift+Left    |
| Move row up                        | нет                    |
| Move row down                      | нет                    |
| Move column left                   | нет                    |
| Move column right                  | нет                    |
| Sort rows ascending                | нет                    |
| Sort rows descending               | нет                    |
| Convert CSV/TSV selection to table | нет                    |
| Insert table...                    | нет                    |

`Tab` учитывает контекст: если курсор находится внутри Markdown-таблицы, таблица выравнивается.
Вне таблицы плагин передает стандартную команду Scintilla `Tab`.

## Сборка

Нужны Visual Studio 2022 Build Tools.

```cmd
MSBuild.exe vs.proj\NppPluginTemplate.vcxproj /p:Configuration=Release /p:Platform=x64
```

DLL будет записана сюда:

```text
bin64\MarkdownTableEditor.dll
```

## Локальная установка

Для 64-битной установки Notepad++ скопируйте DLL в каталог:

```text
C:\Program Files\Notepad++\plugins\MarkdownTableEditor\MarkdownTableEditor.dll
```

После копирования DLL перезапустите Notepad++.

## Тесты

Быстрая сборка и запуск тестов ядра через MSBuild:

```cmd
MSBuild.exe tests\CoreSmoke.vcxproj /p:Configuration=Debug /p:Platform=x64
tests\CoreSmoke.exe
```

Альтернативный запуск напрямую через `cl`:

```cmd
cl /EHsc /W4 /WX /std:c++14 /I src tests\CoreSmoke.cpp src\MarkdownTableCore.cpp /Fe:tests\CoreSmoke.exe
tests\CoreSmoke.exe
```

Smoke-тесты проверяют выравнивание, навигацию по ячейкам, операции со строками и колонками, сортировку, CSV/TSV-конвертацию, создание таблицы, escaped pipes, таблицы без внешнего `|`, UTF-8/CJK-ширину и защиту от некорректного удаления строк.

## Примечания

Проект основан на официальном C++-шаблоне плагина Notepad++.
Поведение редактирования таблиц вдохновлено пакетом Sublime Text Table Editor, но C++-ядро в этом репозитории реализовано с нуля для Notepad++.
