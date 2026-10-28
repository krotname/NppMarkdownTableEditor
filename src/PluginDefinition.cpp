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
#include "AutoFormatCoordinator.h"
#include "PluginCommands.h"
#include "PluginLocalization.h"
#include "PluginSettings.h"

#include <commctrl.h>

#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")

//
// The plugin data that Notepad++ needs
//
FuncItem funcItem[nbFunc];

//
// The data of Notepad++ that you can use in your plugin commands
//
NppData nppData;

using namespace MarkdownTablePlugin;

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

static_assert(nbFunc == commandCount, "Plugin command ABI must match localization");
const UINT_PTR fitToWindowResizeTimerId = 0x4D54;
const UINT fitToWindowResizeDelayMs = 160;
const std::size_t wrapStabilizationPassLimit = 8;

// Win32 resources live for the plugin; coordinator owns buffer/view lifetimes.
struct PluginRuntimeState
{
	AutoFormatCoordinator autoFormat;
	PluginLocalization localization;
	PluginSettings settings;
	HINSTANCE moduleHandle = NULL;
	bool wordWrapWarningComboActive = false;
	bool mainScintillaSubclassInstalled = false;
	bool secondScintillaSubclassInstalled = false;
	bool nppSubclassInstalled = false;
	HBITMAP toolbarBmps[nbFunc] = {};
	HICON toolbarIcons[nbFunc] = {};
	HICON toolbarIconDarkModes[nbFunc] = {};
} g_runtime;

uptr_t currentBufferId();
bool selectionEmpty(HWND scintilla);
EditorView editorView(HWND scintilla)
{
	return scintilla == nppData._scintillaSecondHandle ? EditorView::Secondary : EditorView::Main;
}

void cancelFitToWindowResizeTimers();

struct PluginEditGuard
{
	PluginEditGuard()
	{
		g_runtime.autoFormat.beginPluginEdit(currentBufferId());
		cancelFitToWindowResizeTimers();
	}
	~PluginEditGuard()
	{
		g_runtime.autoFormat.endPluginEdit();
		cancelFitToWindowResizeTimers();
	}
	PluginEditGuard(const PluginEditGuard &) = delete;
	PluginEditGuard &operator=(const PluginEditGuard &) = delete;
};

bool shouldPreserveEnterColumn(bool activeEditor, bool emptySelection, bool tableLine, UINT message, WPARAM key);
bool captureEnterColumnToPreserve(HWND hwnd, UINT message, WPARAM key, std::size_t &column);
void restoreEnterColumn(HWND hwnd, std::size_t column);

const UiText &uiText() { return g_runtime.localization.uiText(); }
const wchar_t *commandMenuText(std::size_t index) { return g_runtime.localization.commandMenuText(index); }

void applyCommandNames()
{
	for (int index = 0; index < nbFunc; ++index)
		lstrcpyn(funcItem[index]._itemName, commandMenuText(static_cast<std::size_t>(index)), menuItemSize);
}

void setUiLanguage(UiLanguage language)
{
	g_runtime.localization.setLanguage(language);
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

bool wordWrapAutoFitWarningSuppressed()
{
	return g_runtime.settings.wordWrapAutoFitWarningSuppressed(pluginConfigFilePath());
}

void setWordWrapAutoFitWarningSuppressed(bool suppressed)
{
	g_runtime.settings.setWordWrapAutoFitWarningSuppressed(pluginConfigFilePath(), suppressed);
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
			HWND messageText = ::CreateWindowEx(0, TEXT("STATIC"), text.wordWrapAutoFitWarningMessage, WS_CHILD | WS_VISIBLE | SS_LEFT, 18, 18, 404, 72, hwnd, NULL, g_runtime.moduleHandle, NULL);
			HWND check = ::CreateWindowEx(0, TEXT("BUTTON"), text.doNotShowAgain, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 18, 100, 250, 24, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(wordWrapAutoFitDontShowAgainId)), g_runtime.moduleHandle, NULL);
			HWND ok = ::CreateWindowEx(0, TEXT("BUTTON"), text.okButton, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 178, 136, 84, 28, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)), g_runtime.moduleHandle, NULL);

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
		wc.hInstance = g_runtime.moduleHandle;
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
	HWND dialog = ::CreateWindowEx(WS_EX_DLGMODALFRAME, className, uiText().wordWrapAutoFitWarningTitle, WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, width, height, nppData._nppHandle, NULL, g_runtime.moduleHandle, &state);
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
	const bool comboActive = g_runtime.autoFormat.autoFitEnabled() && standardWordWrapEnabled(currentScintilla());
	if (!comboActive)
	{
		g_runtime.wordWrapWarningComboActive = false;
		return;
	}
	if (g_runtime.wordWrapWarningComboActive)
		return;
	if (!nppData._nppHandle || !::IsWindowVisible(nppData._nppHandle))
		return;

	g_runtime.wordWrapWarningComboActive = true;
	if (!shouldShowWordWrapAutoFitWarning(g_runtime.autoFormat.autoFitEnabled(), true, wordWrapAutoFitWarningSuppressed()))
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

