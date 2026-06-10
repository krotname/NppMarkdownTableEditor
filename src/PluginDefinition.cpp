//this file is part of notepad++
//Copyright (C)2022 Don HO <don.h@free.fr>
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "PluginDefinition.h"
#include "MarkdownTableCore.h"

#include <commctrl.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cwchar>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

//
// The plugin data that Notepad++ needs
//
FuncItem funcItem[nbFunc];

//
// The data of Notepad++ that you can use in your plugin commands
//
NppData nppData;

namespace
{
struct TableSizeDialogState
{
	std::size_t columns = 3;
	std::size_t dataRows = 3;
	bool accepted = false;
};

struct InsertText
{
	std::string text;
	std::size_t caretDelta = 0;
};

const int tableSizeColumnsId = 1001;
const int tableSizeRowsId = 1002;
const UINT tableSizeMaxColumns = 50;
const UINT tableSizeMaxRows = 200;

HINSTANCE g_moduleHandle = NULL;

const UINT sciGetTextRangeLegacy = 2162;

struct LegacySciCharacterRange
{
	Sci_PositionCR cpMin;
	Sci_PositionCR cpMax;
};

struct LegacySciTextRange
{
	LegacySciCharacterRange chrg;
	char *lpstrText;
};

ShortcutKey g_alignShortcut = { true, true, true, '1' };
ShortcutKey g_nextCellShortcut = { true, true, true, '2' };
ShortcutKey g_previousCellShortcut = { true, true, true, '3' };
ShortcutKey g_insertRowShortcut = { true, true, true, '4' };
ShortcutKey g_deleteRowShortcut = { true, true, true, '5' };
ShortcutKey g_insertColumnShortcut = { true, true, true, '6' };
ShortcutKey g_deleteColumnShortcut = { true, true, true, '7' };
ShortcutKey g_moveRowUpShortcut = { true, true, true, '8' };
ShortcutKey g_moveRowDownShortcut = { true, true, true, '9' };
ShortcutKey g_moveColumnLeftShortcut = { true, true, true, VK_OEM_4 };
ShortcutKey g_moveColumnRightShortcut = { true, true, true, VK_OEM_6 };
ShortcutKey g_sortRowsAscendingShortcut = { true, true, true, VK_OEM_PLUS };
ShortcutKey g_sortRowsDescendingShortcut = { true, true, true, VK_OEM_MINUS };
ShortcutKey g_convertCsvTsvShortcut = { true, true, true, '0' };
ShortcutKey g_insertTableShortcut = { true, true, true, VK_OEM_5 };
ShortcutKey g_tabShortcut = { false, false, false, VK_TAB };
ShortcutKey g_wrapLongCellsShortcut = { true, true, true, 'W' };

const std::size_t tabCommandIndex = 15;
const std::size_t wrapLongCellsCommandIndex = 16;
const std::size_t notepadWordWrapCommandIndex = 17;
const std::size_t autoWrapLongCellsCommandIndex = 18;
const std::size_t fitToWindowOnResizeCommandIndex = 19;
// Notepad++ IDM_VIEW_WRAP = IDM + 4000 + 22.
const int notepadWordWrapCommandId = 44022;
const UINT_PTR fitToWindowResizeTimerId = 0x4D54;
const UINT fitToWindowResizeDelayMs = 160;

bool g_autoWrapLongCells = false;
bool g_fitToWindowOnResize = false;
bool g_fitToWindowInProgress = false;
std::size_t g_lastFitToWindowColumns = 0;
HWND g_pendingFitToWindowScintilla = NULL;
WNDPROC g_originalMainScintillaProc = NULL;
WNDPROC g_originalSecondScintillaProc = NULL;
HBITMAP g_tabToolbarBmp = NULL;
HICON g_tabToolbarIcon = NULL;
HICON g_tabToolbarIconDarkMode = NULL;
HBITMAP g_wrapLongCellsToolbarBmp = NULL;
HICON g_wrapLongCellsToolbarIcon = NULL;
HICON g_wrapLongCellsToolbarIconDarkMode = NULL;
HBITMAP g_notepadWordWrapToolbarBmp = NULL;
HICON g_notepadWordWrapToolbarIcon = NULL;
HICON g_notepadWordWrapToolbarIconDarkMode = NULL;
HBITMAP g_autoWrapToolbarBmp = NULL;
HICON g_autoWrapToolbarIcon = NULL;
HICON g_autoWrapToolbarIconDarkMode = NULL;
HBITMAP g_fitToWindowOnResizeToolbarBmp = NULL;
HICON g_fitToWindowOnResizeToolbarIcon = NULL;
HICON g_fitToWindowOnResizeToolbarIconDarkMode = NULL;

enum class UiLanguage
{
	English,
	MandarinChinese,
	Hindi,
	Spanish,
	Arabic,
	French,
	Bengali,
	Portuguese,
	Russian,
	Indonesian,
	Urdu,
	German,
	Japanese,
	NigerianPidgin,
	Marathi,
	Telugu,
	Turkish,
	Tamil,
	YueChinese,
	Vietnamese
};

struct UiText
{
	const wchar_t *pluginMenuName;
	const wchar_t *commands[nbFunc];
	const wchar_t *tableSizeTitle;
	const wchar_t *tableSizeColumnsLabel;
	const wchar_t *tableSizeRowsLabel;
	const wchar_t *okButton;
	const wchar_t *cancelButton;
	const wchar_t *tableSizeRangeError;
	const wchar_t *putCaretInsideTable;
	const wchar_t *couldNotEditTable;
	const wchar_t *selectCsvTsv;
	const wchar_t *couldNotConvertCsvTsv;
};

#define UI_TEXT_WITH_ENGLISH_MESSAGES(menuName, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15) \
{ \
	menuName, \
	{ c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15 L" (MD)", NULL, NULL, L"Table wrap (MD)", L"Auto fit on resize (MD)" }, \
	L"Insert Markdown table", \
	L"Columns:", \
	L"Data rows:", \
	L"OK", \
	L"Cancel", \
	L"Use 1-50 columns and 0-200 data rows.", \
	L"Put the caret inside a Markdown pipe table first.", \
	L"Could not edit the Markdown table.", \
	L"Select CSV/TSV text or put the caret inside a CSV/TSV block first.", \
	L"Could not convert the selected CSV/TSV text." \
}

const UiText englishUiText =
{
	L"Markdown Table Editor",
	{
		L"Align table",
		L"Next cell",
		L"Previous cell",
		L"Insert row below",
		L"Delete row",
		L"Insert column right",
		L"Delete column",
		L"Move row up",
		L"Move row down",
		L"Move column left",
		L"Move column right",
		L"Sort rows ascending",
		L"Sort rows descending",
		L"Convert CSV/TSV to table",
		L"Insert table...",
		L"Tab: align table or indent (MD)",
		L"Fit table to window",
		L"Notepad++ word wrap (MD)",
		L"Table wrap (MD)",
		L"Auto fit on resize (MD)"
	},
	L"Insert Markdown table",
	L"Columns:",
	L"Data rows:",
	L"OK",
	L"Cancel",
	L"Use 1-50 columns and 0-200 data rows.",
	L"Put the caret inside a Markdown pipe table first.",
	L"Could not edit the Markdown table.",
	L"Select CSV/TSV text or put the caret inside a CSV/TSV block first.",
	L"Could not convert the selected CSV/TSV text."
};

const UiText chineseUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Markdown \u8868\u683C\u7F16\u8F91\u5668",
	L"\u5BF9\u9F50\u8868\u683C",
	L"\u4E0B\u4E00\u4E2A\u5355\u5143\u683C",
	L"\u4E0A\u4E00\u4E2A\u5355\u5143\u683C",
	L"\u5728\u4E0B\u65B9\u63D2\u5165\u884C",
	L"\u5220\u9664\u884C",
	L"\u5728\u53F3\u4FA7\u63D2\u5165\u5217",
	L"\u5220\u9664\u5217",
	L"\u4E0A\u79FB\u884C",
	L"\u4E0B\u79FB\u884C",
	L"\u5DE6\u79FB\u5217",
	L"\u53F3\u79FB\u5217",
	L"\u6309\u5347\u5E8F\u6392\u5E8F\u884C",
	L"\u6309\u964D\u5E8F\u6392\u5E8F\u884C",
	L"\u5C06 CSV/TSV \u8F6C\u4E3A\u8868\u683C",
	L"\u63D2\u5165\u65B0\u8868\u683C",
	L"Tab\uFF1A\u5BF9\u9F50 Markdown \u8868\u683C"
);

const UiText hindiUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Markdown \u0924\u093E\u0932\u093F\u0915\u093E \u0938\u0902\u092A\u093E\u0926\u0915",
	L"\u0924\u093E\u0932\u093F\u0915\u093E \u0938\u0902\u0930\u0947\u0916\u093F\u0924 \u0915\u0930\u0947\u0902",
	L"\u0905\u0917\u0932\u093E \u0938\u0947\u0932",
	L"\u092A\u093F\u091B\u0932\u093E \u0938\u0947\u0932",
	L"\u0928\u0940\u091A\u0947 \u092A\u0902\u0915\u094D\u0924\u093F \u0921\u093E\u0932\u0947\u0902",
	L"\u092A\u0902\u0915\u094D\u0924\u093F \u0939\u091F\u093E\u090F\u0902",
	L"\u0926\u093E\u0908\u0902 \u0913\u0930 \u0915\u0949\u0932\u092E \u0921\u093E\u0932\u0947\u0902",
	L"\u0915\u0949\u0932\u092E \u0939\u091F\u093E\u090F\u0902",
	L"\u092A\u0902\u0915\u094D\u0924\u093F \u090A\u092A\u0930 \u0932\u0947 \u091C\u093E\u090F\u0902",
	L"\u092A\u0902\u0915\u094D\u0924\u093F \u0928\u0940\u091A\u0947 \u0932\u0947 \u091C\u093E\u090F\u0902",
	L"\u0915\u0949\u0932\u092E \u092C\u093E\u090F\u0902 \u0932\u0947 \u091C\u093E\u090F\u0902",
	L"\u0915\u0949\u0932\u092E \u0926\u093E\u090F\u0902 \u0932\u0947 \u091C\u093E\u090F\u0902",
	L"\u092A\u0902\u0915\u094D\u0924\u093F\u092F\u093E\u0902 \u0906\u0930\u094B\u0939\u0940 \u0915\u094D\u0930\u092E \u092E\u0947\u0902 \u091B\u093E\u0902\u091F\u0947\u0902",
	L"\u092A\u0902\u0915\u094D\u0924\u093F\u092F\u093E\u0902 \u0905\u0935\u0930\u094B\u0939\u0940 \u0915\u094D\u0930\u092E \u092E\u0947\u0902 \u091B\u093E\u0902\u091F\u0947\u0902",
	L"CSV/TSV \u0915\u094B \u0924\u093E\u0932\u093F\u0915\u093E \u092E\u0947\u0902 \u092C\u0926\u0932\u0947\u0902",
	L"\u0928\u0908 \u0924\u093E\u0932\u093F\u0915\u093E \u0921\u093E\u0932\u0947\u0902",
	L"Tab: Markdown \u0924\u093E\u0932\u093F\u0915\u093E \u0938\u0902\u0930\u0947\u0916\u093F\u0924 \u0915\u0930\u0947\u0902"
);

const UiText spanishUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Editor de tablas Markdown",
	L"Alinear tabla",
	L"Celda siguiente",
	L"Celda anterior",
	L"Insertar fila debajo",
	L"Eliminar fila",
	L"Insertar columna a la derecha",
	L"Eliminar columna",
	L"Mover fila arriba",
	L"Mover fila abajo",
	L"Mover columna a la izquierda",
	L"Mover columna a la derecha",
	L"Ordenar filas ascendente",
	L"Ordenar filas descendente",
	L"Convertir CSV/TSV en tabla",
	L"Insertar tabla nueva",
	L"Tab: alinear tabla Markdown"
);

const UiText arabicUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"\u0645\u062D\u0631\u0631 \u062C\u062F\u0627\u0648\u0644 Markdown",
	L"\u0645\u062D\u0627\u0630\u0627\u0629 \u0627\u0644\u062C\u062F\u0648\u0644",
	L"\u0627\u0644\u062E\u0644\u064A\u0629 \u0627\u0644\u062A\u0627\u0644\u064A\u0629",
	L"\u0627\u0644\u062E\u0644\u064A\u0629 \u0627\u0644\u0633\u0627\u0628\u0642\u0629",
	L"\u0625\u062F\u0631\u0627\u062C \u0635\u0641 \u0628\u0627\u0644\u0623\u0633\u0641\u0644",
	L"\u062D\u0630\u0641 \u0627\u0644\u0635\u0641",
	L"\u0625\u062F\u0631\u0627\u062C \u0639\u0645\u0648\u062F \u0625\u0644\u0649 \u0627\u0644\u064A\u0645\u064A\u0646",
	L"\u062D\u0630\u0641 \u0627\u0644\u0639\u0645\u0648\u062F",
	L"\u0646\u0642\u0644 \u0627\u0644\u0635\u0641 \u0644\u0623\u0639\u0644\u0649",
	L"\u0646\u0642\u0644 \u0627\u0644\u0635\u0641 \u0644\u0623\u0633\u0641\u0644",
	L"\u0646\u0642\u0644 \u0627\u0644\u0639\u0645\u0648\u062F \u0625\u0644\u0649 \u0627\u0644\u064A\u0633\u0627\u0631",
	L"\u0646\u0642\u0644 \u0627\u0644\u0639\u0645\u0648\u062F \u0625\u0644\u0649 \u0627\u0644\u064A\u0645\u064A\u0646",
	L"\u0641\u0631\u0632 \u0627\u0644\u0635\u0641\u0648\u0641 \u062A\u0635\u0627\u0639\u062F\u064A\u0627",
	L"\u0641\u0631\u0632 \u0627\u0644\u0635\u0641\u0648\u0641 \u062A\u0646\u0627\u0632\u0644\u064A\u0627",
	L"\u062A\u062D\u0648\u064A\u0644 CSV/TSV \u0625\u0644\u0649 \u062C\u062F\u0648\u0644",
	L"\u0625\u062F\u0631\u0627\u062C \u062C\u062F\u0648\u0644 \u062C\u062F\u064A\u062F",
	L"Tab: \u0645\u062D\u0627\u0630\u0627\u0629 \u062C\u062F\u0648\u0644 Markdown"
);

const UiText frenchUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Editeur de tableaux Markdown",
	L"Aligner le tableau",
	L"Cellule suivante",
	L"Cellule precedente",
	L"Inserer une ligne dessous",
	L"Supprimer la ligne",
	L"Inserer une colonne a droite",
	L"Supprimer la colonne",
	L"Monter la ligne",
	L"Descendre la ligne",
	L"Deplacer la colonne a gauche",
	L"Deplacer la colonne a droite",
	L"Trier les lignes croissant",
	L"Trier les lignes decroissant",
	L"Convertir CSV/TSV en tableau",
	L"Inserer un nouveau tableau",
	L"Tab : aligner le tableau Markdown"
);

