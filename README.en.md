# Markdown Table Editor for Notepad++

[![CI](https://github.com/krotname/NppMarkdownTableEditor/actions/workflows/CI_build.yml/badge.svg)](https://github.com/krotname/NppMarkdownTableEditor/actions/workflows/CI_build.yml)
[![CodeQL](https://github.com/krotname/NppMarkdownTableEditor/actions/workflows/codeql.yml/badge.svg?branch=master)](https://github.com/krotname/NppMarkdownTableEditor/actions/workflows/codeql.yml?query=branch%3Amaster)
[![Coverage](https://codecov.io/gh/krotname/NppMarkdownTableEditor/branch/master/graph/badge.svg)](https://codecov.io/gh/krotname/NppMarkdownTableEditor)
[![OpenSSF Scorecard](https://api.securityscorecards.dev/projects/github.com/krotname/NppMarkdownTableEditor/badge)](https://securityscorecards.dev/viewer/?uri=github.com/krotname/NppMarkdownTableEditor)
[![Release](https://img.shields.io/github/v/release/krotname/NppMarkdownTableEditor?label=release)](https://github.com/krotname/NppMarkdownTableEditor/releases/latest)
[![License](https://img.shields.io/github/license/krotname/NppMarkdownTableEditor)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-14-00599C)](https://isocpp.org/)
[![Plugin List PR](https://img.shields.io/badge/Plugin%20List-PR%20%231115-f97316)](https://github.com/notepad-plus-plus/nppPluginList/pull/1115)
[![Website](https://img.shields.io/badge/website-markdowntableeditor.krot.name-0f766e)](https://markdowntableeditor.krot.name/)

Markdown Table Editor turns Notepad++ into a convenient Markdown table editor.
Paste a messy table from someone else or from an AI tool, press `Tab`, and the plugin aligns the columns, preserves Markdown formatting,
and helps you quickly rearrange rows, columns, and data.

## Related Projects

- JetBrains IDEs version: [IdeaMarkdownTableEditor](https://github.com/krotname/IdeaMarkdownTableEditor)

## Demo

![Markdown table alignment example in Notepad++](docs/demo.gif)

The GIF is built from real Notepad++ screenshots on Windows: a regular `.md` file is open, and the `Align table` command is triggered from the plugin menu.

## Why Use It

- You do not need to leave Notepad++ for a separate Markdown editor just to fix tables.
- Large pipe tables stay readable as plain text.
- `Tab`, sorting, and row or column operations save manual spacing work.
- CSV/TSV text can be converted into a clean Markdown table quickly.
- UTF-8 and CJK characters are considered when calculating column width.
- Plugin menus, commands, dialogs, and messages follow the Notepad++ localization for popular languages.

## Features

- `Tab` inside a Markdown table aligns the table.
- Outside Markdown tables, `Tab` works as the normal Notepad++ indent action.
- Align the table around the caret.
- Move to the next or previous cell.
- Insert, delete, and move rows.
- Insert, delete, and move columns.
- Sort rows by the current column in ascending or descending order.
- Wrap long cells into continuation rows fitted to the current Notepad++ editor width so wide registries are easier to edit in plain text; if space is extremely tight, long words are split inside the cell.
- Convert selected CSV/TSV text or the current CSV/TSV block into a Markdown table.
- CSV/TSV block detection ignores commas inside quotes and does not capture adjacent plain text.
- Insert a new table with a selected number of columns and rows.
- Preserve Markdown alignment markers: `---`, `:---`, `---:`, `:---:`.
- Correctly handle escaped pipes: `\|`.
- Large-table operations are optimized and guarded by dedicated CI performance benchmarks.

## Installation

1. Download the ZIP archive from the latest release: https://github.com/krotname/NppMarkdownTableEditor/releases/latest
2. Extract the archive.
3. Copy the `MarkdownTableEditor` folder into the Notepad++ plugins directory.
4. Restart Notepad++.
5. Open `Plugins > Markdown Table Editor`.

The usual path for 64-bit Notepad++ is:

```text
C:\Program Files\Notepad++\plugins\MarkdownTableEditor\MarkdownTableEditor.dll
```

If Windows does not allow writing to `Program Files`, install the plugin in the user plugins directory:

```text
%LOCALAPPDATA%\Notepad++\plugins\MarkdownTableEditor\MarkdownTableEditor.dll
```

## Publication

- Official Notepad++ Plugin List pull request: https://github.com/notepad-plus-plus/nppPluginList/pull/1115

## Compatibility

| Notepad++ architecture | Minimum version |
| ---------------------- | --------------- |
| x86                    | 7.5.9           |
| x64                    | 8.3.1           |
| ARM64                  | 8.3.1           |

On x64 Notepad++ 7.5.9-8.2.1, the plugin loads and the menu item is visible, but table editing commands do not change the document. Notepad++ 7.5.9 was not released as an ARM64 build.

## Commands

| Command                                        | What It Does                                                         |
| ---------------------------------------------- | -------------------------------------------------------------------- |
| `Tab: align table or indent`                   | Aligns the table at the caret; outside tables it works as normal Tab |
| `Align table`                                  | Aligns the current Markdown table                                    |
| `Next cell` / `Previous cell`                  | Moves the caret between cells                                        |
| `Insert row below` / `Delete row`              | Adds or deletes a row                                                |
| `Insert column right` / `Delete column`        | Adds or deletes a column                                             |
| `Move row up` / `Move row down`                | Moves the current row                                                |
| `Move column left` / `Move column right`       | Moves the current column                                             |
| `Sort rows ascending` / `Sort rows descending` | Sorts rows by the current column                                     |
| `Fit table to window`                          | Fits the current table to the visible width: it narrows long cells or rejoins continuation rows when the window is wider |
| `Notepad++ word wrap (MD)`                     | Toggles Notepad++ visual word wrap next to the table-wrap button      |
| `Table wrap (MD)`                              | Toggles table wrapping: when enabled, it fits the current table and then keeps wrapping after `Tab`/`Align table` |
| `Auto fit on resize (MD)`                      | Toggles automatic fitting of the current table when the editor width changes; while enabled, manual `Fit table to window` is unavailable |
| `Convert CSV/TSV to table`                     | Converts selected CSV/TSV or the current block to a Markdown table   |
| `Insert table...`                              | Inserts a new table with the requested size                          |

For example, select `Name,Score` and the next line `Anna,10`, or place the caret inside such a block.
Run `Plugins > Markdown Table Editor > Convert CSV/TSV to table`.
You will get a Markdown table with `Name` and `Score` columns.

Default keyboard shortcuts:

Except for the contextual `Tab`, commands use `Ctrl+Alt+Shift` with the top number row and adjacent keys to avoid standard JetBrains IDE and Notepad++ shortcuts.

| Command                      | Shortcut           |
| ---------------------------- | ------------------ |
| `Tab: align table or indent` | `Tab`              |
| `Align table`                | `Ctrl+Alt+Shift+1` |
| `Next cell`                  | `Ctrl+Alt+Shift+2` |
| `Previous cell`              | `Ctrl+Alt+Shift+3` |
| `Insert row below`           | `Ctrl+Alt+Shift+4` |
| `Delete row`                 | `Ctrl+Alt+Shift+5` |
| `Insert column right`        | `Ctrl+Alt+Shift+6` |
| `Delete column`              | `Ctrl+Alt+Shift+7` |
| `Move row up`                | `Ctrl+Alt+Shift+8` |
| `Move row down`              | `Ctrl+Alt+Shift+9` |
| `Move column left`           | `Ctrl+Alt+Shift+[` |
| `Move column right`          | `Ctrl+Alt+Shift+]` |
| `Sort rows ascending`        | `Ctrl+Alt+Shift+=` |
| `Sort rows descending`       | `Ctrl+Alt+Shift+-` |
| `Fit table to window`        | `Ctrl+Alt+Shift+W` |
| `Notepad++ word wrap (MD)`   | toolbar button     |
| `Table wrap (MD)`            | toolbar/menu toggle |
| `Auto fit on resize (MD)`    | toolbar/menu toggle |
| `Convert CSV/TSV to table`   | `Ctrl+Alt+Shift+0` |
| `Insert table...`            | `Ctrl+Alt+Shift+\` |

### Table Wrap and Notepad++ Word Wrap

| `Table wrap (MD)` | Notepad++ `Word wrap` | Result |
| --- | --- | --- |
| Off | Off | The table is not reshaped, and long physical lines continue to the right. |
| Off | On | Only Notepad++ visual wrapping is used; the file is unchanged, and wide Markdown tables can look ragged. |
| On | Off | The plugin physically wraps text inside cells and keeps the right table edge inside the visible width while possible. |
| On | On | The plugin keeps table lines inside the visible width; Notepad++ wrapping remains a fallback for very narrow windows or text after manual resizing. |

## Build and Tests

You need Visual Studio 2022 Build Tools. Run the commands below from Developer Command Prompt or after adding MSBuild to `PATH`.

```cmd
msbuild Package.proj /t:Package /p:Configuration=Release /p:Platform=x64
```

The plugin version is defined only in `Version.props`. `Package.proj`, the DLL version resource, and release ZIP names read that value; `/p:Version` is not a version source and is rejected when it does not match `Version.props`.

The built ZIP archives appear in the `build` directory.

Manual build through MSBuild:

```cmd
msbuild vs.proj\MarkdownTableEditor.vcxproj /p:Configuration=Release /p:Platform=x64
```

The DLL is created here:

```text
bin64\MarkdownTableEditor.dll
```

Run core smoke tests:

```cmd
msbuild Package.proj /t:RunCoreSmokeTests
```

C++ core coverage report:

```cmd
msbuild Package.proj /t:Coverage /p:Configuration=Debug /p:Platform=x64
```

Cobertura XML is written to `build/reports/coverage/coverage.cobertura.xml`.

Core performance benchmarks:

```cmd
msbuild Package.proj /t:CorePerformance /p:Configuration=Release /p:Platform=x64
```
