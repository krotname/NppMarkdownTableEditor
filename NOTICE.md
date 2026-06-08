# Уведомления и лицензии

Markdown Table Editor для Notepad++ распространяется как GPL-3.0-or-later.
Корневой `license.txt` содержит текст GNU General Public License v3.

## Собственный код проекта

C++-ядро Markdown-таблиц и интеграционный код плагина являются частью этого проекта.
Исходники, у которых нет отдельного заголовка стороннего автора, распространяются как GPL-3.0-or-later.

## Сторонние файлы и шаблоны

Проект использует официальный C++-шаблон плагина Notepad++:

- https://github.com/npp-plugins/plugintemplate

Часть файлов шаблона и интерфейсов Notepad++ сохраняет исходные copyright notices Don HO,
Jens Lorenz и других авторов. В репозитории встречаются файлы с заголовками
GPL-2.0-or-later и GPL-3.0-or-later; для собранного пакета проекта используется
GPL-3.0-or-later, совместимая с этими "or later" уведомлениями и текущими GPLv3
заголовками Notepad++.

Файл `src/MarkdownTableEditor.rc` адаптирован из шаблона Notepad++ plugin demo и сохраняет
LGPL-3.0-or-later уведомление для этого resource-файла.

Файлы Scintilla (`src/Scintilla.h`, `src/Sci_Position.h`) используют лицензию Scintilla.

Тесты golden fixtures используют single-header библиотеку `nlohmann/json`, расположенную в
`tests/third_party/nlohmann`, по лицензии MIT.

Тексты лицензий, которые относятся к сторонним файлам:

- `LICENSES/GPL-3.0-or-later.txt`
- `LICENSES/LGPL-3.0-or-later.txt`
- `LICENSES/Scintilla.txt`
- `tests/third_party/nlohmann/LICENSE.MIT`

## Вдохновение

Рабочий процесс редактирования Markdown-таблиц вдохновлен пакетом Sublime Text Table Editor:

- https://packagecontrol.io/packages/Table%20Editor
- https://github.com/vkocubinsky/SublimeTableEditor

C++-ядро таблиц в этом репозитории является самостоятельной реализацией для Notepad++.