const UiText bengaliUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Markdown \u099F\u09C7\u09AC\u09BF\u09B2 \u09B8\u09AE\u09CD\u09AA\u09BE\u09A6\u0995",
	L"\u099F\u09C7\u09AC\u09BF\u09B2 \u09B8\u09BE\u09B0\u09BF\u09AC\u09A6\u09CD\u09A7 \u0995\u09B0\u09C1\u09A8",
	L"\u09AA\u09B0\u09C7\u09B0 \u09B8\u09C7\u09B2",
	L"\u0986\u0997\u09C7\u09B0 \u09B8\u09C7\u09B2",
	L"\u09A8\u09BF\u099A\u09C7 \u09B8\u09BE\u09B0\u09BF \u09A2\u09CB\u0995\u09BE\u09A8",
	L"\u09B8\u09BE\u09B0\u09BF \u09AE\u09C1\u099B\u09C1\u09A8",
	L"\u09A1\u09BE\u09A8\u09C7 \u0995\u09B2\u09BE\u09AE \u09A2\u09CB\u0995\u09BE\u09A8",
	L"\u0995\u09B2\u09BE\u09AE \u09AE\u09C1\u099B\u09C1\u09A8",
	L"\u09B8\u09BE\u09B0\u09BF \u0989\u09AA\u09B0\u09C7 \u09B8\u09B0\u09BE\u09A8",
	L"\u09B8\u09BE\u09B0\u09BF \u09A8\u09BF\u099A\u09C7 \u09B8\u09B0\u09BE\u09A8",
	L"\u0995\u09B2\u09BE\u09AE \u09AC\u09BE\u09AE\u09C7 \u09B8\u09B0\u09BE\u09A8",
	L"\u0995\u09B2\u09BE\u09AE \u09A1\u09BE\u09A8\u09C7 \u09B8\u09B0\u09BE\u09A8",
	L"\u09B8\u09BE\u09B0\u09BF \u098A\u09B0\u09CD\u09A7\u09CD\u09AC\u0995\u09CD\u09B0\u09AE\u09C7 \u09B8\u09BE\u099C\u09BE\u09A8",
	L"\u09B8\u09BE\u09B0\u09BF \u09A8\u09BF\u09AE\u09CD\u09A8\u0995\u09CD\u09B0\u09AE\u09C7 \u09B8\u09BE\u099C\u09BE\u09A8",
	L"CSV/TSV \u099F\u09C7\u09AC\u09BF\u09B2\u09C7 \u09B0\u09C2\u09AA\u09BE\u09A8\u09CD\u09A4\u09B0 \u0995\u09B0\u09C1\u09A8",
	L"\u09A8\u09A4\u09C1\u09A8 \u099F\u09C7\u09AC\u09BF\u09B2 \u09A2\u09CB\u0995\u09BE\u09A8",
	L"Tab: Markdown \u099F\u09C7\u09AC\u09BF\u09B2 \u09B8\u09BE\u09B0\u09BF\u09AC\u09A6\u09CD\u09A7 \u0995\u09B0\u09C1\u09A8"
);

const UiText portugueseUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Editor de tabelas Markdown",
	L"Alinhar tabela",
	L"Proxima celula",
	L"Celula anterior",
	L"Inserir linha abaixo",
	L"Excluir linha",
	L"Inserir coluna a direita",
	L"Excluir coluna",
	L"Mover linha para cima",
	L"Mover linha para baixo",
	L"Mover coluna para a esquerda",
	L"Mover coluna para a direita",
	L"Ordenar linhas crescente",
	L"Ordenar linhas decrescente",
	L"Converter CSV/TSV em tabela",
	L"Inserir nova tabela",
	L"Tab: alinhar tabela Markdown"
);

const UiText indonesianUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Editor Tabel Markdown",
	L"Ratakan tabel",
	L"Sel berikutnya",
	L"Sel sebelumnya",
	L"Sisipkan baris di bawah",
	L"Hapus baris",
	L"Sisipkan kolom kanan",
	L"Hapus kolom",
	L"Pindahkan baris ke atas",
	L"Pindahkan baris ke bawah",
	L"Pindahkan kolom ke kiri",
	L"Pindahkan kolom ke kanan",
	L"Urutkan baris naik",
	L"Urutkan baris turun",
	L"Konversi CSV/TSV ke tabel",
	L"Sisipkan tabel baru",
	L"Tab: ratakan tabel Markdown"
);

const UiText urduUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Markdown \u062C\u062F\u0648\u0644 \u0627\u06CC\u0688\u06CC\u0679\u0631",
	L"\u062C\u062F\u0648\u0644 \u0633\u06CC\u062F\u06BE\u0627 \u06A9\u0631\u06CC\u06BA",
	L"\u0627\u06AF\u0644\u0627 \u062E\u0627\u0646\u06C1",
	L"\u067E\u0686\u06BE\u0644\u0627 \u062E\u0627\u0646\u06C1",
	L"\u0646\u06CC\u0686\u06D2 \u0642\u0637\u0627\u0631 \u062F\u0627\u062E\u0644 \u06A9\u0631\u06CC\u06BA",
	L"\u0642\u0637\u0627\u0631 \u062D\u0630\u0641 \u06A9\u0631\u06CC\u06BA",
	L"\u062F\u0627\u0626\u06CC\u06BA \u06A9\u0627\u0644\u0645 \u062F\u0627\u062E\u0644 \u06A9\u0631\u06CC\u06BA",
	L"\u06A9\u0627\u0644\u0645 \u062D\u0630\u0641 \u06A9\u0631\u06CC\u06BA",
	L"\u0642\u0637\u0627\u0631 \u0627\u0648\u067E\u0631 \u0645\u0646\u062A\u0642\u0644 \u06A9\u0631\u06CC\u06BA",
	L"\u0642\u0637\u0627\u0631 \u0646\u06CC\u0686\u06D2 \u0645\u0646\u062A\u0642\u0644 \u06A9\u0631\u06CC\u06BA",
	L"\u06A9\u0627\u0644\u0645 \u0628\u0627\u0626\u06CC\u06BA \u0645\u0646\u062A\u0642\u0644 \u06A9\u0631\u06CC\u06BA",
	L"\u06A9\u0627\u0644\u0645 \u062F\u0627\u0626\u06CC\u06BA \u0645\u0646\u062A\u0642\u0644 \u06A9\u0631\u06CC\u06BA",
	L"\u0642\u0637\u0627\u0631\u06CC\u06BA \u0635\u0639\u0648\u062F\u06CC \u062A\u0631\u062A\u06CC\u0628 \u062F\u06CC\u06BA",
	L"\u0642\u0637\u0627\u0631\u06CC\u06BA \u0646\u0632\u0648\u0644\u06CC \u062A\u0631\u062A\u06CC\u0628 \u062F\u06CC\u06BA",
	L"CSV/TSV \u06A9\u0648 \u062C\u062F\u0648\u0644 \u0628\u0646\u0627\u0626\u06CC\u06BA",
	L"\u0646\u06CC\u0627 \u062C\u062F\u0648\u0644 \u062F\u0627\u062E\u0644 \u06A9\u0631\u06CC\u06BA",
	L"Tab: Markdown \u062C\u062F\u0648\u0644 \u0633\u06CC\u062F\u06BE\u0627 \u06A9\u0631\u06CC\u06BA"
);

const UiText germanUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Markdown-Tabelleneditor",
	L"Tabelle ausrichten",
	L"Naechste Zelle",
	L"Vorherige Zelle",
	L"Zeile darunter einfuegen",
	L"Zeile loeschen",
	L"Spalte rechts einfuegen",
	L"Spalte loeschen",
	L"Zeile nach oben",
	L"Zeile nach unten",
	L"Spalte nach links",
	L"Spalte nach rechts",
	L"Zeilen aufsteigend sortieren",
	L"Zeilen absteigend sortieren",
	L"CSV/TSV in Tabelle umwandeln",
	L"Neue Tabelle einfuegen",
	L"Tab: Markdown-Tabelle ausrichten"
);

const UiText russianUiText =
{
	L"\u0420\u0435\u0434\u0430\u043A\u0442\u043E\u0440 Markdown-\u0442\u0430\u0431\u043B\u0438\u0446",
	{
		L"\u0412\u044B\u0440\u043E\u0432\u043D\u044F\u0442\u044C \u0442\u0430\u0431\u043B\u0438\u0446\u0443",
		L"\u0421\u043B\u0435\u0434\u0443\u044E\u0449\u0430\u044F \u044F\u0447\u0435\u0439\u043A\u0430",
		L"\u041F\u0440\u0435\u0434\u044B\u0434\u0443\u0449\u0430\u044F \u044F\u0447\u0435\u0439\u043A\u0430",
		L"\u0412\u0441\u0442\u0430\u0432\u0438\u0442\u044C \u0441\u0442\u0440\u043E\u043A\u0443 \u043D\u0438\u0436\u0435",
		L"\u0423\u0434\u0430\u043B\u0438\u0442\u044C \u0441\u0442\u0440\u043E\u043A\u0443",
		L"\u0412\u0441\u0442\u0430\u0432\u0438\u0442\u044C \u0441\u0442\u043E\u043B\u0431\u0435\u0446 \u0441\u043F\u0440\u0430\u0432\u0430",
		L"\u0423\u0434\u0430\u043B\u0438\u0442\u044C \u0441\u0442\u043E\u043B\u0431\u0435\u0446",
		L"\u041F\u0435\u0440\u0435\u043C\u0435\u0441\u0442\u0438\u0442\u044C \u0441\u0442\u0440\u043E\u043A\u0443 \u0432\u0432\u0435\u0440\u0445",
		L"\u041F\u0435\u0440\u0435\u043C\u0435\u0441\u0442\u0438\u0442\u044C \u0441\u0442\u0440\u043E\u043A\u0443 \u0432\u043D\u0438\u0437",
		L"\u041F\u0435\u0440\u0435\u043C\u0435\u0441\u0442\u0438\u0442\u044C \u0441\u0442\u043E\u043B\u0431\u0435\u0446 \u0432\u043B\u0435\u0432\u043E",
		L"\u041F\u0435\u0440\u0435\u043C\u0435\u0441\u0442\u0438\u0442\u044C \u0441\u0442\u043E\u043B\u0431\u0435\u0446 \u0432\u043F\u0440\u0430\u0432\u043E",
		L"\u0421\u043E\u0440\u0442\u0438\u0440\u043E\u0432\u0430\u0442\u044C \u0441\u0442\u0440\u043E\u043A\u0438 \u043F\u043E \u0432\u043E\u0437\u0440\u0430\u0441\u0442\u0430\u043D\u0438\u044E",
		L"\u0421\u043E\u0440\u0442\u0438\u0440\u043E\u0432\u0430\u0442\u044C \u0441\u0442\u0440\u043E\u043A\u0438 \u043F\u043E \u0443\u0431\u044B\u0432\u0430\u043D\u0438\u044E",
		L"\u041F\u0440\u0435\u043E\u0431\u0440\u0430\u0437\u043E\u0432\u0430\u0442\u044C CSV/TSV \u0432 \u0442\u0430\u0431\u043B\u0438\u0446\u0443",
		L"\u0412\u0441\u0442\u0430\u0432\u0438\u0442\u044C \u0442\u0430\u0431\u043B\u0438\u0446\u0443...",
		L"Tab: \u0432\u044B\u0440\u043E\u0432\u043D\u044F\u0442\u044C \u0442\u0430\u0431\u043B\u0438\u0446\u0443 \u0438\u043B\u0438 \u0441\u0434\u0435\u043B\u0430\u0442\u044C \u043E\u0442\u0441\u0442\u0443\u043F (MD)",
		L"\u041F\u043E\u0434\u043E\u0433\u043D\u0430\u0442\u044C \u0442\u0430\u0431\u043B\u0438\u0446\u0443 \u043F\u043E\u0434 \u043E\u043A\u043D\u043E",
		L"\u041F\u0435\u0440\u0435\u043D\u043E\u0441 \u0441\u0442\u0440\u043E\u043A Notepad++ (MD)",
		L"\u041F\u0435\u0440\u0435\u043D\u043E\u0441 \u0432\u043D\u0443\u0442\u0440\u0438 \u0442\u0430\u0431\u043B\u0438\u0446\u044B (MD)",
		L"\u0410\u0432\u0442\u043E\u043F\u043E\u0434\u0433\u043E\u043D\u043A\u0430 \u043F\u0440\u0438 \u0438\u0437\u043C\u0435\u043D\u0435\u043D\u0438\u0438 \u043E\u043A\u043D\u0430 (MD)"
	},
	L"\u0412\u0441\u0442\u0430\u0432\u0438\u0442\u044C Markdown-\u0442\u0430\u0431\u043B\u0438\u0446\u0443",
	L"\u0421\u0442\u043E\u043B\u0431\u0446\u044B:",
	L"\u0421\u0442\u0440\u043E\u043A\u0438 \u0434\u0430\u043D\u043D\u044B\u0445:",
	L"\u041E\u041A",
	L"\u041E\u0442\u043C\u0435\u043D\u0430",
	L"\u0414\u043E\u043F\u0443\u0441\u0442\u0438\u043C\u043E: 1-50 \u0441\u0442\u043E\u043B\u0431\u0446\u043E\u0432 \u0438 0-200 \u0441\u0442\u0440\u043E\u043A \u0434\u0430\u043D\u043D\u044B\u0445.",
	L"\u041F\u043E\u043C\u0435\u0441\u0442\u0438\u0442\u0435 \u043A\u0443\u0440\u0441\u043E\u0440 \u0432\u043D\u0443\u0442\u0440\u044C Markdown-\u0442\u0430\u0431\u043B\u0438\u0446\u044B \u0441 \u0432\u0435\u0440\u0442\u0438\u043A\u0430\u043B\u044C\u043D\u044B\u043C\u0438 \u0447\u0435\u0440\u0442\u0430\u043C\u0438.",
	L"\u041D\u0435 \u0443\u0434\u0430\u043B\u043E\u0441\u044C \u0438\u0437\u043C\u0435\u043D\u0438\u0442\u044C Markdown-\u0442\u0430\u0431\u043B\u0438\u0446\u0443.",
	L"\u0412\u044B\u0434\u0435\u043B\u0438\u0442\u0435 CSV/TSV-\u0442\u0435\u043A\u0441\u0442 \u0438\u043B\u0438 \u043F\u043E\u043C\u0435\u0441\u0442\u0438\u0442\u0435 \u043A\u0443\u0440\u0441\u043E\u0440 \u0432\u043D\u0443\u0442\u0440\u044C CSV/TSV-\u0431\u043B\u043E\u043A\u0430.",
	L"\u041D\u0435 \u0443\u0434\u0430\u043B\u043E\u0441\u044C \u043F\u0440\u0435\u043E\u0431\u0440\u0430\u0437\u043E\u0432\u0430\u0442\u044C \u0432\u044B\u0431\u0440\u0430\u043D\u043D\u044B\u0439 CSV/TSV-\u0442\u0435\u043A\u0441\u0442."
};

