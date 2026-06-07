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
#include "menuCmdID.h"

#include <algorithm>
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
struct DocumentLine
{
	std::string text;
	std::string eol;
	std::size_t start = 0;
	std::size_t contentEnd = 0;
	std::size_t fullEnd = 0;
};

struct TableSizeDialogState
{
	std::size_t columns = 3;
	std::size_t dataRows = 2;
	bool accepted = false;
};

const int tableSizeColumnsId = 1001;
const int tableSizeRowsId = 1002;
const UINT tableSizeMaxColumns = 20;
const UINT tableSizeMaxRows = 50;

HINSTANCE g_moduleHandle = NULL;

ShortcutKey g_alignShortcut = { true, true, false, 'A' };
ShortcutKey g_nextCellShortcut = { true, true, false, VK_RIGHT };
ShortcutKey g_previousCellShortcut = { true, true, false, VK_LEFT };
ShortcutKey g_insertRowShortcut = { true, true, false, VK_DOWN };
ShortcutKey g_deleteRowShortcut = { true, true, false, VK_UP };
ShortcutKey g_insertColumnShortcut = { true, true, true, VK_RIGHT };
ShortcutKey g_deleteColumnShortcut = { true, true, true, VK_LEFT };
ShortcutKey g_tabShortcut = { false, false, false, VK_TAB };

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
	::MessageBox(nppData._nppHandle, message, NPP_PLUGIN_NAME, MB_OK | MB_ICONINFORMATION);
}