// Кнопку на панели инструментов получают только четыре основные команды. Остальные
// шестнадцать остаются в меню плагина и на своих горячих клавишах: панель Notepad++ общая
// для всех плагинов, и двадцать кнопок вытесняли с неё всё прочее (issue #11).
bool commandHasToolbarButton(std::size_t commandIndex)
{
	switch (commandIndex)
	{
	case alignCommandIndex:
	case autoAlignTableCommandIndex:
	case wrapLongCellsCommandIndex:
	case autoFitTableCommandIndex:
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
		g_runtime.toolbarBmps[commandIndex],
		g_runtime.toolbarIcons[commandIndex],
		g_runtime.toolbarIconDarkModes[commandIndex],
		toolbarIconKindForCommand(commandIndex));
}

void destroyCommandToolbarIconHandles(std::size_t commandIndex)
{
	if (commandIndex >= static_cast<std::size_t>(nbFunc))
		return;

	destroyToolbarIconHandles(
		g_runtime.toolbarBmps[commandIndex],
		g_runtime.toolbarIcons[commandIndex],
		g_runtime.toolbarIconDarkModes[commandIndex]);
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
	setToolbarCheckState(autoFitTableCommandIndex, g_runtime.autoFormat.autoFitEnabled());
}

void setAutoAlignTableToolbarCheckState()
{
	setToolbarCheckState(autoAlignTableCommandIndex, g_runtime.autoFormat.autoAlignEnabled());
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
	return !g_runtime.autoFormat.autoFitEnabled();
}

bool alignTableCommandEnabled()
{
	return !g_runtime.autoFormat.autoAlignEnabled();
}

bool autoAlignTableToggleEnabled()
{
	return !g_runtime.autoFormat.autoFitEnabled();
}




bool scintillaModificationShouldRunAutoTableFormat(int modificationType)
{
	return (modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT | SC_PERFORMED_UNDO | SC_PERFORMED_REDO)) != 0;
}







bool shouldTryQueuedInitialAutoTableFormatForNotification(UINT notificationCode)
{
	return notificationCode != NPPN_FILEOPENED;
}

bool shouldDeferInitialAutoTableFormatForDocumentLength(LRESULT textLength)
{
	return textLength <= 0;
}

// Automatic work runs on the editor thread. Bound it before reading text;
// explicit table commands remain available regardless of document size.
const LRESULT automaticDocumentByteLimit = 1024 * 1024;
const LRESULT automaticDocumentLineLimit = 10000;
const std::size_t automaticDocumentOutputByteLimit = 1024 * 1024;
const Sci_Position automaticTableByteLimit = 128 * 1024;
const std::size_t automaticTableLineLimit = 500;
const std::size_t automaticTableColumnLimit = 64;
const std::size_t automaticTableCellLimit = 8192;
const std::size_t automaticTableOutputByteLimit = 256 * 1024;

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
	if (scintilla)
		g_runtime.autoFormat.rememberWidth(editorView(scintilla), currentBufferId(), availableDisplayColumns(scintilla));
}

bool fitCurrentTableToWindow(bool quiet = true, CaretPlacement caretPlacement = CaretPlacement::ActionTarget)
{
	AutoFormatOperation operation(g_runtime.autoFormat, FormatMode::Fit);
	return operation && runTableAction(MarkdownTable::Action::WrapLongCells, quiet, caretPlacement);
}