const UiText japaneseUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Markdown \u30C6\u30FC\u30D6\u30EB\u30A8\u30C7\u30A3\u30BF\u30FC",
	L"\u30C6\u30FC\u30D6\u30EB\u3092\u6574\u5217",
	L"\u6B21\u306E\u30BB\u30EB",
	L"\u524D\u306E\u30BB\u30EB",
	L"\u4E0B\u306B\u884C\u3092\u633F\u5165",
	L"\u884C\u3092\u524A\u9664",
	L"\u53F3\u306B\u5217\u3092\u633F\u5165",
	L"\u5217\u3092\u524A\u9664",
	L"\u884C\u3092\u4E0A\u3078\u79FB\u52D5",
	L"\u884C\u3092\u4E0B\u3078\u79FB\u52D5",
	L"\u5217\u3092\u5DE6\u3078\u79FB\u52D5",
	L"\u5217\u3092\u53F3\u3078\u79FB\u52D5",
	L"\u884C\u3092\u6607\u9806\u3067\u4E26\u3079\u66FF\u3048",
	L"\u884C\u3092\u964D\u9806\u3067\u4E26\u3079\u66FF\u3048",
	L"CSV/TSV \u3092\u30C6\u30FC\u30D6\u30EB\u306B\u5909\u63DB",
	L"\u65B0\u3057\u3044\u30C6\u30FC\u30D6\u30EB\u3092\u633F\u5165",
	L"Tab: Markdown \u30C6\u30FC\u30D6\u30EB\u3092\u6574\u5217"
);

const UiText nigerianPidginUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Markdown Table Editor",
	L"Arrange table",
	L"Next cell",
	L"Previous cell",
	L"Put row below",
	L"Delete row",
	L"Put column for right",
	L"Delete column",
	L"Move row up",
	L"Move row down",
	L"Move column left",
	L"Move column right",
	L"Sort rows small to big",
	L"Sort rows big to small",
	L"Change CSV/TSV to table",
	L"Put new table",
	L"Tab: arrange Markdown table"
);

const UiText marathiUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Markdown \u0924\u0915\u094D\u0924\u093E \u0938\u0902\u092A\u093E\u0926\u0915",
	L"\u0924\u0915\u094D\u0924\u093E \u0938\u0902\u0930\u0947\u0916\u093F\u0924 \u0915\u0930\u093E",
	L"\u092A\u0941\u0922\u0940\u0932 \u0938\u0947\u0932",
	L"\u092E\u093E\u0917\u0940\u0932 \u0938\u0947\u0932",
	L"\u0916\u093E\u0932\u0940 \u0913\u0933 \u0918\u093E\u0932\u093E",
	L"\u0913\u0933 \u0939\u091F\u0935\u093E",
	L"\u0909\u091C\u0935\u0940\u0915\u0921\u0947 \u0938\u094D\u0924\u0902\u092D \u0918\u093E\u0932\u093E",
	L"\u0938\u094D\u0924\u0902\u092D \u0939\u091F\u0935\u093E",
	L"\u0913\u0933 \u0935\u0930 \u0939\u0932\u0935\u093E",
	L"\u0913\u0933 \u0916\u093E\u0932\u0940 \u0939\u0932\u0935\u093E",
	L"\u0938\u094D\u0924\u0902\u092D \u0921\u093E\u0935\u0940\u0915\u0921\u0947 \u0939\u0932\u0935\u093E",
	L"\u0938\u094D\u0924\u0902\u092D \u0909\u091C\u0935\u0940\u0915\u0921\u0947 \u0939\u0932\u0935\u093E",
	L"\u0913\u0933\u0940 \u091A\u0922\u0924\u094D\u092F\u093E \u0915\u094D\u0930\u092E\u093E\u0928\u0947 \u0932\u093E\u0935\u093E",
	L"\u0913\u0933\u0940 \u0909\u0924\u0930\u0924\u094D\u092F\u093E \u0915\u094D\u0930\u092E\u093E\u0928\u0947 \u0932\u093E\u0935\u093E",
	L"CSV/TSV \u0924\u0915\u094D\u0924\u094D\u092F\u093E\u0924 \u092C\u0926\u0932\u093E",
	L"\u0928\u0935\u093E \u0924\u0915\u094D\u0924\u093E \u0918\u093E\u0932\u093E",
	L"Tab: Markdown \u0924\u0915\u094D\u0924\u093E \u0938\u0902\u0930\u0947\u0916\u093F\u0924 \u0915\u0930\u093E"
);

const UiText teluguUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Markdown \u0C2A\u0C1F\u0C4D\u0C1F\u0C3F\u0C15 \u0C0E\u0C21\u0C3F\u0C1F\u0C30\u0C4D",
	L"\u0C2A\u0C1F\u0C4D\u0C1F\u0C3F\u0C15\u0C28\u0C41 \u0C38\u0C30\u0C3F\u0C2A\u0C30\u0C1A\u0C41",
	L"\u0C24\u0C26\u0C41\u0C2A\u0C30\u0C3F \u0C38\u0C46\u0C32\u0C4D",
	L"\u0C2E\u0C41\u0C28\u0C41\u0C2A\u0C1F\u0C3F \u0C38\u0C46\u0C32\u0C4D",
	L"\u0C15\u0C4D\u0C30\u0C3F\u0C02\u0C26 \u0C35\u0C30\u0C41\u0C38 \u0C1A\u0C4A\u0C2A\u0C4D\u0C2A\u0C3F\u0C02\u0C1A\u0C41",
	L"\u0C35\u0C30\u0C41\u0C38\u0C28\u0C41 \u0C24\u0C4A\u0C32\u0C17\u0C3F\u0C02\u0C1A\u0C41",
	L"\u0C15\u0C41\u0C21\u0C3F\u0C35\u0C48\u0C2A\u0C41 \u0C28\u0C3F\u0C32\u0C41\u0C35\u0C41 \u0C35\u0C30\u0C41\u0C38 \u0C1A\u0C4A\u0C2A\u0C4D\u0C2A\u0C3F\u0C02\u0C1A\u0C41",
	L"\u0C28\u0C3F\u0C32\u0C41\u0C35\u0C41 \u0C35\u0C30\u0C41\u0C38\u0C28\u0C41 \u0C24\u0C4A\u0C32\u0C17\u0C3F\u0C02\u0C1A\u0C41",
	L"\u0C35\u0C30\u0C41\u0C38\u0C28\u0C41 \u0C2A\u0C48\u0C15\u0C3F \u0C24\u0C30\u0C32\u0C3F\u0C02\u0C1A\u0C41",
	L"\u0C35\u0C30\u0C41\u0C38\u0C28\u0C41 \u0C15\u0C4D\u0C30\u0C3F\u0C02\u0C26\u0C3F\u0C15\u0C3F \u0C24\u0C30\u0C32\u0C3F\u0C02\u0C1A\u0C41",
	L"\u0C28\u0C3F\u0C32\u0C41\u0C35\u0C41 \u0C35\u0C30\u0C41\u0C38\u0C28\u0C41 \u0C0E\u0C21\u0C2E\u0C15\u0C41 \u0C24\u0C30\u0C32\u0C3F\u0C02\u0C1A\u0C41",
	L"\u0C28\u0C3F\u0C32\u0C41\u0C35\u0C41 \u0C35\u0C30\u0C41\u0C38\u0C28\u0C41 \u0C15\u0C41\u0C21\u0C3F\u0C15\u0C3F \u0C24\u0C30\u0C32\u0C3F\u0C02\u0C1A\u0C41",
	L"\u0C35\u0C30\u0C41\u0C38\u0C32\u0C28\u0C41 \u0C06\u0C30\u0C4B\u0C39\u0C23\u0C02\u0C17\u0C3E \u0C15\u0C4D\u0C30\u0C2E\u0C2C\u0C26\u0C4D\u0C27\u0C40\u0C15\u0C30\u0C3F\u0C02\u0C1A\u0C41",
	L"\u0C35\u0C30\u0C41\u0C38\u0C32\u0C28\u0C41 \u0C05\u0C35\u0C30\u0C4B\u0C39\u0C23\u0C02\u0C17\u0C3E \u0C15\u0C4D\u0C30\u0C2E\u0C2C\u0C26\u0C4D\u0C27\u0C40\u0C15\u0C30\u0C3F\u0C02\u0C1A\u0C41",
	L"CSV/TSV \u0C28\u0C41 \u0C2A\u0C1F\u0C4D\u0C1F\u0C3F\u0C15\u0C17\u0C3E \u0C2E\u0C3E\u0C30\u0C4D\u0C1A\u0C41",
	L"\u0C15\u0C4A\u0C24\u0C4D\u0C24 \u0C2A\u0C1F\u0C4D\u0C1F\u0C3F\u0C15\u0C28\u0C41 \u0C1A\u0C4A\u0C2A\u0C4D\u0C2A\u0C3F\u0C02\u0C1A\u0C41",
	L"Tab: Markdown \u0C2A\u0C1F\u0C4D\u0C1F\u0C3F\u0C15\u0C28\u0C41 \u0C38\u0C30\u0C3F\u0C2A\u0C30\u0C1A\u0C41"
);

const UiText turkishUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Markdown Tablo D\u00FCzenleyici",
	L"Tabloyu hizala",
	L"Sonraki h\u00FCcre",
	L"\u00D6nceki h\u00FCcre",
	L"Alta sat\u0131r ekle",
	L"Sat\u0131r\u0131 sil",
	L"Sa\u011Fa s\u00FCtun ekle",
	L"S\u00FCtunu sil",
	L"Sat\u0131r\u0131 yukar\u0131 ta\u015F\u0131",
	L"Sat\u0131r\u0131 a\u015Fa\u011F\u0131 ta\u015F\u0131",
	L"S\u00FCtunu sola ta\u015F\u0131",
	L"S\u00FCtunu sa\u011Fa ta\u015F\u0131",
	L"Sat\u0131rlar\u0131 artan s\u0131rala",
	L"Sat\u0131rlar\u0131 azalan s\u0131rala",
	L"CSV/TSV'yi tabloya d\u00F6n\u00FC\u015Ft\u00FCr",
	L"Yeni tablo ekle",
	L"Tab: Markdown tablosunu hizala"
);

const UiText tamilUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Markdown \u0B85\u0B9F\u0BCD\u0B9F\u0BB5\u0BA3\u0BC8 \u0BA4\u0BBF\u0BB0\u0BC1\u0BA4\u0BCD\u0BA4\u0BBF",
	L"\u0B85\u0B9F\u0BCD\u0B9F\u0BB5\u0BA3\u0BC8\u0BAF\u0BC8 \u0B92\u0BB4\u0BC1\u0B99\u0BCD\u0B95\u0BC1\u0BAA\u0B9F\u0BC1\u0BA4\u0BCD\u0BA4\u0BC1",
	L"\u0B85\u0B9F\u0BC1\u0BA4\u0BCD\u0BA4 \u0B9A\u0BC6\u0BB2\u0BCD",
	L"\u0BAE\u0BC1\u0BA8\u0BCD\u0BA4\u0BC8\u0BAF \u0B9A\u0BC6\u0BB2\u0BCD",
	L"\u0B95\u0BC0\u0BB4\u0BC7 \u0BB5\u0BB0\u0BBF\u0B9A\u0BC8 \u0B9A\u0BC6\u0BB0\u0BC1\u0B95\u0BC1",
	L"\u0BB5\u0BB0\u0BBF\u0B9A\u0BC8\u0BAF\u0BC8 \u0BA8\u0BC0\u0B95\u0BCD\u0B95\u0BC1",
	L"\u0BB5\u0BB2\u0BAA\u0BCD\u0BAA\u0BC1\u0BB1\u0BAE\u0BCD \u0BA8\u0BC6\u0B9F\u0BC1\u0BB5\u0BB0\u0BBF\u0B9A\u0BC8 \u0B9A\u0BC6\u0BB0\u0BC1\u0B95\u0BC1",
	L"\u0BA8\u0BC6\u0B9F\u0BC1\u0BB5\u0BB0\u0BBF\u0B9A\u0BC8 \u0BA8\u0BC0\u0B95\u0BCD\u0B95\u0BC1",
	L"\u0BB5\u0BB0\u0BBF\u0B9A\u0BC8\u0BAF\u0BC8 \u0BAE\u0BC7\u0BB2\u0BC7 \u0BA8\u0B95\u0BB0\u0BCD\u0BA4\u0BCD\u0BA4\u0BC1",
	L"\u0BB5\u0BB0\u0BBF\u0B9A\u0BC8\u0BAF\u0BC8 \u0B95\u0BC0\u0BB4\u0BC7 \u0BA8\u0B95\u0BB0\u0BCD\u0BA4\u0BCD\u0BA4\u0BC1",
	L"\u0BA8\u0BC6\u0B9F\u0BC1\u0BB5\u0BB0\u0BBF\u0B9A\u0BC8\u0BAF\u0BC8 \u0B87\u0B9F\u0BAA\u0BCD\u0BAA\u0BC1\u0BB1\u0BAE\u0BCD \u0BA8\u0B95\u0BB0\u0BCD\u0BA4\u0BCD\u0BA4\u0BC1",
	L"\u0BA8\u0BC6\u0B9F\u0BC1\u0BB5\u0BB0\u0BBF\u0B9A\u0BC8\u0BAF\u0BC8 \u0BB5\u0BB2\u0BAA\u0BCD\u0BAA\u0BC1\u0BB1\u0BAE\u0BCD \u0BA8\u0B95\u0BB0\u0BCD\u0BA4\u0BCD\u0BA4\u0BC1",
	L"\u0BB5\u0BB0\u0BBF\u0B9A\u0BC8\u0B95\u0BB3\u0BC8 \u0B8F\u0BB1\u0BC1\u0BB5\u0BB0\u0BBF\u0B9A\u0BC8\u0BAF\u0BBF\u0BB2\u0BCD \u0B85\u0B9F\u0BC1\u0B95\u0BCD\u0B95\u0BC1",
	L"\u0BB5\u0BB0\u0BBF\u0B9A\u0BC8\u0B95\u0BB3\u0BC8 \u0B87\u0BB1\u0B99\u0BCD\u0B95\u0BC1\u0BB5\u0BB0\u0BBF\u0B9A\u0BC8\u0BAF\u0BBF\u0BB2\u0BCD \u0B85\u0B9F\u0BC1\u0B95\u0BCD\u0B95\u0BC1",
	L"CSV/TSV \u0B90 \u0B85\u0B9F\u0BCD\u0B9F\u0BB5\u0BA3\u0BC8\u0BAF\u0BBE\u0B95 \u0BAE\u0BBE\u0BB1\u0BCD\u0BB1\u0BC1",
	L"\u0BAA\u0BC1\u0BA4\u0BBF\u0BAF \u0B85\u0B9F\u0BCD\u0B9F\u0BB5\u0BA3\u0BC8 \u0B9A\u0BC6\u0BB0\u0BC1\u0B95\u0BC1",
	L"Tab: Markdown \u0B85\u0B9F\u0BCD\u0B9F\u0BB5\u0BA3\u0BC8\u0BAF\u0BC8 \u0B92\u0BB4\u0BC1\u0B99\u0BCD\u0B95\u0BC1\u0BAA\u0B9F\u0BC1\u0BA4\u0BCD\u0BA4\u0BC1"
);

