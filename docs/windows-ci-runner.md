# Домашний CI Notepad++

Windows build и plugin CodeQL выполняются в одноразовых Windows-контейнерах
на White с Hyper-V isolation. Оператор выдаёт repository-scoped JIT одному job,
сохраняет результат и логи, затем удаляет контейнер и регистрацию runner.
Notepad++ GUI smoke использует GitHub-hosted `windows-latest`: контейнерная
среда не предоставляет интерактивный desktop для проверки окон и меню.
[GUI smoke на Windows desktop](https://github.com/krotname/NppMarkdownTableEditor/actions/runs/38134737239/job/114456189578)
прошёл на head `c0d0314`; полный CI окончательного head проверяется отдельно.

## Маршруты и границы доверия

| Job | Runner |
| --- | --- |
| Windows build, plugin CodeQL, release и interface sync | `CI_WINDOWS_RUNS_ON`; домашний одноразовый Windows runner |
| CI и Release: Notepad++ compatibility smoke | GitHub-hosted `windows-latest` |
| Actionlint, dependency review, portable-core CodeQL, Scorecard | `arc-prod-adler-npp-mte` |
| Подписанная публикация Scorecard в OpenSSF после native scan ветки `master` | `arc-prod-adler-docker-npp-mte` |
| Fork, посторонний автор, bot или повторный запуск посторонним | Пропуск по owner guards, включая GUI job |

Оба actor, автор PR и исходный репозиторий должны принадлежать владельцу.
Это ограничение сохраняется и для hosted GUI jobs; в Release оно наследуется
через обязательный `version` job с guard ветки `master`/тега `v*`.
Release и interface sync сохраняют отдельные guards ветки `master`/тега `v*`.
`CI_WINDOWS_RUNS_ON` и `CI_LINUX_RUNS_ON` допускают JSON-массив labels только
соответствующего домашнего пула. Общие или hosted runners не подставлять.
`windows-latest` закреплён только за GUI job и не является fallback домашнего пула.
Отказ домашнего сервера оставляет его jobs в очереди.

Политика Actions `all_external_contributors` требует предварительного решения
владельца для workflows внешних участников. Guards в YAML не изолируют произвольно
изменённый workflow: перед таким решением нужно проверить точный head и отсутствие
прямых запросов домашнего runner. PAT, SSH и Vault остаются у оператора White;
гость получает только одноразовую JIT-конфигурацию и штатный `GITHUB_TOKEN` job.

## Среда Windows

Канонический lifecycle находится в `krotname/VpnOps`, `ops/ci-windows-white`.
Деплой оператора разрешён только из `main`; White запускает ограниченную задачу
планировщика каждые пять минут без перекрытия. Не регистрировать постоянный runner
на рабочем desktop White и не переносить туда сборки из изолированных контейнеров.

Контейнер ограничен 12 GiB RAM, шестью vCPU и отдельным writable слоем 64 GiB.
Внутри образа зарегистрированы VS 2022 Build Tools в `C:\BuildTools`, Windows SDK
и v143 x86/x64/ARM64. Readiness проверяет регистрацию toolchain, обычные VC imports,
компиляцию, ресурс и линковку до выдачи JIT. Рабочий процесс идёт под `ContainerUser`.

Git, JDK и PowerShell подключены read-only; host Visual Studio и Windows Kits
не подменяют установку внутри образа. `Package.proj` использует full-framework
MSBuild, а не `dotnet msbuild`. Недостающие OS/C++ компоненты устанавливают при
подготовке образа; непривилегированный job не меняет Visual Studio.

Домашний worker последовательно выполняет jobs; новый job получает новый контейнер.
GUI job независимо проверяет `NppCompatibilitySmoke` и `NppLatestSmoke` на hosted
Windows desktop. Оператор White не обслуживает этот job и не запускает его на хосте.
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
Публикация в OpenSSF сохраняет официальный Scorecard action, `publish_results: true`
и GitHub OIDC в отдельном Docker job только на `master`. Docker daemon и socket
изолированы внутри pod; нативный scan не получает Docker socket.
Debug x64 добавляет уже установленный Git shell в PATH перед Codecov.
Jobs не используют sudo, apt или pipx.