std::string getText(HWND scintilla)
{
	const LRESULT lengthResult = ::SendMessage(scintilla, SCI_GETLENGTH, 0, 0);
	if (lengthResult <= 0)
		return std::string();

	const std::size_t length = static_cast<std::size_t>(lengthResult);
	std::vector<char> buffer(length + 1, '\0');
	::SendMessage(scintilla, SCI_GETTEXT, static_cast<WPARAM>(buffer.size()), reinterpret_cast<LPARAM>(&buffer[0]));
	return std::string(&buffer[0], length);
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

std::vector<DocumentLine> splitDocument(const std::string &text)
{
	std::vector<DocumentLine> lines;
	std::size_t pos = 0;

	while (pos < text.size())
	{
		DocumentLine line;
		line.start = pos;
		while (pos < text.size() && text[pos] != '\r' && text[pos] != '\n')
			++pos;

		line.contentEnd = pos;
		line.text = text.substr(line.start, line.contentEnd - line.start);

		if (pos < text.size())
		{
			if (text[pos] == '\r' && pos + 1 < text.size() && text[pos + 1] == '\n')
			{
				line.eol = "\r\n";
				pos += 2;
			}
			else
			{
				line.eol.assign(1, text[pos]);
				++pos;
			}
		}

		line.fullEnd = pos;
		lines.push_back(line);
	}

	if (lines.empty())
	{
		DocumentLine line;
		lines.push_back(line);
	}

	return lines;
}

std::string chooseEol(const std::vector<DocumentLine> &lines, std::size_t first, std::size_t last)
{
	for (std::size_t i = first; i <= last && i < lines.size(); ++i)
	{
		if (!lines[i].eol.empty())
			return lines[i].eol;
	}
	return "\r\n";
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

std::size_t positionForLineColumn(const std::vector<DocumentLine> &lines, std::size_t firstLine, const std::vector<std::string> &replacementLines, const std::string &eol, std::size_t row, std::size_t columnOffset)
{
	return lines[firstLine].start + offsetForLineColumn(replacementLines, eol, row, columnOffset);
}

void replaceSelectionWithEdit(HWND scintilla, const MarkdownTable::EditResult &edit)
{
	const LRESULT selectionStart = ::SendMessage(scintilla, SCI_GETSELECTIONSTART, 0, 0);
	const std::string eol = currentEol(scintilla);
	const std::string replacement = joinLines(edit.lines, eol);
	const std::size_t targetOffset = offsetForLineColumn(edit.lines, eol, edit.targetRow, edit.targetColumnOffset);

	::SendMessage(scintilla, SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(replacement.c_str()));
	::SendMessage(scintilla, SCI_GOTOPOS, static_cast<WPARAM>(selectionStart + static_cast<LRESULT>(targetOffset)), 0);
}

LRESULT CALLBACK tableSizeDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	TableSizeDialogState *state = reinterpret_cast<TableSizeDialogState *>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));

	switch (message)
	{
		case WM_CREATE:
		{
			CREATESTRUCT *create = reinterpret_cast<CREATESTRUCT *>(lParam);
			state = reinterpret_cast<TableSizeDialogState *>(create->lpCreateParams);
			::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

			HFONT font = reinterpret_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
			HWND labelColumns = ::CreateWindowEx(0, TEXT("STATIC"), TEXT("Columns:"), WS_CHILD | WS_VISIBLE, 18, 20, 110, 22, hwnd, NULL, g_moduleHandle, NULL);
			HWND editColumns = ::CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, 140, 18, 120, 24, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(tableSizeColumnsId)), g_moduleHandle, NULL);
			HWND labelRows = ::CreateWindowEx(0, TEXT("STATIC"), TEXT("Data rows:"), WS_CHILD | WS_VISIBLE, 18, 54, 110, 22, hwnd, NULL, g_moduleHandle, NULL);
			HWND editRows = ::CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, 140, 52, 120, 24, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(tableSizeRowsId)), g_moduleHandle, NULL);
			HWND ok = ::CreateWindowEx(0, TEXT("BUTTON"), TEXT("OK"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 86, 96, 78, 26, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)), g_moduleHandle, NULL);
			HWND cancel = ::CreateWindowEx(0, TEXT("BUTTON"), TEXT("Cancel"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 176, 96, 78, 26, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)), g_moduleHandle, NULL);

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
					::MessageBox(hwnd, TEXT("Use 1-20 columns and 0-50 data rows."), NPP_PLUGIN_NAME, MB_OK | MB_ICONWARNING);
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
	HWND dialog = ::CreateWindowEx(WS_EX_DLGMODALFRAME, className, TEXT("Insert Markdown table"), WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, width, height, nppData._nppHandle, NULL, g_moduleHandle, &state);
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

	const std::string text = getText(scintilla);
	std::vector<DocumentLine> lines = splitDocument(text);
	if (lines.empty())
		return false;

	const LRESULT currentPosResult = ::SendMessage(scintilla, SCI_GETCURRENTPOS, 0, 0);
	const LRESULT currentLineResult = ::SendMessage(scintilla, SCI_LINEFROMPOSITION, static_cast<WPARAM>(currentPosResult), 0);
	if (currentLineResult < 0)
		return false;

	std::size_t currentLine = static_cast<std::size_t>(currentLineResult);
	if (currentLine >= lines.size())
		currentLine = lines.size() - 1;

	if (!MarkdownTable::isPotentialTableLine(lines[currentLine].text))
	{
		if (!quiet)
			showMessage(L"Put the caret inside a Markdown pipe table first.");
		return false;
	}

	std::size_t firstLine = currentLine;
	while (firstLine > 0 && MarkdownTable::isPotentialTableLine(lines[firstLine - 1].text))
		--firstLine;

	std::size_t lastLine = currentLine;
	while (lastLine + 1 < lines.size() && MarkdownTable::isPotentialTableLine(lines[lastLine + 1].text))
		++lastLine;

	std::vector<std::string> tableLines;
	for (std::size_t i = firstLine; i <= lastLine; ++i)
		tableLines.push_back(lines[i].text);

	const std::size_t row = currentLine - firstLine;
	const std::size_t byteColumn = static_cast<std::size_t>((std::max)(static_cast<LRESULT>(0), currentPosResult - static_cast<LRESULT>(lines[currentLine].start)));
	const std::size_t column = MarkdownTable::columnFromCursor(lines[currentLine].text, byteColumn);
	MarkdownTable::EditResult edit = MarkdownTable::apply(tableLines, row, column, action);
	if (!edit.ok)
	{
		if (!quiet)
			showMessage(L"Could not edit the Markdown table.");
		return false;
	}

	const std::string eol = chooseEol(lines, firstLine, lastLine);
	const std::string replacement = joinLines(edit.lines, eol);
	const std::size_t targetPosition = positionForLineColumn(lines, firstLine, edit.lines, eol, edit.targetRow, edit.targetColumnOffset);

	::SendMessage(scintilla, SCI_SETTARGETRANGE, static_cast<WPARAM>(lines[firstLine].start), static_cast<LPARAM>(lines[lastLine].contentEnd));
	::SendMessage(scintilla, SCI_REPLACETARGET, static_cast<WPARAM>(replacement.size()), reinterpret_cast<LPARAM>(replacement.c_str()));
	::SendMessage(scintilla, SCI_GOTOPOS, static_cast<WPARAM>(targetPosition), 0);
	return true;
}