const UiText yueChineseUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Markdown \u8868\u683C\u7DE8\u8F2F\u5668",
	L"\u5C0D\u9F4A\u8868\u683C",
	L"\u4E0B\u4E00\u500B\u5132\u5B58\u683C",
	L"\u4E0A\u4E00\u500B\u5132\u5B58\u683C",
	L"\u55BA\u4E0B\u9762\u63D2\u5165\u5217",
	L"\u522A\u9664\u5217",
	L"\u55BA\u53F3\u908A\u63D2\u5165\u6B04",
	L"\u522A\u9664\u6B04",
	L"\u4E0A\u79FB\u5217",
	L"\u4E0B\u79FB\u5217",
	L"\u5DE6\u79FB\u6B04",
	L"\u53F3\u79FB\u6B04",
	L"\u905E\u589E\u6392\u5E8F\u5217",
	L"\u905E\u6E1B\u6392\u5E8F\u5217",
	L"\u5C07 CSV/TSV \u8F49\u6210\u8868\u683C",
	L"\u63D2\u5165\u65B0\u8868\u683C",
	L"Tab\uFF1A\u5C0D\u9F4A Markdown \u8868\u683C"
);

const UiText vietnameseUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Tr\u00ECnh s\u1EEDa b\u1EA3ng Markdown",
	L"C\u0103n ch\u1EC9nh b\u1EA3ng",
	L"\u00D4 ti\u1EBFp theo",
	L"\u00D4 tr\u01B0\u1EDBc",
	L"Ch\u00E8n h\u00E0ng b\u00EAn d\u01B0\u1EDBi",
	L"X\u00F3a h\u00E0ng",
	L"Ch\u00E8n c\u1ED9t b\u00EAn ph\u1EA3i",
	L"X\u00F3a c\u1ED9t",
	L"Di chuy\u1EC3n h\u00E0ng l\u00EAn",
	L"Di chuy\u1EC3n h\u00E0ng xu\u1ED1ng",
	L"Di chuy\u1EC3n c\u1ED9t sang tr\u00E1i",
	L"Di chuy\u1EC3n c\u1ED9t sang ph\u1EA3i",
	L"S\u1EAFp x\u1EBFp h\u00E0ng t\u0103ng d\u1EA7n",
	L"S\u1EAFp x\u1EBFp h\u00E0ng gi\u1EA3m d\u1EA7n",
	L"Chuy\u1EC3n CSV/TSV th\u00E0nh b\u1EA3ng",
	L"Ch\u00E8n b\u1EA3ng m\u1EDBi",
	L"Tab: c\u0103n ch\u1EC9nh b\u1EA3ng Markdown"
);

UiLanguage g_uiLanguage = UiLanguage::English;

const UiText &uiText()
{
	switch (g_uiLanguage)
	{
	case UiLanguage::MandarinChinese:
		return chineseUiText;
	case UiLanguage::Hindi:
		return hindiUiText;
	case UiLanguage::Spanish:
		return spanishUiText;
	case UiLanguage::Arabic:
		return arabicUiText;
	case UiLanguage::French:
		return frenchUiText;
	case UiLanguage::Bengali:
		return bengaliUiText;
	case UiLanguage::Portuguese:
		return portugueseUiText;
	case UiLanguage::Russian:
		return russianUiText;
	case UiLanguage::Indonesian:
		return indonesianUiText;
	case UiLanguage::Urdu:
		return urduUiText;
	case UiLanguage::German:
		return germanUiText;
	case UiLanguage::Japanese:
		return japaneseUiText;
	case UiLanguage::NigerianPidgin:
		return nigerianPidginUiText;
	case UiLanguage::Marathi:
		return marathiUiText;
	case UiLanguage::Telugu:
		return teluguUiText;
	case UiLanguage::Turkish:
		return turkishUiText;
	case UiLanguage::Tamil:
		return tamilUiText;
	case UiLanguage::YueChinese:
		return yueChineseUiText;
	case UiLanguage::Vietnamese:
		return vietnameseUiText;
	default:
		return englishUiText;
	}
}

const wchar_t *commandText(std::size_t index)
{
	const UiText &localized = uiText();
	if (index < static_cast<std::size_t>(nbFunc) && localized.commands[index])
		return localized.commands[index];
	return englishUiText.commands[index];
}

std::string toLowerAscii(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	return value;
}

bool containsMarker(const std::string &value, const char *marker)
{
	return value.find(marker) != std::string::npos;
}

bool matchesLanguageCode(const std::string &value, const char *code)
{
	const std::string exact = std::string(code) + ".xml";
	const std::string hyphenPrefix = std::string(code) + "-";
	const std::string underscorePrefix = std::string(code) + "_";
	return value == exact || value.find(hyphenPrefix) == 0 || value.find(underscorePrefix) == 0;
}

UiLanguage languageFromNativeLangFileName(const std::string &nativeLangFileName)
{
	const std::string lower = toLowerAscii(nativeLangFileName);
	if (containsMarker(lower, "chinesetraditional") || containsMarker(lower, "chinese_traditional") ||
		containsMarker(lower, "traditionalchinese") || containsMarker(lower, "cantonese") ||
		matchesLanguageCode(lower, "yue") || matchesLanguageCode(lower, "zh-hk") ||
		matchesLanguageCode(lower, "zh_hk") || matchesLanguageCode(lower, "zh-tw") ||
		matchesLanguageCode(lower, "zh_tw"))
		return UiLanguage::YueChinese;
	if (containsMarker(lower, "chinesesimplified") || containsMarker(lower, "chinese_simplified") ||
		containsMarker(lower, "simplifiedchinese") || containsMarker(lower, "mandarin") ||
		matchesLanguageCode(lower, "zh-cn") || matchesLanguageCode(lower, "zh_cn") ||
		matchesLanguageCode(lower, "zh"))
		return UiLanguage::MandarinChinese;
	if (containsMarker(lower, "hindi") || matchesLanguageCode(lower, "hi"))
		return UiLanguage::Hindi;
	if (containsMarker(lower, "spanish") || containsMarker(lower, "espanol") || matchesLanguageCode(lower, "es"))
		return UiLanguage::Spanish;
	if (containsMarker(lower, "arabic") || matchesLanguageCode(lower, "ar"))
		return UiLanguage::Arabic;
	if (containsMarker(lower, "french") || containsMarker(lower, "francais") || matchesLanguageCode(lower, "fr"))
		return UiLanguage::French;
	if (containsMarker(lower, "bengali") || containsMarker(lower, "bangla") || matchesLanguageCode(lower, "bn"))
		return UiLanguage::Bengali;
	if (containsMarker(lower, "portuguese") || containsMarker(lower, "portugues") || matchesLanguageCode(lower, "pt"))
		return UiLanguage::Portuguese;
	if (containsMarker(lower, "russian") || matchesLanguageCode(lower, "ru"))
		return UiLanguage::Russian;
	if (containsMarker(lower, "indonesian") || containsMarker(lower, "bahasaindonesia") || matchesLanguageCode(lower, "id"))
		return UiLanguage::Indonesian;
	if (containsMarker(lower, "urdu") || matchesLanguageCode(lower, "ur"))
		return UiLanguage::Urdu;
	if (containsMarker(lower, "german") || containsMarker(lower, "deutsch") || matchesLanguageCode(lower, "de"))
		return UiLanguage::German;
	if (containsMarker(lower, "japanese") || matchesLanguageCode(lower, "ja"))
		return UiLanguage::Japanese;
	if (containsMarker(lower, "pidgin") || containsMarker(lower, "nigerian") || matchesLanguageCode(lower, "pcm"))
		return UiLanguage::NigerianPidgin;
	if (containsMarker(lower, "marathi") || matchesLanguageCode(lower, "mr"))
		return UiLanguage::Marathi;
	if (containsMarker(lower, "telugu") || matchesLanguageCode(lower, "te"))
		return UiLanguage::Telugu;
	if (containsMarker(lower, "turkish") || containsMarker(lower, "turkce") || matchesLanguageCode(lower, "tr"))
		return UiLanguage::Turkish;
	if (containsMarker(lower, "tamil") || matchesLanguageCode(lower, "ta"))
		return UiLanguage::Tamil;
	if (containsMarker(lower, "vietnamese") || containsMarker(lower, "tiengviet") || matchesLanguageCode(lower, "vi"))
		return UiLanguage::Vietnamese;
	return UiLanguage::English;
}

void applyCommandNames()
{
	for (int index = 0; index < nbFunc; ++index)
		lstrcpyn(funcItem[index]._itemName, commandText(static_cast<std::size_t>(index)), menuItemSize);
}

void setUiLanguage(UiLanguage language)
{
	g_uiLanguage = language;
	applyCommandNames();
}

HWND currentScintilla()
{
	int which = -1;
	::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, reinterpret_cast<LPARAM>(&which));
	if (which == -1)
		return NULL;
	return which == 0 ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}

void showMessage(const wchar_t *message)
{
	::MessageBox(nppData._nppHandle, message, uiText().pluginMenuName, MB_OK | MB_ICONINFORMATION);
}

std::string nativeLangFileName()
{
	if (!nppData._nppHandle)
		return std::string();

	const LRESULT required = ::SendMessage(nppData._nppHandle, NPPM_GETNATIVELANGFILENAME, 0, 0);
	if (required <= 0)
		return std::string();

	std::vector<char> buffer(static_cast<std::size_t>(required) + 1, '\0');
	const LRESULT copied = ::SendMessage(nppData._nppHandle, NPPM_GETNATIVELANGFILENAME, static_cast<WPARAM>(buffer.size()), reinterpret_cast<LPARAM>(&buffer[0]));
	if (copied <= 0)
		return std::string();

	return std::string(&buffer[0], static_cast<std::size_t>(copied));
}

std::string extractNativeLangFileName(const std::string &text)
{
	const std::string marker = "filename=\"";
	const std::size_t start = text.find(marker);
	if (start == std::string::npos)
		return std::string();

	const std::size_t valueStart = start + marker.size();
	const std::size_t valueEnd = text.find('"', valueStart);
	if (valueEnd == std::string::npos || valueEnd <= valueStart)
		return std::string();

	return text.substr(valueStart, valueEnd - valueStart);
}

std::string nativeLangFileNameFromDisk()
{
#ifdef MARKDOWN_TABLE_PLUGIN_TESTING
	return std::string();
#else
	const char *appData = std::getenv("APPDATA");
	if (!appData || !*appData)
		return std::string();

	const std::string path = std::string(appData) + "\\Notepad++\\nativeLang.xml";
	std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
	if (!input)
		return std::string();

	std::string text;
	text.reserve(4096);
	char buffer[1024];
	while (input && text.size() < 4096)
	{
		input.read(buffer, sizeof(buffer));
		text.append(buffer, static_cast<std::size_t>(input.gcount()));
	}
	return extractNativeLangFileName(text);
#endif
}

std::string resolvedNativeLangFileName()
{
	const std::string fromNotepad = nativeLangFileName();
	if (!fromNotepad.empty())
		return fromNotepad;
	return nativeLangFileNameFromDisk();
}

void refreshUiLanguageState()
{
	setUiLanguage(languageFromNativeLangFileName(resolvedNativeLangFileName()));
}

bool isPluginCommandId(UINT menuId)
{
	if (menuId == static_cast<UINT>(-1))
		return false;

	for (int index = 0; index < nbFunc; ++index)
	{
		if (funcItem[index]._cmdID != 0 && menuId == static_cast<UINT>(funcItem[index]._cmdID))
			return true;
	}
	return false;
}

bool setMenuItemTextByPosition(HMENU menu, int position, const wchar_t *text)
{
	MENUITEMINFO info;
	ZeroMemory(&info, sizeof(info));
	info.cbSize = sizeof(info);
	info.fMask = MIIM_STRING;
	info.dwTypeData = const_cast<wchar_t *>(text);
	return ::SetMenuItemInfo(menu, static_cast<UINT>(position), TRUE, &info) != FALSE;
}

bool menuContainsPluginCommand(HMENU menu)
{
	if (!menu)
		return false;

	const int count = ::GetMenuItemCount(menu);
	for (int index = 0; index < count; ++index)
	{
		if (isPluginCommandId(::GetMenuItemID(menu, index)))
			return true;

		HMENU subMenu = ::GetSubMenu(menu, index);
		if (subMenu && menuContainsPluginCommand(subMenu))
			return true;
	}
	return false;
}

bool updatePluginSubmenuTitle(HMENU menu)
{
	if (!menu)
		return false;

	const int count = ::GetMenuItemCount(menu);
	for (int index = 0; index < count; ++index)
	{
		HMENU subMenu = ::GetSubMenu(menu, index);
		if (subMenu && menuContainsPluginCommand(subMenu))
			return setMenuItemTextByPosition(menu, index, uiText().pluginMenuName);

		if (subMenu && updatePluginSubmenuTitle(subMenu))
			return true;
	}
	return false;
}

bool updateCommandMenuItem(HMENU menu, UINT commandId, const wchar_t *text)
{
	if (!menu)
		return false;

	const int count = ::GetMenuItemCount(menu);
	for (int index = 0; index < count; ++index)
	{
		if (::GetMenuItemID(menu, index) == commandId)
			return setMenuItemTextByPosition(menu, index, text);

		HMENU subMenu = ::GetSubMenu(menu, index);
		if (subMenu && updateCommandMenuItem(subMenu, commandId, text))
			return true;
	}
	return false;
}

void updateNotepadPluginMenu()
{
	if (!nppData._nppHandle)
		return;

	HMENU pluginsMenu = reinterpret_cast<HMENU>(::SendMessage(nppData._nppHandle, NPPM_GETMENUHANDLE, NPPPLUGINMENU, 0));
	if (!pluginsMenu)
		return;

	updatePluginSubmenuTitle(pluginsMenu);
	for (int index = 0; index < nbFunc; ++index)
	{
		if (funcItem[index]._cmdID != 0)
			updateCommandMenuItem(pluginsMenu, static_cast<UINT>(funcItem[index]._cmdID), commandText(static_cast<std::size_t>(index)));
	}
	::DrawMenuBar(nppData._nppHandle);
}

std::uint32_t argb(unsigned char alpha, unsigned char red, unsigned char green, unsigned char blue)
{
	return (static_cast<std::uint32_t>(alpha) << 24) |
		(static_cast<std::uint32_t>(red) << 16) |
		(static_cast<std::uint32_t>(green) << 8) |
		static_cast<std::uint32_t>(blue);
}

void setIconPixel(std::uint32_t *pixels, int x, int y, std::uint32_t color)
{
	if (!pixels || x < 0 || x >= 16 || y < 0 || y >= 16)
		return;
	pixels[y * 16 + x] = color;
}

void drawIconHorizontalLine(std::uint32_t *pixels, int x1, int x2, int y, std::uint32_t color)
{
	for (int x = x1; x <= x2; ++x)
		setIconPixel(pixels, x, y, color);
}

void drawIconVerticalLine(std::uint32_t *pixels, int x, int y1, int y2, std::uint32_t color)
{
	for (int y = y1; y <= y2; ++y)
		setIconPixel(pixels, x, y, color);
}

enum class ToolbarIconKind
{
	TabAction,
	TableWrap,
	EditorWordWrap,
	AutoWrap
};

