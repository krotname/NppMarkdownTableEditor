// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 krotname

#include "PluginShortcutTests.h"
#include "../src/PluginDefinition.h"

#include <cwchar>
#include <iostream>
#include <set>
#include <sstream>
#include <string>

extern FuncItem funcItem[nbFunc];

namespace
{
struct ExpectedCommand
{
	std::size_t index;
	const char *label;
	const wchar_t *itemName;
	PFUNCPLUGINCMD function;
	bool ctrl;
	bool alt;
	bool shift;
	UCHAR key;
};

void fail(int &failures, const std::string &name, const std::string &message)
{
	++failures;
	std::cerr << name << " failed: " << message << "\n";
}

void expectTrue(int &failures, const std::string &name, bool value)
{
	if (!value)
		fail(failures, name, "expected true");
}

void expectSize(int &failures, const std::string &name, std::size_t actual, std::size_t expected)
{
	if (actual == expected)
		return;

	std::ostringstream message;
	message << "expected " << expected << ", got " << actual;
	fail(failures, name, message.str());
}

void expectCommandName(int &failures, const ExpectedCommand &expected, const FuncItem &actual)
{
	if (std::wcscmp(actual._itemName, expected.itemName) == 0)
		return;

	fail(failures, std::string(expected.label) + " name", "unexpected menu item name");
}

std::string shortcutId(const ShortcutKey &shortcut)
{
	std::ostringstream id;
	id
		<< shortcut._isCtrl << ':'
		<< shortcut._isAlt << ':'
		<< shortcut._isShift << ':'
		<< static_cast<unsigned int>(shortcut._key);
	return id.str();
}

void expectShortcut(int &failures, const ExpectedCommand &expected, const ShortcutKey &actual)
{
	expectTrue(failures, std::string(expected.label) + " ctrl modifier", actual._isCtrl == expected.ctrl);
	expectTrue(failures, std::string(expected.label) + " alt modifier", actual._isAlt == expected.alt);
	expectTrue(failures, std::string(expected.label) + " shift modifier", actual._isShift == expected.shift);
	expectSize(failures, std::string(expected.label) + " key", static_cast<std::size_t>(actual._key), static_cast<std::size_t>(expected.key));
}

}

int runPluginShortcutTests()
{
	int failures = 0;
	commandMenuInit();

	const ExpectedCommand expected[] =
	{
		{ 0, "align", L"Align table", alignTable, true, true, true, static_cast<UCHAR>('1') },
		{ 1, "next cell", L"Next cell", nextCell, true, true, true, static_cast<UCHAR>('2') },
		{ 2, "previous cell", L"Previous cell", previousCell, true, true, true, static_cast<UCHAR>('3') },
		{ 3, "insert row", L"Insert row below", insertRowBelow, true, true, true, static_cast<UCHAR>('4') },
		{ 4, "delete row", L"Delete row", deleteRow, true, true, true, static_cast<UCHAR>('5') },
		{ 5, "insert column", L"Insert column right", insertColumnRight, true, true, true, static_cast<UCHAR>('6') },
		{ 6, "delete column", L"Delete column", deleteColumn, true, true, true, static_cast<UCHAR>('7') },
		{ 7, "move row up", L"Move row up", moveRowUp, true, true, true, static_cast<UCHAR>('8') },
		{ 8, "move row down", L"Move row down", moveRowDown, true, true, true, static_cast<UCHAR>('9') },
		{ 9, "move column left", L"Move column left", moveColumnLeft, true, true, true, static_cast<UCHAR>(VK_OEM_4) },
		{ 10, "move column right", L"Move column right", moveColumnRight, true, true, true, static_cast<UCHAR>(VK_OEM_6) },
		{ 11, "sort ascending", L"Sort rows ascending", sortRowsAscending, true, true, true, static_cast<UCHAR>(VK_OEM_PLUS) },
		{ 12, "sort descending", L"Sort rows descending", sortRowsDescending, true, true, true, static_cast<UCHAR>(VK_OEM_MINUS) },
		{ 13, "convert csv tsv", L"Convert CSV/TSV to table", convertCsvTsvSelectionToTable, true, true, true, static_cast<UCHAR>('0') },
		{ 14, "insert table", L"Insert table...", insertTable, true, true, true, static_cast<UCHAR>(VK_OEM_5) },
		{ 15, "tab", L"Tab: align table or indent", tabOrIndent, false, false, false, static_cast<UCHAR>(VK_TAB) }
	};

	const std::size_t expectedCount = sizeof(expected) / sizeof(expected[0]);
	expectSize(failures, "Notepad++ command count", static_cast<std::size_t>(nbFunc), expectedCount);

	std::set<std::string> shortcuts;
	for (std::size_t index = 0; index < expectedCount; ++index)
	{
		const ExpectedCommand &expectedCommand = expected[index];
		const FuncItem &actual = funcItem[expectedCommand.index];
		expectCommandName(failures, expectedCommand, actual);
		expectTrue(failures, std::string(expectedCommand.label) + " function binding", actual._pFunc == expectedCommand.function);
		expectTrue(failures, std::string(expectedCommand.label) + " shortcut exists", actual._pShKey != nullptr);
		expectTrue(failures, std::string(expectedCommand.label) + " is unchecked by default", !actual._init2Check);
		if (actual._pShKey == nullptr)
			continue;

		expectShortcut(failures, expectedCommand, *actual._pShKey);
		const std::string id = shortcutId(*actual._pShKey);
		expectTrue(failures, std::string(expectedCommand.label) + " shortcut is unique", shortcuts.insert(id).second);
	}

	return failures;
}
