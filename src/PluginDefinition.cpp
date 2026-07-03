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

struct WordWrapAutoFitWarningDialogState
{
	bool dontShowAgain = false;
};

enum class CaretPlacement
{
	ActionTarget,
	PreserveCellOffset
};

struct InsertText
{
	std::string text;
	std::size_t caretDelta = 0;
};

struct CellBounds
{
	bool found = false;
	std::size_t cellStart = 0;
	std::size_t cellEnd = 0;
	std::size_t contentStart = 0;
	std::size_t contentEnd = 0;
};

struct CellCaretSnapshot
{
	bool valid = false;
	std::size_t row = 0;
	std::size_t logicalRow = 0;
	std::size_t column = 0;
	std::size_t offset = 0;
	std::string prefix;
};

struct CellCaretPosition
{
	bool found = false;
	std::size_t row = 0;
	std::size_t columnOffset = 0;
};

struct LogicalRowMap
{
	std::size_t columns = 0;
	std::vector<std::size_t> baseRowForRow;
	std::vector<std::size_t> logicalRowForRow;
};

struct DocumentTableRange
{
	std::size_t firstLine = 0;
	std::size_t lastLine = 0;
};

struct DocumentTableReplacement
{
	std::size_t firstLine = 0;
	std::size_t lastLine = 0;
	std::string text;
};

const int tableSizeColumnsId = 1001;
const int tableSizeRowsId = 1002;
const int wordWrapAutoFitDontShowAgainId = 1003;
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
ShortcutKey g_narrowColumnShortcut = { true, true, true, VK_OEM_COMMA };
ShortcutKey g_widenColumnShortcut = { true, true, true, VK_OEM_PERIOD };
ShortcutKey g_moveRowUpShortcut = { true, true, true, '8' };
ShortcutKey g_moveRowDownShortcut = { true, true, true, '9' };
ShortcutKey g_moveColumnLeftShortcut = { true, true, true, VK_OEM_4 };
ShortcutKey g_moveColumnRightShortcut = { true, true, true, VK_OEM_6 };
ShortcutKey g_sortRowsAscendingShortcut = { true, true, true, VK_OEM_PLUS };
ShortcutKey g_sortRowsDescendingShortcut = { true, true, true, VK_OEM_MINUS };
ShortcutKey g_convertCsvTsvShortcut = { true, true, true, '0' };
ShortcutKey g_insertTableShortcut = { true, true, true, VK_OEM_5 };
ShortcutKey g_wrapLongCellsShortcut = { true, true, true, 'W' };
ShortcutKey g_autoAlignTableShortcut = { true, true, true, 'A' };
ShortcutKey g_autoFitTableShortcut = { true, true, true, 'F' };

const std::size_t alignCommandIndex = 0;
const std::size_t autoAlignTableCommandIndex = 1;
const std::size_t wrapLongCellsCommandIndex = 2;
const std::size_t autoFitTableCommandIndex = 3;
const std::size_t nextCellCommandIndex = 4;
const std::size_t previousCellCommandIndex = 5;
const std::size_t insertRowCommandIndex = 6;
const std::size_t deleteRowCommandIndex = 7;
const std::size_t insertColumnCommandIndex = 8;
const std::size_t deleteColumnCommandIndex = 9;
const std::size_t narrowColumnCommandIndex = 10;
const std::size_t widenColumnCommandIndex = 11;
const std::size_t moveRowUpCommandIndex = 12;
const std::size_t moveRowDownCommandIndex = 13;
const std::size_t moveColumnLeftCommandIndex = 14;
const std::size_t moveColumnRightCommandIndex = 15;
const std::size_t sortRowsAscendingCommandIndex = 16;
const std::size_t sortRowsDescendingCommandIndex = 17;
const std::size_t convertCsvTsvCommandIndex = 18;
const std::size_t insertTableCommandIndex = 19;
const UINT_PTR fitToWindowResizeTimerId = 0x4D54;
const UINT fitToWindowResizeDelayMs = 160;
const std::size_t wrapStabilizationPassLimit = 8;

bool g_autoFitTable = true;
bool g_autoAlignTable = true;
bool g_fitToWindowInProgress = false;
bool g_autoAlignInProgress = false;
int g_pluginEditDepth = 0;
int g_pluginEditUpdateAutoFormatSkips = 0;
int g_pluginEditGlobalAutoFormatSkips = 0;
int g_pluginEditResizeAutoFitSkips = 0;
bool g_wordWrapAutoFitWarningSuppressed = false;
bool g_wordWrapAutoFitWarningSuppressionLoaded = false;
bool g_wordWrapAutoFitWarningComboActive = false;
std::size_t g_lastFitToWindowColumns = 0;
std::vector<uptr_t> g_initialAutoFormatBuffers;
std::vector<uptr_t> g_pendingInitialAutoFormatBuffers;
HWND g_pendingFitToWindowScintilla = NULL;
WNDPROC g_originalMainScintillaProc = NULL;
WNDPROC g_originalSecondScintillaProc = NULL;
WNDPROC g_originalNppProc = NULL;
HBITMAP g_toolbarBmps[nbFunc] = {};
HICON g_toolbarIcons[nbFunc] = {};
HICON g_toolbarIconDarkModes[nbFunc] = {};

void cancelFitToWindowResizeTimers();

struct PluginEditGuard
{
	PluginEditGuard()
	{
		++g_pluginEditDepth;
		g_pluginEditUpdateAutoFormatSkips = (std::max)(g_pluginEditUpdateAutoFormatSkips, 2);
		g_pluginEditGlobalAutoFormatSkips = (std::max)(g_pluginEditGlobalAutoFormatSkips, 2);
		g_pluginEditResizeAutoFitSkips = (std::max)(g_pluginEditResizeAutoFitSkips, 1);
		cancelFitToWindowResizeTimers();
	}

	~PluginEditGuard()
	{
		if (g_pluginEditDepth > 0)
			--g_pluginEditDepth;
		cancelFitToWindowResizeTimers();
	}
};

bool consumePluginEditAutoFormatSkip(int &skips)
{
	if (g_pluginEditDepth > 0)
		return true;
	if (skips > 0)
	{
		--skips;
		return true;
	}
	return false;
}

bool shouldPreserveEnterColumn(bool activeEditor, bool emptySelection, bool tableLine, UINT message, WPARAM key);
bool captureEnterColumnToPreserve(HWND hwnd, UINT message, WPARAM key, std::size_t &column);
void restoreEnterColumn(HWND hwnd, std::size_t column);

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
	const wchar_t *wordWrapAutoFitWarningTitle;
	const wchar_t *wordWrapAutoFitWarningMessage;
	const wchar_t *doNotShowAgain;
};