void clearIcon(std::uint32_t *pixels)
{
	for (int y = 0; y < 16; ++y)
		for (int x = 0; x < 16; ++x)
			pixels[y * 16 + x] = argb(0, 0, 0, 0);
}

void drawMdBadge(std::uint32_t *pixels, std::uint32_t color)
{
	drawIconVerticalLine(pixels, 1, 1, 5, color);
	drawIconVerticalLine(pixels, 5, 1, 5, color);
	setIconPixel(pixels, 2, 2, color);
	setIconPixel(pixels, 3, 3, color);
	setIconPixel(pixels, 4, 2, color);

	drawIconVerticalLine(pixels, 7, 1, 5, color);
	drawIconHorizontalLine(pixels, 7, 10, 1, color);
	drawIconHorizontalLine(pixels, 7, 10, 5, color);
	drawIconVerticalLine(pixels, 11, 2, 4, color);
}

void drawMiniTable(std::uint32_t *pixels, std::uint32_t grid, std::uint32_t muted)
{
	drawIconHorizontalLine(pixels, 1, 14, 8, grid);
	drawIconHorizontalLine(pixels, 1, 14, 11, muted);
	drawIconHorizontalLine(pixels, 1, 14, 14, grid);
	drawIconVerticalLine(pixels, 1, 8, 14, grid);
	drawIconVerticalLine(pixels, 7, 8, 14, muted);
	drawIconVerticalLine(pixels, 14, 8, 14, grid);
}

void drawEditorLines(std::uint32_t *pixels, std::uint32_t color, std::uint32_t muted)
{
	drawIconHorizontalLine(pixels, 2, 13, 3, color);
	drawIconHorizontalLine(pixels, 2, 13, 6, muted);
	drawIconHorizontalLine(pixels, 2, 9, 9, color);
	drawIconHorizontalLine(pixels, 2, 13, 13, muted);
}

void drawTabActionArrow(std::uint32_t *pixels, std::uint32_t accent)
{
	drawIconHorizontalLine(pixels, 1, 5, 8, accent);
	drawIconHorizontalLine(pixels, 1, 9, 10, accent);
	drawIconHorizontalLine(pixels, 3, 12, 13, accent);
	setIconPixel(pixels, 12, 11, accent);
	setIconPixel(pixels, 13, 12, accent);
	setIconPixel(pixels, 12, 14, accent);
	drawIconVerticalLine(pixels, 14, 8, 14, accent);
}

void drawAutoWrapArrow(std::uint32_t *pixels, std::uint32_t accent)
{
	drawIconHorizontalLine(pixels, 8, 13, 9, accent);
	drawIconVerticalLine(pixels, 13, 9, 12, accent);
	drawIconHorizontalLine(pixels, 5, 13, 12, accent);
	setIconPixel(pixels, 5, 12, accent);
	setIconPixel(pixels, 6, 11, accent);
	setIconPixel(pixels, 6, 13, accent);
}

void drawMarkdownToolbarIcon(std::uint32_t *pixels, bool darkMode, ToolbarIconKind kind)
{
	const std::uint32_t grid = darkMode ? argb(255, 232, 238, 246) : argb(255, 35, 48, 61);
	const std::uint32_t muted = darkMode ? argb(255, 142, 157, 174) : argb(255, 130, 145, 160);
	const std::uint32_t tabAccent = darkMode ? argb(255, 107, 203, 255) : argb(255, 0, 120, 215);
	const std::uint32_t wrapAccent = darkMode ? argb(255, 93, 224, 164) : argb(255, 20, 145, 82);
	const std::uint32_t editorAccent = darkMode ? argb(255, 255, 191, 105) : argb(255, 196, 112, 0);

	clearIcon(pixels);
	if (kind == ToolbarIconKind::TabAction)
	{
		drawMdBadge(pixels, grid);
		drawTabActionArrow(pixels, tabAccent);
	}
	else if (kind == ToolbarIconKind::TableWrap || kind == ToolbarIconKind::AutoWrap)
	{
		drawMdBadge(pixels, grid);
		drawMiniTable(pixels, grid, muted);
		drawAutoWrapArrow(pixels, wrapAccent);
	}
	else
	{
		drawEditorLines(pixels, grid, muted);
		drawAutoWrapArrow(pixels, editorAccent);
	}
}

HBITMAP createToolbarBitmap(ToolbarIconKind kind, bool darkMode)
{
	BITMAPINFO info;
	ZeroMemory(&info, sizeof(info));
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = 16;
	info.bmiHeader.biHeight = -16;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;

	void *bits = NULL;
	HBITMAP bitmap = ::CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &bits, NULL, 0);
	if (!bitmap || !bits)
		return bitmap;

	drawMarkdownToolbarIcon(reinterpret_cast<std::uint32_t *>(bits), darkMode, kind);
	return bitmap;
}

HICON createToolbarIcon(ToolbarIconKind kind, bool darkMode)
{
	HBITMAP color = createToolbarBitmap(kind, darkMode);
	if (!color)
		return NULL;

	const unsigned char maskBits[32] = { 0 };
	HBITMAP mask = ::CreateBitmap(16, 16, 1, 1, maskBits);
	if (!mask)
	{
		::DeleteObject(color);
		return NULL;
	}

	ICONINFO iconInfo;
	ZeroMemory(&iconInfo, sizeof(iconInfo));
	iconInfo.fIcon = TRUE;
	iconInfo.hbmColor = color;
	iconInfo.hbmMask = mask;
	HICON icon = ::CreateIconIndirect(&iconInfo);

	::DeleteObject(color);
	::DeleteObject(mask);
	return icon;
}

bool ensureToolbarIconHandles(HBITMAP &bitmap, HICON &icon, HICON &darkModeIcon, ToolbarIconKind kind)
{
	if (!bitmap)
		bitmap = createToolbarBitmap(kind, false);
	if (!icon)
		icon = createToolbarIcon(kind, false);
	if (!darkModeIcon)
		darkModeIcon = createToolbarIcon(kind, true);
	return bitmap && icon && darkModeIcon;
}

void destroyToolbarIconHandles(HBITMAP &bitmap, HICON &icon, HICON &darkModeIcon)
{
	if (bitmap)
	{
		::DeleteObject(bitmap);
		bitmap = NULL;
	}
	if (icon)
	{
		::DestroyIcon(icon);
		icon = NULL;
	}
	if (darkModeIcon)
	{
		::DestroyIcon(darkModeIcon);
		darkModeIcon = NULL;
	}
}

bool ensureTabToolbarIconHandles()
{
	return ensureToolbarIconHandles(g_tabToolbarBmp, g_tabToolbarIcon, g_tabToolbarIconDarkMode, ToolbarIconKind::TabAction);
}

bool ensureWrapLongCellsToolbarIconHandles()
{
	return ensureToolbarIconHandles(g_wrapLongCellsToolbarBmp, g_wrapLongCellsToolbarIcon, g_wrapLongCellsToolbarIconDarkMode, ToolbarIconKind::TableWrap);
}

bool ensureNotepadWordWrapToolbarIconHandles()
{
	return ensureToolbarIconHandles(g_notepadWordWrapToolbarBmp, g_notepadWordWrapToolbarIcon, g_notepadWordWrapToolbarIconDarkMode, ToolbarIconKind::EditorWordWrap);
}

bool ensureAutoWrapToolbarIconHandles()
{
	return ensureToolbarIconHandles(g_autoWrapToolbarBmp, g_autoWrapToolbarIcon, g_autoWrapToolbarIconDarkMode, ToolbarIconKind::AutoWrap);
}

bool ensureFitToWindowOnResizeToolbarIconHandles()
{
	return ensureToolbarIconHandles(
		g_fitToWindowOnResizeToolbarBmp,
		g_fitToWindowOnResizeToolbarIcon,
		g_fitToWindowOnResizeToolbarIconDarkMode,
		ToolbarIconKind::TableWrap);
}

void destroyTabToolbarIconHandles()
{
	destroyToolbarIconHandles(g_tabToolbarBmp, g_tabToolbarIcon, g_tabToolbarIconDarkMode);
}

void destroyAutoWrapToolbarIconHandles()
{
	destroyToolbarIconHandles(g_autoWrapToolbarBmp, g_autoWrapToolbarIcon, g_autoWrapToolbarIconDarkMode);
}

void destroyWrapLongCellsToolbarIconHandles()
{
	destroyToolbarIconHandles(g_wrapLongCellsToolbarBmp, g_wrapLongCellsToolbarIcon, g_wrapLongCellsToolbarIconDarkMode);
}

void destroyNotepadWordWrapToolbarIconHandles()
{
	destroyToolbarIconHandles(g_notepadWordWrapToolbarBmp, g_notepadWordWrapToolbarIcon, g_notepadWordWrapToolbarIconDarkMode);
}

void destroyFitToWindowOnResizeToolbarIconHandles()
{
	destroyToolbarIconHandles(
		g_fitToWindowOnResizeToolbarBmp,
		g_fitToWindowOnResizeToolbarIcon,
		g_fitToWindowOnResizeToolbarIconDarkMode);
}

BOOL CALLBACK findToolbarWindowCallback(HWND hwnd, LPARAM lParam)
{
	wchar_t className[64] = { 0 };
	::GetClassName(hwnd, className, static_cast<int>(sizeof(className) / sizeof(className[0])));
	if (std::wcscmp(className, L"ToolbarWindow32") == 0)
	{
		HWND *result = reinterpret_cast<HWND *>(lParam);
		*result = hwnd;
		return FALSE;
	}
	return TRUE;
}

HWND findToolbarWindow(HWND parent)
{
	if (!parent)
		return NULL;

	HWND toolbar = NULL;
	::EnumChildWindows(parent, findToolbarWindowCallback, reinterpret_cast<LPARAM>(&toolbar));
	return toolbar;
}

void setToolbarCheckState(std::size_t commandIndex, bool checked)
{
	HWND toolbar = findToolbarWindow(nppData._nppHandle);
	if (!toolbar || commandIndex >= static_cast<std::size_t>(nbFunc) || funcItem[commandIndex]._cmdID == 0)
		return;

	::SendMessage(
		toolbar,
		TB_CHECKBUTTON,
		static_cast<WPARAM>(funcItem[commandIndex]._cmdID),
		MAKELPARAM(checked ? TRUE : FALSE, 0));
}

void setToolbarEnabledState(std::size_t commandIndex, bool enabled)
{
	HWND toolbar = findToolbarWindow(nppData._nppHandle);
	if (!toolbar || commandIndex >= static_cast<std::size_t>(nbFunc) || funcItem[commandIndex]._cmdID == 0)
		return;

	::SendMessage(
		toolbar,
		TB_ENABLEBUTTON,
		static_cast<WPARAM>(funcItem[commandIndex]._cmdID),
		MAKELPARAM(enabled ? TRUE : FALSE, 0));
}

void setCommandEnabledState(std::size_t commandIndex, bool enabled)
{
	if (!nppData._nppHandle || commandIndex >= static_cast<std::size_t>(nbFunc) || funcItem[commandIndex]._cmdID == 0)
		return;

	HMENU pluginsMenu = reinterpret_cast<HMENU>(::SendMessage(nppData._nppHandle, NPPM_GETMENUHANDLE, NPPPLUGINMENU, 0));
	if (pluginsMenu)
	{
		::EnableMenuItem(
			pluginsMenu,
			static_cast<UINT>(funcItem[commandIndex]._cmdID),
			MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_GRAYED));
		::DrawMenuBar(nppData._nppHandle);
	}
	setToolbarEnabledState(commandIndex, enabled);
}

void setAutoWrapToolbarCheckState()
{
	setToolbarCheckState(autoWrapLongCellsCommandIndex, g_autoWrapLongCells);
}

void setFitToWindowOnResizeToolbarCheckState()
{
	setToolbarCheckState(fitToWindowOnResizeCommandIndex, g_fitToWindowOnResize);
}

bool notepadWordWrapEnabled()
{
	HWND scintilla = currentScintilla();
	if (!scintilla)
		return false;

	return ::SendMessage(scintilla, SCI_GETWRAPMODE, 0, 0) != SC_WRAP_NONE;
}

void setNotepadWordWrapToolbarCheckState()
{
	setToolbarCheckState(notepadWordWrapCommandIndex, notepadWordWrapEnabled());
}

bool shouldApplyAutoWrapAfterAction(MarkdownTable::Action action)
{
	return g_autoWrapLongCells && action == MarkdownTable::Action::Align;
}

MarkdownTable::Action coreActionForPluginAction(MarkdownTable::Action action)
{
	return action == MarkdownTable::Action::WrapLongCells
		? MarkdownTable::Action::Align
		: action;
}

bool shouldFitToWindowAfterAction(MarkdownTable::Action action)
{
	return action == MarkdownTable::Action::WrapLongCells || shouldApplyAutoWrapAfterAction(action);
}

bool fitTableToWindowCommandEnabled()
{
	return !g_fitToWindowOnResize;
}

bool shouldRunFitToWindowAfterResize(bool enabled, bool inProgress, bool activeEditor, std::size_t previousColumns, std::size_t currentColumns)
{
	return enabled && !inProgress && activeEditor && previousColumns != 0 && previousColumns != currentColumns;
}

int availableTextPixelWidth(HWND scintilla)
{
	if (!scintilla)
		return 0;

	RECT client;
	if (!::GetClientRect(scintilla, &client))
		return 0;

	int textLeft = 0;
	const LRESULT currentPos = ::SendMessage(scintilla, SCI_GETCURRENTPOS, 0, 0);
	const LRESULT currentLine = ::SendMessage(scintilla, SCI_LINEFROMPOSITION, static_cast<WPARAM>(currentPos), 0);
	if (currentLine >= 0)
	{
		const LRESULT lineStart = ::SendMessage(scintilla, SCI_POSITIONFROMLINE, static_cast<WPARAM>(currentLine), 0);
		const LRESULT lineStartX = ::SendMessage(scintilla, SCI_POINTXFROMPOSITION, 0, static_cast<LPARAM>(lineStart));
		if (lineStartX > 0 && lineStartX < (client.right - client.left))
			textLeft = static_cast<int>(lineStartX);
	}

	const int safeMargin = 10;
	const int availableWidth = (client.right - client.left) - textLeft - safeMargin;
	return availableWidth > 0 ? availableWidth : 0;
}

std::size_t availableDisplayColumns(HWND scintilla)
{
	const int availableWidth = availableTextPixelWidth(scintilla);
	if (availableWidth <= 0)
		return 120;

	const std::string sample(120, '0');
	const LRESULT sampleWidth = ::SendMessage(scintilla, SCI_TEXTWIDTH, 0, reinterpret_cast<LPARAM>(sample.c_str()));
	if (sampleWidth <= 0)
		return 120;

	const std::size_t columns = static_cast<std::size_t>((static_cast<long long>(availableWidth) * static_cast<long long>(sample.size())) / sampleWidth);
	return (std::max)(columns, static_cast<std::size_t>(1));
}

