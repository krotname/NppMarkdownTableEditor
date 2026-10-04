# White Windows CI runner

Owner CI, C++ CodeQL, release builds, release publishing and interface sync target
`[self-hosted, Windows, X64, adler-white-npp]`. This is preparation, not a completed
deployment: on 2026-10-04 the White host was unavailable and the repository had no
registered runner. Do not merge the migration until the PR has successful White CI.

## Routing and trust

| Event | Route |
| --- | --- |
| Canonical repository; original actor and rerun actor are the owner | White Windows |
| PR also authored by the owner, from this repository, not a fork | White Windows |
| Other PR author, fork (including the owner's fork), bot or non-owner rerun | Public GitHub-hosted Windows/Linux |
| Release dispatch/tag | Owner only; `master` or `v*` tag; all jobs depend on the gated version job |
| Release dry run | Owner only; a reviewed branch, including the migration branch |
| Interface sync | Owner only; `master` |

The optional repository variable `CI_WINDOWS_RUNS_ON` is a JSON array of labels.
Leave it unset for the default route. An override must retain `self-hosted`,
`Windows`, `X64` and this repository's unique label; it must not target a hosted,
shared, organisation-wide or unrelated runner. There is no hosted fallback for
an offline trusted runner.

Linux actionlint, dependency review, portable-core CodeQL and native Scorecard
target the repository-scoped Direct pool `arc-prod-adler-npp-mte`. The exact
`CI_LINUX_RUNS_ON` JSON override is `["arc-prod-adler-npp-mte"]`; set it only after
the canonical pool is live. Unset overrides select this same pool rather than
skip the checks. External/untrusted PRs retain public hosted Linux. Both actors,
the PR author and the same-repository/non-fork checks guard every home route.
Windows plugin CodeQL remains required on White; the Linux core lane also builds
and runs the existing core, golden-fixture and scenario tests and uses a separate
SARIF category. It does not replace plugin analysis.

Actionlint 1.7.12 and Scorecard 5.5.0 are downloaded with pinned SHA256 checks.
Actionlint keeps shellcheck and pyflakes (installed into an unprivileged venv).
Scorecard uses native `--format sarif`, enabled by `ENABLE_SARIF=true`, retaining
the SARIF artifact and GitHub code-scanning upload. The CLI does not publish to
Scorecard's external REST dataset. No job needs sudo, apt, pipx or a Docker socket.
Manual dependency-review dispatch requires exact base/head commit inputs.

These predicates enforce the reviewed workflows' routing; they are not a sandbox
for arbitrary modified workflow YAML. Before bringing a public-repository runner
online, restrict repository write access and require approval for workflows from
all outside collaborators in Actions settings. Review the current PR head and
workflow changes before any such approval. Do not approve a workflow that removes
the routing guards or directly requests a home runner.
The repository API policy was set and read back as `all_external_contributors`
on 2026-10-04 before admitting home public-PR jobs.

## Install on ADLER-WHITE-W1

1. Restore access to the physical host and confirm its identity and Windows x64
   operating system. Use a dedicated low-privilege CI account, isolated from
   personal browser profiles, credentials and other repositories' workspaces.
2. Install current Git for Windows (including Git Bash), PowerShell 7 (`pwsh`),
   Windows PowerShell 5.1, and GitHub CLI (`gh`) on the runner account's PATH.
   Bash release steps require `mapfile`, `find`, `sort`, `sha256sum`, `sed` and `awk`.
   Do not persist a personal GitHub CLI login for the worker; workflows use the
   per-job `GITHUB_TOKEN`.
3. Install Visual Studio 2022/MSBuild with the v143 x86/x64 and ARM64 C++ tools
   (`Microsoft.VisualStudio.Component.VC.Tools.x86.x64` and
   `Microsoft.VisualStudio.Component.VC.Tools.ARM64`), the Windows 10/11 SDK and
   native code coverage/test tools. Verify the selected VS instance contains
   `Common7\IDE\Extensions\Microsoft\CodeCoverage.Console\Microsoft.CodeCoverage.Console.exe`.
   `Package.proj` uses full-framework MSBuild/CodeTaskFactory, not `dotnet msbuild`.
   CI refuses to modify Visual Studio on a persistent runner; install missing
   ARM64 tools administratively before enabling it.
4. Download the current official `actions/runner` Windows x64 release, verify its
   published SHA256 and extract it into a dedicated Npp runner directory. Use a
   current runner supporting the pinned Node 24 actions (at least 2.327.1), with
   automatic updates enabled. Keep its work and tool-cache directories separate
   from the IDEA runner.
5. Register at **repository scope**, using the short-lived registration token from
   `krotname/NppMarkdownTableEditor` Actions settings via the interactive prompt:

   ```powershell
   .\config.cmd --url https://github.com/krotname/NppMarkdownTableEditor --name adler-white-npp --labels adler-white-npp --work _work
   ```

   Do not put the token in a command line, repository file or log. Check that the
   registered labels include the automatic `self-hosted`, `Windows`, `X64` labels
   plus `adler-white-npp`. Do not reuse the retired runner registration.
6. Run `run.cmd` in the dedicated account's interactive desktop, or a scheduled
   task that runs only while that account is logged on. The Notepad++ smoke
   launches actual Win32/x64 GUI processes; a Session 0 Windows service is not an
   acceptable unverified substitute. Start with one worker, which serialises the
   six matrix jobs; retain the full matrix instead of reducing it.
7. Verify HTTPS/download access from the runner account to GitHub/API/release
   assets, Actions artifact/OIDC endpoints and Codecov. The smoke downloads
   Notepad++ 7.5.9 Win32, 8.3.1 x64 and latest x64. CodeQL must be able to download
   its bundle and submit SARIF. Do not reuse another service's reserved ports.

## Acceptance before merge

1. Rerun the PR checks as the owner after the runner is online. Inspect job logs
   for the actual runner name and confirm it is ADLER-WHITE-W1.
2. Require all six `Build and tests` jobs: Debug/Release × x64/Win32/ARM64.
   The x64 builds keep the core and plugin-shortcut unit tests. Require Debug x64
   native coverage (minimum 70%), Release x64 performance benchmarks, smoke-script
   safety tests and all three Release DLL artifacts.
3. Require `Tests / Notepad++ compatibility smoke`, including both
   `NppCompatibilitySmoke` and `NppLatestSmoke`, and `Analyze (cpp)` on White.
4. Dispatch **Release build dry run** on the reviewed migration branch as the
   owner. Require all three package platforms, ZIP contents/version validation,
   unit tests and coverage. This workflow does not publish a release.
5. After a real Linux pool is configured, rerun actionlint and dependency review;
   verify Scorecard separately. Skipped checks are not evidence of successful CI.
6. Keep the PR open while White is unavailable or any required check fails. Release
   publishing, checksums, SBOM, provenance and interface-sync checks remain in the
   workflows; do not create a release solely to test this migration.
