# Markdown Table Editor for Notepad++

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

## Features

- `Tab` inside a Markdown table aligns the table.
- Align the table around the caret.
- Move to the next or previous cell.
- Insert, delete, and move rows.
- Insert, delete, and move columns.
- Sort rows by the current column in ascending or descending order.
- Convert selected CSV/TSV text into a Markdown table.
- Insert a new table with a selected number of columns and rows.
- Preserve Markdown alignment markers: `---`, `:---`, `---:`, `:---:`.
- Correctly handle escaped pipes: `\|`.

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

- Official Notepad++ Plugin List pull request: https://github.com/notepad-plus-plus/nppPluginList/pull/1111

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
| `Convert CSV/TSV selection to table`           | Converts selected CSV/TSV to a Markdown table                        |
| `Insert table...`                              | Inserts a new table with the requested size                          |

For example, select `Name,Score` and the next line `Anna,10`.
Run `Plugins > Markdown Table Editor > Convert CSV/TSV selection to table`.
You will get a Markdown table with `Name` and `Score` columns.

Default keyboard shortcuts:

| Command                      | Shortcut               |
| ---------------------------- | ---------------------- |
| `Tab: align table or indent` | `Tab`                  |
| `Align table`                | `Ctrl+Alt+A`           |
| `Next cell`                  | `Ctrl+Alt+Right`       |
| `Previous cell`              | `Ctrl+Alt+Left`        |
| `Insert row below`           | `Ctrl+Alt+Down`        |
| `Delete row`                 | `Ctrl+Alt+Up`          |
| `Insert column right`        | `Ctrl+Alt+Shift+Right` |
| `Delete column`              | `Ctrl+Alt+Shift+Left`  |

## Build and Tests

You need Visual Studio 2022 Build Tools. Run the commands below from Developer PowerShell/Developer Command Prompt or after adding MSBuild to `PATH`.

```powershell
msbuild Package.proj /t:Package /p:Configuration=Release /p:Platform=x64
```

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
msbuild tests\CoreSmoke.vcxproj /p:Configuration=Debug /p:Platform=x64
tests\CoreSmoke.exe
```

## License

The main package is distributed under GPL-3.0-or-later. The full GNU GPL v3 text is in [LICENSE](LICENSE) and [license.txt](license.txt); `license.txt` is also included in the ZIP for Notepad++ Plugin Admin.
Third-party notices and additional license texts are listed in [NOTICE.md](NOTICE.md) and [LICENSES](LICENSES).