bool autoAlignCurrentTable(bool quiet = true, CaretPlacement caretPlacement = CaretPlacement::ActionTarget)
{
	AutoFormatOperation operation(g_runtime.autoFormat, FormatMode::Align);
	return operation && runTableAction(MarkdownTable::Action::Align, quiet, caretPlacement);
}

bool executeDocumentAutoFormat(FormatMode mode)
{
	AutoFormatOperation operation(g_runtime.autoFormat, mode);
	if (!operation)
		return false;
	const bool changed = runAutoTableFormatForDocument(mode == FormatMode::Fit ? MarkdownTable::Action::WrapLongCells : MarkdownTable::Action::Align);
	if (mode == FormatMode::Fit)
		rememberCurrentFitToWindowWidth();
	return changed;
}

bool autoFormatCurrentDocumentAfterGlobalEdit()
{
	HWND scintilla = currentScintilla();
	const FormatMode mode = g_runtime.autoFormat.onGlobalModified(editorView(scintilla), currentBufferId(), scintilla != NULL, selectionEmpty(scintilla));
	return executeDocumentAutoFormat(mode);
}

void fitToWindowAfterResize(HWND resizedScintilla)
{
	HWND scintilla = currentScintilla();
	const bool active = scintilla && scintilla == resizedScintilla;
	const FormatMode mode = g_runtime.autoFormat.takeResize(editorView(resizedScintilla), currentBufferId(), active,
		active ? availableDisplayColumns(scintilla) : 0, active && selectionEmpty(scintilla));
	if (mode == FormatMode::Fit)
		fitCurrentTableToWindow(true, CaretPlacement::PreserveCellOffset);
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
	if (!resizedScintilla || resizedScintilla != currentScintilla())
		return;
	const EditorView view = editorView(resizedScintilla);
	if (!g_runtime.autoFormat.scheduleResize(view, currentBufferId()))
		return;
	if (!::SetTimer(resizedScintilla, fitToWindowResizeTimerId, fitToWindowResizeDelayMs, NULL))
		g_runtime.autoFormat.cancelResize(view);
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
	const bool active = scintilla && scintilla == updatedScintilla;
	const FormatMode mode = g_runtime.autoFormat.onContentUpdate(editorView(updatedScintilla), currentBufferId(), active,
		contentUpdated, active && selectionEmpty(scintilla));
	if (mode == FormatMode::Fit)
		fitCurrentTableToWindow(true, CaretPlacement::PreserveCellOffset);
	else if (mode == FormatMode::Align)
		autoAlignCurrentTable(true, CaretPlacement::PreserveCellOffset);
}

uptr_t currentBufferId()
{
	if (!nppData._nppHandle)
		return 0;
	return static_cast<uptr_t>(::SendMessage(nppData._nppHandle, NPPM_GETCURRENTBUFFERID, 0, 0));
}

void runInitialAutoTableFormatForBuffer(const SCNotification *notification)
{
	const uptr_t activeBuffer = currentBufferId();
	const uptr_t buffer = notification && notification->nmhdr.idFrom ? notification->nmhdr.idFrom : activeBuffer;
	if (notification && (notification->nmhdr.code == NPPN_FILEOPENED || notification->nmhdr.code == NPPN_BUFFERACTIVATED))
		g_runtime.autoFormat.queueInitial(buffer);
	if (notification && !shouldTryQueuedInitialAutoTableFormatForNotification(notification->nmhdr.code))
		return;
	HWND scintilla = currentScintilla();
	const bool active = scintilla && activeBuffer == buffer;
	const bool hasText = active && !shouldDeferInitialAutoTableFormatForDocumentLength(::SendMessage(scintilla, SCI_GETLENGTH, 0, 0));
	const FormatMode mode = g_runtime.autoFormat.takeInitial(editorView(scintilla), buffer, active, hasText, active && selectionEmpty(scintilla));
	executeDocumentAutoFormat(mode);
}

