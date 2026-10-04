# Исправление и проверка Markdown Table Editor для Notepad++

Изменение общего ядра готово только после одинакового результата C++, Java и
TypeScript на общей golden-фикстуре. Изменение команды редактора дополнительно
проверяется на точном диапазоне замены: соседний текст и пустые строки сохраняются.
Этот ранбук фиксирует опыт совместного исправления пяти репозиториев 03.10.2026.

## Связанные репозитории и границы

| Репозиторий | Ответственность | Ранбук |
| --- | --- | --- |
| NppMarkdownTableEditor | C++ core, Scintilla, DLL | Этот документ |
| IdeaMarkdownTableEditor | Java core, JetBrains adapter | [IDEA](https://github.com/krotname/IdeaMarkdownTableEditor/blob/main/docs/RUNBOOK.md) |
| VsCodeMarkdownTableEditor | TypeScript core, Extension Host | [VS Code](https://github.com/krotname/VsCodeMarkdownTableEditor/blob/main/docs/RUNBOOK.md) |
| MarkdownTableEditorSite | Публичный webroot, GHCR image | [Сайт](https://github.com/krotname/MarkdownTableEditorSite/blob/main/docs/RUNBOOK.md) |
| ProdOps | GitOps, Blue, nginx, HTTPRoute | [Production](https://github.com/krotname/ProdOps/blob/main/docs/MARKDOWN-TABLE-EDITOR-RUNBOOK.md) |

Общее поведение меняется в `src/MarkdownTableCore.cpp`,
`core/src/main/java/name/krot/markdowntable/core/MarkdownTableEngine.java` в IDEA
и `src/core.ts` в VS Code. Диапазоны документа, выделение и обработка событий
остаются в адаптерах. Исправление только одного порта оставляет регрессию в других.

## Минимальный проход по багу

1. Сохранить минимальный вход, выбранный диапазон/курсор и ожидаемый результат.
   В баге редактора указать соседние строки, LF/CRLF/CR и способ вызова команды.
2. Воспроизвести неверный результат до изменения. Различать ошибку core и ошибку
   извлечения/замены диапазона адаптером.
3. Для общего поведения добавить случай во все три golden-файла и проверить
   исправление всеми тремя ядрами. Для диапазона добавить тест адаптера.
4. Выполнить только применимые проверки ниже, затем PR и проверки окончательного SHA.

### Паритет golden-файлов

Файлы должны быть семантически одинаковыми JSON, включая вход, результат, диапазон
и ошибку. Сравнение только имён случаев или количества тестов недостаточно.
Из каталога Npp при соседнем расположении репозиториев:

```powershell
@'
import json
from pathlib import Path
base = Path.cwd().parent
paths = [
    base / 'NppMarkdownTableEditor/test-fixtures/markdown-table-core-golden.json',
    base / 'IdeaMarkdownTableEditor/core/src/test/resources/markdown-table-core-golden.json',
    base / 'VsCodeMarkdownTableEditor/test-fixtures/markdown-table-core-golden.json',
]
fixtures = [json.loads(p.read_text(encoding='utf-8-sig')) for p in paths]
assert fixtures[0] == fixtures[1] == fixtures[2], 'golden fixtures differ'
print('C++/Java/TypeScript golden parity: OK')
'@ | python -
```

Затем запустить тесты каждого ядра по его ранбуку: одинаковый JSON сам по себе
не доказывает, что три реализации дают ожидаемые результаты.

### Границы CSV/TSV: обязательные сценарии при изменении парсера

В обозначениях ниже `\t`, `\n`, `\r` — управляющие символы тестового входа.

| Сценарий | Пример или условие | Ожидание |
| --- | --- | --- |
| Пустые крайние ячейки | `\tA\t`, строка только из TAB | Ячейки и запись сохраняются |
| Неровная TSV | Заголовок с запятой, body с TAB | Разделитель учитывает body |
| Позднее quoted TAB | `Title, notes\nA\tfoo\nB\tbar\n\t"x\ty"` | Остаётся TSV |
| Многострочная CSV | TAB внутри продолжения quoted-поля | Продолжение не становится TSV-записью |
| Литеральная кавычка | После обычного текста ячейки | Не открывает многострочное поле |
| Одна ячейка | Крайняя/средняя запись рядом с таблицей | Core и выбранный диапазон согласованы |
| Конец диапазона | LF, CRLF, CR, несколько пустых строк | Точный исходный суффикс сохранён |

Определение разделителя использует выбранную грамматику и начала **логических**
записей. Физическая строка внутри quoted CSV-поля не голосует за TSV. При равной
структурной поддержке нельзя безусловно выбирать TAB только по его наличию внутри
значения; нельзя и безусловно выбирать CSV при любом quoted TAB.

Некоторые входы допустимы в двух грамматиках. Например,
`Title, notes\nSmith, John\t10\nDoe, Jane\t20` допускает двухколоночную CSV.
Сначала проверить документированный выбор и существующие фикстуры; для TSV
можно явно заключить одноколоночный заголовок в кавычки: `"Title, notes"`.
Замечание ревью без однозначного ожидаемого результата не заменяет воспроизведение.

## Локальные проверки Windows

Нужны Visual Studio 2022 Build Tools с C++ workload. Из Developer Command Prompt:

```cmd
msbuild Package.proj "/t:RunCoreSmokeTests;CorePerformance" /p:Configuration=Release /p:Platform=x64
```

В обычном PowerShell уже установленный MSBuild можно вызвать абсолютным путём:

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe' Package.proj '/t:RunCoreSmokeTests;CorePerformance' /p:Configuration=Release /p:Platform=x64
```

Для изменений shortcuts, совместимости или упаковки соответственно:

```cmd
msbuild Package.proj /t:RunPluginShortcutSmokeTests /p:Configuration=Release /p:Platform=x64
msbuild Package.proj /t:RunCompatibilityScriptSafetyTests
msbuild Package.proj /t:Coverage /p:Configuration=Debug /p:Platform=x64
msbuild Package.proj /t:Package /p:Configuration=Release /p:Platform=x64
```

Покрытие: `build/reports/coverage/coverage.cobertura.xml`. Версия задаётся в
`Version.props`; `/p:Version` не заменяет каноническую версию. Результат `Package`
и merge исправления не означают публикацию в Plugin List или новый релиз.

### Latest Notepad++ smoke и GitHub API

При `403`/rate limit в latest-release lookup сначала проверить способ авторизации
API, а не переделывать DLL. `Invoke-NppCompatibilitySmoke.ps1` использует `GH_TOKEN`
только для HTTPS и точного host `api.github.com`, с timeout 30 секунд. В CI токен
передаётся окружением с `contents: read`; секрет не включать в URL, аргументы или лог.
Без токена разрешён анонимный запрос, но доступная квота не гарантирована.
Safety-тест проверяет host, HTTPS, timeout и отсутствие токена на постороннем URL.

## PR, CI и завершение

1. Начать отдельную ветку `feature/…` от свежего `origin/master`, сохранить чужие
   изменения и занятые worktree. Добавлять в commit только свои файлы.
2. После целевых проверок commit/push/PR. Привязать проверки к точному финальному
   head SHA; падение сетевого smoke отличать от ошибки продукта по логам.
3. Для кода провести применимое ревью. После исправлений проверить delta и CI;
   не превращать ревью в бесконечный цикл. Docs-only diff не требует code/security
   review. Какие CI запускаются для документации, определяет актуальный workflow.
4. Merge без force/admin обхода; дождаться применимых trunk checks именно merge SHA.
   Синхронизировать свой checkout и проверить `git status`.
5. Для релиза отдельно выполнить release workflow и readback опубликованных
   артефактов. Изменение ранбука само по себе не требует версии или релизного тега.

`gh pr view <PR> --json headRefOid,statusCheckRollup` и
`gh run list --commit <SHA> --json workflowName,status,conclusion,url` помогают
разделить проверенный head, merge и опубликованный результат. Отсутствие запуска
из-за документированного path filter не обозначать как успешный CI-run.

## Проверенный исторический результат

03.10.2026: 85 scenario-проверок, 188 golden-проверок; performance gates прошли.
Это снимок аудита, а не неизменное ожидаемое количество тестов. Исправления:
[PR #40](https://github.com/krotname/NppMarkdownTableEditor/pull/40),
[#41](https://github.com/krotname/NppMarkdownTableEditor/pull/41),
[#42](https://github.com/krotname/NppMarkdownTableEditor/pull/42),
[#43](https://github.com/krotname/NppMarkdownTableEditor/pull/43),
[#44](https://github.com/krotname/NppMarkdownTableEditor/pull/44).