bool tableFitsAvailableWidth(HWND scintilla, const std::vector<std::string> &lines)
{
	const int availableWidth = availableTextPixelWidth(scintilla);
	if (availableWidth <= 0)
		return true;

	for (std::size_t i = 0; i < lines.size(); ++i)
	{
		const LRESULT width = ::SendMessage(scintilla, SCI_TEXTWIDTH, 0, reinterpret_cast<LPARAM>(lines[i].c_str()));
		if (width > availableWidth)
			return false;
	}
	return true;
}

int coreIndex(std::size_t value);
bool runTableAction(MarkdownTable::Action action, bool quiet);

MarkdownTable::EditResult applyWrappedToVisibleWidth(HWND scintilla, const MarkdownTable::EditResult &edit)
{
	std::size_t columns = availableDisplayColumns(scintilla);
	MarkdownTable::EditResult best;
	for (std::size_t attempt = 0; attempt < 32; ++attempt)
	{
		MarkdownTable::EditResult wrapped = MarkdownTable::applyWrappedToWidth(
			edit.lines,
			coreIndex(edit.targetRow),
			coreIndex(edit.targetColumn),
			columns);
		if (!wrapped.ok)
			break;

		best = wrapped;
		if (tableFitsAvailableWidth(scintilla, best.lines) || columns <= 1)
			break;

		const std::size_t step = (std::max)(static_cast<std::size_t>(1), columns / 8);
		columns = columns > step ? columns - step : 1;
	}
	return best.ok ? best : edit;
}

int coreIndex(std::size_t value)
{
	const std::size_t maxInt = static_cast<std::size_t>((std::numeric_limits<int>::max)());
	return value > maxInt ? (std::numeric_limits<int>::max)() : static_cast<int>(value);
}

void rememberCurrentFitToWindowWidth()
{
	HWND scintilla = currentScintilla();
	g_lastFitToWindowColumns = scintilla ? availableDisplayColumns(scintilla) : 0;
}

bool fitCurrentTableToWindow(bool quiet)
{
	if (g_fitToWindowInProgress)
		return false;

	g_fitToWindowInProgress = true;
	const bool changed = runTableAction(MarkdownTable::Action::WrapLongCells, quiet);
	g_fitToWindowInProgress = false;
	return changed;
}

void fitToWindowAfterResize(HWND resizedScintilla)
{
	if (!g_fitToWindowOnResize || g_fitToWindowInProgress)
		return;

	HWND scintilla = currentScintilla();
	if (!scintilla || scintilla != resizedScintilla)
		return;

	const std::size_t columns = availableDisplayColumns(scintilla);
	if (g_lastFitToWindowColumns == 0)
	{
		g_lastFitToWindowColumns = columns;
		return;
	}
	if (!shouldRunFitToWindowAfterResize(g_fitToWindowOnResize, g_fitToWindowInProgress, true, g_lastFitToWindowColumns, columns))
		return;

	g_lastFitToWindowColumns = columns;
	fitCurrentTableToWindow(true);
}

void scheduleFitToWindowAfterResize(HWND resizedScintilla)
{
	if (!g_fitToWindowOnResize || !resizedScintilla)
		return;

	g_pendingFitToWindowScintilla = resizedScintilla;
	::SetTimer(resizedScintilla, fitToWindowResizeTimerId, fitToWindowResizeDelayMs, NULL);
}

void cancelFitToWindowResizeTimer(HWND scintilla)
{
	if (scintilla)
		::KillTimer(scintilla, fitToWindowResizeTimerId);
	if (g_pendingFitToWindowScintilla == scintilla)
		g_pendingFitToWindowScintilla = NULL;
}

void cancelFitToWindowResizeTimers()
{
	cancelFitToWindowResizeTimer(nppData._scintillaMainHandle);
	cancelFitToWindowResizeTimer(nppData._scintillaSecondHandle);
}

WNDPROC originalScintillaProc(HWND hwnd)
{
	if (hwnd == nppData._scintillaMainHandle)
		return g_originalMainScintillaProc;
	if (hwnd == nppData._scintillaSecondHandle)
		return g_originalSecondScintillaProc;
	return NULL;
}

LRESULT CALLBACK fitToWindowScintillaProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	WNDPROC original = originalScintillaProc(hwnd);
	if (!original)
		return ::DefWindowProc(hwnd, message, wParam, lParam);

	if (message == WM_TIMER && wParam == fitToWindowResizeTimerId)
	{
		cancelFitToWindowResizeTimer(hwnd);
		fitToWindowAfterResize(hwnd);
		return 0;
	}

	const LRESULT result = ::CallWindowProc(original, hwnd, message, wParam, lParam);
	if (message == WM_SIZE)
		scheduleFitToWindowAfterResize(hwnd);
	return result;
}

void subclassScintillaWindow(HWND hwnd, WNDPROC &original)
{
	if (!hwnd || original)
		return;

	original = reinterpret_cast<WNDPROC>(::SetWindowLongPtr(
		hwnd,
		GWLP_WNDPROC,
		reinterpret_cast<LONG_PTR>(fitToWindowScintillaProc)));
}

void removeScintillaSubclass(HWND hwnd, WNDPROC &original)
{
	if (!hwnd || !original)
	{
		original = NULL;
		return;
	}

	if (reinterpret_cast<WNDPROC>(::GetWindowLongPtr(hwnd, GWLP_WNDPROC)) == fitToWindowScintillaProc)
	{
		::SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original));
	}
	original = NULL;
}

std::string getRangeText(HWND scintilla, Sci_Position start, Sci_Position end)
{
	if (end <= start)
		return std::string();

	const std::size_t length = static_cast<std::size_t>(end - start);
	std::vector<char> buffer(length + 1, '\0');
	Sci_TextRangeFull range;
	range.chrg.cpMin = start;
	range.chrg.cpMax = end;
	range.lpstrText = &buffer[0];
	LRESULT copied = ::SendMessage(scintilla, SCI_GETTEXTRANGEFULL, 0, reinterpret_cast<LPARAM>(&range));
	if (copied <= 0)
	{
		LegacySciTextRange legacyRange;
		legacyRange.chrg.cpMin = static_cast<Sci_PositionCR>(start);
		legacyRange.chrg.cpMax = static_cast<Sci_PositionCR>(end);
		legacyRange.lpstrText = &buffer[0];
		copied = ::SendMessage(scintilla, sciGetTextRangeLegacy, 0, reinterpret_cast<LPARAM>(&legacyRange));
		if (copied <= 0)
			return std::string();
	}
	return std::string(&buffer[0], static_cast<std::size_t>(copied));
}

Sci_Position lineStartPosition(HWND scintilla, std::size_t line)
{
	const LRESULT position = ::SendMessage(scintilla, SCI_POSITIONFROMLINE, static_cast<WPARAM>(line), 0);
	return position < 0 ? 0 : static_cast<Sci_Position>(position);
}

Sci_Position lineEndPosition(HWND scintilla, std::size_t line)
{
	const LRESULT position = ::SendMessage(scintilla, SCI_GETLINEENDPOSITION, static_cast<WPARAM>(line), 0);
	return position < 0 ? lineStartPosition(scintilla, line) : static_cast<Sci_Position>(position);
}

std::string getLineText(HWND scintilla, std::size_t line)
{
	return getRangeText(scintilla, lineStartPosition(scintilla, line), lineEndPosition(scintilla, line));
}

std::string getSelectedText(HWND scintilla)
{
	const LRESULT lengthResult = ::SendMessage(scintilla, SCI_GETSELTEXT, 0, 0);
	if (lengthResult <= 1)
		return std::string();

	const std::size_t length = static_cast<std::size_t>(lengthResult);
	std::vector<char> buffer(length, '\0');
	::SendMessage(scintilla, SCI_GETSELTEXT, 0, reinterpret_cast<LPARAM>(&buffer[0]));
	return std::string(&buffer[0]);
}

std::string currentEol(HWND scintilla)
{
	const LRESULT mode = ::SendMessage(scintilla, SCI_GETEOLMODE, 0, 0);
	if (mode == SC_EOL_LF)
		return "\n";
	if (mode == SC_EOL_CR)
		return "\r";
	return "\r\n";
}

std::string chooseEol(const std::string &text, const std::string &fallback)
{
	if (text.find("\r\n") != std::string::npos)
		return "\r\n";
	if (text.find('\r') != std::string::npos)
		return "\r";
	if (text.find('\n') != std::string::npos)
		return "\n";
	return fallback;
}

class ScintillaUndoAction
{
public:
	explicit ScintillaUndoAction(HWND scintilla) : scintilla_(scintilla)
	{
		if (scintilla_)
			::SendMessage(scintilla_, SCI_BEGINUNDOACTION, 0, 0);
	}

	~ScintillaUndoAction()
	{
		if (scintilla_)
			::SendMessage(scintilla_, SCI_ENDUNDOACTION, 0, 0);
	}

	ScintillaUndoAction(const ScintillaUndoAction &) = delete;
	ScintillaUndoAction &operator=(const ScintillaUndoAction &) = delete;

private:
	HWND scintilla_;
};

std::string separatorAfterLine(HWND scintilla, std::size_t line, std::size_t lineCount)
{
	if (line + 1 >= lineCount)
		return std::string();

	const Sci_Position end = lineEndPosition(scintilla, line);
	const Sci_Position nextStart = lineStartPosition(scintilla, line + 1);
	return getRangeText(scintilla, end, nextStart);
}

std::string chooseEol(HWND scintilla, std::size_t first, std::size_t last, std::size_t lineCount)
{
	for (std::size_t i = first; i <= last && i < lineCount; ++i)
	{
		const std::string separator = separatorAfterLine(scintilla, i, lineCount);
		if (!separator.empty())
			return separator;
	}
	return currentEol(scintilla);
}

std::string joinLines(const std::vector<std::string> &lines, const std::string &eol)
{
	std::string result;
	for (std::size_t i = 0; i < lines.size(); ++i)
	{
		if (i != 0)
			result += eol;
		result += lines[i];
	}
	return result;
}

std::size_t offsetForLineColumn(const std::vector<std::string> &lines, const std::string &eol, std::size_t row, std::size_t columnOffset)
{
	std::size_t offset = 0;
	for (std::size_t i = 0; i < row && i < lines.size(); ++i)
		offset += lines[i].size() + eol.size();
	return offset + columnOffset;
}

std::size_t positionForLineColumn(HWND scintilla, std::size_t firstLine, const std::vector<std::string> &replacementLines, const std::string &eol, std::size_t row, std::size_t columnOffset)
{
	return static_cast<std::size_t>(lineStartPosition(scintilla, firstLine)) + offsetForLineColumn(replacementLines, eol, row, columnOffset);
}

Sci_Position documentLength(HWND scintilla)
{
	const LRESULT length = ::SendMessage(scintilla, SCI_GETLENGTH, 0, 0);
	return length < 0 ? 0 : static_cast<Sci_Position>(length);
}

Sci_Position safeLinePosition(HWND scintilla, Sci_Position position)
{
	const Sci_Position length = documentLength(scintilla);
	if (length <= 0 || position < 0)
		return 0;
	if (position > length)
		return length;
	return position;
}

bool isLineStart(HWND scintilla, Sci_Position position)
{
	if (position <= 0)
		return true;

	const LRESULT line = ::SendMessage(scintilla, SCI_LINEFROMPOSITION, static_cast<WPARAM>(safeLinePosition(scintilla, position)), 0);
	if (line < 0)
		return true;
	return lineStartPosition(scintilla, static_cast<std::size_t>(line)) == position;
}

bool isLineEnd(HWND scintilla, Sci_Position position)
{
	const Sci_Position length = documentLength(scintilla);
	if (position >= length)
		return true;

	const LRESULT line = ::SendMessage(scintilla, SCI_LINEFROMPOSITION, static_cast<WPARAM>(safeLinePosition(scintilla, position)), 0);
	if (line < 0)
		return true;
	return lineEndPosition(scintilla, static_cast<std::size_t>(line)) == position;
}

std::string chooseEolForInsertion(HWND scintilla, Sci_Position start, Sci_Position end)
{
	const LRESULT lineCountResult = ::SendMessage(scintilla, SCI_GETLINECOUNT, 0, 0);
	if (lineCountResult <= 0)
		return currentEol(scintilla);

	const std::size_t lineCount = static_cast<std::size_t>(lineCountResult);
	const LRESULT startLineResult = ::SendMessage(scintilla, SCI_LINEFROMPOSITION, static_cast<WPARAM>(safeLinePosition(scintilla, start)), 0);
	const LRESULT endLineResult = ::SendMessage(scintilla, SCI_LINEFROMPOSITION, static_cast<WPARAM>(safeLinePosition(scintilla, end)), 0);
	if (startLineResult < 0 || endLineResult < 0)
		return currentEol(scintilla);

	std::size_t firstLine = static_cast<std::size_t>((std::min)(startLineResult, endLineResult));
	std::size_t lastLine = static_cast<std::size_t>((std::max)(startLineResult, endLineResult));
	if (firstLine >= lineCount)
		firstLine = lineCount - 1;
	if (lastLine >= lineCount)
		lastLine = lineCount - 1;
	if (start != end && lastLine > firstLine && end == lineStartPosition(scintilla, lastLine))
		--lastLine;
	return chooseEol(scintilla, firstLine, lastLine, lineCount);
}

InsertText tableInsertText(HWND scintilla, Sci_Position start, Sci_Position end, const std::string &table, const std::string &eol)
{
	InsertText insertText;
	if (start > 0 && !isLineStart(scintilla, start))
	{
		insertText.text += eol;
		insertText.caretDelta += eol.size();
	}
	insertText.text += table;
	if (end < documentLength(scintilla) && !isLineEnd(scintilla, end))
		insertText.text += eol;
	return insertText;
}

void replaceRangeWithEdit(HWND scintilla, Sci_Position start, Sci_Position end, const std::string &source, const MarkdownTable::EditResult &edit)
{
	const std::string eol = chooseEol(source, currentEol(scintilla));
	const std::string replacement = joinLines(edit.lines, eol);
	const std::size_t targetOffset = offsetForLineColumn(edit.lines, eol, edit.targetRow, edit.targetColumnOffset);

	ScintillaUndoAction undo(scintilla);
	::SendMessage(scintilla, SCI_SETTARGETRANGE, static_cast<WPARAM>(start), static_cast<LPARAM>(end));
	::SendMessage(scintilla, SCI_REPLACETARGET, static_cast<WPARAM>(replacement.size()), reinterpret_cast<LPARAM>(replacement.c_str()));
	::SendMessage(scintilla, SCI_GOTOPOS, static_cast<WPARAM>(start + static_cast<Sci_Position>(targetOffset)), 0);
}