void runPendingInitialAutoTableFormatForCurrentBuffer()
{
	if (g_runtime.autoFormat.initialPending(currentBufferId()))
		runInitialAutoTableFormatForBuffer(NULL);
}

void runInitialAutoTableFormatForCurrentBuffer()
{
	g_runtime.autoFormat.queueInitial(currentBufferId());
	runInitialAutoTableFormatForBuffer(NULL);
}

void handleScintillaUpdateUiInternal(const SCNotification *notification)
{
	if (!notification)
		return;

	const bool contentUpdated = (notification->updated & SC_UPDATE_CONTENT) != 0;
	if (contentUpdated && currentScintilla() == reinterpret_cast<HWND>(notification->nmhdr.hwndFrom))
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
	if (!shouldScheduleFitToWindowAfterZoom(g_runtime.autoFormat.autoFitEnabled(), g_runtime.autoFormat.fitInProgress(), activeEditor))
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
	{
		::KillTimer(scintilla, fitToWindowResizeTimerId);
		g_runtime.autoFormat.cancelResize(editorView(scintilla));
	}
}

void cancelFitToWindowResizeTimers()
{
	cancelFitToWindowResizeTimer(nppData._scintillaMainHandle);
	cancelFitToWindowResizeTimer(nppData._scintillaSecondHandle);
}

const UINT_PTR fitToWindowScintillaSubclassId = 0x4D544553;
const UINT_PTR fitToWindowNppSubclassId = 0x4D54454E;

LRESULT CALLBACK fitToWindowScintillaProc(
	HWND hwnd,
	UINT message,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR /*subclassId*/,
	DWORD_PTR /*referenceData*/)
{
	if (message == WM_TIMER && wParam == fitToWindowResizeTimerId)
	{
		::KillTimer(hwnd, fitToWindowResizeTimerId);
		fitToWindowAfterResize(hwnd);
		return 0;
	}

	std::size_t preservedEnterColumn = 0;
	const bool preserveEnterColumn = captureEnterColumnToPreserve(hwnd, message, wParam, preservedEnterColumn);
	const LRESULT result = ::DefSubclassProc(hwnd, message, wParam, lParam);
	if (preserveEnterColumn)
		restoreEnterColumn(hwnd, preservedEnterColumn);
	if (shouldScheduleFitToWindowAfterResizeMessage(message, wParam, lParam))
		scheduleFitToWindowAfterResize(hwnd);
	return result;
}

LRESULT CALLBACK fitToWindowNppProc(
	HWND hwnd,
	UINT message,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR /*subclassId*/,
	DWORD_PTR /*referenceData*/)
{
	const LRESULT result = ::DefSubclassProc(hwnd, message, wParam, lParam);
	if (shouldScheduleFitToWindowAfterResizeMessage(message, wParam, lParam))
		scheduleFitToWindowAfterResize(currentScintilla());
	return result;
}

void subclassScintillaWindow(HWND hwnd, bool &installed)
{
	if (!hwnd || installed)
		return;
	installed = ::SetWindowSubclass(hwnd, fitToWindowScintillaProc, fitToWindowScintillaSubclassId, 0) != FALSE;
}

void subclassNppWindow()
{
	if (!nppData._nppHandle || g_runtime.nppSubclassInstalled)
		return;
	g_runtime.nppSubclassInstalled = ::SetWindowSubclass(nppData._nppHandle, fitToWindowNppProc, fitToWindowNppSubclassId, 0) != FALSE;
}

void removeScintillaSubclass(HWND hwnd, bool &installed)
{
	if (hwnd && installed)
		::RemoveWindowSubclass(hwnd, fitToWindowScintillaProc, fitToWindowScintillaSubclassId);
	installed = false;
}

void removeNppSubclass()
{
	if (nppData._nppHandle && g_runtime.nppSubclassInstalled)
		::RemoveWindowSubclass(nppData._nppHandle, fitToWindowNppProc, fitToWindowNppSubclassId);
	g_runtime.nppSubclassInstalled = false;
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
	if (lineEndPosition(hwnd, static_cast<std::size_t>(line)) - lineStartPosition(hwnd, static_cast<std::size_t>(line)) > automaticTableByteLimit)
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
	if (lengthResult <= 0)
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
	const std::vector<MarkdownTable::TableRange> coreRanges = MarkdownTable::findTableRanges(lines);
	for (std::size_t i = 0; i < coreRanges.size(); ++i)
	{
		DocumentTableRange range;
		range.firstLine = coreRanges[i].firstRow;
		range.lastLine = coreRanges[i].lastRow;
		ranges.push_back(range);
	}
	return ranges;
}