#define UI_TEXT_WITH_ENGLISH_MESSAGES(menuName, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14) \
{ \
	menuName, \
	{ c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, NULL, NULL, NULL, NULL, NULL }, \
	L"Insert Markdown table", \
	L"Columns:", \
	L"Data rows:", \
	L"OK", \
	L"Cancel", \
	L"Use 1-50 columns and 0-200 data rows.", \
	L"Put the caret inside a Markdown pipe table first.", \
	L"Could not edit the Markdown table.", \
	L"Select CSV/TSV text or put the caret inside a CSV/TSV block first.", \
	L"Could not convert the selected CSV/TSV text.", \
	L"Power Auto fit and Notepad++ Word Wrap", \
	L"Notepad++ Word Wrap is on.\r\nPower Auto fit will edit Markdown table text to fit the window. Word Wrap only changes screen display.", \
	L"Do not show again" \
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
		L"Narrow column",
		L"Widen column",
		NULL,
		NULL,
		NULL
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
	L"Could not convert the selected CSV/TSV text.",
	L"Power Auto fit and Notepad++ Word Wrap",
	L"Notepad++ Word Wrap is on.\r\nPower Auto fit will edit Markdown table text to fit the window. Word Wrap only changes screen display.",
	L"Do not show again"
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
	L"\u63D2\u5165\u65B0\u8868\u683C"
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
	L"\u0928\u0908 \u0924\u093E\u0932\u093F\u0915\u093E \u0921\u093E\u0932\u0947\u0902"
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
	L"Insertar tabla nueva"
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
	L"\u0625\u062F\u0631\u0627\u062C \u062C\u062F\u0648\u0644 \u062C\u062F\u064A\u062F"
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
	L"Inserer un nouveau tableau"
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
	L"\u09A8\u09A4\u09C1\u09A8 \u099F\u09C7\u09AC\u09BF\u09B2 \u09A2\u09CB\u0995\u09BE\u09A8"
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
	L"Inserir nova tabela"
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
	L"Sisipkan tabel baru"
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
	L"\u0646\u06CC\u0627 \u062C\u062F\u0648\u0644 \u062F\u0627\u062E\u0644 \u06A9\u0631\u06CC\u06BA"
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
	L"Neue Tabelle einfuegen"
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
		NULL,
		NULL,
		NULL
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
	L"\u041D\u0435 \u0443\u0434\u0430\u043B\u043E\u0441\u044C \u043F\u0440\u0435\u043E\u0431\u0440\u0430\u0437\u043E\u0432\u0430\u0442\u044C \u0432\u044B\u0431\u0440\u0430\u043D\u043D\u044B\u0439 CSV/TSV-\u0442\u0435\u043A\u0441\u0442.",
	L"Power \u0410\u0432\u0442\u043E\u043F\u043E\u0434\u0433\u043E\u043D\u043A\u0430 \u0438 \u043F\u0435\u0440\u0435\u043D\u043E\u0441 \u0441\u0442\u0440\u043E\u043A",
	L"\u041F\u0435\u0440\u0435\u043D\u043E\u0441 \u0441\u0442\u0440\u043E\u043A Notepad++ \u0432\u043A\u043B\u044E\u0447\u0451\u043D.\r\nPower \u0430\u0432\u0442\u043E\u043F\u043E\u0434\u0433\u043E\u043D\u043A\u0430 \u0431\u0443\u0434\u0435\u0442 \u043C\u0435\u043D\u044F\u0442\u044C Markdown-\u0442\u0435\u043A\u0441\u0442 \u0442\u0430\u0431\u043B\u0438\u0446\u044B \u043F\u043E\u0434 \u0448\u0438\u0440\u0438\u043D\u0443 \u043E\u043A\u043D\u0430. \u041F\u0435\u0440\u0435\u043D\u043E\u0441 \u0441\u0442\u0440\u043E\u043A Notepad++ \u0432\u043B\u0438\u044F\u0435\u0442 \u0442\u043E\u043B\u044C\u043A\u043E \u043D\u0430 \u043E\u0442\u043E\u0431\u0440\u0430\u0436\u0435\u043D\u0438\u0435.",
	L"\u0411\u043E\u043B\u044C\u0448\u0435 \u043D\u0435 \u043F\u043E\u043A\u0430\u0437\u044B\u0432\u0430\u0442\u044C"
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
	L"\u65B0\u3057\u3044\u30C6\u30FC\u30D6\u30EB\u3092\u633F\u5165"
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
	L"Put new table"
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
	L"\u0928\u0935\u093E \u0924\u0915\u094D\u0924\u093E \u0918\u093E\u0932\u093E"
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
	L"\u0C15\u0C4A\u0C24\u0C4D\u0C24 \u0C2A\u0C1F\u0C4D\u0C1F\u0C3F\u0C15\u0C28\u0C41 \u0C1A\u0C4A\u0C2A\u0C4D\u0C2A\u0C3F\u0C02\u0C1A\u0C41"
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
	L"Yeni tablo ekle"
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
	L"\u0BAA\u0BC1\u0BA4\u0BBF\u0BAF \u0B85\u0B9F\u0BCD\u0B9F\u0BB5\u0BA3\u0BC8 \u0B9A\u0BC6\u0BB0\u0BC1\u0B95\u0BC1"
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
	L"\u63D2\u5165\u65B0\u8868\u683C"
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
	L"Ch\u00E8n b\u1EA3ng m\u1EDBi"
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

const wchar_t *localizedSpecialCommandText(std::size_t index)
{
	switch (g_uiLanguage)
	{
	case UiLanguage::MandarinChinese:
		switch (index)
		{
		case alignCommandIndex: return L"\u5BF9\u9F50\u8868\u683C\uFF08\u4E0D\u6539\u53D8\u5BBD\u5EA6\uFF09";
		case autoAlignTableCommandIndex: return L"\u7F16\u8F91\u540E\u81EA\u52A8\u5BF9\u9F50\uFF08\u4E0D\u6539\u53D8\u5BBD\u5EA6\uFF09";
		case wrapLongCellsCommandIndex: return L"\u5C06\u8868\u683C\u5BBD\u5EA6\u9002\u5E94\u7A97\u53E3";
		case autoFitTableCommandIndex: return L"\u81EA\u52A8\u5C06\u8868\u683C\u5BBD\u5EA6\u9002\u5E94\u7A97\u53E3";
		default: return NULL;
		}
	case UiLanguage::Hindi:
		switch (index)
		{
		case alignCommandIndex: return L"\u0924\u093E\u0932\u093F\u0915\u093E \u0938\u0902\u0930\u0947\u0916\u093F\u0924 \u0915\u0930\u0947\u0902 (\u091A\u094C\u0921\u093C\u093E\u0908 \u0928 \u092C\u0926\u0932\u0947\u0902)";
		case autoAlignTableCommandIndex: return L"\u0938\u0902\u092A\u093E\u0926\u0928 \u0915\u0947 \u092C\u093E\u0926 \u0938\u094D\u0935\u0924\u0903 \u0938\u0902\u0930\u0947\u0916\u093F\u0924 \u0915\u0930\u0947\u0902 (\u091A\u094C\u0921\u093C\u093E\u0908 \u0928 \u092C\u0926\u0932\u0947\u0902)";
		case wrapLongCellsCommandIndex: return L"\u0924\u093E\u0932\u093F\u0915\u093E \u0915\u0940 \u091A\u094C\u0921\u093C\u093E\u0908 \u0915\u094B \u0935\u093F\u0902\u0921\u094B \u092E\u0947\u0902 \u092B\u093F\u091F \u0915\u0930\u0947\u0902";
		case autoFitTableCommandIndex: return L"\u0924\u093E\u0932\u093F\u0915\u093E \u0915\u0940 \u091A\u094C\u0921\u093C\u093E\u0908 \u0915\u094B \u0935\u093F\u0902\u0921\u094B \u092E\u0947\u0902 \u0938\u094D\u0935\u0924\u0903 \u092B\u093F\u091F \u0915\u0930\u0947\u0902";
		default: return NULL;
		}
	case UiLanguage::Spanish:
		switch (index)
		{
		case alignCommandIndex: return L"Alinear tabla (sin cambiar ancho)";
		case autoAlignTableCommandIndex: return L"Alinear automaticamente tras editar (sin cambiar ancho)";
		case wrapLongCellsCommandIndex: return L"Ajustar ancho de tabla a ventana";
		case autoFitTableCommandIndex: return L"Ajustar automaticamente ancho de tabla a ventana";
		default: return NULL;
		}
	case UiLanguage::Arabic:
		switch (index)
		{
		case alignCommandIndex: return L"\u0645\u062D\u0627\u0630\u0627\u0629 \u0627\u0644\u062C\u062F\u0648\u0644 (\u0628\u062F\u0648\u0646 \u062A\u063A\u064A\u064A\u0631 \u0627\u0644\u0639\u0631\u0636)";
		case autoAlignTableCommandIndex: return L"\u0645\u062D\u0627\u0630\u0627\u0629 \u062A\u0644\u0642\u0627\u0626\u064A\u0629 \u0628\u0639\u062F \u0627\u0644\u062A\u062D\u0631\u064A\u0631 (\u0628\u062F\u0648\u0646 \u062A\u063A\u064A\u064A\u0631 \u0627\u0644\u0639\u0631\u0636)";
		case wrapLongCellsCommandIndex: return L"\u0645\u0644\u0627\u0621\u0645\u0629 \u0639\u0631\u0636 \u0627\u0644\u062C\u062F\u0648\u0644 \u0644\u0644\u0646\u0627\u0641\u0630\u0629";
		case autoFitTableCommandIndex: return L"\u0645\u0644\u0627\u0621\u0645\u0629 \u062A\u0644\u0642\u0627\u0626\u064A\u0629 \u0644\u0639\u0631\u0636 \u0627\u0644\u062C\u062F\u0648\u0644 \u0644\u0644\u0646\u0627\u0641\u0630\u0629";
		default: return NULL;
		}
	case UiLanguage::French:
		switch (index)
		{
		case alignCommandIndex: return L"Aligner le tableau (sans changer la largeur)";
		case autoAlignTableCommandIndex: return L"Alignement auto apres modification (sans changer la largeur)";
		case wrapLongCellsCommandIndex: return L"Ajuster la largeur du tableau a la fenetre";
		case autoFitTableCommandIndex: return L"Ajustement auto de la largeur du tableau a la fenetre";
		default: return NULL;
		}
	case UiLanguage::Bengali:
		switch (index)
		{
		case alignCommandIndex: return L"\u099F\u09C7\u09AC\u09BF\u09B2 \u09B8\u09BE\u09B0\u09BF\u09AC\u09A6\u09CD\u09A7 \u0995\u09B0\u09C1\u09A8 (\u09AA\u09CD\u09B0\u09B8\u09CD\u09A5 \u09A8\u09BE \u09AC\u09A6\u09B2\u09C7)";
		case autoAlignTableCommandIndex: return L"\u09B8\u09AE\u09CD\u09AA\u09BE\u09A6\u09A8\u09BE\u09B0 \u09AA\u09B0\u09C7 \u09B8\u09CD\u09AC\u09AF\u09BC\u0982\u0995\u09CD\u09B0\u09BF\u09AF\u09BC \u09B8\u09BE\u09B0\u09BF\u09AC\u09A6\u09CD\u09A7\u0995\u09B0\u09A3 (\u09AA\u09CD\u09B0\u09B8\u09CD\u09A5 \u09A8\u09BE \u09AC\u09A6\u09B2\u09C7)";
		case wrapLongCellsCommandIndex: return L"\u099F\u09C7\u09AC\u09BF\u09B2\u09C7\u09B0 \u09AA\u09CD\u09B0\u09B8\u09CD\u09A5 \u0989\u0987\u09A8\u09CD\u09A1\u09CB\u09B0 \u09B8\u09BE\u09A5\u09C7 \u09AE\u09BE\u09A8\u09BE\u09A8\u09B8\u0987 \u0995\u09B0\u09C1\u09A8";
		case autoFitTableCommandIndex: return L"\u099F\u09C7\u09AC\u09BF\u09B2\u09C7\u09B0 \u09AA\u09CD\u09B0\u09B8\u09CD\u09A5 \u0989\u0987\u09A8\u09CD\u09A1\u09CB\u09B0 \u09B8\u09BE\u09A5\u09C7 \u09B8\u09CD\u09AC\u09AF\u09BC\u0982\u0995\u09CD\u09B0\u09BF\u09AF\u09BC\u09AD\u09BE\u09AC\u09C7 \u09AE\u09BE\u09A8\u09BE\u09A8\u09B8\u0987 \u0995\u09B0\u09C1\u09A8";
		default: return NULL;
		}
	case UiLanguage::Portuguese:
		switch (index)
		{
		case alignCommandIndex: return L"Alinhar tabela (sem alterar largura)";
		case autoAlignTableCommandIndex: return L"Alinhar automaticamente apos edicao (sem alterar largura)";
		case wrapLongCellsCommandIndex: return L"Ajustar largura da tabela a janela";
		case autoFitTableCommandIndex: return L"Ajustar automaticamente largura da tabela a janela";
		default: return NULL;
		}
	case UiLanguage::Russian:
		switch (index)
		{
		case alignCommandIndex: return L"\u0412\u044B\u0440\u043E\u0432\u043D\u044F\u0442\u044C \u0442\u0430\u0431\u043B\u0438\u0446\u0443 (\u0431\u0435\u0437 \u0438\u0437\u043C\u0435\u043D\u0435\u043D\u0438\u044F \u0448\u0438\u0440\u0438\u043D\u044B)";
		case autoAlignTableCommandIndex: return L"\u0410\u0432\u0442\u043E\u0432\u044B\u0440\u0430\u0432\u043D\u0438\u0432\u0430\u043D\u0438\u0435 \u043F\u043E\u0441\u043B\u0435 \u043F\u0440\u0430\u0432\u043A\u0438 (\u0431\u0435\u0437 \u0438\u0437\u043C\u0435\u043D\u0435\u043D\u0438\u044F \u0448\u0438\u0440\u0438\u043D\u044B)";
		case wrapLongCellsCommandIndex: return L"\u041F\u043E\u0434\u043E\u0433\u043D\u0430\u0442\u044C \u0448\u0438\u0440\u0438\u043D\u0443 \u0442\u0430\u0431\u043B\u0438\u0446\u044B \u043F\u043E\u0434 \u043E\u043A\u043D\u043E";
		case autoFitTableCommandIndex: return L"\u0410\u0432\u0442\u043E\u043F\u043E\u0434\u0433\u043E\u043D\u043A\u0430 \u0448\u0438\u0440\u0438\u043D\u044B \u0442\u0430\u0431\u043B\u0438\u0446\u044B \u043F\u043E\u0434 \u043E\u043A\u043D\u043E";
		case narrowColumnCommandIndex: return L"\u0421\u0443\u0437\u0438\u0442\u044C \u0441\u0442\u043E\u043B\u0431\u0435\u0446";
		case widenColumnCommandIndex: return L"\u0420\u0430\u0441\u0448\u0438\u0440\u0438\u0442\u044C \u0441\u0442\u043E\u043B\u0431\u0435\u0446";
		default: return NULL;
		}
	case UiLanguage::Indonesian:
		switch (index)
		{
		case alignCommandIndex: return L"Ratakan tabel (tanpa mengubah lebar)";
		case autoAlignTableCommandIndex: return L"Ratakan otomatis setelah edit (tanpa mengubah lebar)";
		case wrapLongCellsCommandIndex: return L"Sesuaikan lebar tabel ke jendela";
		case autoFitTableCommandIndex: return L"Sesuaikan otomatis lebar tabel ke jendela";
		default: return NULL;
		}
	case UiLanguage::Urdu:
		switch (index)
		{
		case alignCommandIndex: return L"\u062C\u062F\u0648\u0644 \u0633\u06CC\u062F\u06BE\u0627 \u06A9\u0631\u06CC\u06BA (\u0686\u0648\u0691\u0627\u0626\u06CC \u0628\u062F\u0644\u06D2 \u0628\u063A\u06CC\u0631)";
		case autoAlignTableCommandIndex: return L"\u062A\u0631\u0645\u06CC\u0645 \u06A9\u06D2 \u0628\u0639\u062F \u062E\u0648\u062F\u06A9\u0627\u0631 \u0633\u06CC\u062F\u06BE (\u0686\u0648\u0691\u0627\u0626\u06CC \u0628\u062F\u0644\u06D2 \u0628\u063A\u06CC\u0631)";
		case wrapLongCellsCommandIndex: return L"\u062C\u062F\u0648\u0644 \u06A9\u06CC \u0686\u0648\u0691\u0627\u0626\u06CC \u0648\u0646\u0688\u0648 \u06A9\u06D2 \u0645\u0637\u0627\u0628\u0642 \u06A9\u0631\u06CC\u06BA";
		case autoFitTableCommandIndex: return L"\u062C\u062F\u0648\u0644 \u06A9\u06CC \u0686\u0648\u0691\u0627\u0626\u06CC \u062E\u0648\u062F\u06A9\u0627\u0631 \u0637\u0648\u0631 \u067E\u0631 \u0648\u0646\u0688\u0648 \u06A9\u06D2 \u0645\u0637\u0627\u0628\u0642 \u06A9\u0631\u06CC\u06BA";
		default: return NULL;
		}
	case UiLanguage::German:
		switch (index)
		{
		case alignCommandIndex: return L"Tabelle ausrichten (Breite unveraendert)";
		case autoAlignTableCommandIndex: return L"Nach Bearbeitung automatisch ausrichten (Breite unveraendert)";
		case wrapLongCellsCommandIndex: return L"Tabellenbreite ans Fenster anpassen";
		case autoFitTableCommandIndex: return L"Tabellenbreite automatisch ans Fenster anpassen";
		default: return NULL;
		}
	case UiLanguage::Japanese:
		switch (index)
		{
		case alignCommandIndex: return L"\u30C6\u30FC\u30D6\u30EB\u3092\u6574\u5217\uFF08\u5E45\u3092\u5909\u66F4\u3057\u306A\u3044\uFF09";
		case autoAlignTableCommandIndex: return L"\u7DE8\u96C6\u5F8C\u306B\u81EA\u52D5\u6574\u5217\uFF08\u5E45\u3092\u5909\u66F4\u3057\u306A\u3044\uFF09";
		case wrapLongCellsCommandIndex: return L"\u30C6\u30FC\u30D6\u30EB\u5E45\u3092\u30A6\u30A3\u30F3\u30C9\u30A6\u306B\u5408\u308F\u305B\u308B";
		case autoFitTableCommandIndex: return L"\u30C6\u30FC\u30D6\u30EB\u5E45\u3092\u30A6\u30A3\u30F3\u30C9\u30A6\u306B\u81EA\u52D5\u8ABF\u6574";
		default: return NULL;
		}
	case UiLanguage::NigerianPidgin:
		switch (index)
		{
		case alignCommandIndex: return L"Arrange table (no change width)";
		case autoAlignTableCommandIndex: return L"Auto arrange after edit (no change width)";
		case wrapLongCellsCommandIndex: return L"Fit table width to window";
		case autoFitTableCommandIndex: return L"Auto fit table width to window";
		default: return NULL;
		}
	case UiLanguage::Marathi:
		switch (index)
		{
		case alignCommandIndex: return L"\u0924\u0915\u094D\u0924\u093E \u0938\u0902\u0930\u0947\u0916\u093F\u0924 \u0915\u0930\u093E (\u0930\u0941\u0902\u0926\u0940 \u0928 \u092C\u0926\u0932\u0924\u093E)";
		case autoAlignTableCommandIndex: return L"\u0938\u0902\u092A\u093E\u0926\u0928\u093E\u0928\u0902\u0924\u0930 \u0938\u094D\u0935\u092F\u0902\u091A\u0932\u093F\u0924 \u0938\u0902\u0930\u0947\u0916\u0928 (\u0930\u0941\u0902\u0926\u0940 \u0928 \u092C\u0926\u0932\u0924\u093E)";
		case wrapLongCellsCommandIndex: return L"\u0924\u0915\u094D\u0924\u094D\u092F\u093E\u091A\u0940 \u0930\u0941\u0902\u0926\u0940 \u0935\u093F\u0902\u0921\u094B\u0932\u093E \u092C\u0938\u0935\u093E";
		case autoFitTableCommandIndex: return L"\u0924\u0915\u094D\u0924\u094D\u092F\u093E\u091A\u0940 \u0930\u0941\u0902\u0926\u0940 \u0935\u093F\u0902\u0921\u094B\u0932\u093E \u0906\u092A\u094B\u0906\u092A \u092C\u0938\u0935\u093E";
		default: return NULL;
		}
	case UiLanguage::Telugu:
		switch (index)
		{
		case alignCommandIndex: return L"\u0C2A\u0C1F\u0C4D\u0C1F\u0C3F\u0C15\u0C28\u0C41 \u0C38\u0C30\u0C3F\u0C2A\u0C30\u0C1A\u0C41 (\u0C35\u0C46\u0C21\u0C32\u0C4D\u0C2A\u0C41 \u0C2E\u0C3E\u0C30\u0C4D\u0C1A\u0C15\u0C41\u0C02\u0C21\u0C3E)";
		case autoAlignTableCommandIndex: return L"\u0C38\u0C35\u0C30\u0C23 \u0C24\u0C30\u0C4D\u0C35\u0C3E\u0C24 \u0C38\u0C4D\u0C35\u0C2F\u0C02\u0C1A\u0C3E\u0C32\u0C15 \u0C38\u0C30\u0C3F\u0C2A\u0C30\u0C1A\u0C41 (\u0C35\u0C46\u0C21\u0C32\u0C4D\u0C2A\u0C41 \u0C2E\u0C3E\u0C30\u0C4D\u0C1A\u0C15\u0C41\u0C02\u0C21\u0C3E)";
		case wrapLongCellsCommandIndex: return L"\u0C2A\u0C1F\u0C4D\u0C1F\u0C3F\u0C15 \u0C35\u0C46\u0C21\u0C32\u0C4D\u0C2A\u0C41\u0C28\u0C41 \u0C35\u0C3F\u0C02\u0C21\u0C4B\u0C15\u0C41 \u0C38\u0C30\u0C3F\u0C2A\u0C30\u0C1A\u0C41";
		case autoFitTableCommandIndex: return L"\u0C2A\u0C1F\u0C4D\u0C1F\u0C3F\u0C15 \u0C35\u0C46\u0C21\u0C32\u0C4D\u0C2A\u0C41\u0C28\u0C41 \u0C35\u0C3F\u0C02\u0C21\u0C4B\u0C15\u0C41 \u0C38\u0C4D\u0C35\u0C2F\u0C02\u0C1A\u0C3E\u0C32\u0C15\u0C02\u0C17\u0C3E \u0C38\u0C30\u0C3F\u0C2A\u0C30\u0C1A\u0C41";
		default: return NULL;
		}
	case UiLanguage::Turkish:
		switch (index)
		{
		case alignCommandIndex: return L"Tabloyu hizala (genisligi degistirme)";
		case autoAlignTableCommandIndex: return L"Duzenlemeden sonra otomatik hizala (genisligi degistirme)";
		case wrapLongCellsCommandIndex: return L"Tablo genisligini pencereye sigdir";
		case autoFitTableCommandIndex: return L"Tablo genisligini pencereye otomatik sigdir";
		default: return NULL;
		}
	case UiLanguage::Tamil:
		switch (index)
		{
		case alignCommandIndex: return L"\u0B85\u0B9F\u0BCD\u0B9F\u0BB5\u0BA3\u0BC8\u0BAF\u0BC8 \u0B92\u0BB4\u0BC1\u0B99\u0BCD\u0B95\u0BC1\u0BAA\u0B9F\u0BC1\u0BA4\u0BCD\u0BA4\u0BC1 (\u0B85\u0B95\u0BB2\u0BA4\u0BCD\u0BA4\u0BC8 \u0BAE\u0BBE\u0BB1\u0BCD\u0BB1\u0BBE\u0BAE\u0BB2\u0BCD)";
		case autoAlignTableCommandIndex: return L"\u0BA4\u0BBF\u0BB0\u0BC1\u0BA4\u0BCD\u0BA4\u0BA4\u0BCD\u0BA4\u0BBF\u0BB1\u0BCD\u0B95\u0BC1\u0BAA\u0BCD \u0BAA\u0BBF\u0BB1\u0B95\u0BC1 \u0BA4\u0BBE\u0BA9\u0BBE\u0B95 \u0B92\u0BB4\u0BC1\u0B99\u0BCD\u0B95\u0BC1\u0BAA\u0B9F\u0BC1\u0BA4\u0BCD\u0BA4\u0BC1 (\u0B85\u0B95\u0BB2\u0BA4\u0BCD\u0BA4\u0BC8 \u0BAE\u0BBE\u0BB1\u0BCD\u0BB1\u0BBE\u0BAE\u0BB2\u0BCD)";
		case wrapLongCellsCommandIndex: return L"\u0B85\u0B9F\u0BCD\u0B9F\u0BB5\u0BA3\u0BC8 \u0B85\u0B95\u0BB2\u0BA4\u0BCD\u0BA4\u0BC8 \u0B9A\u0BBE\u0BB3\u0BB0\u0BA4\u0BCD\u0BA4\u0BBF\u0BB1\u0BCD\u0B95\u0BC1 \u0BAA\u0BCA\u0BB0\u0BC1\u0BA4\u0BCD\u0BA4\u0BC1";
		case autoFitTableCommandIndex: return L"\u0B85\u0B9F\u0BCD\u0B9F\u0BB5\u0BA3\u0BC8 \u0B85\u0B95\u0BB2\u0BA4\u0BCD\u0BA4\u0BC8 \u0B9A\u0BBE\u0BB3\u0BB0\u0BA4\u0BCD\u0BA4\u0BBF\u0BB1\u0BCD\u0B95\u0BC1 \u0BA4\u0BBE\u0BA9\u0BBE\u0B95 \u0BAA\u0BCA\u0BB0\u0BC1\u0BA4\u0BCD\u0BA4\u0BC1";
		default: return NULL;
		}
	case UiLanguage::YueChinese:
		switch (index)
		{
		case alignCommandIndex: return L"\u5C0D\u9F4A\u8868\u683C\uFF08\u4E0D\u6539\u8B8A\u95CA\u5EA6\uFF09";
		case autoAlignTableCommandIndex: return L"\u7DE8\u8F2F\u5F8C\u81EA\u52D5\u5C0D\u9F4A\uFF08\u4E0D\u6539\u8B8A\u95CA\u5EA6\uFF09";
		case wrapLongCellsCommandIndex: return L"\u5C07\u8868\u683C\u95CA\u5EA6\u914D\u5408\u8996\u7A97";
		case autoFitTableCommandIndex: return L"\u81EA\u52D5\u5C07\u8868\u683C\u95CA\u5EA6\u914D\u5408\u8996\u7A97";
		default: return NULL;
		}
	case UiLanguage::Vietnamese:
		switch (index)
		{
		case alignCommandIndex: return L"C\u0103n ch\u1EC9nh b\u1EA3ng (kh\u00F4ng \u0111\u1ED5i chi\u1EC1u r\u1ED9ng)";
		case autoAlignTableCommandIndex: return L"T\u1EF1 \u0111\u1ED9ng c\u0103n ch\u1EC9nh sau khi s\u1EEDa (kh\u00F4ng \u0111\u1ED5i chi\u1EC1u r\u1ED9ng)";
		case wrapLongCellsCommandIndex: return L"V\u1EEBa chi\u1EC1u r\u1ED9ng b\u1EA3ng v\u1EDBi c\u1EEDa s\u1ED5";
		case autoFitTableCommandIndex: return L"T\u1EF1 \u0111\u1ED9ng v\u1EEBa chi\u1EC1u r\u1ED9ng b\u1EA3ng v\u1EDBi c\u1EEDa s\u1ED5";
		default: return NULL;
		}
	default:
		switch (index)
		{
		case alignCommandIndex: return L"Align table (no width change)";
		case autoAlignTableCommandIndex: return L"Auto align after edit (no width change)";
		case wrapLongCellsCommandIndex: return L"Fit table width to window";
		case autoFitTableCommandIndex: return L"Auto fit table width to window";
		case narrowColumnCommandIndex: return L"Narrow column";
		case widenColumnCommandIndex: return L"Widen column";
		default: return NULL;
		}
	}
}

const wchar_t *autoModeCommandText(std::size_t index, const wchar_t *text)
{
	if (!text)
		return text;
	const wchar_t *prefix = NULL;
	switch (index)
	{
	case autoAlignTableCommandIndex:
		prefix = L"Light ";
		break;
	case autoFitTableCommandIndex:
		prefix = L"Power ";
		break;
	default:
		return text;
	}

	static std::wstring modeText[nbFunc];
	modeText[index] = prefix;
	modeText[index] += text;
	return modeText[index].c_str();
}

std::size_t legacyCommandIndex(std::size_t index)
{
	switch (index)
	{
	case nextCellCommandIndex: return 1;
	case previousCellCommandIndex: return 2;
	case insertRowCommandIndex: return 3;
	case deleteRowCommandIndex: return 4;
	case insertColumnCommandIndex: return 5;
	case deleteColumnCommandIndex: return 6;
	case narrowColumnCommandIndex: return 15;
	case widenColumnCommandIndex: return 16;
	case moveRowUpCommandIndex: return 7;
	case moveRowDownCommandIndex: return 8;
	case moveColumnLeftCommandIndex: return 9;
	case moveColumnRightCommandIndex: return 10;
	case sortRowsAscendingCommandIndex: return 11;
	case sortRowsDescendingCommandIndex: return 12;
	case convertCsvTsvCommandIndex: return 13;
	case insertTableCommandIndex: return 14;
	default: return static_cast<std::size_t>(-1);
	}
}

const wchar_t *commandText(std::size_t index)
{
	if (const wchar_t *special = localizedSpecialCommandText(index))
		return autoModeCommandText(index, special);

	const UiText &localized = uiText();
	const std::size_t legacy = legacyCommandIndex(index);
	if (legacy >= static_cast<std::size_t>(nbFunc))
		return autoModeCommandText(index, englishUiText.commands[0]);
	if (legacy < static_cast<std::size_t>(nbFunc) && localized.commands[legacy])
		return autoModeCommandText(index, localized.commands[legacy]);
	return autoModeCommandText(index, englishUiText.commands[legacy]);
}

const wchar_t *commandShortcutText(std::size_t index)
{
	switch (index)
	{
	case alignCommandIndex: return L"Ctrl+Alt+Shift+1";
	case autoAlignTableCommandIndex: return L"Ctrl+Alt+Shift+A";
	case wrapLongCellsCommandIndex: return L"Ctrl+Alt+Shift+W";
	case autoFitTableCommandIndex: return L"Ctrl+Alt+Shift+F";
	case nextCellCommandIndex: return L"Ctrl+Alt+Shift+2";
	case previousCellCommandIndex: return L"Ctrl+Alt+Shift+3";
	case insertRowCommandIndex: return L"Ctrl+Alt+Shift+4";
	case deleteRowCommandIndex: return L"Ctrl+Alt+Shift+5";
	case insertColumnCommandIndex: return L"Ctrl+Alt+Shift+6";
	case deleteColumnCommandIndex: return L"Ctrl+Alt+Shift+7";
	case narrowColumnCommandIndex: return L"Ctrl+Alt+Shift+,";
	case widenColumnCommandIndex: return L"Ctrl+Alt+Shift+.";
	case moveRowUpCommandIndex: return L"Ctrl+Alt+Shift+8";
	case moveRowDownCommandIndex: return L"Ctrl+Alt+Shift+9";
	case moveColumnLeftCommandIndex: return L"Ctrl+Alt+Shift+[";
	case moveColumnRightCommandIndex: return L"Ctrl+Alt+Shift+]";
	case sortRowsAscendingCommandIndex: return L"Ctrl+Alt+Shift+=";
	case sortRowsDescendingCommandIndex: return L"Ctrl+Alt+Shift+-";
	case convertCsvTsvCommandIndex: return L"Ctrl+Alt+Shift+0";
	case insertTableCommandIndex: return L"Ctrl+Alt+Shift+\\";
	default: return NULL;
	}
}

const wchar_t *commandMenuText(std::size_t index)
{
	const wchar_t *text = commandText(index);
	const wchar_t *shortcut = commandShortcutText(index);
	if (!shortcut || !*shortcut || index >= static_cast<std::size_t>(nbFunc))
		return text;

	static std::wstring menuText[nbFunc];
	menuText[index] = text;
	menuText[index] += L"\t";
	menuText[index] += shortcut;
	return menuText[index].c_str();
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
		lstrcpyn(funcItem[index]._itemName, commandMenuText(static_cast<std::size_t>(index)), menuItemSize);
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

std::wstring pluginConfigFilePath()
{
	if (!nppData._nppHandle)
		return std::wstring();

	const LRESULT required = ::SendMessage(nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, 0, 0);
	if (required <= 0)
		return std::wstring();

	std::vector<wchar_t> buffer(static_cast<std::size_t>(required) + 1, L'\0');
	const LRESULT copied = ::SendMessage(
		nppData._nppHandle,
		NPPM_GETPLUGINSCONFIGDIR,
		static_cast<WPARAM>(buffer.size()),
		reinterpret_cast<LPARAM>(&buffer[0]));
	if (copied == FALSE || buffer.empty() || buffer[0] == L'\0')
		return std::wstring();

	std::wstring path(&buffer[0]);
	const wchar_t last = path.empty() ? L'\0' : path[path.size() - 1];
	if (last != L'\\' && last != L'/')
		path += L"\\";
	path += L"MarkdownTableEditor.ini";
	return path;
}

bool loadWordWrapAutoFitWarningSuppression()
{
	const std::wstring path = pluginConfigFilePath();
	if (path.empty())
		return false;

	g_wordWrapAutoFitWarningSuppressed = ::GetPrivateProfileInt(
		L"Warnings",
		L"HideWordWrapAutoFitWarning",
		0,
		path.c_str()) != 0;
	g_wordWrapAutoFitWarningSuppressionLoaded = true;
	return true;
}

bool wordWrapAutoFitWarningSuppressed()
{
	if (!g_wordWrapAutoFitWarningSuppressionLoaded)
		loadWordWrapAutoFitWarningSuppression();
	return g_wordWrapAutoFitWarningSuppressed;
}

void setWordWrapAutoFitWarningSuppressed(bool suppressed)
{
	g_wordWrapAutoFitWarningSuppressed = suppressed;
	g_wordWrapAutoFitWarningSuppressionLoaded = true;

	const std::wstring path = pluginConfigFilePath();
	if (path.empty())
		return;

	::WritePrivateProfileString(
		L"Warnings",
		L"HideWordWrapAutoFitWarning",
		suppressed ? L"1" : L"0",
		path.c_str());
}

bool standardWordWrapEnabledForMode(LRESULT wrapMode)
{
	return wrapMode != SC_WRAP_NONE;
}

bool standardWordWrapEnabled(HWND scintilla)
{
	if (!scintilla)
		return false;

	const LRESULT wrapMode = ::SendMessage(scintilla, SCI_GETWRAPMODE, 0, 0);
	return standardWordWrapEnabledForMode(wrapMode);
}

bool shouldShowWordWrapAutoFitWarning(bool autoFitEnabled, bool wordWrapEnabled, bool warningSuppressed)
{
	return autoFitEnabled && wordWrapEnabled && !warningSuppressed;
}

LRESULT CALLBACK wordWrapAutoFitWarningDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	WordWrapAutoFitWarningDialogState *state = reinterpret_cast<WordWrapAutoFitWarningDialogState *>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));

	switch (message)
	{
		case WM_CREATE:
		{
			const UiText &text = uiText();
			CREATESTRUCT *create = reinterpret_cast<CREATESTRUCT *>(lParam);
			state = reinterpret_cast<WordWrapAutoFitWarningDialogState *>(create->lpCreateParams);
			::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

			HFONT font = reinterpret_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
			HWND messageText = ::CreateWindowEx(0, TEXT("STATIC"), text.wordWrapAutoFitWarningMessage, WS_CHILD | WS_VISIBLE | SS_LEFT, 18, 18, 404, 72, hwnd, NULL, g_moduleHandle, NULL);
			HWND check = ::CreateWindowEx(0, TEXT("BUTTON"), text.doNotShowAgain, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 18, 100, 250, 24, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(wordWrapAutoFitDontShowAgainId)), g_moduleHandle, NULL);
			HWND ok = ::CreateWindowEx(0, TEXT("BUTTON"), text.okButton, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 178, 136, 84, 28, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)), g_moduleHandle, NULL);

			HWND controls[] = { messageText, check, ok };
			for (std::size_t i = 0; i < sizeof(controls) / sizeof(controls[0]); ++i)
				::SendMessage(controls[i], WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

			::SetFocus(ok);
			return 0;
		}

		case WM_COMMAND:
		{
			const int command = LOWORD(wParam);
			if (command == IDOK)
			{
				const LRESULT checked = ::SendDlgItemMessage(hwnd, wordWrapAutoFitDontShowAgainId, BM_GETCHECK, 0, 0);
				if (state)
					state->dontShowAgain = checked == BST_CHECKED;
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

bool promptWordWrapAutoFitWarning(WordWrapAutoFitWarningDialogState &state)
{
	if (!nppData._nppHandle)
		return false;

	const TCHAR className[] = TEXT("MarkdownTableEditorWordWrapAutoFitWarning");
	static bool registered = false;
	if (!registered)
	{
		WNDCLASS wc;
		ZeroMemory(&wc, sizeof(wc));
		wc.lpfnWndProc = wordWrapAutoFitWarningDialogProc;
		wc.hInstance = g_moduleHandle;
		wc.hCursor = ::LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
		wc.lpszClassName = className;
		if (!::RegisterClass(&wc))
			return false;
		registered = true;
	}

	RECT ownerRect;
	if (!::GetWindowRect(nppData._nppHandle, &ownerRect))
	{
		ownerRect.left = 100;
		ownerRect.top = 100;
		ownerRect.right = 700;
		ownerRect.bottom = 500;
	}

	const int width = 460;
	const int height = 210;
	const int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - width) / 2;
	const int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - height) / 2;
	HWND dialog = ::CreateWindowEx(WS_EX_DLGMODALFRAME, className, uiText().wordWrapAutoFitWarningTitle, WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, width, height, nppData._nppHandle, NULL, g_moduleHandle, &state);
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
	return true;
}

