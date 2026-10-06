# Домашний CI Notepad++

Windows jobs выполняются в одноразовой Windows VM на Black. Оператор на White
создаёт свежий диск из закрытой базовой VM, выдаёт repository-scoped JIT одному job,
сохраняет результат и логи, затем останавливает VM и удаляет только её диск.
Первый реальный [GUI smoke](https://github.com/krotname/NppMarkdownTableEditor/actions/runs/37389564596/job/112031144077)
прошёл для Notepad++ 7.5.9 Win32, 8.3.1 x64 и 8.9.8.1 x64 на head `607efd2`.
Это подтверждает GUI-среду; полный CI окончательного head проверяется отдельно.

## Маршруты и границы доверия

| Job | Runner |
| --- | --- |
| Windows build, GUI, plugin CodeQL, release и interface sync | `self-hosted, Windows, X64, adler-white-npp, adler-black-ephemeral` |
| Actionlint, dependency review, portable-core CodeQL, Scorecard | `arc-prod-adler-npp-mte` |
| Fork, посторонний автор, bot или повторный запуск посторонним | Пропуск; hosted fallback отсутствует |

Оба actor, автор PR и исходный репозиторий должны принадлежать владельцу.
Release и interface sync сохраняют отдельные guards ветки `master`/тега `v*`.
`CI_WINDOWS_RUNS_ON` и `CI_LINUX_RUNS_ON` допускают JSON-массив labels только
соответствующего домашнего пула. Общие или hosted runners не подставлять.
Отказ домашнего сервера оставляет job в очереди.

Политика Actions `all_external_contributors` требует предварительного решения
владельца для workflows внешних участников. Guards в YAML не изолируют произвольно
изменённый workflow: перед таким решением нужно проверить точный head и отсутствие
прямых запросов домашнего runner. PAT, SSH и Vault остаются у оператора White;
гость получает только одноразовую JIT-конфигурацию и штатный `GITHUB_TOKEN` job.

## Среда Windows

Канонический lifecycle находится в `krotname/VpnOps`, `ops/ci-windows-black`.
Деплой оператора разрешён только из `main`; White запускает ограниченную задачу
планировщика каждые пять минут без перекрытия. Не регистрировать постоянный runner
на рабочем desktop White и не переносить туда сборки из изолированной VM.

На Black VM ограничена 24 GiB RAM, шестью vCPU и отдельным дисковым томом 48 GiB.
Гость Windows Server 2022 Evaluation имеет ограниченный срок лицензии; базовую
среду нужно заменить до его окончания. Сеть гостя закрывает домашние/частные адреса
и IPv6, сохраняя публичные загрузки и DNS. Используется QEMU `-audio none`.

Базовая среда содержит Git, PowerShell 7/5.1, GitHub CLI без личного входа,
Actions runner и Visual Studio Community 2026 с MSBuild, v143 x86/x64/ARM64 и
`CodeCoverage.Console`. `Package.proj` использует full-framework MSBuild,
а не `dotnet msbuild`. Недостающие OS/C++ компоненты устанавливают в базовую среду;
job непривилегированного `ci-build` не меняет Visual Studio.

Автовход `ci-build` использует защищённое хранилище Windows; GUI runner запускается
в интерактивной Session 1. Session 0 для Notepad++ smoke непригодна. Единственный
worker последовательно выполняет полную матрицу; новый job получает новый диск.
Исчезнувшая регистрация runner сама по себе не подтверждает успех: требуются
actual job ID, head SHA, conclusion и завершение bootstrap гостя.

## Приёмка

1. На окончательном head потребовать все шесть `Build and tests`: Debug/Release ×
   x64/Win32/ARM64, включая coverage Debug x64 минимум 70%, performance Release x64,
   safety tests и три Release DLL artifact.
2. Потребовать `Tests / Notepad++ compatibility smoke` с `NppCompatibilitySmoke`
   и `NppLatestSmoke`, а также Windows plugin CodeQL и Linux core CodeQL.
3. Выполнить `Release build dry run` на проверенной ветке: все три платформы,
   ZIP/version validation, unit tests и coverage. Настоящий релиз ради теста не создавать.
4. Подтвердить Actionlint, dependency review и Scorecard на реальных домашних runners.
   Linux core analysis сохраняет core/golden/scenario tests и отдельную SARIF category;
   он не заменяет Windows plugin analysis.
5. После merge проверить CI `master`, actual runner/job IDs и сохранённые логи.
   Publish, SBOM, checksums, provenance и interface sync остаются в workflows.

Actionlint 1.7.12 и Scorecard 5.5.0 сохраняют опубликованные SHA256 загрузок.
Scorecard запускается нативно без Docker socket, с `ENABLE_SARIF=true` и
`.github/scorecard-policy.yml`; результаты default branch и точного head разбираются
перед загрузкой. Dependency-review dispatch требует точные base/head commits.
Jobs не используют sudo, apt или pipx.