bool automaticTableWorkAllowed(const std::vector<std::string> &lines)
{
	if (lines.empty())
		return false;
	std::size_t estimatedColumns = 1;
	std::size_t maximumLineBytes = 0;
	for (std::size_t i = 0; i < lines.size(); ++i)
	{
		// Counting every pipe intentionally overestimates escaped/outer delimiters.
		estimatedColumns = (std::max)(estimatedColumns, 1 + static_cast<std::size_t>(std::count(lines[i].begin(), lines[i].end(), '|')));
		maximumLineBytes = (std::max)(maximumLineBytes, lines[i].size());
	}
	if (estimatedColumns > automaticTableColumnLimit || lines.size() > automaticTableCellLimit / estimatedColumns)
		return false;
	// Each cell is no wider than its source line; allow delimiters, padding and EOL.
	return maximumLineBytes + 4 <= automaticTableOutputByteLimit / (estimatedColumns * lines.size());
}

bool collectDocumentTableReplacement(HWND scintilla, const std::vector<std::string> &documentLines, std::size_t lineCount, MarkdownTable::Action action, const DocumentTableRange &range, DocumentTableReplacement &replacement)
{
	if (range.firstLine >= documentLines.size() || range.lastLine >= documentLines.size() || range.firstLine > range.lastLine)
		return false;

	std::vector<std::string> tableLines;
	for (std::size_t i = range.firstLine; i <= range.lastLine; ++i)
		tableLines.push_back(documentLines[i]);
	if (!automaticTableWorkAllowed(tableLines))
		return false;

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
	if (!scintilla || ::SendMessage(scintilla, SCI_GETREADONLY, 0, 0))
		return false;
	const LRESULT textLength = ::SendMessage(scintilla, SCI_GETLENGTH, 0, 0);
	if (textLength <= 0 || textLength > automaticDocumentByteLimit)
		return false;

	const LRESULT lineCountResult = ::SendMessage(scintilla, SCI_GETLINECOUNT, 0, 0);
	if (lineCountResult <= 0 || lineCountResult > automaticDocumentLineLimit)
		return false;
	const std::size_t lineCount = static_cast<std::size_t>(lineCountResult);
	const std::vector<std::string> documentLines = readDocumentLines(scintilla, lineCount);
	const std::vector<DocumentTableRange> ranges = findDocumentTableRanges(documentLines);
	if (ranges.empty())
		return false;

	std::vector<DocumentTableReplacement> replacements;
	std::size_t replacementBytes = 0;
	for (std::size_t i = 0; i < ranges.size(); ++i)
	{
		if (ranges[i].lastLine - ranges[i].firstLine + 1 > automaticTableLineLimit ||
			lineEndPosition(scintilla, ranges[i].lastLine) - lineStartPosition(scintilla, ranges[i].firstLine) > automaticTableByteLimit)
			continue;
		DocumentTableReplacement replacement;
		if (collectDocumentTableReplacement(scintilla, documentLines, lineCount, action, ranges[i], replacement))
		{
			if (replacement.text.size() > automaticDocumentOutputByteLimit - replacementBytes)
				return false;
			replacementBytes += replacement.text.size();
			replacements.push_back(replacement);
		}
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

std::string delimitedReplacementText(const std::string &source, const MarkdownTable::EditResult &edit, const std::string &eol)
{
	std::string replacement = joinLines(edit.lines, eol);
	std::size_t end = source.size();
	while (end > 0 && (source[end - 1] == '\r' || source[end - 1] == '\n'))
	{
		const char last = source[--end];
		if (last == '\n' && end > 0 && source[end - 1] == '\r')
			--end;
		replacement += eol;
	}
	return replacement;
}

void replaceRangeWithEdit(HWND scintilla, Sci_Position start, Sci_Position end, const std::string &source, const MarkdownTable::EditResult &edit)
{
	const std::string eol = chooseEol(source, currentEol(scintilla));
	const std::string replacement = delimitedReplacementText(source, edit, eol);
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
			HWND labelColumns = ::CreateWindowEx(0, TEXT("STATIC"), text.tableSizeColumnsLabel, WS_CHILD | WS_VISIBLE, 18, 20, 110, 22, hwnd, NULL, g_runtime.moduleHandle, NULL);
			HWND editColumns = ::CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, 140, 18, 120, 24, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(tableSizeColumnsId)), g_runtime.moduleHandle, NULL);
			HWND labelRows = ::CreateWindowEx(0, TEXT("STATIC"), text.tableSizeRowsLabel, WS_CHILD | WS_VISIBLE, 18, 54, 110, 22, hwnd, NULL, g_runtime.moduleHandle, NULL);
			HWND editRows = ::CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, 140, 52, 120, 24, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(tableSizeRowsId)), g_runtime.moduleHandle, NULL);
			HWND ok = ::CreateWindowEx(0, TEXT("BUTTON"), text.okButton, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 86, 96, 78, 26, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)), g_runtime.moduleHandle, NULL);
			HWND cancel = ::CreateWindowEx(0, TEXT("BUTTON"), text.cancelButton, WS_CHILD | WS_VISIBLE | WS_TABSTOP, 176, 96, 78, 26, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)), g_runtime.moduleHandle, NULL);

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
		wc.hInstance = g_runtime.moduleHandle;
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
	HWND dialog = ::CreateWindowEx(WS_EX_DLGMODALFRAME, className, uiText().tableSizeTitle, WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, width, height, nppData._nppHandle, NULL, g_runtime.moduleHandle, &state);
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