void replaceSelectionWithInsertedTable(HWND scintilla, const MarkdownTable::EditResult &edit)
{
	const LRESULT selectionStartResult = ::SendMessage(scintilla, SCI_GETSELECTIONSTART, 0, 0);
	const LRESULT selectionEndResult = ::SendMessage(scintilla, SCI_GETSELECTIONEND, 0, 0);
	if (selectionStartResult < 0 || selectionEndResult < 0)
		return;

	const Sci_Position selectionStart = static_cast<Sci_Position>((std::min)(selectionStartResult, selectionEndResult));
	const Sci_Position selectionEnd = static_cast<Sci_Position>((std::max)(selectionStartResult, selectionEndResult));
	const std::string eol = chooseEolForInsertion(scintilla, selectionStart, selectionEnd);
	const std::string table = joinLines(edit.lines, eol);
	const InsertText insertText = tableInsertText(scintilla, selectionStart, selectionEnd, table, eol);
	const std::size_t targetOffset = insertText.caretDelta + offsetForLineColumn(edit.lines, eol, edit.targetRow, edit.targetColumnOffset);

	ScintillaUndoAction undo(scintilla);
	::SendMessage(scintilla, SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(insertText.text.c_str()));
	::SendMessage(scintilla, SCI_GOTOPOS, static_cast<WPARAM>(selectionStart + static_cast<LRESULT>(targetOffset)), 0);
}

LRESULT CALLBACK tableSizeDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	TableSizeDialogState *state = reinterpret_cast<TableSizeDialogState *>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));

	switch (message)
	{
		case WM_CREATE:
		{
			const UiText &text = uiText();
			CREATESTRUCT *create = reinterpret_cast<CREATESTRUCT *>(lParam);
			state = reinterpret_cast<TableSizeDialogState *>(create->lpCreateParams);
			::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

			HFONT font = reinterpret_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
			HWND labelColumns = ::CreateWindowEx(0, TEXT("STATIC"), text.tableSizeColumnsLabel, WS_CHILD | WS_VISIBLE, 18, 20, 110, 22, hwnd, NULL, g_moduleHandle, NULL);
			HWND editColumns = ::CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, 140, 18, 120, 24, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(tableSizeColumnsId)), g_moduleHandle, NULL);
			HWND labelRows = ::CreateWindowEx(0, TEXT("STATIC"), text.tableSizeRowsLabel, WS_CHILD | WS_VISIBLE, 18, 54, 110, 22, hwnd, NULL, g_moduleHandle, NULL);
			HWND editRows = ::CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, 140, 52, 120, 24, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(tableSizeRowsId)), g_moduleHandle, NULL);
			HWND ok = ::CreateWindowEx(0, TEXT("BUTTON"), text.okButton, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 86, 96, 78, 26, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)), g_moduleHandle, NULL);
			HWND cancel = ::CreateWindowEx(0, TEXT("BUTTON"), text.cancelButton, WS_CHILD | WS_VISIBLE | WS_TABSTOP, 176, 96, 78, 26, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)), g_moduleHandle, NULL);

			HWND controls[] = { labelColumns, editColumns, labelRows, editRows, ok, cancel };
			for (std::size_t i = 0; i < sizeof(controls) / sizeof(controls[0]); ++i)
				::SendMessage(controls[i], WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

			::SetDlgItemInt(hwnd, tableSizeColumnsId, static_cast<UINT>(state->columns), FALSE);
			::SetDlgItemInt(hwnd, tableSizeRowsId, static_cast<UINT>(state->dataRows), FALSE);
			::SetFocus(editColumns);
			return 0;
		}

		case WM_COMMAND:
		{
			const int command = LOWORD(wParam);
			if (command == IDOK)
			{
				BOOL columnsOk = FALSE;
				BOOL rowsOk = FALSE;
				const UINT columns = ::GetDlgItemInt(hwnd, tableSizeColumnsId, &columnsOk, FALSE);
				const UINT dataRows = ::GetDlgItemInt(hwnd, tableSizeRowsId, &rowsOk, FALSE);
				if (!columnsOk || !rowsOk || columns < 1 || columns > tableSizeMaxColumns || dataRows > tableSizeMaxRows)
				{
					::MessageBox(hwnd, uiText().tableSizeRangeError, uiText().pluginMenuName, MB_OK | MB_ICONWARNING);
					return 0;
				}

				state->columns = columns;
				state->dataRows = dataRows;
				state->accepted = true;
				::DestroyWindow(hwnd);
				return 0;
			}
			if (command == IDCANCEL)
			{
				::DestroyWindow(hwnd);
				return 0;
			}
			break;
		}

		case WM_CLOSE:
			::DestroyWindow(hwnd);
			return 0;
	}

	return ::DefWindowProc(hwnd, message, wParam, lParam);
}

bool promptTableSize(TableSizeDialogState &state)
{
	const TCHAR className[] = TEXT("MarkdownTableEditorTableSizeDialog");
	static bool registered = false;
	if (!registered)
	{
		WNDCLASS wc;
		ZeroMemory(&wc, sizeof(wc));
		wc.lpfnWndProc = tableSizeDialogProc;
		wc.hInstance = g_moduleHandle;
		wc.hCursor = ::LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
		wc.lpszClassName = className;
		if (!::RegisterClass(&wc))
			return false;
		registered = true;
	}

	RECT ownerRect;
	::GetWindowRect(nppData._nppHandle, &ownerRect);
	const int width = 300;
	const int height = 170;
	const int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - width) / 2;
	const int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - height) / 2;
	HWND dialog = ::CreateWindowEx(WS_EX_DLGMODALFRAME, className, uiText().tableSizeTitle, WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, width, height, nppData._nppHandle, NULL, g_moduleHandle, &state);
	if (!dialog)
		return false;

	::EnableWindow(nppData._nppHandle, FALSE);
	::ShowWindow(dialog, SW_SHOW);
	::SetForegroundWindow(dialog);

	MSG msg;
	while (::IsWindow(dialog))
	{
		const BOOL messageResult = ::GetMessage(&msg, NULL, 0, 0);
		if (messageResult <= 0)
		{
			if (messageResult == 0)
				::PostQuitMessage(static_cast<int>(msg.wParam));
			break;
		}

		if (!::IsDialogMessage(dialog, &msg))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
	}

	::EnableWindow(nppData._nppHandle, TRUE);
	::SetForegroundWindow(nppData._nppHandle);
	return state.accepted;
}

bool runTableAction(MarkdownTable::Action action, bool quiet)
{
	HWND scintilla = currentScintilla();
	if (!scintilla)
		return false;

	const LRESULT lineCountResult = ::SendMessage(scintilla, SCI_GETLINECOUNT, 0, 0);
	if (lineCountResult <= 0)
		return false;
	const std::size_t lineCount = static_cast<std::size_t>(lineCountResult);

	const LRESULT currentPosResult = ::SendMessage(scintilla, SCI_GETCURRENTPOS, 0, 0);
	const LRESULT currentLineResult = ::SendMessage(scintilla, SCI_LINEFROMPOSITION, static_cast<WPARAM>(currentPosResult), 0);
	if (currentLineResult < 0)
		return false;

	std::size_t currentLine = static_cast<std::size_t>(currentLineResult);
	if (currentLine >= lineCount)
		currentLine = lineCount - 1;

	const std::string currentLineText = getLineText(scintilla, currentLine);
	if (!MarkdownTable::isPotentialTableLine(currentLineText))
	{
		if (!quiet)
			showMessage(uiText().putCaretInsideTable);
		return false;
	}

	std::size_t potentialFirstLine = currentLine;
	while (potentialFirstLine > 0 && MarkdownTable::isPotentialTableLine(getLineText(scintilla, potentialFirstLine - 1)))
		--potentialFirstLine;

	std::size_t potentialLastLine = currentLine;
	while (potentialLastLine + 1 < lineCount && MarkdownTable::isPotentialTableLine(getLineText(scintilla, potentialLastLine + 1)))
		++potentialLastLine;

	std::vector<std::string> candidateLines;
	for (std::size_t i = potentialFirstLine; i <= potentialLastLine; ++i)
		candidateLines.push_back(getLineText(scintilla, i));

	const MarkdownTable::TableRange tableRange = MarkdownTable::findTableRange(candidateLines, coreIndex(currentLine - potentialFirstLine));
	if (!tableRange.found)
	{
		if (!quiet)
			showMessage(uiText().couldNotEditTable);
		return false;
	}

	const std::size_t firstLine = potentialFirstLine + tableRange.firstRow;
	const std::size_t lastLine = potentialFirstLine + tableRange.lastRow;
	std::vector<std::string> tableLines;
	for (std::size_t i = tableRange.firstRow; i <= tableRange.lastRow; ++i)
		tableLines.push_back(candidateLines[i]);

	const std::size_t row = currentLine - firstLine;
	const Sci_Position currentLineStart = lineStartPosition(scintilla, currentLine);
	const std::size_t byteColumn = static_cast<std::size_t>((std::max)(static_cast<LRESULT>(0), currentPosResult - static_cast<LRESULT>(currentLineStart)));
	const std::size_t column = MarkdownTable::columnFromCursor(currentLineText, byteColumn);
	const MarkdownTable::Action coreAction = coreActionForPluginAction(action);
	MarkdownTable::EditResult edit = MarkdownTable::apply(tableLines, coreIndex(row), coreIndex(column), coreAction);
	if (!edit.ok)
	{
		if (!quiet)
			showMessage(uiText().couldNotEditTable);
		return false;
	}
	if (shouldFitToWindowAfterAction(action))
	{
		edit = applyWrappedToVisibleWidth(scintilla, edit);
	}

	const std::string eol = chooseEol(scintilla, firstLine, lastLine, lineCount);
	const std::string replacement = joinLines(edit.lines, eol);
	const std::size_t targetPosition = positionForLineColumn(scintilla, firstLine, edit.lines, eol, edit.targetRow, edit.targetColumnOffset);

	ScintillaUndoAction undo(scintilla);
	::SendMessage(scintilla, SCI_SETTARGETRANGE, static_cast<WPARAM>(lineStartPosition(scintilla, firstLine)), static_cast<LPARAM>(lineEndPosition(scintilla, lastLine)));
	::SendMessage(scintilla, SCI_REPLACETARGET, static_cast<WPARAM>(replacement.size()), reinterpret_cast<LPARAM>(replacement.c_str()));
	::SendMessage(scintilla, SCI_GOTOPOS, static_cast<WPARAM>(targetPosition), 0);
	return true;
}

bool isDelimitedLine(const std::string &line)
{
	bool inQuotes = false;
	for (std::size_t i = 0; i < line.size(); ++i)
	{
		const char ch = line[i];
		if (inQuotes)
		{
			if (ch == '"' && i + 1 < line.size() && line[i + 1] == '"')
				++i;
			else if (ch == '"')
				inQuotes = false;
		}
		else if (ch == '"')
		{
			inQuotes = true;
		}
		else if (ch == ',' || ch == '\t')
		{
			return true;
		}
	}
	return false;
}

bool findCurrentDelimitedBlock(HWND scintilla, Sci_Position currentPos, Sci_Position &start, Sci_Position &end)
{
	const LRESULT lineCountResult = ::SendMessage(scintilla, SCI_GETLINECOUNT, 0, 0);
	if (lineCountResult <= 0)
		return false;

	const std::size_t lineCount = static_cast<std::size_t>(lineCountResult);
	LRESULT currentLineResult = ::SendMessage(scintilla, SCI_LINEFROMPOSITION, static_cast<WPARAM>(safeLinePosition(scintilla, currentPos)), 0);
	if (currentLineResult < 0)
		return false;

	std::size_t currentLine = static_cast<std::size_t>(currentLineResult);
	if (currentLine >= lineCount)
		currentLine = lineCount - 1;
	if (!isDelimitedLine(getLineText(scintilla, currentLine)))
		return false;

	std::size_t firstLine = currentLine;
	while (firstLine > 0 && isDelimitedLine(getLineText(scintilla, firstLine - 1)))
		--firstLine;

	std::size_t lastLine = currentLine;
	while (lastLine + 1 < lineCount && isDelimitedLine(getLineText(scintilla, lastLine + 1)))
		++lastLine;

	if (firstLine == lastLine)
		return false;

	start = lineStartPosition(scintilla, firstLine);
	end = lineEndPosition(scintilla, lastLine);
	return true;
}

bool runConvertDelimitedSelection()
{
	HWND scintilla = currentScintilla();
	if (!scintilla)
		return false;

	LRESULT selectionStartResult = ::SendMessage(scintilla, SCI_GETSELECTIONSTART, 0, 0);
	LRESULT selectionEndResult = ::SendMessage(scintilla, SCI_GETSELECTIONEND, 0, 0);
	if (selectionStartResult < 0 || selectionEndResult < 0)
		return false;

	Sci_Position rangeStart = static_cast<Sci_Position>((std::min)(selectionStartResult, selectionEndResult));
	Sci_Position rangeEnd = static_cast<Sci_Position>((std::max)(selectionStartResult, selectionEndResult));
	std::string source;
	if (rangeStart == rangeEnd)
	{
		const LRESULT currentPosResult = ::SendMessage(scintilla, SCI_GETCURRENTPOS, 0, 0);
		if (currentPosResult < 0 || !findCurrentDelimitedBlock(scintilla, static_cast<Sci_Position>(currentPosResult), rangeStart, rangeEnd))
		{
			showMessage(uiText().selectCsvTsv);
			return false;
		}
		source = getRangeText(scintilla, rangeStart, rangeEnd);
	}
	else
	{
		source = getSelectedText(scintilla);
	}

	MarkdownTable::EditResult edit = MarkdownTable::convertDelimitedToTable(source);
	if (!edit.ok)
	{
		showMessage(uiText().couldNotConvertCsvTsv);
		return false;
	}

	replaceRangeWithEdit(scintilla, rangeStart, rangeEnd, source, edit);
	return true;
}

bool runInsertTable()
{
	HWND scintilla = currentScintilla();
	if (!scintilla)
		return false;

	TableSizeDialogState state;
	if (!promptTableSize(state))
		return false;

	MarkdownTable::EditResult edit = MarkdownTable::createTable(coreIndex(state.columns), coreIndex(state.dataRows));
	if (!edit.ok)
		return false;

	replaceSelectionWithInsertedTable(scintilla, edit);
	return true;
}

}

