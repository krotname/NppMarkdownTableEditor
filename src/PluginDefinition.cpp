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

std::size_t positionForLineColumn(const std::vector<DocumentLine> &lines, std::size_t firstLine, const std::vector<std::string> &replacementLines, const std::string &eol, std::size_t row, std::size_t columnOffset)
{
	std::size_t position = lines[firstLine].start;
	for (std::size_t i = 0; i < row && i < replacementLines.size(); ++i)
		position += replacementLines[i].size() + eol.size();
	return position + columnOffset;
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
}

//
// Initialize your plugin data here
// It will be called while plugin loading   
void pluginInit(HANDLE /*hModule*/)
{
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
	setCommand(11, TEXT("Tab: align table or indent"), tabOrIndent, &g_tabShortcut, false);
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

void tabOrIndent()
{
	if (runTableAction(MarkdownTable::Action::Align, true))
		return;

	HWND scintilla = currentScintilla();
	if (scintilla)
		::SendMessage(scintilla, SCI_TAB, 0, 0);
}