bool startsWithTablePipe(const std::string &line)
{
	const std::size_t first = line.find_first_not_of(" \t\r\n\f\v");
	return first != std::string::npos && line[first] == '|';
}

bool oversizedNeighborMayBelongToTable(HWND scintilla, std::size_t line, const std::vector<std::string> &lines, const MarkdownTable::TableRange &range)
{
	if (!startsWithTablePipe(lines[range.firstRow]) || !startsWithTablePipe(lines[range.firstRow + 1]))
		return true; // An optional-leading-pipe table can continue anywhere in the oversized line.
	const Sci_Position start = lineStartPosition(scintilla, line);
	const std::string prefix = getRangeText(scintilla, start, start + 64);
	const std::size_t first = prefix.find_first_not_of(" \t\r\n\f\v");
	// Unknown (long indentation) must not turn an incomplete table into an editable prefix.
	return first == std::string::npos || prefix[first] == '|';
}

bool runTableAction(MarkdownTable::Action action, bool quiet, CaretPlacement caretPlacement)
{
	HWND scintilla = currentScintilla();
	if (!scintilla || (quiet && ::SendMessage(scintilla, SCI_GETREADONLY, 0, 0)))
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

	const Sci_Position currentLineStart = lineStartPosition(scintilla, currentLine);
	const Sci_Position currentLineEnd = lineEndPosition(scintilla, currentLine);
	if (quiet && currentLineEnd - currentLineStart > automaticTableByteLimit)
		return false;
	const std::string currentLineText = getLineText(scintilla, currentLine);
	if (!MarkdownTable::isPotentialTableLine(currentLineText))
	{
		if (!quiet)
			showMessage(uiText().putCaretInsideTable);
		return false;
	}

	std::size_t potentialFirstLine = currentLine;
	bool truncatedBefore = false;
	bool oversizedBefore = false;
	while (potentialFirstLine > 0)
	{
		if (quiet && lineEndPosition(scintilla, potentialFirstLine - 1) - lineStartPosition(scintilla, potentialFirstLine - 1) > automaticTableByteLimit)
		{
			oversizedBefore = true;
			break;
		}
		if (!MarkdownTable::isPotentialTableLine(getLineText(scintilla, potentialFirstLine - 1)))
			break;
		if (quiet && (currentLine - potentialFirstLine + 1 >= automaticTableLineLimit ||
			currentLineEnd - lineStartPosition(scintilla, potentialFirstLine - 1) > automaticTableByteLimit))
		{
			--potentialFirstLine;
			truncatedBefore = true;
			break;
		}
		--potentialFirstLine;
	}

	std::size_t potentialLastLine = currentLine;
	bool oversizedAfter = false;
	while (potentialLastLine + 1 < lineCount)
	{
		if (quiet && lineEndPosition(scintilla, potentialLastLine + 1) - lineStartPosition(scintilla, potentialLastLine + 1) > automaticTableByteLimit)
		{
			oversizedAfter = true;
			break;
		}
		if (!MarkdownTable::isPotentialTableLine(getLineText(scintilla, potentialLastLine + 1)))
			break;
		if (quiet && (potentialLastLine - currentLine + 1 >= automaticTableLineLimit ||
			lineEndPosition(scintilla, potentialLastLine + 1) - currentLineStart > automaticTableByteLimit))
		{
			// Keep one lookahead row so the core can distinguish a table continuation
			// from pipe-containing prose before applying limits to the actual table.
			++potentialLastLine;
			break;
		}
		++potentialLastLine;
	}

	std::vector<std::string> candidateLines;
	for (std::size_t i = potentialFirstLine; i <= potentialLastLine; ++i)
		candidateLines.push_back(getLineText(scintilla, i));

	const MarkdownTable::TableRange tableRange = MarkdownTable::findTableRange(candidateLines, coreIndex(currentLine - potentialFirstLine));
	if (quiet && truncatedBefore && tableRange.found && tableRange.firstRow == 0)
		return false;
	if (!tableRange.found)
	{
		if (!quiet)
			showMessage(uiText().couldNotEditTable);
		return false;
	}

	const std::size_t firstLine = potentialFirstLine + tableRange.firstRow;
	const std::size_t lastLine = potentialFirstLine + tableRange.lastRow;
	if (quiet && ((oversizedBefore && tableRange.firstRow == 0 &&
		oversizedNeighborMayBelongToTable(scintilla, potentialFirstLine - 1, candidateLines, tableRange)) ||
		(oversizedAfter && tableRange.lastRow + 1 == candidateLines.size() &&
		oversizedNeighborMayBelongToTable(scintilla, potentialLastLine + 1, candidateLines, tableRange))))
		return false;
	if (quiet && (lastLine - firstLine + 1 > automaticTableLineLimit ||
		lineEndPosition(scintilla, lastLine) - lineStartPosition(scintilla, firstLine) > automaticTableByteLimit))
		return false;
	std::vector<std::string> tableLines;
	for (std::size_t i = tableRange.firstRow; i <= tableRange.lastRow; ++i)
		tableLines.push_back(candidateLines[i]);
	if (quiet && !automaticTableWorkAllowed(tableLines))
		return false;

	const std::size_t row = currentLine - firstLine;
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
	if (::SendMessage(scintilla, SCI_GETREADONLY, 0, 0))
		return false;

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
	g_runtime.autoFormat.forgetBuffer(notification ? notification->nmhdr.idFrom : 0);
}