void checkWordWrapAutoFitWarningInternal()
{
	const bool comboActive = g_autoFitTable && standardWordWrapEnabled(currentScintilla());
	if (!comboActive)
	{
		g_wordWrapAutoFitWarningComboActive = false;
		return;
	}
	if (g_wordWrapAutoFitWarningComboActive)
		return;
	if (!nppData._nppHandle || !::IsWindowVisible(nppData._nppHandle))
		return;

	g_wordWrapAutoFitWarningComboActive = true;
	if (!shouldShowWordWrapAutoFitWarning(g_autoFitTable, true, wordWrapAutoFitWarningSuppressed()))
		return;

	WordWrapAutoFitWarningDialogState state;
	if (promptWordWrapAutoFitWarning(state) && state.dontShowAgain)
		setWordWrapAutoFitWarningSuppressed(true);
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

HMENU pluginCommandMenu(HMENU menu)
{
	if (!menu)
		return NULL;

	const int count = ::GetMenuItemCount(menu);
	for (int index = 0; index < count; ++index)
	{
		if (isPluginCommandId(::GetMenuItemID(menu, index)))
			return menu;

		HMENU subMenu = ::GetSubMenu(menu, index);
		if (HMENU found = pluginCommandMenu(subMenu))
			return found;
	}
	return NULL;
}

int commandMenuPosition(HMENU menu, UINT commandId)
{
	if (!menu)
		return -1;

	const int count = ::GetMenuItemCount(menu);
	for (int index = 0; index < count; ++index)
	{
		if (::GetMenuItemID(menu, index) == commandId)
			return index;
	}
	return -1;
}

bool menuItemIsSeparator(HMENU menu, int position)
{
	if (!menu || position < 0 || position >= ::GetMenuItemCount(menu))
		return false;

	MENUITEMINFO info;
	ZeroMemory(&info, sizeof(info));
	info.cbSize = sizeof(info);
	info.fMask = MIIM_FTYPE;
	if (!::GetMenuItemInfo(menu, static_cast<UINT>(position), TRUE, &info))
		return false;
	return (info.fType & MFT_SEPARATOR) != 0;
}

void ensurePluginCommandSeparator(HMENU pluginsMenu)
{
	if (funcItem[autoFitTableCommandIndex]._cmdID == 0)
		return;

	HMENU commandMenu = pluginCommandMenu(pluginsMenu);
	const int position = commandMenuPosition(commandMenu, static_cast<UINT>(funcItem[autoFitTableCommandIndex]._cmdID));
	if (position < 0 || menuItemIsSeparator(commandMenu, position + 1))
		return;

	::InsertMenu(commandMenu, static_cast<UINT>(position + 1), MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
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
			updateCommandMenuItem(pluginsMenu, static_cast<UINT>(funcItem[index]._cmdID), commandMenuText(static_cast<std::size_t>(index)));
	}
	ensurePluginCommandSeparator(pluginsMenu);
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
	TableFitWidth,
	TableAutoFitWidth,
	TableAlign,
	TableAutoAlign,
	TableNextCell,
	TablePreviousCell,
	TableInsertRow,
	TableDeleteRow,
	TableInsertColumn,
	TableDeleteColumn,
	TableNarrowColumn,
	TableWidenColumn,
	TableMoveRowUp,
	TableMoveRowDown,
	TableMoveColumnLeft,
	TableMoveColumnRight,
	TableSortAscending,
	TableSortDescending,
	TableConvertDelimited,
	TableInsertTable
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

void drawAlignRows(std::uint32_t *pixels, std::uint32_t accent)
{
	drawIconHorizontalLine(pixels, 3, 12, 9, accent);
	drawIconHorizontalLine(pixels, 3, 12, 10, accent);
	drawIconHorizontalLine(pixels, 3, 12, 12, accent);
	drawIconHorizontalLine(pixels, 3, 12, 13, accent);
}

void drawFitWidthArrows(std::uint32_t *pixels, std::uint32_t accent)
{
	drawIconHorizontalLine(pixels, 4, 11, 4, accent);
	drawIconHorizontalLine(pixels, 4, 11, 5, accent);
	drawIconVerticalLine(pixels, 2, 3, 5, accent);
	drawIconVerticalLine(pixels, 13, 3, 5, accent);
	setIconPixel(pixels, 3, 4, accent);
	setIconPixel(pixels, 4, 3, accent);
	setIconPixel(pixels, 4, 5, accent);
	setIconPixel(pixels, 12, 4, accent);
	setIconPixel(pixels, 11, 3, accent);
	setIconPixel(pixels, 11, 5, accent);
}

void drawAutoCorner(std::uint32_t *pixels, std::uint32_t accent)
{
	drawIconHorizontalLine(pixels, 10, 13, 1, accent);
	drawIconVerticalLine(pixels, 13, 1, 4, accent);
	setIconPixel(pixels, 12, 5, accent);
	setIconPixel(pixels, 11, 4, accent);
	setIconPixel(pixels, 10, 4, accent);
	setIconPixel(pixels, 10, 5, accent);
}

void drawColumnResizeArrows(std::uint32_t *pixels, std::uint32_t accent, bool widen)
{
	drawIconHorizontalLine(pixels, 2, 6, 4, accent);
	drawIconHorizontalLine(pixels, 9, 13, 4, accent);
	if (widen)
	{
		setIconPixel(pixels, 3, 3, accent);
		setIconPixel(pixels, 3, 5, accent);
		setIconPixel(pixels, 12, 3, accent);
		setIconPixel(pixels, 12, 5, accent);
	}
	else
	{
		setIconPixel(pixels, 5, 3, accent);
		setIconPixel(pixels, 5, 5, accent);
		setIconPixel(pixels, 10, 3, accent);
		setIconPixel(pixels, 10, 5, accent);
	}
}

void drawIconPlus(std::uint32_t *pixels, int x, int y, std::uint32_t color)
{
	drawIconHorizontalLine(pixels, x - 2, x + 2, y, color);
	drawIconVerticalLine(pixels, x, y - 2, y + 2, color);
}

void drawIconMinus(std::uint32_t *pixels, int x, int y, std::uint32_t color)
{
	drawIconHorizontalLine(pixels, x - 2, x + 2, y, color);
}

void drawArrowRight(std::uint32_t *pixels, int x1, int x2, int y, std::uint32_t color)
{
	drawIconHorizontalLine(pixels, x1, x2, y, color);
	setIconPixel(pixels, x2 - 1, y - 1, color);
	setIconPixel(pixels, x2 - 2, y - 2, color);
	setIconPixel(pixels, x2 - 1, y + 1, color);
	setIconPixel(pixels, x2 - 2, y + 2, color);
}

void drawArrowLeft(std::uint32_t *pixels, int x1, int x2, int y, std::uint32_t color)
{
	drawIconHorizontalLine(pixels, x1, x2, y, color);
	setIconPixel(pixels, x1 + 1, y - 1, color);
	setIconPixel(pixels, x1 + 2, y - 2, color);
	setIconPixel(pixels, x1 + 1, y + 1, color);
	setIconPixel(pixels, x1 + 2, y + 2, color);
}

void drawArrowDown(std::uint32_t *pixels, int x, int y1, int y2, std::uint32_t color)
{
	drawIconVerticalLine(pixels, x, y1, y2, color);
	setIconPixel(pixels, x - 1, y2 - 1, color);
	setIconPixel(pixels, x - 2, y2 - 2, color);
	setIconPixel(pixels, x + 1, y2 - 1, color);
	setIconPixel(pixels, x + 2, y2 - 2, color);
}

void drawArrowUp(std::uint32_t *pixels, int x, int y1, int y2, std::uint32_t color)
{
	drawIconVerticalLine(pixels, x, y1, y2, color);
	setIconPixel(pixels, x - 1, y1 + 1, color);
	setIconPixel(pixels, x - 2, y1 + 2, color);
	setIconPixel(pixels, x + 1, y1 + 1, color);
	setIconPixel(pixels, x + 2, y1 + 2, color);
}

void drawHighlightedRow(std::uint32_t *pixels, std::uint32_t color)
{
	drawIconHorizontalLine(pixels, 2, 13, 11, color);
	drawIconHorizontalLine(pixels, 2, 13, 12, color);
}

void drawHighlightedColumn(std::uint32_t *pixels, std::uint32_t color)
{
	drawIconVerticalLine(pixels, 7, 9, 13, color);
	drawIconVerticalLine(pixels, 8, 9, 13, color);
}

void drawSortBars(std::uint32_t *pixels, std::uint32_t color, bool descending)
{
	const int widths[3] = { 4, 6, 8 };
	for (int row = 0; row < 3; ++row)
	{
		const int width = descending ? widths[2 - row] : widths[row];
		const int y = 3 + row * 3;
		drawIconHorizontalLine(pixels, 2, 2 + width, y, color);
		drawIconHorizontalLine(pixels, 2, 2 + width, y + 1, color);
	}
	if (descending)
		drawArrowDown(pixels, 13, 2, 11, color);
	else
		drawArrowUp(pixels, 13, 2, 11, color);
}

void drawDelimitedDots(std::uint32_t *pixels, std::uint32_t color)
{
	for (int y = 2; y <= 5; y += 3)
	{
		setIconPixel(pixels, 2, y, color);
		setIconPixel(pixels, 5, y, color);
		setIconPixel(pixels, 8, y, color);
	}
	drawArrowRight(pixels, 9, 13, 4, color);
}

void drawMarkdownToolbarIcon(std::uint32_t *pixels, bool darkMode, ToolbarIconKind kind)
{
	const std::uint32_t grid = darkMode ? argb(255, 248, 250, 252) : argb(255, 12, 18, 28);
	const std::uint32_t muted = darkMode ? argb(255, 190, 202, 216) : argb(255, 74, 85, 104);
	const std::uint32_t fitAccent = darkMode ? argb(255, 255, 82, 82) : argb(255, 232, 0, 32);
	const std::uint32_t alignAccent = darkMode ? argb(255, 64, 176, 255) : argb(255, 0, 88, 255);
	const std::uint32_t editAccent = darkMode ? argb(255, 74, 222, 128) : argb(255, 22, 163, 74);

	clearIcon(pixels);
	if (kind == ToolbarIconKind::TableFitWidth || kind == ToolbarIconKind::TableAutoFitWidth)
	{
		drawMdBadge(pixels, grid);
		drawMiniTable(pixels, grid, muted);
		drawFitWidthArrows(pixels, fitAccent);
		if (kind == ToolbarIconKind::TableAutoFitWidth)
			drawAutoCorner(pixels, alignAccent);
	}
	else if (kind == ToolbarIconKind::TableAlign || kind == ToolbarIconKind::TableAutoAlign)
	{
		drawMdBadge(pixels, grid);
		drawMiniTable(pixels, grid, muted);
		drawAlignRows(pixels, alignAccent);
		if (kind == ToolbarIconKind::TableAutoAlign)
			drawAutoCorner(pixels, fitAccent);
	}
	else if (kind == ToolbarIconKind::TableNextCell || kind == ToolbarIconKind::TablePreviousCell)
	{
		drawMdBadge(pixels, grid);
		drawMiniTable(pixels, grid, muted);
		if (kind == ToolbarIconKind::TableNextCell)
			drawArrowRight(pixels, 3, 12, 4, editAccent);
		else
			drawArrowLeft(pixels, 3, 12, 4, editAccent);
	}
	else if (kind == ToolbarIconKind::TableInsertRow || kind == ToolbarIconKind::TableDeleteRow)
	{
		drawMdBadge(pixels, grid);
		drawMiniTable(pixels, grid, muted);
		drawHighlightedRow(pixels, editAccent);
		if (kind == ToolbarIconKind::TableInsertRow)
			drawIconPlus(pixels, 12, 4, editAccent);
		else
			drawIconMinus(pixels, 12, 4, fitAccent);
	}
	else if (kind == ToolbarIconKind::TableInsertColumn || kind == ToolbarIconKind::TableDeleteColumn)
	{
		drawMdBadge(pixels, grid);
		drawMiniTable(pixels, grid, muted);
		drawHighlightedColumn(pixels, editAccent);
		if (kind == ToolbarIconKind::TableInsertColumn)
			drawIconPlus(pixels, 12, 4, editAccent);
		else
			drawIconMinus(pixels, 12, 4, fitAccent);
	}
	else if (kind == ToolbarIconKind::TableNarrowColumn || kind == ToolbarIconKind::TableWidenColumn)
	{
		drawMdBadge(pixels, grid);
		drawMiniTable(pixels, grid, muted);
		drawColumnResizeArrows(pixels, fitAccent, kind == ToolbarIconKind::TableWidenColumn);
	}
	else if (kind == ToolbarIconKind::TableMoveRowUp || kind == ToolbarIconKind::TableMoveRowDown)
	{
		drawMdBadge(pixels, grid);
		drawMiniTable(pixels, grid, muted);
		drawHighlightedRow(pixels, muted);
		if (kind == ToolbarIconKind::TableMoveRowUp)
			drawArrowUp(pixels, 12, 2, 6, editAccent);
		else
			drawArrowDown(pixels, 12, 2, 6, editAccent);
	}
	else if (kind == ToolbarIconKind::TableMoveColumnLeft || kind == ToolbarIconKind::TableMoveColumnRight)
	{
		drawMdBadge(pixels, grid);
		drawMiniTable(pixels, grid, muted);
		drawHighlightedColumn(pixels, muted);
		if (kind == ToolbarIconKind::TableMoveColumnLeft)
			drawArrowLeft(pixels, 3, 12, 4, editAccent);
		else
			drawArrowRight(pixels, 3, 12, 4, editAccent);
	}
	else if (kind == ToolbarIconKind::TableSortAscending || kind == ToolbarIconKind::TableSortDescending)
	{
		drawSortBars(pixels, alignAccent, kind == ToolbarIconKind::TableSortDescending);
	}
	else if (kind == ToolbarIconKind::TableConvertDelimited)
	{
		drawDelimitedDots(pixels, alignAccent);
		drawMiniTable(pixels, grid, muted);
	}
	else if (kind == ToolbarIconKind::TableInsertTable)
	{
		drawMdBadge(pixels, grid);
		drawMiniTable(pixels, grid, muted);
		drawIconPlus(pixels, 12, 4, editAccent);
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

ToolbarIconKind toolbarIconKindForCommand(std::size_t commandIndex)
{
	switch (commandIndex)
	{
	case alignCommandIndex: return ToolbarIconKind::TableAlign;
	case autoAlignTableCommandIndex: return ToolbarIconKind::TableAutoAlign;
	case wrapLongCellsCommandIndex: return ToolbarIconKind::TableFitWidth;
	case autoFitTableCommandIndex: return ToolbarIconKind::TableAutoFitWidth;
	case nextCellCommandIndex: return ToolbarIconKind::TableNextCell;
	case previousCellCommandIndex: return ToolbarIconKind::TablePreviousCell;
	case insertRowCommandIndex: return ToolbarIconKind::TableInsertRow;
	case deleteRowCommandIndex: return ToolbarIconKind::TableDeleteRow;
	case insertColumnCommandIndex: return ToolbarIconKind::TableInsertColumn;
	case deleteColumnCommandIndex: return ToolbarIconKind::TableDeleteColumn;
	case narrowColumnCommandIndex: return ToolbarIconKind::TableNarrowColumn;
	case widenColumnCommandIndex: return ToolbarIconKind::TableWidenColumn;
	case moveRowUpCommandIndex: return ToolbarIconKind::TableMoveRowUp;
	case moveRowDownCommandIndex: return ToolbarIconKind::TableMoveRowDown;
	case moveColumnLeftCommandIndex: return ToolbarIconKind::TableMoveColumnLeft;
	case moveColumnRightCommandIndex: return ToolbarIconKind::TableMoveColumnRight;
	case sortRowsAscendingCommandIndex: return ToolbarIconKind::TableSortAscending;
	case sortRowsDescendingCommandIndex: return ToolbarIconKind::TableSortDescending;
	case convertCsvTsvCommandIndex: return ToolbarIconKind::TableConvertDelimited;
	case insertTableCommandIndex: return ToolbarIconKind::TableInsertTable;
	default: return ToolbarIconKind::TableAlign;
	}
}

bool commandHasExplicitToolbarIconKind(std::size_t commandIndex)
{
	switch (commandIndex)
	{
	case alignCommandIndex:
	case autoAlignTableCommandIndex:
	case wrapLongCellsCommandIndex:
	case autoFitTableCommandIndex:
	case nextCellCommandIndex:
	case previousCellCommandIndex:
	case insertRowCommandIndex:
	case deleteRowCommandIndex:
	case insertColumnCommandIndex:
	case deleteColumnCommandIndex:
	case narrowColumnCommandIndex:
	case widenColumnCommandIndex:
	case moveRowUpCommandIndex:
	case moveRowDownCommandIndex:
	case moveColumnLeftCommandIndex:
	case moveColumnRightCommandIndex:
	case sortRowsAscendingCommandIndex:
	case sortRowsDescendingCommandIndex:
	case convertCsvTsvCommandIndex:
	case insertTableCommandIndex:
		return true;
	default:
		return false;
	}
}

bool ensureCommandToolbarIconHandles(std::size_t commandIndex)
{
	if (commandIndex >= static_cast<std::size_t>(nbFunc))
		return false;
	if (!commandHasExplicitToolbarIconKind(commandIndex))
		return false;

	return ensureToolbarIconHandles(
		g_toolbarBmps[commandIndex],
		g_toolbarIcons[commandIndex],
		g_toolbarIconDarkModes[commandIndex],
		toolbarIconKindForCommand(commandIndex));
}

void destroyCommandToolbarIconHandles(std::size_t commandIndex)
{
	if (commandIndex >= static_cast<std::size_t>(nbFunc))
		return;

	destroyToolbarIconHandles(
		g_toolbarBmps[commandIndex],
		g_toolbarIcons[commandIndex],
		g_toolbarIconDarkModes[commandIndex]);
}

void destroyAllToolbarIconHandles()
{
	for (std::size_t commandIndex = 0; commandIndex < static_cast<std::size_t>(nbFunc); ++commandIndex)
		destroyCommandToolbarIconHandles(commandIndex);
}

bool ensureAlignToolbarIconHandles()
{
	return ensureCommandToolbarIconHandles(alignCommandIndex);
}

bool ensureWrapLongCellsToolbarIconHandles()
{
	return ensureCommandToolbarIconHandles(wrapLongCellsCommandIndex);
}

bool ensureAutoFitTableToolbarIconHandles()
{
	return ensureCommandToolbarIconHandles(autoFitTableCommandIndex);
}

bool ensureAutoAlignTableToolbarIconHandles()
{
	return ensureCommandToolbarIconHandles(autoAlignTableCommandIndex);
}

bool ensureNarrowColumnToolbarIconHandles()
{
	return ensureCommandToolbarIconHandles(narrowColumnCommandIndex);
}

bool ensureWidenColumnToolbarIconHandles()
{
	return ensureCommandToolbarIconHandles(widenColumnCommandIndex);
}

void destroyAlignToolbarIconHandles()
{
	destroyCommandToolbarIconHandles(alignCommandIndex);
}

void destroyWrapLongCellsToolbarIconHandles()
{
	destroyCommandToolbarIconHandles(wrapLongCellsCommandIndex);
}

void destroyAutoFitTableToolbarIconHandles()
{
	destroyCommandToolbarIconHandles(autoFitTableCommandIndex);
}

void destroyAutoAlignTableToolbarIconHandles()
{
	destroyCommandToolbarIconHandles(autoAlignTableCommandIndex);
}

void destroyNarrowColumnToolbarIconHandles()
{
	destroyCommandToolbarIconHandles(narrowColumnCommandIndex);
}

void destroyWidenColumnToolbarIconHandles()
{
	destroyCommandToolbarIconHandles(widenColumnCommandIndex);
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

void setAutoFitTableToolbarCheckState()
{
	setToolbarCheckState(autoFitTableCommandIndex, g_autoFitTable);
}

void setAutoAlignTableToolbarCheckState()
{
	setToolbarCheckState(autoAlignTableCommandIndex, g_autoAlignTable);
}

bool shouldApplyAutoFitAfterAction(MarkdownTable::Action action)
{
	(void)action;
	return false;
}

MarkdownTable::Action coreActionForPluginAction(MarkdownTable::Action action)
{
	return action == MarkdownTable::Action::WrapLongCells
		? MarkdownTable::Action::Align
		: action;
}

bool shouldFitToWindowAfterAction(MarkdownTable::Action action)
{
	return action == MarkdownTable::Action::WrapLongCells;
}

bool fitTableToWindowCommandEnabled()
{
	return !g_autoFitTable;
}

bool alignTableCommandEnabled()
{
	return !g_autoAlignTable;
}

bool autoAlignTableToggleEnabled()
{
	return !g_autoFitTable;
}

bool shouldRunFitToWindowAfterResize(bool enabled, bool inProgress, bool activeEditor, std::size_t previousColumns, std::size_t currentColumns)
{
	return enabled && !inProgress && activeEditor && currentColumns != 0 && (previousColumns == 0 || previousColumns != currentColumns);
}

bool shouldRunInitialFitWhenTogglingAutoFitTable(bool currentlyEnabled)
{
	return !currentlyEnabled;
}

bool shouldRunAutoTableFormatAfterUpdate(bool autoAlignEnabled, bool autoFitEnabled, bool alignInProgress, bool fitInProgress, bool pluginEditInProgress, bool activeEditor, bool contentUpdated)
{
	return (autoAlignEnabled || autoFitEnabled) && !alignInProgress && !fitInProgress && !pluginEditInProgress && activeEditor && contentUpdated;
}

bool scintillaModificationShouldRunAutoTableFormat(int modificationType)
{
	return (modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT | SC_PERFORMED_UNDO | SC_PERFORMED_REDO)) != 0;
}

bool shouldRunAutoFitAfterZoom(bool autoFitEnabled, bool fitInProgress, bool activeEditor)
{
	(void)autoFitEnabled;
	(void)fitInProgress;
	(void)activeEditor;
	return false;
}

bool shouldScheduleFitToWindowAfterZoom(bool autoFitEnabled, bool fitInProgress, bool activeEditor)
{
	return autoFitEnabled && !fitInProgress && activeEditor;
}

bool shouldRunAutoTableFormatAfterGlobalModified(bool autoAlignEnabled, bool autoFitEnabled, bool alignInProgress, bool fitInProgress, bool pluginEditInProgress)
{
	return (autoAlignEnabled || autoFitEnabled) && !alignInProgress && !fitInProgress && !pluginEditInProgress;
}

bool shouldRunInitialAlignWhenTogglingAutoAlignTable(bool currentlyEnabled)
{
	return !currentlyEnabled;
}

bool shouldRunInitialAutoTableFormatForBuffer(bool autoAlignEnabled, bool autoFitEnabled, bool alignInProgress, bool fitInProgress, bool activeEditor, bool alreadyHandled)
{
	return (autoAlignEnabled || autoFitEnabled) && !alignInProgress && !fitInProgress && activeEditor && !alreadyHandled;
}

bool shouldQueueInitialAutoTableFormatForOpenedBuffer(bool autoAlignEnabled, bool autoFitEnabled, bool alreadyHandled)
{
	return (autoAlignEnabled || autoFitEnabled) && !alreadyHandled;
}

bool shouldTryQueuedInitialAutoTableFormatForNotification(UINT notificationCode)
{
	return notificationCode != NPPN_FILEOPENED;
}

bool shouldDeferInitialAutoTableFormatForDocumentLength(LRESULT textLength)
{
	return textLength <= 0;
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
bool runTableAction(MarkdownTable::Action action, bool quiet, CaretPlacement caretPlacement = CaretPlacement::ActionTarget);
bool runAutoTableFormatForDocument(MarkdownTable::Action action);

MarkdownTable::EditResult applyWrappedToWidthUntilStable(const std::vector<std::string> &lines, int row, int column, std::size_t maxTableWidth)
{
	MarkdownTable::EditResult current = MarkdownTable::applyWrappedToWidth(lines, row, column, maxTableWidth);
	if (!current.ok)
		return current;

	for (std::size_t pass = 1; pass < wrapStabilizationPassLimit; ++pass)
	{
		MarkdownTable::EditResult next = MarkdownTable::applyWrappedToWidth(
			current.lines,
			coreIndex(current.targetRow),
			coreIndex(current.targetColumn),
			maxTableWidth);
		if (!next.ok)
			break;

		const bool stable = next.lines == current.lines;
		current = next;
		if (stable)
			break;
	}

	return current;
}

MarkdownTable::EditResult applyWrappedToVisibleWidth(HWND scintilla, const MarkdownTable::EditResult &edit)
{
	std::size_t columns = availableDisplayColumns(scintilla);
	MarkdownTable::EditResult best;
	for (std::size_t attempt = 0; attempt < 32; ++attempt)
	{
		MarkdownTable::EditResult wrapped = applyWrappedToWidthUntilStable(
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

bool fitCurrentTableToWindow(bool quiet, CaretPlacement caretPlacement = CaretPlacement::ActionTarget)
{
	if (g_fitToWindowInProgress)
		return false;

	g_fitToWindowInProgress = true;
	const bool changed = runTableAction(MarkdownTable::Action::WrapLongCells, quiet, caretPlacement);
	g_fitToWindowInProgress = false;
	return changed;
}

bool autoAlignCurrentTable(bool quiet, CaretPlacement caretPlacement = CaretPlacement::ActionTarget)
{
	if (g_autoAlignInProgress || g_fitToWindowInProgress)
		return false;

	g_autoAlignInProgress = true;
	const bool changed = runTableAction(MarkdownTable::Action::Align, quiet, caretPlacement);
	g_autoAlignInProgress = false;
	return changed;
}

bool autoFormatCurrentDocumentAfterGlobalEdit()
{
	const bool pluginEditAutoFormatBlocked = consumePluginEditAutoFormatSkip(g_pluginEditGlobalAutoFormatSkips);
	if (!shouldRunAutoTableFormatAfterGlobalModified(
		g_autoAlignTable,
		g_autoFitTable,
		g_autoAlignInProgress,
		g_fitToWindowInProgress,
		pluginEditAutoFormatBlocked))
		return false;

	if (g_autoFitTable)
	{
		g_fitToWindowInProgress = true;
		const bool changed = runAutoTableFormatForDocument(MarkdownTable::Action::WrapLongCells);
		g_fitToWindowInProgress = false;
		rememberCurrentFitToWindowWidth();
		return changed;
	}

	g_autoAlignInProgress = true;
	const bool changed = runAutoTableFormatForDocument(MarkdownTable::Action::Align);
	g_autoAlignInProgress = false;
	return changed;
}

void fitToWindowAfterResize(HWND resizedScintilla)
{
	if (!g_autoFitTable || g_fitToWindowInProgress)
		return;

	HWND scintilla = currentScintilla();
	if (!scintilla || scintilla != resizedScintilla)
		return;
	if (consumePluginEditAutoFormatSkip(g_pluginEditResizeAutoFitSkips))
		return;

	const std::size_t columns = availableDisplayColumns(scintilla);
	const std::size_t previousColumns = g_lastFitToWindowColumns;
	g_lastFitToWindowColumns = columns;
	if (!shouldRunFitToWindowAfterResize(g_autoFitTable, g_fitToWindowInProgress, true, previousColumns, columns))
		return;

	fitCurrentTableToWindow(true);
}

bool selectionEmpty(HWND scintilla)
{
	if (!scintilla)
		return false;

	const LRESULT start = ::SendMessage(scintilla, SCI_GETSELECTIONSTART, 0, 0);
	const LRESULT end = ::SendMessage(scintilla, SCI_GETSELECTIONEND, 0, 0);
	return start == end;
}

void scheduleFitToWindowAfterResize(HWND resizedScintilla)
{
	if (!g_autoFitTable || !resizedScintilla)
		return;

	g_pendingFitToWindowScintilla = resizedScintilla;
	::SetTimer(resizedScintilla, fitToWindowResizeTimerId, fitToWindowResizeDelayMs, NULL);
}

bool shouldScheduleFitToWindowAfterResizeMessage(UINT message, WPARAM wParam, UINT windowPosFlags)
{
	if (message == WM_SIZE)
		return wParam != SIZE_MINIMIZED;
	if (message == WM_WINDOWPOSCHANGED)
		return (windowPosFlags & SWP_NOSIZE) == 0;
	if (message == WM_EXITSIZEMOVE || message == WM_DPICHANGED)
		return true;
	return false;
}

bool shouldScheduleFitToWindowAfterResizeMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	UINT windowPosFlags = 0;
	if (message == WM_WINDOWPOSCHANGED && lParam)
	{
		const WINDOWPOS *windowPos = reinterpret_cast<const WINDOWPOS *>(lParam);
		windowPosFlags = windowPos->flags;
	}
	return shouldScheduleFitToWindowAfterResizeMessage(message, wParam, windowPosFlags);
}

void runAutoTableFormatAfterUpdate(HWND updatedScintilla, bool contentUpdated)
{
	HWND scintilla = currentScintilla();
	const bool activeEditor = scintilla && scintilla == updatedScintilla;
	const bool canAutoFormat =
		(g_autoAlignTable || g_autoFitTable) &&
		!g_autoAlignInProgress &&
		!g_fitToWindowInProgress &&
		activeEditor &&
		contentUpdated;
	const bool pluginEditAutoFormatBlocked = canAutoFormat && consumePluginEditAutoFormatSkip(g_pluginEditUpdateAutoFormatSkips);
	if (!shouldRunAutoTableFormatAfterUpdate(
		g_autoAlignTable,
		g_autoFitTable,
		g_autoAlignInProgress,
		g_fitToWindowInProgress,
		pluginEditAutoFormatBlocked,
		activeEditor,
		contentUpdated))
		return;
	if (!selectionEmpty(scintilla))
		return;

	if (g_autoFitTable)
		fitCurrentTableToWindow(true, CaretPlacement::PreserveCellOffset);
	else
		autoAlignCurrentTable(true, CaretPlacement::PreserveCellOffset);
}

bool initialAutoFormatBufferHandled(uptr_t bufferId)
{
	return std::find(g_initialAutoFormatBuffers.begin(), g_initialAutoFormatBuffers.end(), bufferId) != g_initialAutoFormatBuffers.end();
}

bool initialAutoFormatBufferPending(uptr_t bufferId)
{
	return std::find(g_pendingInitialAutoFormatBuffers.begin(), g_pendingInitialAutoFormatBuffers.end(), bufferId) != g_pendingInitialAutoFormatBuffers.end();
}

void markInitialAutoFormatBufferHandled(uptr_t bufferId)
{
	if (!initialAutoFormatBufferHandled(bufferId))
		g_initialAutoFormatBuffers.push_back(bufferId);
}

void queueInitialAutoFormatBuffer(uptr_t bufferId)
{
	if (!initialAutoFormatBufferPending(bufferId))
		g_pendingInitialAutoFormatBuffers.push_back(bufferId);
}

void removeInitialAutoFormatBuffer(uptr_t bufferId)
{
	g_initialAutoFormatBuffers.erase(std::remove(g_initialAutoFormatBuffers.begin(), g_initialAutoFormatBuffers.end(), bufferId), g_initialAutoFormatBuffers.end());
	g_pendingInitialAutoFormatBuffers.erase(std::remove(g_pendingInitialAutoFormatBuffers.begin(), g_pendingInitialAutoFormatBuffers.end(), bufferId), g_pendingInitialAutoFormatBuffers.end());
}

void removePendingInitialAutoFormatBuffer(uptr_t bufferId)
{
	g_pendingInitialAutoFormatBuffers.erase(std::remove(g_pendingInitialAutoFormatBuffers.begin(), g_pendingInitialAutoFormatBuffers.end(), bufferId), g_pendingInitialAutoFormatBuffers.end());
}

uptr_t currentBufferId()
{
	if (!nppData._nppHandle)
		return 0;
	return static_cast<uptr_t>(::SendMessage(nppData._nppHandle, NPPM_GETCURRENTBUFFERID, 0, 0));
}

void runInitialAutoTableFormatForBuffer(const SCNotification *notification)
{
	const uptr_t notifiedBufferId = notification ? notification->nmhdr.idFrom : 0;
	const uptr_t activeBufferId = currentBufferId();
	const uptr_t bufferId = notifiedBufferId != 0 ? notifiedBufferId : activeBufferId;
	if (bufferId == 0)
		return;

	const bool alreadyHandled = initialAutoFormatBufferHandled(bufferId);
	if (notification && (notification->nmhdr.code == NPPN_FILEOPENED || notification->nmhdr.code == NPPN_BUFFERACTIVATED))
	{
		if (shouldQueueInitialAutoTableFormatForOpenedBuffer(g_autoAlignTable, g_autoFitTable, alreadyHandled))
			queueInitialAutoFormatBuffer(bufferId);
	}
	if (notification && !shouldTryQueuedInitialAutoTableFormatForNotification(notification->nmhdr.code))
		return;
	if (!initialAutoFormatBufferPending(bufferId))
		return;

	HWND scintilla = currentScintilla();
	const bool activeEditor = scintilla != NULL && activeBufferId == bufferId;
	const bool shouldRun = shouldRunInitialAutoTableFormatForBuffer(
		g_autoAlignTable,
		g_autoFitTable,
		g_autoAlignInProgress,
		g_fitToWindowInProgress,
		activeEditor,
		alreadyHandled);
	if (!shouldRun)
		return;
	const LRESULT textLength = scintilla ? ::SendMessage(scintilla, SCI_GETLENGTH, 0, 0) : 0;
	if (shouldDeferInitialAutoTableFormatForDocumentLength(textLength))
		return;
	if (!selectionEmpty(scintilla))
		return;
	removePendingInitialAutoFormatBuffer(bufferId);
	markInitialAutoFormatBufferHandled(bufferId);

	if (g_autoFitTable)
	{
		g_fitToWindowInProgress = true;
		runAutoTableFormatForDocument(MarkdownTable::Action::WrapLongCells);
		g_fitToWindowInProgress = false;
		rememberCurrentFitToWindowWidth();
	}
	else
	{
		g_autoAlignInProgress = true;
		runAutoTableFormatForDocument(MarkdownTable::Action::Align);
		g_autoAlignInProgress = false;
	}
}

void runPendingInitialAutoTableFormatForCurrentBuffer()
{
	const uptr_t bufferId = currentBufferId();
	if (bufferId == 0 || !initialAutoFormatBufferPending(bufferId))
		return;
	runInitialAutoTableFormatForBuffer(NULL);
}

void runInitialAutoTableFormatForCurrentBuffer()
{
	const uptr_t bufferId = currentBufferId();
	if (bufferId == 0)
		return;

	const bool alreadyHandled = initialAutoFormatBufferHandled(bufferId);
	if (shouldQueueInitialAutoTableFormatForOpenedBuffer(g_autoAlignTable, g_autoFitTable, alreadyHandled))
		queueInitialAutoFormatBuffer(bufferId);
	runInitialAutoTableFormatForBuffer(NULL);
}

void handleScintillaUpdateUiInternal(const SCNotification *notification)
{
	if (!notification)
		return;

	const bool contentUpdated = (notification->updated & SC_UPDATE_CONTENT) != 0;
	if (contentUpdated)
		runPendingInitialAutoTableFormatForCurrentBuffer();
	runAutoTableFormatAfterUpdate(reinterpret_cast<HWND>(notification->nmhdr.hwndFrom), contentUpdated);
	checkWordWrapAutoFitWarningInternal();
}

void handleScintillaModifiedInternal(const SCNotification *notification)
{
	if (!notification || !scintillaModificationShouldRunAutoTableFormat(notification->modificationType))
		return;

	runAutoTableFormatAfterUpdate(reinterpret_cast<HWND>(notification->nmhdr.hwndFrom), true);
}

void handleScintillaZoomInternal(const SCNotification *notification)
{
	HWND scintilla = currentScintilla();
	const bool activeEditor = notification && scintilla && scintilla == reinterpret_cast<HWND>(notification->nmhdr.hwndFrom);
	if (!shouldScheduleFitToWindowAfterZoom(g_autoFitTable, g_fitToWindowInProgress, activeEditor))
		return;
	if (!selectionEmpty(scintilla))
		return;

	scheduleFitToWindowAfterResize(scintilla);
}

void handleGlobalModifiedInternal()
{
	autoFormatCurrentDocumentAfterGlobalEdit();
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

	std::size_t preservedEnterColumn = 0;
	const bool preserveEnterColumn = captureEnterColumnToPreserve(hwnd, message, wParam, preservedEnterColumn);
	const LRESULT result = ::CallWindowProc(original, hwnd, message, wParam, lParam);
	if (preserveEnterColumn)
		restoreEnterColumn(hwnd, preservedEnterColumn);
	if (shouldScheduleFitToWindowAfterResizeMessage(message, wParam, lParam))
		scheduleFitToWindowAfterResize(hwnd);
	return result;
}

LRESULT CALLBACK fitToWindowNppProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	WNDPROC original = g_originalNppProc;
	if (!original)
		return ::DefWindowProc(hwnd, message, wParam, lParam);

	const LRESULT result = ::CallWindowProc(original, hwnd, message, wParam, lParam);
	if (shouldScheduleFitToWindowAfterResizeMessage(message, wParam, lParam))
		scheduleFitToWindowAfterResize(currentScintilla());
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

void subclassNppWindow()
{
	if (!nppData._nppHandle || g_originalNppProc)
		return;

	g_originalNppProc = reinterpret_cast<WNDPROC>(::SetWindowLongPtr(
		nppData._nppHandle,
		GWLP_WNDPROC,
		reinterpret_cast<LONG_PTR>(fitToWindowNppProc)));
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

void removeNppSubclass()
{
	if (!nppData._nppHandle || !g_originalNppProc)
	{
		g_originalNppProc = NULL;
		return;
	}

	if (reinterpret_cast<WNDPROC>(::GetWindowLongPtr(nppData._nppHandle, GWLP_WNDPROC)) == fitToWindowNppProc)
	{
		::SetWindowLongPtr(nppData._nppHandle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_originalNppProc));
	}
	g_originalNppProc = NULL;
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

bool shouldPreserveEnterColumn(bool activeEditor, bool emptySelection, bool tableLine, UINT message, WPARAM key)
{
	return activeEditor && emptySelection && tableLine && message == WM_KEYDOWN && key == VK_RETURN;
}

bool captureEnterColumnToPreserve(HWND hwnd, UINT message, WPARAM key, std::size_t &column)
{
	if (message != WM_KEYDOWN || key != VK_RETURN)
		return false;
	if (!hwnd || hwnd != currentScintilla() || !selectionEmpty(hwnd))
		return false;

	const LRESULT currentPos = ::SendMessage(hwnd, SCI_GETCURRENTPOS, 0, 0);
	if (currentPos < 0)
		return false;

	const LRESULT line = ::SendMessage(hwnd, SCI_LINEFROMPOSITION, static_cast<WPARAM>(currentPos), 0);
	if (line < 0)
		return false;

	const bool tableLine = MarkdownTable::isPotentialTableLine(getLineText(hwnd, static_cast<std::size_t>(line)));
	if (!shouldPreserveEnterColumn(true, true, tableLine, message, key))
		return false;

	const LRESULT currentColumn = ::SendMessage(hwnd, SCI_GETCOLUMN, static_cast<WPARAM>(currentPos), 0);
	if (currentColumn < 0)
		return false;

	column = static_cast<std::size_t>(currentColumn);
	return true;
}

void restoreEnterColumn(HWND hwnd, std::size_t column)
{
	if (!hwnd)
		return;

	const LRESULT currentPos = ::SendMessage(hwnd, SCI_GETCURRENTPOS, 0, 0);
	if (currentPos < 0)
		return;

	const LRESULT line = ::SendMessage(hwnd, SCI_LINEFROMPOSITION, static_cast<WPARAM>(currentPos), 0);
	if (line < 0)
		return;

	const LRESULT target = ::SendMessage(hwnd, SCI_FINDCOLUMN, static_cast<WPARAM>(line), static_cast<LPARAM>(column));
	if (target >= 0 && target != currentPos)
		::SendMessage(hwnd, SCI_GOTOPOS, static_cast<WPARAM>(target), 0);
}

std::string getSelectedText(HWND scintilla)
{
	const LRESULT lengthResult = ::SendMessage(scintilla, SCI_GETSELTEXT, 0, 0);
	if (lengthResult <= 1)
		return std::string();

	const std::size_t length = static_cast<std::size_t>(lengthResult);
	std::vector<char> buffer(length + 1, '\0');
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

std::vector<std::string> readDocumentLines(HWND scintilla, std::size_t lineCount)
{
	std::vector<std::string> lines;
	lines.reserve(lineCount);
	for (std::size_t i = 0; i < lineCount; ++i)
		lines.push_back(getLineText(scintilla, i));
	return lines;
}

std::vector<DocumentTableRange> findDocumentTableRanges(const std::vector<std::string> &lines)
{
	std::vector<DocumentTableRange> ranges;
	std::size_t line = 0;
	while (line < lines.size())
	{
		if (!MarkdownTable::isPotentialTableLine(lines[line]))
		{
			++line;
			continue;
		}

		const std::size_t blockStart = line;
		std::vector<std::string> blockLines;
		while (line < lines.size() && MarkdownTable::isPotentialTableLine(lines[line]))
		{
			blockLines.push_back(lines[line]);
			++line;
		}

		std::size_t searchRow = 0;
		while (searchRow < blockLines.size())
		{
			bool found = false;
			for (std::size_t targetRow = searchRow; targetRow < blockLines.size(); ++targetRow)
			{
				const MarkdownTable::TableRange tableRange = MarkdownTable::findTableRange(blockLines, coreIndex(targetRow));
				if (!tableRange.found || tableRange.lastRow < searchRow)
					continue;

				DocumentTableRange range;
				range.firstLine = blockStart + tableRange.firstRow;
				range.lastLine = blockStart + tableRange.lastRow;
				if (ranges.empty() || range.firstLine > ranges.back().lastLine)
					ranges.push_back(range);
				searchRow = tableRange.lastRow + 1;
				found = true;
				break;
			}

			if (!found)
				break;
		}
	}
	return ranges;
}

bool collectDocumentTableReplacement(HWND scintilla, const std::vector<std::string> &documentLines, std::size_t lineCount, MarkdownTable::Action action, const DocumentTableRange &range, DocumentTableReplacement &replacement)
{
	if (range.firstLine >= documentLines.size() || range.lastLine >= documentLines.size() || range.firstLine > range.lastLine)
		return false;

	std::vector<std::string> tableLines;
	for (std::size_t i = range.firstLine; i <= range.lastLine; ++i)
		tableLines.push_back(documentLines[i]);

	MarkdownTable::EditResult edit = MarkdownTable::apply(tableLines, 0, 0, coreActionForPluginAction(action));
	if (!edit.ok)
		return false;
	if (shouldFitToWindowAfterAction(action))
		edit = applyWrappedToVisibleWidth(scintilla, edit);
	if (!edit.ok)
		return false;

	const std::string eol = chooseEol(scintilla, range.firstLine, range.lastLine, lineCount);
	const std::string source = joinLines(tableLines, eol);
	const std::string target = joinLines(edit.lines, eol);
	if (source == target)
		return false;

	replacement.firstLine = range.firstLine;
	replacement.lastLine = range.lastLine;
	replacement.text = target;
	return true;
}

bool runAutoTableFormatForDocument(MarkdownTable::Action action)
{
	HWND scintilla = currentScintilla();
	if (!scintilla)
		return false;

	const LRESULT lineCountResult = ::SendMessage(scintilla, SCI_GETLINECOUNT, 0, 0);
	if (lineCountResult <= 0)
		return false;
	const std::size_t lineCount = static_cast<std::size_t>(lineCountResult);
	const std::vector<std::string> documentLines = readDocumentLines(scintilla, lineCount);
	const std::vector<DocumentTableRange> ranges = findDocumentTableRanges(documentLines);
	if (ranges.empty())
		return false;

	std::vector<DocumentTableReplacement> replacements;
	for (std::size_t i = 0; i < ranges.size(); ++i)
	{
		DocumentTableReplacement replacement;
		if (collectDocumentTableReplacement(scintilla, documentLines, lineCount, action, ranges[i], replacement))
			replacements.push_back(replacement);
	}
	if (replacements.empty())
		return false;

	const LRESULT currentPosResult = ::SendMessage(scintilla, SCI_GETCURRENTPOS, 0, 0);
	ScintillaUndoAction undo(scintilla);
	PluginEditGuard pluginEdit;
	for (std::size_t i = replacements.size(); i > 0; --i)
	{
		const DocumentTableReplacement &replacement = replacements[i - 1];
		const Sci_Position replaceStart = lineStartPosition(scintilla, replacement.firstLine);
		const Sci_Position replaceEnd = lineEndPosition(scintilla, replacement.lastLine);
		::SendMessage(scintilla, SCI_SETTARGETRANGE, static_cast<WPARAM>(replaceStart), static_cast<LPARAM>(replaceEnd));
		::SendMessage(scintilla, SCI_REPLACETARGET, static_cast<WPARAM>(replacement.text.size()), reinterpret_cast<LPARAM>(replacement.text.c_str()));
	}

	if (currentPosResult >= 0)
	{
		const LRESULT lengthResult = ::SendMessage(scintilla, SCI_GETLENGTH, 0, 0);
		const Sci_Position targetPosition = static_cast<Sci_Position>((std::min)(currentPosResult, (std::max)(static_cast<LRESULT>(0), lengthResult)));
		::SendMessage(scintilla, SCI_GOTOPOS, static_cast<WPARAM>(targetPosition), 0);
	}
	return true;
}

std::size_t offsetForLineColumn(const std::vector<std::string> &lines, const std::string &eol, std::size_t row, std::size_t columnOffset)
{
	std::size_t offset = 0;
	for (std::size_t i = 0; i < row && i < lines.size(); ++i)
		offset += lines[i].size() + eol.size();
	return offset + columnOffset;
}

bool markdownSpace(char ch)
{
	return ch == ' ' || ch == '\t';
}

bool escapedAt(const std::string &line, std::size_t index)
{
	std::size_t backslashes = 0;
	while (index > 0 && line[index - 1] == '\\')
	{
		++backslashes;
		--index;
	}
	return (backslashes % 2) == 1;
}

std::string trimMarkdownSpaces(const std::string &text)
{
	std::size_t first = 0;
	while (first < text.size() && markdownSpace(text[first]))
		++first;

	std::size_t last = text.size();
	while (last > first && markdownSpace(text[last - 1]))
		--last;

	return text.substr(first, last - first);
}

std::string trimLeadingMarkdownSpaces(const std::string &text)
{
	std::size_t first = 0;
	while (first < text.size() && markdownSpace(text[first]))
		++first;
	return text.substr(first);
}

std::string withoutMarkdownSpaces(const std::string &text)
{
	std::string compact;
	compact.reserve(text.size());
	for (std::size_t i = 0; i < text.size(); ++i)
	{
		if (!markdownSpace(text[i]))
			compact.push_back(text[i]);
	}
	return compact;
}

std::size_t trailingMarkdownSpaceCount(const std::string &text)
{
	std::size_t count = 0;
	for (std::size_t i = text.size(); i > 0 && markdownSpace(text[i - 1]); --i)
		++count;
	return count;
}

std::string withoutTrailingMarkdownSpaces(const std::string &text)
{
	const std::size_t trailing = trailingMarkdownSpaceCount(text);
	return trailing == 0 ? text : text.substr(0, text.size() - trailing);
}

std::size_t nextUtf8ByteOffset(const std::string &text, std::size_t offset)
{
	if (offset >= text.size())
		return text.size();

	const unsigned char ch = static_cast<unsigned char>(text[offset]);
	std::size_t width = 1;
	if ((ch & 0x80) == 0)
		width = 1;
	else if ((ch & 0xE0) == 0xC0)
		width = 2;
	else if ((ch & 0xF0) == 0xE0)
		width = 3;
	else if ((ch & 0xF8) == 0xF0)
		width = 4;

	if (offset + width > text.size())
		return text.size();
	return offset + width;
}

std::size_t originalOffsetForCompactPrefix(const std::string &text, std::size_t compactBytes)
{
	std::size_t offset = 0;
	std::size_t seen = 0;
	while (offset < text.size() && seen < compactBytes)
	{
		if (markdownSpace(text[offset]))
		{
			++offset;
			continue;
		}

		const std::size_t next = nextUtf8ByteOffset(text, offset);
		seen += next - offset;
		offset = next;
	}
	return offset;
}

std::size_t simpleDisplayWidth(const std::string &text)
{
	std::size_t width = 0;
	for (std::size_t offset = 0; offset < text.size(); offset = nextUtf8ByteOffset(text, offset))
		++width;
	return width;
}

bool isPluginSeparatorCell(const std::string &cell)
{
	const std::string value = trimMarkdownSpaces(cell);
	bool hasDash = false;
	for (std::size_t i = 0; i < value.size(); ++i)
	{
		if (value[i] == '-')
			hasDash = true;
		else if (value[i] != ':')
			return false;
	}
	return hasDash;
}

bool isPluginSeparatorRow(const std::vector<std::string> &cells)
{
	if (cells.empty())
		return false;
	for (std::size_t i = 0; i < cells.size(); ++i)
	{
		if (!isPluginSeparatorCell(cells[i]))
			return false;
	}
	return true;
}

bool pluginCellHasText(const std::string &cell)
{
	return !trimMarkdownSpaces(cell).empty();
}

std::size_t pluginNonEmptyCellCount(const std::vector<std::string> &cells)
{
	std::size_t count = 0;
	for (std::size_t i = 0; i < cells.size(); ++i)
	{
		if (pluginCellHasText(cells[i]))
			++count;
	}
	return count;
}

bool isPluginAsciiAlphaNumeric(unsigned char ch)
{
	return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9');
}

bool isPluginWordContinuationStart(const std::string &value)
{
	if (value.empty())
		return false;
	const unsigned char ch = static_cast<unsigned char>(value[0]);
	return isPluginAsciiAlphaNumeric(ch) || ch == '_' || ch >= 0x80;
}

bool isPluginWordContinuationEnd(const std::string &value)
{
	const std::string trimmed = trimMarkdownSpaces(value);
	if (trimmed.empty())
		return false;
	const unsigned char ch = static_cast<unsigned char>(trimmed[trimmed.size() - 1]);
	return isPluginAsciiAlphaNumeric(ch) || ch == '_' || ch == '-' || ch >= 0x80;
}

std::string pluginFirstToken(const std::string &value)
{
	std::size_t end = 0;
	while (end < value.size() && !markdownSpace(value[end]))
		end = nextUtf8ByteOffset(value, end);
	return value.substr(0, end);
}

bool pluginLooksLikeSplitWordRemainder(const std::string &token)
{
	if (token.empty())
		return false;

	const std::size_t width = simpleDisplayWidth(token);
	const unsigned char first = static_cast<unsigned char>(token[0]);
	if (first >= 0x80)
		return width <= 4;
	return width <= 2;
}

bool shouldJoinPluginContinuationWithoutSpace(const std::string &target, const std::string &continuation)
{
	const std::string targetValue = trimMarkdownSpaces(target);
	const std::string continuationValue = trimMarkdownSpaces(continuation);
	if (targetValue.empty() || continuationValue.empty())
		return false;
	if (!isPluginWordContinuationEnd(targetValue) || !isPluginWordContinuationStart(continuationValue))
		return false;

	const unsigned char targetEnd = static_cast<unsigned char>(targetValue[targetValue.size() - 1]);
	const unsigned char continuationStart = static_cast<unsigned char>(continuationValue[0]);
	if (targetEnd == '-' && (isPluginAsciiAlphaNumeric(continuationStart) || continuationStart >= 0x80))
		return true;

	return pluginLooksLikeSplitWordRemainder(pluginFirstToken(continuationValue));
}

void appendPluginContinuationCell(std::string &target, const std::string &continuation, bool preserveTrailingSpaces)
{
	const std::string value = preserveTrailingSpaces
		? trimLeadingMarkdownSpaces(continuation)
		: trimMarkdownSpaces(continuation);
	if (value.empty())
		return;
	if (!trimMarkdownSpaces(target).empty() && !shouldJoinPluginContinuationWithoutSpace(target, value))
		target += " ";
	target += value;
}

CellBounds cellBoundsForColumn(const std::string &line, std::size_t column)
{
	CellBounds bounds;
	std::size_t first = 0;
	while (first < line.size() && markdownSpace(line[first]))
		++first;

	std::size_t last = line.size();
	while (last > first && markdownSpace(line[last - 1]))
		--last;

	if (first < last && line[first] == '|' && !escapedAt(line, first))
		++first;
	if (last > first && line[last - 1] == '|' && !escapedAt(line, last - 1))
		--last;

	std::size_t cellStart = first;
	std::size_t currentColumn = 0;
	for (std::size_t i = first; i <= last; ++i)
	{
		const bool atCellEnd = i == last || (line[i] == '|' && !escapedAt(line, i));
		if (!atCellEnd)
			continue;

		if (currentColumn == column)
		{
			std::size_t contentStart = cellStart;
			while (contentStart < i && markdownSpace(line[contentStart]))
				++contentStart;

			std::size_t contentEnd = i;
			while (contentEnd > contentStart && markdownSpace(line[contentEnd - 1]))
				--contentEnd;

			bounds.found = true;
			bounds.cellStart = cellStart;
			bounds.cellEnd = i;
			bounds.contentStart = contentStart;
			bounds.contentEnd = contentEnd;
			return bounds;
		}

		++currentColumn;
		cellStart = i + 1;
	}

	return bounds;
}

std::string fullCellValueForColumn(const std::string &line, std::size_t column)
{
	const CellBounds bounds = cellBoundsForColumn(line, column);
	if (!bounds.found)
		return std::string();
	return line.substr(bounds.contentStart, bounds.contentEnd - bounds.contentStart);
}

std::string cellPrefixForColumn(const std::string &line, std::size_t column, std::size_t byteColumn)
{
	const CellBounds bounds = cellBoundsForColumn(line, column);
	if (!bounds.found)
		return std::string();

	const std::size_t clampedColumn = (std::min)(byteColumn, line.size());
	if (clampedColumn <= bounds.contentStart)
		return std::string();

	return line.substr(bounds.contentStart, (std::min)(clampedColumn, bounds.cellEnd) - bounds.contentStart);
}

std::size_t tableColumnCount(const std::vector<std::string> &lines)
{
	std::size_t columns = 0;
	for (std::size_t row = 0; row < lines.size(); ++row)
	{
		for (std::size_t column = 0;; ++column)
		{
			if (!cellBoundsForColumn(lines[row], column).found)
				break;
			columns = (std::max)(columns, column + 1);
		}
	}
	return columns;
}

std::vector<std::string> rowCellsForColumns(const std::string &line, std::size_t columns)
{
	std::vector<std::string> cells;
	cells.reserve(columns);
	for (std::size_t column = 0; column < columns; ++column)
		cells.push_back(fullCellValueForColumn(line, column));
	return cells;
}

bool isLikelyPluginContinuationRow(const std::vector<std::string> &row, const std::vector<std::string> &baseRow, std::size_t columns)
{
	if (columns < 2 || row.size() < columns || baseRow.size() < columns)
		return false;

	const std::size_t nonEmpty = pluginNonEmptyCellCount(row);
	if (nonEmpty == 0 || nonEmpty == columns)
		return false;

	std::size_t emptyWhereBaseHasText = 0;
	for (std::size_t column = 0; column < columns; ++column)
	{
		if (!pluginCellHasText(row[column]) && pluginCellHasText(baseRow[column]))
			++emptyWhereBaseHasText;
	}

	const std::size_t requiredAnchors = (std::max)(static_cast<std::size_t>(1), columns / 3);
	return emptyWhereBaseHasText >= requiredAnchors;
}

void appendPluginContinuationCells(std::vector<std::string> &target, const std::vector<std::string> &continuation, std::size_t columns)
{
	if (target.size() < columns)
		target.resize(columns);

	for (std::size_t column = 0; column < columns && column < continuation.size(); ++column)
		appendPluginContinuationCell(target[column], continuation[column], false);
}

LogicalRowMap buildLogicalRowMap(const std::vector<std::string> &lines)
{
	LogicalRowMap map;
	map.columns = tableColumnCount(lines);
	map.baseRowForRow.assign(lines.size(), static_cast<std::size_t>(-1));
	map.logicalRowForRow.assign(lines.size(), static_cast<std::size_t>(-1));
	if (map.columns == 0)
		return map;

	std::size_t separatorRow = static_cast<std::size_t>(-1);
	std::vector<std::vector<std::string>> cellsByRow;
	cellsByRow.reserve(lines.size());
	for (std::size_t row = 0; row < lines.size(); ++row)
	{
		cellsByRow.push_back(rowCellsForColumns(lines[row], map.columns));
		if (separatorRow == static_cast<std::size_t>(-1) && isPluginSeparatorRow(cellsByRow.back()))
			separatorRow = row;
	}

	std::size_t baseRow = static_cast<std::size_t>(-1);
	std::size_t baseLogicalRow = static_cast<std::size_t>(-1);
	std::size_t nextLogicalRow = 0;
	std::vector<std::string> baseCells;
	for (std::size_t row = 0; row < lines.size(); ++row)
	{
		const bool separator = row == separatorRow;
		const bool canBeContinuation = separatorRow != static_cast<std::size_t>(-1)
			&& row > separatorRow
			&& baseRow != static_cast<std::size_t>(-1)
			&& !separator;
		const bool continuation = canBeContinuation && isLikelyPluginContinuationRow(cellsByRow[row], baseCells, map.columns);
		if (continuation)
		{
			map.baseRowForRow[row] = baseRow;
			map.logicalRowForRow[row] = baseLogicalRow;
			appendPluginContinuationCells(baseCells, cellsByRow[row], map.columns);
			continue;
		}

		baseRow = row;
		baseLogicalRow = nextLogicalRow++;
		baseCells = cellsByRow[row];
		map.baseRowForRow[row] = baseRow;
		map.logicalRowForRow[row] = baseLogicalRow;
	}

	return map;
}

CellCaretSnapshot captureCellCaret(const std::string &line, std::size_t row, std::size_t column, std::size_t byteColumn)
{
	CellCaretSnapshot snapshot;
	const CellBounds bounds = cellBoundsForColumn(line, column);
	if (!bounds.found)
		return snapshot;

	const std::size_t clampedColumn = (std::min)(byteColumn, line.size());
	std::size_t offset = 0;
	if (clampedColumn > bounds.contentStart)
		offset = (std::min)(clampedColumn, bounds.cellEnd) - bounds.contentStart;

	snapshot.valid = true;
	snapshot.row = row;
	snapshot.column = column;
	snapshot.offset = offset;
	snapshot.prefix = line.substr(bounds.contentStart, offset);
	return snapshot;
}

CellCaretSnapshot captureLogicalCellCaret(const std::vector<std::string> &lines, std::size_t row, std::size_t column, std::size_t byteColumn)
{
	if (row >= lines.size())
		return CellCaretSnapshot();

	CellCaretSnapshot snapshot = captureCellCaret(lines[row], row, column, byteColumn);
	if (!snapshot.valid)
		return snapshot;

	const LogicalRowMap map = buildLogicalRowMap(lines);
	if (row >= map.baseRowForRow.size() || map.baseRowForRow[row] == static_cast<std::size_t>(-1))
		return snapshot;

	const std::size_t baseRow = map.baseRowForRow[row];
	snapshot.logicalRow = map.logicalRowForRow[row];
	snapshot.prefix.clear();
	for (std::size_t physicalRow = baseRow; physicalRow <= row && physicalRow < lines.size(); ++physicalRow)
	{
		if (map.baseRowForRow[physicalRow] != baseRow)
			continue;
		const bool current = physicalRow == row;
		const std::string fragment = current
			? cellPrefixForColumn(lines[physicalRow], column, byteColumn)
			: fullCellValueForColumn(lines[physicalRow], column);
		appendPluginContinuationCell(snapshot.prefix, fragment, current);
	}
	snapshot.offset = snapshot.prefix.size();
	return snapshot;
}

CellCaretPosition cellCaretPositionInLines(const std::vector<std::string> &lines, const CellCaretSnapshot &snapshot)
{
	CellCaretPosition position;
	if (!snapshot.valid || lines.empty())
		return position;

	const std::size_t trailingSpaces = trailingMarkdownSpaceCount(snapshot.prefix);
	if (trailingSpaces > 0)
	{
		CellCaretSnapshot visibleSnapshot = snapshot;
		visibleSnapshot.prefix = withoutTrailingMarkdownSpaces(snapshot.prefix);
		visibleSnapshot.offset = visibleSnapshot.prefix.size();
		position = cellCaretPositionInLines(lines, visibleSnapshot);
		if (position.found && position.row < lines.size())
		{
			const CellBounds bounds = cellBoundsForColumn(lines[position.row], snapshot.column);
			if (bounds.found)
				position.columnOffset = (std::min)(position.columnOffset + trailingSpaces, bounds.cellEnd);
		}
		return position;
	}

	const LogicalRowMap map = buildLogicalRowMap(lines);
	if (map.logicalRowForRow.empty())
		return position;

	std::size_t baseRow = static_cast<std::size_t>(-1);
	for (std::size_t row = 0; row < map.logicalRowForRow.size(); ++row)
	{
		if (map.logicalRowForRow[row] == snapshot.logicalRow && map.baseRowForRow[row] == row)
		{
			baseRow = row;
			break;
		}
	}

	if (baseRow == static_cast<std::size_t>(-1))
	{
		if (snapshot.row >= lines.size())
			return position;
		baseRow = snapshot.row;
	}

	std::string logicalPrefix;
	for (std::size_t row = baseRow; row < lines.size(); ++row)
	{
		if (row >= map.baseRowForRow.size() || map.baseRowForRow[row] != baseRow)
			break;

		const std::string value = fullCellValueForColumn(lines[row], snapshot.column);
		if (value.empty())
			continue;

		const bool joinWithoutSpace = trimMarkdownSpaces(logicalPrefix).empty()
			|| shouldJoinPluginContinuationWithoutSpace(logicalPrefix, value);
		const std::string separator = joinWithoutSpace ? std::string() : std::string(" ");
		const std::string candidate = logicalPrefix + separator + value;
		const std::string compactSnapshotPrefix = withoutMarkdownSpaces(snapshot.prefix);
		const std::string compactCandidate = withoutMarkdownSpaces(candidate);
		const bool compactMatch = !compactSnapshotPrefix.empty()
			&& compactSnapshotPrefix.size() <= compactCandidate.size()
			&& compactCandidate.compare(0, compactSnapshotPrefix.size(), compactSnapshotPrefix) == 0;
		if (snapshot.prefix.size() <= candidate.size()
			&& candidate.compare(0, snapshot.prefix.size(), snapshot.prefix) == 0)
		{
			const std::size_t offsetInAddedText = snapshot.prefix.size() - logicalPrefix.size();
			const std::size_t offsetInValue = offsetInAddedText > separator.size()
				? offsetInAddedText - separator.size()
				: 0;
			const CellBounds bounds = cellBoundsForColumn(lines[row], snapshot.column);
			if (!bounds.found)
				break;

			position.found = true;
			position.row = row;
			position.columnOffset = (std::min)(bounds.contentStart + offsetInValue, bounds.contentEnd);
			return position;
		}
		if (compactMatch)
		{
			const std::size_t offsetInCandidate = originalOffsetForCompactPrefix(candidate, compactSnapshotPrefix.size());
			if (offsetInCandidate >= logicalPrefix.size())
			{
				const std::size_t offsetInAddedText = offsetInCandidate - logicalPrefix.size();
				const std::size_t offsetInValue = offsetInAddedText > separator.size()
					? offsetInAddedText - separator.size()
					: 0;
				const CellBounds bounds = cellBoundsForColumn(lines[row], snapshot.column);
				if (!bounds.found)
					break;

				position.found = true;
				position.row = row;
				position.columnOffset = (std::min)(bounds.contentStart + offsetInValue, bounds.contentEnd);
				return position;
			}
		}

		if (snapshot.prefix.size() >= candidate.size()
			&& snapshot.prefix.compare(0, candidate.size(), candidate) == 0)
		{
			logicalPrefix = candidate;
			continue;
		}
		if (compactSnapshotPrefix.size() >= compactCandidate.size()
			&& compactSnapshotPrefix.compare(0, compactCandidate.size(), compactCandidate) == 0)
		{
			logicalPrefix = candidate;
			continue;
		}

		break;
	}

	const std::size_t fallbackRow = (std::min)(baseRow, lines.size() - 1);
	const CellBounds bounds = cellBoundsForColumn(lines[fallbackRow], snapshot.column);
	if (!bounds.found)
		return position;

	position.found = true;
	position.row = fallbackRow;
	position.columnOffset = (std::min)(bounds.contentStart + snapshot.offset, bounds.contentEnd);
	return position;
}

void ensureTrailingCellCaretSpaces(std::vector<std::string> &lines, const CellCaretSnapshot &snapshot)
{
	const std::size_t trailingSpaces = trailingMarkdownSpaceCount(snapshot.prefix);
	if (trailingSpaces == 0)
		return;

	CellCaretSnapshot visibleSnapshot = snapshot;
	visibleSnapshot.prefix = withoutTrailingMarkdownSpaces(snapshot.prefix);
	visibleSnapshot.offset = visibleSnapshot.prefix.size();
	const CellCaretPosition position = cellCaretPositionInLines(lines, visibleSnapshot);
	if (!position.found || position.row >= lines.size())
		return;

	const CellBounds bounds = cellBoundsForColumn(lines[position.row], snapshot.column);
	if (!bounds.found)
		return;

	const std::size_t insertAt = (std::min)((std::max)(position.columnOffset, bounds.contentStart), bounds.cellEnd);
	lines[position.row].insert(insertAt, trailingSpaces, ' ');
}

std::size_t columnOffsetForCellCaret(const std::vector<std::string> &lines, const CellCaretSnapshot &snapshot, std::size_t fallback)
{
	const CellCaretPosition position = cellCaretPositionInLines(lines, snapshot);
	return position.found ? position.columnOffset : fallback;
}

bool shouldMoveCaretToTarget(Sci_Position currentPosition, std::size_t targetPosition)
{
	const std::size_t current = currentPosition > 0 ? static_cast<std::size_t>(currentPosition) : 0;
	return current != targetPosition;
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
	PluginEditGuard pluginEdit;
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
	PluginEditGuard pluginEdit;
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

bool runTableAction(MarkdownTable::Action action, bool quiet, CaretPlacement caretPlacement)
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
	const CellCaretSnapshot preservedCaret = caretPlacement == CaretPlacement::PreserveCellOffset
		? captureLogicalCellCaret(tableLines, row, column, byteColumn)
		: CellCaretSnapshot();
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
	if (caretPlacement == CaretPlacement::PreserveCellOffset && preservedCaret.valid)
		ensureTrailingCellCaretSpaces(edit.lines, preservedCaret);
	const std::string eol = chooseEol(scintilla, firstLine, lastLine, lineCount);
	const std::string replacement = joinLines(edit.lines, eol);
	std::size_t targetRow = edit.targetRow;
	std::size_t targetColumnOffset = edit.targetColumnOffset;
	if (caretPlacement == CaretPlacement::PreserveCellOffset && preservedCaret.valid)
	{
		const CellCaretPosition preservedPosition = cellCaretPositionInLines(edit.lines, preservedCaret);
		if (preservedPosition.found)
		{
			targetRow = preservedPosition.row;
			targetColumnOffset = preservedPosition.columnOffset;
		}
	}
	const std::size_t targetPosition = positionForLineColumn(scintilla, firstLine, edit.lines, eol, targetRow, targetColumnOffset);
	const Sci_Position replaceStart = lineStartPosition(scintilla, firstLine);
	const Sci_Position replaceEnd = lineEndPosition(scintilla, lastLine);
	const std::string source = getRangeText(scintilla, replaceStart, replaceEnd);
	if (source == replacement)
	{
		if (shouldMoveCaretToTarget(currentPosResult, targetPosition))
			::SendMessage(scintilla, SCI_GOTOPOS, static_cast<WPARAM>(targetPosition), 0);
		return true;
	}

	ScintillaUndoAction undo(scintilla);
	PluginEditGuard pluginEdit;
	::SendMessage(scintilla, SCI_SETTARGETRANGE, static_cast<WPARAM>(replaceStart), static_cast<LPARAM>(replaceEnd));
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

void checkWordWrapAutoFitWarning()
{
	checkWordWrapAutoFitWarningInternal();
}

void handleScintillaUpdateUi(const SCNotification *notification)
{
	handleScintillaUpdateUiInternal(notification);
}

void handleScintillaModified(const SCNotification *notification)
{
	handleScintillaModifiedInternal(notification);
}

void handleScintillaZoom(const SCNotification *notification)
{
	handleScintillaZoomInternal(notification);
}

void handleGlobalModified()
{
	handleGlobalModifiedInternal();
}

void handleInitialAutoTableFormatForBuffer(const SCNotification *notification)
{
	runInitialAutoTableFormatForBuffer(notification);
}

void handleInitialAutoTableFormatForCurrentBuffer()
{
	runInitialAutoTableFormatForCurrentBuffer();
}

void forgetInitialAutoTableFormatForBuffer(const SCNotification *notification)
{
	removeInitialAutoFormatBuffer(notification ? notification->nmhdr.idFrom : 0);
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

const wchar_t *commandTextForTests(std::size_t index)
{
	return commandText(index);
}

const wchar_t *commandMenuTextForTests(std::size_t index)
{
	return commandMenuText(index);
}

bool autoFitTableEnabledForTests()
{
	return g_autoFitTable;
}

void setAutoFitTableEnabledForTests(bool enabled)
{
	g_autoFitTable = enabled;
	if (enabled)
		g_autoAlignTable = true;
	if (!enabled)
	{
		g_lastFitToWindowColumns = 0;
		g_wordWrapAutoFitWarningComboActive = false;
	}
}

bool autoAlignTableEnabledForTests()
{
	return g_autoAlignTable;
}

void setAutoAlignTableEnabledForTests(bool enabled)
{
	if (g_autoFitTable && !enabled)
		return;
	g_autoAlignTable = enabled;
}

MarkdownTable::Action coreActionForPluginActionForTests(MarkdownTable::Action action)
{
	return coreActionForPluginAction(action);
}

bool shouldApplyAutoFitAfterActionForTests(MarkdownTable::Action action)
{
	return shouldApplyAutoFitAfterAction(action);
}

bool shouldFitToWindowAfterActionForTests(MarkdownTable::Action action)
{
	return shouldFitToWindowAfterAction(action);
}

bool alignTableCommandEnabledForTests()
{
	return alignTableCommandEnabled();
}

bool fitTableToWindowCommandEnabledForTests()
{
	return fitTableToWindowCommandEnabled();
}

bool autoAlignTableToggleEnabledForTests()
{
	return autoAlignTableToggleEnabled();
}

bool standardWordWrapEnabledForModeForTests(long wrapMode)
{
	return standardWordWrapEnabledForMode(static_cast<LRESULT>(wrapMode));
}

bool shouldShowWordWrapAutoFitWarningForTests(bool autoFitEnabled, bool wordWrapEnabled, bool warningSuppressed)
{
	return shouldShowWordWrapAutoFitWarning(autoFitEnabled, wordWrapEnabled, warningSuppressed);
}

bool shouldRunFitToWindowAfterResizeForTests(bool enabled, bool inProgress, bool activeEditor, std::size_t previousColumns, std::size_t currentColumns)
{
	return shouldRunFitToWindowAfterResize(enabled, inProgress, activeEditor, previousColumns, currentColumns);
}

bool shouldScheduleFitToWindowAfterResizeMessageForTests(bool enabled, UINT message, WPARAM wParam, UINT windowPosFlags)
{
	return enabled && shouldScheduleFitToWindowAfterResizeMessage(message, wParam, windowPosFlags);
}

bool shouldRunInitialFitWhenTogglingAutoFitTableForTests(bool currentlyEnabled)
{
	return shouldRunInitialFitWhenTogglingAutoFitTable(currentlyEnabled);
}

bool shouldRunAutoTableFormatAfterUpdateForTests(bool autoAlignEnabled, bool autoFitEnabled, bool alignInProgress, bool fitInProgress, bool pluginEditInProgress, bool activeEditor, bool contentUpdated)
{
	return shouldRunAutoTableFormatAfterUpdate(autoAlignEnabled, autoFitEnabled, alignInProgress, fitInProgress, pluginEditInProgress, activeEditor, contentUpdated);
}

bool scintillaModificationShouldRunAutoTableFormatForTests(int modificationType)
{
	return scintillaModificationShouldRunAutoTableFormat(modificationType);
}

bool shouldRunAutoFitAfterZoomForTests(bool autoFitEnabled, bool fitInProgress, bool activeEditor)
{
	return shouldRunAutoFitAfterZoom(autoFitEnabled, fitInProgress, activeEditor);
}

bool shouldScheduleFitToWindowAfterZoomForTests(bool autoFitEnabled, bool fitInProgress, bool activeEditor)
{
	return shouldScheduleFitToWindowAfterZoom(autoFitEnabled, fitInProgress, activeEditor);
}

bool shouldRunAutoTableFormatAfterGlobalModifiedForTests(bool autoAlignEnabled, bool autoFitEnabled, bool alignInProgress, bool fitInProgress, bool pluginEditInProgress)
{
	return shouldRunAutoTableFormatAfterGlobalModified(autoAlignEnabled, autoFitEnabled, alignInProgress, fitInProgress, pluginEditInProgress);
}

bool shouldRunInitialAlignWhenTogglingAutoAlignTableForTests(bool currentlyEnabled)
{
	return shouldRunInitialAlignWhenTogglingAutoAlignTable(currentlyEnabled);
}

bool shouldRunInitialAutoTableFormatForBufferForTests(bool autoAlignEnabled, bool autoFitEnabled, bool alignInProgress, bool fitInProgress, bool activeEditor, bool alreadyHandled)
{
	return shouldRunInitialAutoTableFormatForBuffer(autoAlignEnabled, autoFitEnabled, alignInProgress, fitInProgress, activeEditor, alreadyHandled);
}

bool shouldQueueInitialAutoTableFormatForOpenedBufferForTests(bool autoAlignEnabled, bool autoFitEnabled, bool alreadyHandled)
{
	return shouldQueueInitialAutoTableFormatForOpenedBuffer(autoAlignEnabled, autoFitEnabled, alreadyHandled);
}

bool shouldTryQueuedInitialAutoTableFormatForNotificationForTests(UINT notificationCode)
{
	return shouldTryQueuedInitialAutoTableFormatForNotification(notificationCode);
}

bool shouldDeferInitialAutoTableFormatForDocumentLengthForTests(long textLength)
{
	return shouldDeferInitialAutoTableFormatForDocumentLength(static_cast<LRESULT>(textLength));
}

std::vector<std::size_t> documentMarkdownTableRangeLinesForTests(const std::vector<std::string> &lines)
{
	std::vector<std::size_t> result;
	const std::vector<DocumentTableRange> ranges = findDocumentTableRanges(lines);
	for (std::size_t i = 0; i < ranges.size(); ++i)
	{
		result.push_back(ranges[i].firstLine);
		result.push_back(ranges[i].lastLine);
	}
	return result;
}

std::vector<std::string> autoFormatDocumentTablesForTests(const std::vector<std::string> &lines, MarkdownTable::Action action, std::size_t maxTableWidth)
{
	std::vector<std::string> result = lines;
	const std::vector<DocumentTableRange> ranges = findDocumentTableRanges(lines);
	for (std::size_t i = ranges.size(); i > 0; --i)
	{
		const DocumentTableRange &range = ranges[i - 1];
		if (range.firstLine >= result.size() || range.lastLine >= result.size() || range.firstLine > range.lastLine)
			continue;

		std::vector<std::string> tableLines;
		for (std::size_t line = range.firstLine; line <= range.lastLine; ++line)
			tableLines.push_back(result[line]);

		MarkdownTable::EditResult edit = MarkdownTable::apply(tableLines, 0, 0, coreActionForPluginAction(action));
		if (!edit.ok)
			continue;
		if (shouldFitToWindowAfterAction(action))
		{
			const std::size_t width = (std::max)(static_cast<std::size_t>(1), maxTableWidth);
			const MarkdownTable::EditResult wrapped = applyWrappedToWidthUntilStable(edit.lines, 0, 0, width);
			if (wrapped.ok)
				edit = wrapped;
		}

		result.erase(result.begin() + static_cast<std::ptrdiff_t>(range.firstLine), result.begin() + static_cast<std::ptrdiff_t>(range.lastLine + 1));
		result.insert(result.begin() + static_cast<std::ptrdiff_t>(range.firstLine), edit.lines.begin(), edit.lines.end());
	}
	return result;
}

bool shouldMoveCaretToTargetForTests(std::size_t currentPosition, std::size_t targetPosition)
{
	return shouldMoveCaretToTarget(static_cast<Sci_Position>(currentPosition), targetPosition);
}

bool shouldPreserveEnterColumnForTests(bool activeEditor, bool emptySelection, bool tableLine, UINT message, WPARAM key)
{
	return shouldPreserveEnterColumn(activeEditor, emptySelection, tableLine, message, key);
}

UINT fitToWindowResizeDelayMsForTests()
{
	return fitToWindowResizeDelayMs;
}

std::size_t preservedCellCaretColumnOffsetForTests(const std::string &sourceLine, std::size_t column, std::size_t byteColumn, const std::string &replacementLine)
{
	const CellCaretSnapshot snapshot = captureCellCaret(sourceLine, 0, column, byteColumn);
	std::vector<std::string> lines(1, replacementLine);
	ensureTrailingCellCaretSpaces(lines, snapshot);
	return columnOffsetForCellCaret(lines, snapshot, static_cast<std::size_t>(-1));
}

CellCaretPreview preservedCellCaretPositionForTests(const std::vector<std::string> &sourceLines, std::size_t row, std::size_t column, std::size_t byteColumn, const std::vector<std::string> &replacementLines)
{
	CellCaretPreview preview;
	const CellCaretSnapshot snapshot = captureLogicalCellCaret(sourceLines, row, column, byteColumn);
	std::vector<std::string> lines = replacementLines;
	ensureTrailingCellCaretSpaces(lines, snapshot);
	const CellCaretPosition position = cellCaretPositionInLines(lines, snapshot);
	if (position.found)
	{
		preview.row = position.row;
		preview.columnOffset = position.columnOffset;
	}
	return preview;
}

bool ensureAlignToolbarIconsForTests()
{
	return ensureAlignToolbarIconHandles();
}

void destroyAlignToolbarIconsForTests()
{
	destroyAlignToolbarIconHandles();
}

bool ensureWrapLongCellsToolbarIconsForTests()
{
	return ensureWrapLongCellsToolbarIconHandles();
}

void destroyWrapLongCellsToolbarIconsForTests()
{
	destroyWrapLongCellsToolbarIconHandles();
}

bool ensureAutoFitTableToolbarIconsForTests()
{
	return ensureAutoFitTableToolbarIconHandles();
}

void destroyAutoFitTableToolbarIconsForTests()
{
	destroyAutoFitTableToolbarIconHandles();
}

bool ensureAutoAlignTableToolbarIconsForTests()
{
	return ensureAutoAlignTableToolbarIconHandles();
}

void destroyAutoAlignTableToolbarIconsForTests()
{
	destroyAutoAlignTableToolbarIconHandles();
}

bool ensureNarrowColumnToolbarIconsForTests()
{
	return ensureNarrowColumnToolbarIconHandles();
}

void destroyNarrowColumnToolbarIconsForTests()
{
	destroyNarrowColumnToolbarIconHandles();
}

bool ensureWidenColumnToolbarIconsForTests()
{
	return ensureWidenColumnToolbarIconHandles();
}

void destroyWidenColumnToolbarIconsForTests()
{
	destroyWidenColumnToolbarIconHandles();
}

bool ensureAllToolbarIconsForTests()
{
	for (std::size_t commandIndex = 0; commandIndex < static_cast<std::size_t>(nbFunc); ++commandIndex)
	{
		if (!ensureCommandToolbarIconHandles(commandIndex))
			return false;
	}
	return true;
}

void destroyAllToolbarIconsForTests()
{
	destroyAllToolbarIconHandles();
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
	destroyAllToolbarIconHandles();
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
	setCommand(alignCommandIndex, commandMenuText(alignCommandIndex), alignTable, &g_alignShortcut, false);
	setCommand(autoAlignTableCommandIndex, commandMenuText(autoAlignTableCommandIndex), toggleAutoAlignTable, &g_autoAlignTableShortcut, g_autoAlignTable);
	setCommand(wrapLongCellsCommandIndex, commandMenuText(wrapLongCellsCommandIndex), wrapLongCells, &g_wrapLongCellsShortcut, false);
	setCommand(autoFitTableCommandIndex, commandMenuText(autoFitTableCommandIndex), toggleAutoFitTable, &g_autoFitTableShortcut, g_autoFitTable);
	setCommand(nextCellCommandIndex, commandMenuText(nextCellCommandIndex), nextCell, &g_nextCellShortcut, false);
	setCommand(previousCellCommandIndex, commandMenuText(previousCellCommandIndex), previousCell, &g_previousCellShortcut, false);
	setCommand(insertRowCommandIndex, commandMenuText(insertRowCommandIndex), insertRowBelow, &g_insertRowShortcut, false);
	setCommand(deleteRowCommandIndex, commandMenuText(deleteRowCommandIndex), deleteRow, &g_deleteRowShortcut, false);
	setCommand(insertColumnCommandIndex, commandMenuText(insertColumnCommandIndex), insertColumnRight, &g_insertColumnShortcut, false);
	setCommand(deleteColumnCommandIndex, commandMenuText(deleteColumnCommandIndex), deleteColumn, &g_deleteColumnShortcut, false);
	setCommand(narrowColumnCommandIndex, commandMenuText(narrowColumnCommandIndex), narrowColumn, &g_narrowColumnShortcut, false);
	setCommand(widenColumnCommandIndex, commandMenuText(widenColumnCommandIndex), widenColumn, &g_widenColumnShortcut, false);
	setCommand(moveRowUpCommandIndex, commandMenuText(moveRowUpCommandIndex), moveRowUp, &g_moveRowUpShortcut, false);
	setCommand(moveRowDownCommandIndex, commandMenuText(moveRowDownCommandIndex), moveRowDown, &g_moveRowDownShortcut, false);
	setCommand(moveColumnLeftCommandIndex, commandMenuText(moveColumnLeftCommandIndex), moveColumnLeft, &g_moveColumnLeftShortcut, false);
	setCommand(moveColumnRightCommandIndex, commandMenuText(moveColumnRightCommandIndex), moveColumnRight, &g_moveColumnRightShortcut, false);
	setCommand(sortRowsAscendingCommandIndex, commandMenuText(sortRowsAscendingCommandIndex), sortRowsAscending, &g_sortRowsAscendingShortcut, false);
	setCommand(sortRowsDescendingCommandIndex, commandMenuText(sortRowsDescendingCommandIndex), sortRowsDescending, &g_sortRowsDescendingShortcut, false);
	setCommand(convertCsvTsvCommandIndex, commandMenuText(convertCsvTsvCommandIndex), convertCsvTsvSelectionToTable, &g_convertCsvTsvShortcut, false);
	setCommand(insertTableCommandIndex, commandMenuText(insertTableCommandIndex), insertTable, &g_insertTableShortcut, false);
	refreshAutoFitTableUi();
	refreshAutoAlignTableUi();
}

void refreshUiLanguageFromNotepad()
{
	refreshUiLanguageState();
	updateNotepadPluginMenu();
	refreshAutoFitTableUi();
	refreshAutoAlignTableUi();
}

void refreshAutoFitTableUi()
{
	if (!nppData._nppHandle || funcItem[autoFitTableCommandIndex]._cmdID == 0)
		return;

	::SendMessage(
		nppData._nppHandle,
		NPPM_SETMENUITEMCHECK,
		static_cast<WPARAM>(funcItem[autoFitTableCommandIndex]._cmdID),
		static_cast<LPARAM>(g_autoFitTable ? TRUE : FALSE));
	setAutoFitTableToolbarCheckState();
	setCommandEnabledState(wrapLongCellsCommandIndex, fitTableToWindowCommandEnabled());
}

void refreshAutoAlignTableUi()
{
	if (!nppData._nppHandle || funcItem[autoAlignTableCommandIndex]._cmdID == 0)
		return;

	setCommandEnabledState(autoAlignTableCommandIndex, autoAlignTableToggleEnabled());
	setCommandEnabledState(alignCommandIndex, alignTableCommandEnabled());
	::SendMessage(
		nppData._nppHandle,
		NPPM_SETMENUITEMCHECK,
		static_cast<WPARAM>(funcItem[autoAlignTableCommandIndex]._cmdID),
		static_cast<LPARAM>(g_autoAlignTable ? TRUE : FALSE));
	setAutoAlignTableToolbarCheckState();
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
	for (std::size_t commandIndex = 0; commandIndex < static_cast<std::size_t>(nbFunc); ++commandIndex)
	{
		if (funcItem[commandIndex]._cmdID != 0 && ensureCommandToolbarIconHandles(commandIndex))
		{
			registerToolbarIcon(
				commandIndex,
				g_toolbarBmps[commandIndex],
				g_toolbarIcons[commandIndex],
				g_toolbarIconDarkModes[commandIndex]);
		}
	}

	refreshAutoFitTableUi();
	refreshAutoAlignTableUi();
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
	if (!alignTableCommandEnabled())
		return;
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

void narrowColumn()
{
	runTableAction(MarkdownTable::Action::NarrowColumn, false);
}

void widenColumn()
{
	runTableAction(MarkdownTable::Action::WidenColumn, false);
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

void wrapLongCells()
{
	if (!fitTableToWindowCommandEnabled())
		return;
	fitCurrentTableToWindow(false);
}

void toggleAutoFitTable()
{
	const bool enabling = !g_autoFitTable;
	const bool runInitialFit = shouldRunInitialFitWhenTogglingAutoFitTable(g_autoFitTable);
	if (runInitialFit)
		fitCurrentTableToWindow(true);

	g_autoFitTable = !g_autoFitTable;
	if (g_autoFitTable)
	{
		g_autoAlignTable = true;
		rememberCurrentFitToWindowWidth();
	}
	else
	{
		cancelFitToWindowResizeTimers();
		g_lastFitToWindowColumns = 0;
		g_wordWrapAutoFitWarningComboActive = false;
	}
	refreshAutoFitTableUi();
	refreshAutoAlignTableUi();
	if (enabling)
		checkWordWrapAutoFitWarning();
}

void toggleAutoAlignTable()
{
	if (!autoAlignTableToggleEnabled())
		return;

	const bool runInitialAlign = shouldRunInitialAlignWhenTogglingAutoAlignTable(g_autoAlignTable);
	if (runInitialAlign)
		autoAlignCurrentTable(true);

	g_autoAlignTable = !g_autoAlignTable;
	refreshAutoAlignTableUi();
}

void installFitToWindowResizeHooks()
{
	subclassNppWindow();
	subclassScintillaWindow(nppData._scintillaMainHandle, g_originalMainScintillaProc);
	subclassScintillaWindow(nppData._scintillaSecondHandle, g_originalSecondScintillaProc);
	rememberCurrentFitToWindowWidth();
}

void removeFitToWindowResizeHooks()
{
	cancelFitToWindowResizeTimers();
	removeScintillaSubclass(nppData._scintillaMainHandle, g_originalMainScintillaProc);
	removeScintillaSubclass(nppData._scintillaSecondHandle, g_originalSecondScintillaProc);
	removeNppSubclass();
}