#ifdef MARKDOWN_TABLE_PLUGIN_TESTING
namespace MarkdownTablePluginTesting
{
std::string chooseEolFromTextForTests(const std::string &text, const std::string &fallback)
{
	return chooseEol(text, fallback);
}

ReplacementPreview replacementPreviewForTests(const MarkdownTable::EditResult &edit, const std::string &eol)
{
	ReplacementPreview preview;
	preview.text = joinLines(edit.lines, eol);
	preview.caretOffset = offsetForLineColumn(edit.lines, eol, edit.targetRow, edit.targetColumnOffset);
	return preview;
}

ReplacementPreview delimitedReplacementPreviewForTests(const std::string &source, const std::string &fallback, const MarkdownTable::EditResult &edit)
{
	return replacementPreviewForTests(edit, chooseEol(source, fallback));
}

void applyNativeLangFileNameForTests(const std::string &nativeLangFileName)
{
	setUiLanguage(languageFromNativeLangFileName(nativeLangFileName));
}

const wchar_t *pluginMenuNameForTests()
{
	return uiText().pluginMenuName;
}

bool autoWrapLongCellsEnabledForTests()
{
	return g_autoWrapLongCells;
}

void setAutoWrapLongCellsEnabledForTests(bool enabled)
{
	g_autoWrapLongCells = enabled;
}

MarkdownTable::Action coreActionForPluginActionForTests(MarkdownTable::Action action)
{
	return coreActionForPluginAction(action);
}

bool shouldApplyAutoWrapAfterActionForTests(MarkdownTable::Action action)
{
	return shouldApplyAutoWrapAfterAction(action);
}

bool shouldFitToWindowAfterActionForTests(MarkdownTable::Action action)
{
	return shouldFitToWindowAfterAction(action);
}

bool fitToWindowOnResizeEnabledForTests()
{
	return g_fitToWindowOnResize;
}

void setFitToWindowOnResizeEnabledForTests(bool enabled)
{
	g_fitToWindowOnResize = enabled;
	if (!enabled)
		g_lastFitToWindowColumns = 0;
}

bool fitTableToWindowCommandEnabledForTests()
{
	return fitTableToWindowCommandEnabled();
}

bool shouldRunFitToWindowAfterResizeForTests(bool enabled, bool inProgress, bool activeEditor, std::size_t previousColumns, std::size_t currentColumns)
{
	return shouldRunFitToWindowAfterResize(enabled, inProgress, activeEditor, previousColumns, currentColumns);
}

UINT fitToWindowResizeDelayMsForTests()
{
	return fitToWindowResizeDelayMs;
}

bool ensureTabToolbarIconsForTests()
{
	return ensureTabToolbarIconHandles();
}

void destroyTabToolbarIconsForTests()
{
	destroyTabToolbarIconHandles();
}

bool ensureWrapLongCellsToolbarIconsForTests()
{
	return ensureWrapLongCellsToolbarIconHandles();
}

void destroyWrapLongCellsToolbarIconsForTests()
{
	destroyWrapLongCellsToolbarIconHandles();
}

bool ensureNotepadWordWrapToolbarIconsForTests()
{
	return ensureNotepadWordWrapToolbarIconHandles();
}

void destroyNotepadWordWrapToolbarIconsForTests()
{
	destroyNotepadWordWrapToolbarIconHandles();
}

bool ensureAutoWrapToolbarIconsForTests()
{
	return ensureAutoWrapToolbarIconHandles();
}

void destroyAutoWrapToolbarIconsForTests()
{
	destroyAutoWrapToolbarIconHandles();
}

bool ensureFitToWindowOnResizeToolbarIconsForTests()
{
	return ensureFitToWindowOnResizeToolbarIconHandles();
}

void destroyFitToWindowOnResizeToolbarIconsForTests()
{
	destroyFitToWindowOnResizeToolbarIconHandles();
}
}
#endif

//
// Initialize your plugin data here
// It will be called while plugin loading   
void pluginInit(HANDLE hModule)
{
	g_moduleHandle = reinterpret_cast<HINSTANCE>(hModule);
}

//
// Here you can do the clean up, save the parameters (if any) for the next session
//
void pluginCleanUp()
{
	removeFitToWindowResizeHooks();
	destroyTabToolbarIconHandles();
	destroyWrapLongCellsToolbarIconHandles();
	destroyNotepadWordWrapToolbarIconHandles();
	destroyAutoWrapToolbarIconHandles();
	destroyFitToWindowOnResizeToolbarIconHandles();
}

//
// Initialization of your plugin commands
// You should fill your plugins commands here
void commandMenuInit()
{

    //--------------------------------------------//
    //-- STEP 3. CUSTOMIZE YOUR PLUGIN COMMANDS --//
    //--------------------------------------------//
    // with function :
    // setCommand(int index,                      // zero based number to indicate the order of command
    //            TCHAR *commandName,             // the command name that you want to see in plugin menu
    //            PFUNCPLUGINCMD functionPointer, // the symbol of function (function pointer) associated with this command. The body should be defined below. See Step 4.
    //            ShortcutKey *shortcut,          // optional. Define a shortcut to trigger this command
    //            bool check0nInit                // optional. Make this menu item be checked visually
    //            );
	refreshUiLanguageState();
	setCommand(0, commandText(0), alignTable, &g_alignShortcut, false);
	setCommand(1, commandText(1), nextCell, &g_nextCellShortcut, false);
	setCommand(2, commandText(2), previousCell, &g_previousCellShortcut, false);
	setCommand(3, commandText(3), insertRowBelow, &g_insertRowShortcut, false);
	setCommand(4, commandText(4), deleteRow, &g_deleteRowShortcut, false);
	setCommand(5, commandText(5), insertColumnRight, &g_insertColumnShortcut, false);
	setCommand(6, commandText(6), deleteColumn, &g_deleteColumnShortcut, false);
	setCommand(7, commandText(7), moveRowUp, &g_moveRowUpShortcut, false);
	setCommand(8, commandText(8), moveRowDown, &g_moveRowDownShortcut, false);
	setCommand(9, commandText(9), moveColumnLeft, &g_moveColumnLeftShortcut, false);
	setCommand(10, commandText(10), moveColumnRight, &g_moveColumnRightShortcut, false);
	setCommand(11, commandText(11), sortRowsAscending, &g_sortRowsAscendingShortcut, false);
	setCommand(12, commandText(12), sortRowsDescending, &g_sortRowsDescendingShortcut, false);
	setCommand(13, commandText(13), convertCsvTsvSelectionToTable, &g_convertCsvTsvShortcut, false);
	setCommand(14, commandText(14), insertTable, &g_insertTableShortcut, false);
	setCommand(tabCommandIndex, commandText(tabCommandIndex), tabOrIndent, &g_tabShortcut, false);
	setCommand(wrapLongCellsCommandIndex, commandText(wrapLongCellsCommandIndex), wrapLongCells, &g_wrapLongCellsShortcut, false);
	setCommand(notepadWordWrapCommandIndex, commandText(notepadWordWrapCommandIndex), toggleNotepadWordWrap, NULL, notepadWordWrapEnabled());
	setCommand(autoWrapLongCellsCommandIndex, commandText(autoWrapLongCellsCommandIndex), toggleAutoWrapLongCells, NULL, g_autoWrapLongCells);
	setCommand(fitToWindowOnResizeCommandIndex, commandText(fitToWindowOnResizeCommandIndex), toggleFitToWindowOnResize, NULL, g_fitToWindowOnResize);
}

void refreshUiLanguageFromNotepad()
{
	refreshUiLanguageState();
	updateNotepadPluginMenu();
	refreshNotepadWordWrapUi();
	refreshAutoWrapLongCellsUi();
	refreshFitToWindowOnResizeUi();
}

void refreshNotepadWordWrapUi()
{
	if (!nppData._nppHandle || funcItem[notepadWordWrapCommandIndex]._cmdID == 0)
		return;

	const bool enabled = notepadWordWrapEnabled();
	::SendMessage(
		nppData._nppHandle,
		NPPM_SETMENUITEMCHECK,
		static_cast<WPARAM>(funcItem[notepadWordWrapCommandIndex]._cmdID),
		static_cast<LPARAM>(enabled ? TRUE : FALSE));
	setNotepadWordWrapToolbarCheckState();
}

void refreshAutoWrapLongCellsUi()
{
	if (!nppData._nppHandle || funcItem[autoWrapLongCellsCommandIndex]._cmdID == 0)
		return;

	::SendMessage(
		nppData._nppHandle,
		NPPM_SETMENUITEMCHECK,
		static_cast<WPARAM>(funcItem[autoWrapLongCellsCommandIndex]._cmdID),
		static_cast<LPARAM>(g_autoWrapLongCells ? TRUE : FALSE));
	setAutoWrapToolbarCheckState();
}

void refreshFitToWindowOnResizeUi()
{
	if (!nppData._nppHandle || funcItem[fitToWindowOnResizeCommandIndex]._cmdID == 0)
		return;

	::SendMessage(
		nppData._nppHandle,
		NPPM_SETMENUITEMCHECK,
		static_cast<WPARAM>(funcItem[fitToWindowOnResizeCommandIndex]._cmdID),
		static_cast<LPARAM>(g_fitToWindowOnResize ? TRUE : FALSE));
	setFitToWindowOnResizeToolbarCheckState();
	setCommandEnabledState(wrapLongCellsCommandIndex, fitTableToWindowCommandEnabled());
}

void registerToolbarIcon(std::size_t commandIndex, HBITMAP bitmap, HICON icon, HICON darkModeIcon)
{
	if (!nppData._nppHandle || funcItem[commandIndex]._cmdID == 0)
		return;

	toolbarIconsWithDarkMode icons;
	ZeroMemory(&icons, sizeof(icons));
	icons.hToolbarBmp = bitmap;
	icons.hToolbarIcon = icon;
	icons.hToolbarIconDarkMode = darkModeIcon;
	const LRESULT darkModeResult = ::SendMessage(
		nppData._nppHandle,
		NPPM_ADDTOOLBARICON_FORDARKMODE,
		static_cast<WPARAM>(funcItem[commandIndex]._cmdID),
		reinterpret_cast<LPARAM>(&icons));
	if (!darkModeResult)
	{
		toolbarIcons legacyIcons;
		ZeroMemory(&legacyIcons, sizeof(legacyIcons));
		legacyIcons.hToolbarBmp = bitmap;
		legacyIcons.hToolbarIcon = icon;
		::SendMessage(
			nppData._nppHandle,
			NPPM_ADDTOOLBARICON_DEPRECATED,
			static_cast<WPARAM>(funcItem[commandIndex]._cmdID),
			reinterpret_cast<LPARAM>(&legacyIcons));
	}
}

void registerToolbarIcons()
{
	if (!nppData._nppHandle)
		return;

	refreshUiLanguageState();
	if (funcItem[tabCommandIndex]._cmdID != 0 && ensureTabToolbarIconHandles())
		registerToolbarIcon(tabCommandIndex, g_tabToolbarBmp, g_tabToolbarIcon, g_tabToolbarIconDarkMode);

	if (funcItem[wrapLongCellsCommandIndex]._cmdID != 0 && ensureWrapLongCellsToolbarIconHandles())
		registerToolbarIcon(wrapLongCellsCommandIndex, g_wrapLongCellsToolbarBmp, g_wrapLongCellsToolbarIcon, g_wrapLongCellsToolbarIconDarkMode);

	if (funcItem[notepadWordWrapCommandIndex]._cmdID != 0 && ensureNotepadWordWrapToolbarIconHandles())
		registerToolbarIcon(notepadWordWrapCommandIndex, g_notepadWordWrapToolbarBmp, g_notepadWordWrapToolbarIcon, g_notepadWordWrapToolbarIconDarkMode);

	if (funcItem[autoWrapLongCellsCommandIndex]._cmdID != 0 && ensureAutoWrapToolbarIconHandles())
		registerToolbarIcon(autoWrapLongCellsCommandIndex, g_autoWrapToolbarBmp, g_autoWrapToolbarIcon, g_autoWrapToolbarIconDarkMode);

	if (funcItem[fitToWindowOnResizeCommandIndex]._cmdID != 0 && ensureFitToWindowOnResizeToolbarIconHandles())
		registerToolbarIcon(
			fitToWindowOnResizeCommandIndex,
			g_fitToWindowOnResizeToolbarBmp,
			g_fitToWindowOnResizeToolbarIcon,
			g_fitToWindowOnResizeToolbarIconDarkMode);

	refreshNotepadWordWrapUi();
	refreshAutoWrapLongCellsUi();
	refreshFitToWindowOnResizeUi();
}

//
// Here you can do the clean up (especially for the shortcut)
//
void commandMenuCleanUp()
{
	// Don't forget to deallocate your shortcut here
}


//
// This function help you to initialize your plugin commands
//
bool setCommand(size_t index, const TCHAR *cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey *sk, bool check0nInit)
{
    if (index >= nbFunc)
        return false;

    if (!pFunc)
        return false;

    lstrcpyn(funcItem[index]._itemName, cmdName, menuItemSize);
    funcItem[index]._pFunc = pFunc;
    funcItem[index]._init2Check = check0nInit;
    funcItem[index]._pShKey = sk;

    return true;
}

//----------------------------------------------//
//-- STEP 4. DEFINE YOUR ASSOCIATED FUNCTIONS --//
//----------------------------------------------//
void alignTable()
{
	runTableAction(MarkdownTable::Action::Align, false);
}

void nextCell()
{
	runTableAction(MarkdownTable::Action::NextCell, false);
}

void previousCell()
{
	runTableAction(MarkdownTable::Action::PreviousCell, false);
}

void insertRowBelow()
{
	runTableAction(MarkdownTable::Action::InsertRowBelow, false);
}

void deleteRow()
{
	runTableAction(MarkdownTable::Action::DeleteRow, false);
}

void insertColumnRight()
{
	runTableAction(MarkdownTable::Action::InsertColumnRight, false);
}

void deleteColumn()
{
	runTableAction(MarkdownTable::Action::DeleteColumn, false);
}

void moveRowUp()
{
	runTableAction(MarkdownTable::Action::MoveRowUp, false);
}

void moveRowDown()
{
	runTableAction(MarkdownTable::Action::MoveRowDown, false);
}

void moveColumnLeft()
{
	runTableAction(MarkdownTable::Action::MoveColumnLeft, false);
}

void moveColumnRight()
{
	runTableAction(MarkdownTable::Action::MoveColumnRight, false);
}

void sortRowsAscending()
{
	runTableAction(MarkdownTable::Action::SortRowsAscending, false);
}

void sortRowsDescending()
{
	runTableAction(MarkdownTable::Action::SortRowsDescending, false);
}

void convertCsvTsvSelectionToTable()
{
	runConvertDelimitedSelection();
}

void insertTable()
{
	runInsertTable();
}

void tabOrIndent()
{
	if (runTableAction(MarkdownTable::Action::Align, true))
		return;

	HWND scintilla = currentScintilla();
	if (scintilla)
		::SendMessage(scintilla, SCI_TAB, 0, 0);
}

void wrapLongCells()
{
	if (!fitTableToWindowCommandEnabled())
		return;
	fitCurrentTableToWindow(false);
}

void toggleNotepadWordWrap()
{
	if (!nppData._nppHandle)
		return;

	::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, static_cast<LPARAM>(notepadWordWrapCommandId));
	refreshNotepadWordWrapUi();
}

void toggleAutoWrapLongCells()
{
	g_autoWrapLongCells = !g_autoWrapLongCells;
	refreshAutoWrapLongCellsUi();
	if (g_autoWrapLongCells)
		fitCurrentTableToWindow(true);
}

void toggleFitToWindowOnResize()
{
	g_fitToWindowOnResize = !g_fitToWindowOnResize;
	if (g_fitToWindowOnResize)
		rememberCurrentFitToWindowWidth();
	else
	{
		cancelFitToWindowResizeTimers();
		g_lastFitToWindowColumns = 0;
	}
	refreshFitToWindowOnResizeUi();
}

void installFitToWindowResizeHooks()
{
	subclassScintillaWindow(nppData._scintillaMainHandle, g_originalMainScintillaProc);
	subclassScintillaWindow(nppData._scintillaSecondHandle, g_originalSecondScintillaProc);
	rememberCurrentFitToWindowWidth();
}

void removeFitToWindowResizeHooks()
{
	cancelFitToWindowResizeTimers();
	removeScintillaSubclass(nppData._scintillaMainHandle, g_originalMainScintillaProc);
	removeScintillaSubclass(nppData._scintillaSecondHandle, g_originalSecondScintillaProc);
}