#ifdef MARKDOWN_TABLE_PLUGIN_TESTING
namespace MarkdownTablePluginTesting
{
void resetAutoFormatStateForTests()
{
	g_runtime.autoFormat = AutoFormatCoordinator();
}

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
	const std::string eol = chooseEol(source, fallback);
	ReplacementPreview preview = replacementPreviewForTests(edit, eol);
	preview.text = delimitedReplacementText(source, edit, eol);
	return preview;
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
	return g_runtime.localization.commandText(index);
}

const wchar_t *commandMenuTextForTests(std::size_t index)
{
	return commandMenuText(index);
}

bool autoFitTableEnabledForTests()
{
	return g_runtime.autoFormat.autoFitEnabled();
}

void setAutoFitTableEnabledForTests(bool enabled)
{
	g_runtime.autoFormat.setAutoFitEnabled(enabled);
	if (!enabled)
		g_runtime.wordWrapWarningComboActive = false;
}

bool autoAlignTableEnabledForTests()
{
	return g_runtime.autoFormat.autoAlignEnabled();
}

void setAutoAlignTableEnabledForTests(bool enabled)
{
	g_runtime.autoFormat.setAutoAlignEnabled(enabled);
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

bool autoFormatDocumentForTests()
{
	return runAutoTableFormatForDocument(MarkdownTable::Action::Align);
}

bool alignCurrentTableForTests(bool automatic)
{
	return runTableAction(MarkdownTable::Action::Align, automatic, CaretPlacement::PreserveCellOffset);
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

bool commandHasToolbarButtonForTests(std::size_t commandIndex)
{
	return commandHasToolbarButton(commandIndex);
}

std::size_t toolbarButtonCountForTests()
{
	std::size_t count = 0;
	for (std::size_t commandIndex = 0; commandIndex < static_cast<std::size_t>(nbFunc); ++commandIndex)
	{
		if (commandHasToolbarButton(commandIndex))
			++count;
	}
	return count;
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
	g_runtime.moduleHandle = reinterpret_cast<HINSTANCE>(hModule);
}

//
// Here you can do the clean up, save the parameters (if any) for the next session
//
void pluginCleanUp()
{
	removeFitToWindowResizeHooks();
	destroyAllToolbarIconHandles();
	g_runtime.autoFormat = AutoFormatCoordinator();
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
	setCommand(autoAlignTableCommandIndex, commandMenuText(autoAlignTableCommandIndex), toggleAutoAlignTable, &g_autoAlignTableShortcut, g_runtime.autoFormat.autoAlignEnabled());
	setCommand(wrapLongCellsCommandIndex, commandMenuText(wrapLongCellsCommandIndex), wrapLongCells, &g_wrapLongCellsShortcut, false);
	setCommand(autoFitTableCommandIndex, commandMenuText(autoFitTableCommandIndex), toggleAutoFitTable, &g_autoFitTableShortcut, g_runtime.autoFormat.autoFitEnabled());
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
		static_cast<LPARAM>(g_runtime.autoFormat.autoFitEnabled() ? TRUE : FALSE));
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
		static_cast<LPARAM>(g_runtime.autoFormat.autoAlignEnabled() ? TRUE : FALSE));
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
		if (!commandHasToolbarButton(commandIndex))
			continue;
		if (funcItem[commandIndex]._cmdID != 0 && ensureCommandToolbarIconHandles(commandIndex))
		{
			registerToolbarIcon(
				commandIndex,
				g_runtime.toolbarBmps[commandIndex],
				g_runtime.toolbarIcons[commandIndex],
				g_runtime.toolbarIconDarkModes[commandIndex]);
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
	const bool enabling = !g_runtime.autoFormat.autoFitEnabled();
	if (enabling)
		fitCurrentTableToWindow(true);
	g_runtime.autoFormat.setAutoFitEnabled(enabling);
	if (enabling)
		rememberCurrentFitToWindowWidth();
	else
	{
		cancelFitToWindowResizeTimers();
		g_runtime.wordWrapWarningComboActive = false;
	}
	refreshAutoFitTableUi();
	refreshAutoAlignTableUi();
	if (enabling)
		checkWordWrapAutoFitWarningInternal();
}

void toggleAutoAlignTable()
{
	if (!autoAlignTableToggleEnabled())
		return;
	const bool enabling = !g_runtime.autoFormat.autoAlignEnabled();
	if (enabling)
		autoAlignCurrentTable(true);
	g_runtime.autoFormat.setAutoAlignEnabled(enabling);
	refreshAutoAlignTableUi();
}

void installFitToWindowResizeHooks()
{
	subclassNppWindow();
	subclassScintillaWindow(nppData._scintillaMainHandle, g_runtime.mainScintillaSubclassInstalled);
	subclassScintillaWindow(nppData._scintillaSecondHandle, g_runtime.secondScintillaSubclassInstalled);
	rememberCurrentFitToWindowWidth();
}

void removeFitToWindowResizeHooks()
{
	cancelFitToWindowResizeTimers();
	removeScintillaSubclass(nppData._scintillaMainHandle, g_runtime.mainScintillaSubclassInstalled);
	removeScintillaSubclass(nppData._scintillaSecondHandle, g_runtime.secondScintillaSubclassInstalled);
	removeNppSubclass();
}