bool runConvertDelimitedSelection()
{
	HWND scintilla = currentScintilla();
	if (!scintilla)
		return false;

	const LRESULT selectionStart = ::SendMessage(scintilla, SCI_GETSELECTIONSTART, 0, 0);
	const LRESULT selectionEnd = ::SendMessage(scintilla, SCI_GETSELECTIONEND, 0, 0);
	if (selectionStart == selectionEnd)
	{
		showMessage(L"Select CSV or TSV text first.");
		return false;
	}

	const std::string selectedText = getSelectedText(scintilla);
	MarkdownTable::EditResult edit = MarkdownTable::convertDelimitedToTable(selectedText);
	if (!edit.ok)
	{
		showMessage(L"Could not convert the selected CSV/TSV text.");
		return false;
	}

	replaceSelectionWithEdit(scintilla, edit);
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

	MarkdownTable::EditResult edit = MarkdownTable::createTable(state.columns, state.dataRows);
	if (!edit.ok)
		return false;

	replaceSelectionWithEdit(scintilla, edit);
	return true;
}

std::string indentationText(HWND scintilla)
{
	const bool useTabs = ::SendMessage(scintilla, SCI_GETUSETABS, 0, 0) != 0;
	if (useTabs)
		return "\t";

	LRESULT tabWidth = ::SendMessage(scintilla, SCI_GETTABWIDTH, 0, 0);
	if (tabWidth <= 0)
		tabWidth = 4;
	return std::string(static_cast<std::size_t>(tabWidth), ' ');
}

void insertIndentation(HWND scintilla)
{
	const std::string indent = indentationText(scintilla);
	const LRESULT selectionStart = ::SendMessage(scintilla, SCI_GETSELECTIONSTART, 0, 0);
	const LRESULT selectionEnd = ::SendMessage(scintilla, SCI_GETSELECTIONEND, 0, 0);
	if (selectionStart < 0 || selectionEnd < 0)
		return;

	const LRESULT start = (std::min)(selectionStart, selectionEnd);
	const LRESULT end = (std::max)(selectionStart, selectionEnd);
	LRESULT startLine = ::SendMessage(scintilla, SCI_LINEFROMPOSITION, static_cast<WPARAM>(start), 0);
	LRESULT endLine = ::SendMessage(scintilla, SCI_LINEFROMPOSITION, static_cast<WPARAM>(end), 0);
	if (startLine < 0 || endLine < 0)
		return;

	if (start != end && endLine > startLine)
	{
		const LRESULT endLineStart = ::SendMessage(scintilla, SCI_POSITIONFROMLINE, static_cast<WPARAM>(endLine), 0);
		if (end == endLineStart)
			--endLine;
	}

	if (start == end || startLine == endLine)
	{
		::SendMessage(scintilla, SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(indent.c_str()));
		return;
	}

	::SendMessage(scintilla, SCI_BEGINUNDOACTION, 0, 0);
	for (LRESULT line = endLine; line >= startLine; --line)
	{
		const LRESULT position = ::SendMessage(scintilla, SCI_POSITIONFROMLINE, static_cast<WPARAM>(line), 0);
		::SendMessage(scintilla, SCI_INSERTTEXT, static_cast<WPARAM>(position), reinterpret_cast<LPARAM>(indent.c_str()));
	}
	::SendMessage(scintilla, SCI_ENDUNDOACTION, 0, 0);
}
}

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
	setCommand(0, TEXT("Align table"), alignTable, &g_alignShortcut, false);
	setCommand(1, TEXT("Next cell"), nextCell, &g_nextCellShortcut, false);
	setCommand(2, TEXT("Previous cell"), previousCell, &g_previousCellShortcut, false);
	setCommand(3, TEXT("Insert row below"), insertRowBelow, &g_insertRowShortcut, false);
	setCommand(4, TEXT("Delete row"), deleteRow, &g_deleteRowShortcut, false);
	setCommand(5, TEXT("Insert column right"), insertColumnRight, &g_insertColumnShortcut, false);
	setCommand(6, TEXT("Delete column"), deleteColumn, &g_deleteColumnShortcut, false);
	setCommand(7, TEXT("Move row up"), moveRowUp, NULL, false);
	setCommand(8, TEXT("Move row down"), moveRowDown, NULL, false);
	setCommand(9, TEXT("Move column left"), moveColumnLeft, NULL, false);
	setCommand(10, TEXT("Move column right"), moveColumnRight, NULL, false);
	setCommand(11, TEXT("Sort rows ascending"), sortRowsAscending, NULL, false);
	setCommand(12, TEXT("Sort rows descending"), sortRowsDescending, NULL, false);
	setCommand(13, TEXT("Convert CSV/TSV selection to table"), convertCsvTsvSelectionToTable, NULL, false);
	setCommand(14, TEXT("Insert table..."), insertTable, NULL, false);
	setCommand(15, TEXT("Tab: align table or indent"), tabOrIndent, &g_tabShortcut, false);
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
bool setCommand(size_t index, TCHAR *cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey *sk, bool check0nInit) 
{
    if (index >= nbFunc)
        return false;

    if (!pFunc)
        return false;

    lstrcpy(funcItem[index]._itemName, cmdName);
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
		insertIndentation(scintilla);
}
