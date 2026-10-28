// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 krotname

#include "PluginShortcutTests.h"
#include "../src/PluginSettings.h"
#include "../src/PluginDefinition.h"

#include <cwchar>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <set>
#include <sstream>
#include <string>

extern FuncItem funcItem[nbFunc];
extern NppData nppData;

namespace
{
struct ExpectedCommand
{
	std::size_t index;
	const char *label;
	const wchar_t *itemName;
	PFUNCPLUGINCMD function;
	bool checkedOnInit;
	bool hasShortcut;
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

struct EditorProbe
{
	LRESULT bytes = 127803254;
	LRESULT lines = 454045;
	LRESULT lineWidth = 12;
	bool readOnly = false;
	bool continuousTable = false;
	std::string document;
	LRESULT currentPosition = 2;
	int textReads = 0;
	int replacements = 0;
	std::string replacementText;
	std::size_t largestTextRead = 0;
	int activeView = 0;
	uptr_t buffer = 10;
	LRESULT selectionStart = 0;
	LRESULT selectionEnd = 0;
	int undoBegins = 0;
	int undoEnds = 0;
	bool reentrantNotifications = false;
} editorProbe;

std::size_t probeLineStart(std::size_t row)
{
	std::size_t position = 0;
	while (row-- > 0)
	{
		const std::size_t end = editorProbe.document.find('\n', position);
		if (end == std::string::npos)
			return editorProbe.document.size();
		position = end + 1;
	}
	return position;
}

LRESULT CALLBACK probeWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case NPPM_GETCURRENTSCINTILLA: *reinterpret_cast<int *>(lParam) = editorProbe.activeView; return TRUE;
	case NPPM_GETCURRENTBUFFERID: return static_cast<LRESULT>(editorProbe.buffer);
	case SCI_GETSELECTIONSTART: return editorProbe.selectionStart;
	case SCI_GETSELECTIONEND: return editorProbe.selectionEnd;
	case SCI_BEGINUNDOACTION: ++editorProbe.undoBegins; return 0;
	case SCI_ENDUNDOACTION: ++editorProbe.undoEnds; return 0;
	case SCI_GETREADONLY: return editorProbe.readOnly;
	case SCI_GETLENGTH: return editorProbe.document.empty() ? editorProbe.bytes : static_cast<LRESULT>(editorProbe.document.size());
	case SCI_GETLINECOUNT: return editorProbe.document.empty() ? editorProbe.lines : 1 + static_cast<LRESULT>(std::count(editorProbe.document.begin(), editorProbe.document.end(), '\n'));
	case SCI_GETCURRENTPOS: return editorProbe.currentPosition;
	case SCI_GOTOPOS: editorProbe.currentPosition = static_cast<LRESULT>(wParam); return 0;
	case SCI_LINEFROMPOSITION:
		return editorProbe.document.empty() ? static_cast<LRESULT>(wParam) / editorProbe.lineWidth :
			static_cast<LRESULT>(std::count(editorProbe.document.begin(), editorProbe.document.begin() + (std::min)(static_cast<std::size_t>(wParam), editorProbe.document.size()), '\n'));
	case SCI_POSITIONFROMLINE:
		return editorProbe.document.empty() ? static_cast<LRESULT>(wParam) * editorProbe.lineWidth : static_cast<LRESULT>(probeLineStart(static_cast<std::size_t>(wParam)));
	case SCI_GETLINEENDPOSITION:
	{
		if (editorProbe.document.empty())
			return (static_cast<LRESULT>(wParam) + 1) * editorProbe.lineWidth - 1;
		const std::size_t end = editorProbe.document.find('\n', probeLineStart(static_cast<std::size_t>(wParam)));
		return static_cast<LRESULT>(end == std::string::npos ? editorProbe.document.size() : end);
	}
	case SCI_GETTEXTRANGEFULL:
	{
		++editorProbe.textReads;
		Sci_TextRangeFull *range = reinterpret_cast<Sci_TextRangeFull *>(lParam);
		editorProbe.largestTextRead = (std::max)(editorProbe.largestTextRead, static_cast<std::size_t>(range->chrg.cpMax - range->chrg.cpMin));
		if (!editorProbe.document.empty())
		{
			const std::string text = editorProbe.document.substr(static_cast<std::size_t>(range->chrg.cpMin),
				static_cast<std::size_t>(range->chrg.cpMax - range->chrg.cpMin));
			std::memcpy(range->lpstrText, text.c_str(), text.size() + 1);
			return static_cast<LRESULT>(text.size());
		}
		std::string text;
		for (Sci_Position position = range->chrg.cpMin; position < range->chrg.cpMax; ++position)
		{
			const LRESULT row = position / editorProbe.lineWidth;
			const std::size_t column = static_cast<std::size_t>(position % editorProbe.lineWidth);
			const std::string line = row == 0 ? "| A | B |" : row == 1 ? "| - | - |" :
				(row == 2 || editorProbe.continuousTable) ? "| x | yy |" : "";
			text += column == static_cast<std::size_t>(editorProbe.lineWidth - 1) ? '\n' :
				column < line.size() ? line[column] : ' ';
		}
		std::memcpy(range->lpstrText, text.c_str(), text.size() + 1);
		return static_cast<LRESULT>(text.size());
	}
	case SCI_REPLACETARGET:
		++editorProbe.replacements;
		editorProbe.replacementText.assign(reinterpret_cast<const char *>(lParam), static_cast<std::size_t>(wParam));
		if (editorProbe.reentrantNotifications && editorProbe.replacements == 1)
		{
			SCNotification notification = {};
			notification.nmhdr.hwndFrom = hwnd;
			notification.modificationType = SC_MOD_INSERTTEXT;
			notification.updated = SC_UPDATE_CONTENT;
			handleScintillaModified(&notification);
			handleScintillaUpdateUi(&notification);
			handleGlobalModified();
		}
		return static_cast<LRESULT>(wParam);
	default: return ::DefWindowProc(hwnd, message, wParam, lParam);
	}
}

void testWarningSettingsPersistence(int &failures)
{
	wchar_t directory[MAX_PATH] = {};
	wchar_t path[MAX_PATH] = {};
	if (!::GetTempPath(MAX_PATH, directory) || !::GetTempFileName(directory, L"MTE", 0, path))
	{
		fail(failures, "warning settings", "could not create temporary INI");
		return;
	}
	MarkdownTablePlugin::PluginSettings settings;
	expectTrue(failures, "unavailable settings directory keeps default", !settings.wordWrapAutoFitWarningSuppressed(L""));
	expectTrue(failures, "write legacy warning INI key", ::WritePrivateProfileString(L"Warnings", L"HideWordWrapAutoFitWarning", L"1", path) != FALSE);
	expectTrue(failures, "settings retry load after directory becomes available", settings.wordWrapAutoFitWarningSuppressed(path));
	settings.setWordWrapAutoFitWarningSuppressed(path, false);
	MarkdownTablePlugin::PluginSettings reloaded;
	expectTrue(failures, "warning suppression persists under original INI key", !reloaded.wordWrapAutoFitWarningSuppressed(path) &&
		::GetPrivateProfileInt(L"Warnings", L"HideWordWrapAutoFitWarning", 1, path) == 0);
	expectTrue(failures, "temporary settings file removed", ::DeleteFile(path) != FALSE);
}

void testEditorNotificationTransitions(int &failures)
{
	WNDCLASS windowClass = {};
	windowClass.lpfnWndProc = probeWindowProc;
	windowClass.hInstance = ::GetModuleHandle(NULL);
	windowClass.lpszClassName = L"MarkdownTableEditorNotificationTest";
	::RegisterClass(&windowClass);
	HWND mainView = ::CreateWindow(windowClass.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, windowClass.hInstance, NULL);
	HWND secondView = ::CreateWindow(windowClass.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, windowClass.hInstance, NULL);
	expectTrue(failures, "create both Scintilla message probes", mainView && secondView);
	if (mainView && secondView)
	{
		const NppData savedData = nppData;
		nppData._nppHandle = mainView;
		nppData._scintillaMainHandle = mainView;
		nppData._scintillaSecondHandle = secondView;
		editorProbe = EditorProbe();
		MarkdownTablePluginTesting::resetAutoFormatStateForTests();
		MarkdownTablePluginTesting::setAutoFitTableEnabledForTests(false);
		SCNotification notification = {};
		notification.nmhdr.hwndFrom = secondView;
		notification.modificationType = SC_MOD_INSERTTEXT;
		notification.updated = SC_UPDATE_CONTENT;
		handleScintillaModified(&notification);
		expectTrue(failures, "inactive secondary notification does not edit main view", editorProbe.replacements == 0);
		editorProbe.activeView = 1;
		editorProbe.reentrantNotifications = true;
		handleScintillaModified(&notification);
		expectTrue(failures, "active secondary view formats once without recursive notifications", editorProbe.replacements == 1);
		expectTrue(failures, "automatic edit is one balanced undo action", editorProbe.undoBegins == 1 && editorProbe.undoEnds == 1);
		expectTrue(failures, "automatic edit retains caret within cell", editorProbe.currentPosition == 2);

		for (int modification : { SC_PERFORMED_UNDO, SC_PERFORMED_REDO })
		{
			editorProbe = EditorProbe();
			editorProbe.activeView = 1;
			MarkdownTablePluginTesting::resetAutoFormatStateForTests();
			MarkdownTablePluginTesting::setAutoFitTableEnabledForTests(false);
			notification.modificationType = modification;
			handleScintillaModified(&notification);
			expectTrue(failures, "undo/redo notifications retain automatic formatting path", editorProbe.replacements == 1 && editorProbe.undoBegins == 1 && editorProbe.undoEnds == 1);
		}
		editorProbe = EditorProbe();
		editorProbe.activeView = 1;
		editorProbe.selectionStart = 1;
		editorProbe.selectionEnd = 6;
		MarkdownTablePluginTesting::resetAutoFormatStateForTests();
		MarkdownTablePluginTesting::setAutoFitTableEnabledForTests(false);
		handleScintillaModified(&notification);
		handleGlobalModified();
		expectTrue(failures, "content and global notifications preserve selection", editorProbe.replacements == 0 && editorProbe.selectionStart == 1 && editorProbe.selectionEnd == 6);
		nppData = savedData;
		MarkdownTablePluginTesting::resetAutoFormatStateForTests();
	}
	if (mainView) ::DestroyWindow(mainView);
	if (secondView) ::DestroyWindow(secondView);
	::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
}

void testAutomaticWorkBounds(int &failures)
{
	WNDCLASS windowClass = {};
	windowClass.lpfnWndProc = probeWindowProc;
	windowClass.hInstance = ::GetModuleHandle(NULL);
	windowClass.lpszClassName = L"MarkdownTableEditorWorkBoundsTest";
	::RegisterClass(&windowClass);
	HWND window = ::CreateWindow(windowClass.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, windowClass.hInstance, NULL);
	expectTrue(failures, "create editor message probe", window != NULL);
	if (!window)
		return;
	const NppData savedData = nppData;
	nppData._nppHandle = window;
	nppData._scintillaMainHandle = window;
	const bool savedFit = MarkdownTablePluginTesting::autoFitTableEnabledForTests();
	MarkdownTablePluginTesting::setAutoFitTableEnabledForTests(false);

	editorProbe = EditorProbe();
	expectTrue(failures, "122 MiB automatic open avoids scanning text", !MarkdownTablePluginTesting::autoFormatDocumentForTests() && editorProbe.textReads == 0);
	editorProbe.bytes = 500000;
	expectTrue(failures, "many short lines avoid automatic document scan", !MarkdownTablePluginTesting::autoFormatDocumentForTests() && editorProbe.textReads == 0);
	editorProbe.lines = 4;
	editorProbe.readOnly = true;
	expectTrue(failures, "read-only document avoids formatting", !MarkdownTablePluginTesting::autoFormatDocumentForTests() && editorProbe.textReads == 0);
	expectTrue(failures, "read-only current table avoids formatting", !MarkdownTablePluginTesting::alignCurrentTableForTests(true) && editorProbe.textReads == 0);
	editorProbe.readOnly = false;
	editorProbe.lineWidth = 200000;
	expectTrue(failures, "oversized current line is rejected before allocation", !MarkdownTablePluginTesting::alignCurrentTableForTests(true) && editorProbe.textReads == 0);
	editorProbe = EditorProbe();
	editorProbe.continuousTable = true;
	expectTrue(failures, "huge table scan is bounded", !MarkdownTablePluginTesting::alignCurrentTableForTests(true) && editorProbe.textReads <= 1002 && editorProbe.replacements == 0);
	editorProbe = EditorProbe();
	expectTrue(failures, "small table in large document still auto aligns", MarkdownTablePluginTesting::alignCurrentTableForTests(true) && editorProbe.replacements == 1 && editorProbe.textReads < 20);
	editorProbe = EditorProbe();
	expectTrue(failures, "explicit table command works in large document", MarkdownTablePluginTesting::alignCurrentTableForTests(false) && editorProbe.replacements == 1);
	editorProbe = EditorProbe();
	editorProbe.bytes = 48;
	editorProbe.lines = 4;
	expectTrue(failures, "small document still formats on open", MarkdownTablePluginTesting::autoFormatDocumentForTests() && editorProbe.replacements == 1);
	editorProbe = EditorProbe();
	editorProbe.readOnly = true;
	expectTrue(failures, "manual formatting cannot replace read-only text", !MarkdownTablePluginTesting::alignCurrentTableForTests(false) && editorProbe.replacements == 0);
	editorProbe.document = "| A   | B   |\n| --- | --- |\n| x   | yy  |";
	editorProbe.bytes = static_cast<LRESULT>(editorProbe.document.size());
	editorProbe.lines = 3;
	editorProbe.lineWidth = 14;
	nextCell();
	expectTrue(failures, "next cell navigates a formatted read-only table", editorProbe.currentPosition > 2 && editorProbe.replacements == 0);
	previousCell();
	expectTrue(failures, "previous cell navigates a formatted read-only table", editorProbe.currentPosition == 2 && editorProbe.replacements == 0);
	editorProbe = EditorProbe();
	const std::string smallTable = "| A | B |\n| - | - |\n| x | yy |";
	editorProbe.document = std::string(200000, 'x') + "\n" + smallTable;
	editorProbe.currentPosition = 200003;
	expectTrue(failures, "long preceding prose does not disable a small table", MarkdownTablePluginTesting::alignCurrentTableForTests(true) && editorProbe.replacements == 1 && editorProbe.largestTextRead < 100);
	editorProbe = EditorProbe();
	editorProbe.document = smallTable + "\n" + std::string(200000, 'x');
	expectTrue(failures, "long following prose does not disable a small table", MarkdownTablePluginTesting::alignCurrentTableForTests(true) && editorProbe.replacements == 1 && editorProbe.largestTextRead < 100);
	editorProbe = EditorProbe();
	editorProbe.document = smallTable + "\n|" + std::string(200000, 'x') + "|";
	expectTrue(failures, "oversized continuation never formats a table prefix", !MarkdownTablePluginTesting::alignCurrentTableForTests(true) && editorProbe.replacements == 0 && editorProbe.largestTextRead <= 64);
	editorProbe = EditorProbe();
	editorProbe.document = "|" + std::string(200000, 'x') + "|\n" + smallTable;
	editorProbe.currentPosition = 200005;
	expectTrue(failures, "oversized preceding table row is not a scan boundary", !MarkdownTablePluginTesting::alignCurrentTableForTests(true) && editorProbe.replacements == 0 && editorProbe.largestTextRead <= 64);
	editorProbe = EditorProbe();
	editorProbe.document = smallTable + "\n" + std::string(80, ' ') + "|" + std::string(200000, 'x') + "|";
	expectTrue(failures, "unknown oversized indentation skips the whole table", !MarkdownTablePluginTesting::alignCurrentTableForTests(true) && editorProbe.replacements == 0 && editorProbe.largestTextRead <= 64);
	editorProbe = EditorProbe();
	editorProbe.document = "A | B\n- | -\nx | yy\n" + std::string(200000, 'x') + "| y";
	expectTrue(failures, "optional leading pipe table skips an oversized continuation", !MarkdownTablePluginTesting::alignCurrentTableForTests(true) && editorProbe.replacements == 0 && editorProbe.largestTextRead <= 64);
	editorProbe = EditorProbe();
	editorProbe.document = smallTable;
	for (int row = 0; row < 600; ++row)
		editorProbe.document += "\nprose | value";
	expectTrue(failures, "following pipe prose does not count toward the table budget", MarkdownTablePluginTesting::alignCurrentTableForTests(true) && editorProbe.replacements == 1 && editorProbe.textReads < 1020);
	editorProbe = EditorProbe();
	for (int row = 0; row < 600; ++row)
		editorProbe.document += "prose | value\n";
	editorProbe.currentPosition = static_cast<LRESULT>(editorProbe.document.size()) + 2;
	editorProbe.document += smallTable;
	expectTrue(failures, "preceding pipe prose does not count toward the table budget", MarkdownTablePluginTesting::alignCurrentTableForTests(true) && editorProbe.replacements == 1 && editorProbe.textReads < 1020 && editorProbe.replacementText.find("yy") != std::string::npos);
	editorProbe = EditorProbe();
	editorProbe.continuousTable = true;
	editorProbe.currentPosition = 600 * editorProbe.lineWidth + 2;
	expectTrue(failures, "truncated large table without its header is not formatted", !MarkdownTablePluginTesting::alignCurrentTableForTests(true) && editorProbe.replacements == 0 && editorProbe.textReads < 2020);
	editorProbe = EditorProbe();
	editorProbe.document = smallTable;
	for (int row = 0; row < 497; ++row)
		editorProbe.document += "\n| x | yy |";
	editorProbe.document += "\nprose | value";
	expectTrue(failures, "lookahead preserves a table exactly at the line limit", MarkdownTablePluginTesting::alignCurrentTableForTests(true) && editorProbe.replacements == 1);
	editorProbe = EditorProbe();
	for (int column = 0; column < 20000; ++column)
		editorProbe.document += "|x";
	editorProbe.document += "|\n";
	for (int column = 0; column < 20000; ++column)
		editorProbe.document += "|-";
	editorProbe.document += "|";
	for (int row = 0; row < 498; ++row)
		editorProbe.document += "\n|x|";
	expectTrue(failures, "ragged table cannot expand through automatic typing", !MarkdownTablePluginTesting::alignCurrentTableForTests(true) && editorProbe.replacements == 0);
	expectTrue(failures, "ragged table cannot expand through automatic open", !MarkdownTablePluginTesting::autoFormatDocumentForTests() && editorProbe.replacements == 0);
	editorProbe = EditorProbe();
	editorProbe.document = "|" + std::string(30000, 'x') + "| B |\n| - | - |";
	for (int row = 0; row < 20; ++row)
		editorProbe.document += "\n|x|y|";
	expectTrue(failures, "wide cells have a projected output bound", !MarkdownTablePluginTesting::alignCurrentTableForTests(true) && editorProbe.replacements == 0);
	std::string expandingTable;
	for (int row = 0; row < 50; ++row)
	{
		if (row) expandingTable += "\n";
		for (int column = 0; column < 10; ++column)
			expandingTable += "|" + (row == 1 ? std::string("-") : row == column + 2 ? std::string(200, 'x') : std::string("x"));
		expandingTable += "|";
	}
	editorProbe = EditorProbe();
	editorProbe.document = expandingTable;
	expectTrue(failures, "one expanding table remains within the automatic budget", MarkdownTablePluginTesting::autoFormatDocumentForTests() && editorProbe.replacements == 1 && editorProbe.replacementText.size() > 100000);
	editorProbe = EditorProbe();
	for (int table = 0; table < 12; ++table)
		editorProbe.document += expandingTable + "\n\n";
	expectTrue(failures, "aggregate output overflow leaves every table unchanged", !MarkdownTablePluginTesting::autoFormatDocumentForTests() && editorProbe.replacements == 0);
	const std::vector<std::string> noSeparator(10000, "| x | y |");
	const auto scanStart = std::chrono::steady_clock::now();
	expectTrue(failures, "pipe-only document has no table ranges", MarkdownTablePluginTesting::documentMarkdownTableRangeLinesForTests(noSeparator).empty());
	expectTrue(failures, "pipe-only document scan stays below two seconds", std::chrono::steady_clock::now() - scanStart < std::chrono::seconds(2));
	std::cout << "Automatic range scan (10000 pipe lines): " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - scanStart).count() << " ms\n";

	nppData = savedData;
	MarkdownTablePluginTesting::setAutoFitTableEnabledForTests(savedFit);
	::DestroyWindow(window);
	::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
}

void expectSize(int &failures, const std::string &name, std::size_t actual, std::size_t expected)
{
	if (actual == expected)
		return;

	std::ostringstream message;
	message << "expected " << expected << ", got " << actual;
	fail(failures, name, message.str());
}

void expectString(int &failures, const std::string &name, const std::string &actual, const std::string &expected)
{
	if (actual == expected)
		return;

	fail(failures, name, std::string("expected \"") + expected + "\", got \"" + actual + "\"");
}

void expectLines(int &failures, const std::string &name, const std::vector<std::string> &actual, const std::vector<std::string> &expected)
{
	if (actual == expected)
		return;

	std::ostringstream message;
	message << "expected " << expected.size() << " lines, got " << actual.size();
	fail(failures, name, message.str());
}

void expectCommandName(int &failures, const ExpectedCommand &expected, const FuncItem &actual)
{
	if (std::wcscmp(actual._itemName, expected.itemName) == 0)
		return;

	fail(failures, std::string(expected.label) + " name", "unexpected menu item name");
}

void expectWideString(int &failures, const std::string &name, const wchar_t *actual, const wchar_t *expected)
{
	if (std::wcscmp(actual, expected) == 0)
		return;

	fail(failures, name, "unexpected localized text");
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
		{ 0, "align", L"Align table (no width change)\tCtrl+Alt+Shift+1", alignTable, false, true, true, true, true, static_cast<UCHAR>('1') },
		{ 1, "auto align table", L"Light Auto align after edit (no width change)\tCtrl+Alt+Shift+A", toggleAutoAlignTable, true, true, true, true, true, static_cast<UCHAR>('A') },
		{ 2, "fit table width", L"Fit table width to window\tCtrl+Alt+Shift+W", wrapLongCells, false, true, true, true, true, static_cast<UCHAR>('W') },
		{ 3, "auto fit table", L"Power Auto fit table width to window\tCtrl+Alt+Shift+F", toggleAutoFitTable, true, true, true, true, true, static_cast<UCHAR>('F') },
		{ 4, "next cell", L"Next cell\tCtrl+Alt+Shift+2", nextCell, false, true, true, true, true, static_cast<UCHAR>('2') },
		{ 5, "previous cell", L"Previous cell\tCtrl+Alt+Shift+3", previousCell, false, true, true, true, true, static_cast<UCHAR>('3') },
		{ 6, "insert row", L"Insert row below\tCtrl+Alt+Shift+4", insertRowBelow, false, true, true, true, true, static_cast<UCHAR>('4') },
		{ 7, "delete row", L"Delete row\tCtrl+Alt+Shift+5", deleteRow, false, true, true, true, true, static_cast<UCHAR>('5') },
		{ 8, "insert column", L"Insert column right\tCtrl+Alt+Shift+6", insertColumnRight, false, true, true, true, true, static_cast<UCHAR>('6') },
		{ 9, "delete column", L"Delete column\tCtrl+Alt+Shift+7", deleteColumn, false, true, true, true, true, static_cast<UCHAR>('7') },
		{ 10, "narrow column", L"Narrow column\tCtrl+Alt+Shift+,", narrowColumn, false, true, true, true, true, static_cast<UCHAR>(VK_OEM_COMMA) },
		{ 11, "widen column", L"Widen column\tCtrl+Alt+Shift+.", widenColumn, false, true, true, true, true, static_cast<UCHAR>(VK_OEM_PERIOD) },
		{ 12, "move row up", L"Move row up\tCtrl+Alt+Shift+8", moveRowUp, false, true, true, true, true, static_cast<UCHAR>('8') },
		{ 13, "move row down", L"Move row down\tCtrl+Alt+Shift+9", moveRowDown, false, true, true, true, true, static_cast<UCHAR>('9') },
		{ 14, "move column left", L"Move column left\tCtrl+Alt+Shift+[", moveColumnLeft, false, true, true, true, true, static_cast<UCHAR>(VK_OEM_4) },
		{ 15, "move column right", L"Move column right\tCtrl+Alt+Shift+]", moveColumnRight, false, true, true, true, true, static_cast<UCHAR>(VK_OEM_6) },
		{ 16, "sort ascending", L"Sort rows ascending\tCtrl+Alt+Shift+=", sortRowsAscending, false, true, true, true, true, static_cast<UCHAR>(VK_OEM_PLUS) },
		{ 17, "sort descending", L"Sort rows descending\tCtrl+Alt+Shift+-", sortRowsDescending, false, true, true, true, true, static_cast<UCHAR>(VK_OEM_MINUS) },
		{ 18, "convert csv tsv", L"Convert CSV/TSV to table\tCtrl+Alt+Shift+0", convertCsvTsvSelectionToTable, false, true, true, true, true, static_cast<UCHAR>('0') },
		{ 19, "insert table", L"Insert table...\tCtrl+Alt+Shift+\\", insertTable, false, true, true, true, true, static_cast<UCHAR>(VK_OEM_5) }
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
		expectTrue(failures, std::string(expectedCommand.label) + " shortcut state", expectedCommand.hasShortcut ? actual._pShKey != nullptr : actual._pShKey == nullptr);
		expectTrue(failures, std::string(expectedCommand.label) + " initial checked state", actual._init2Check == expectedCommand.checkedOnInit);
		if (actual._pShKey == nullptr)
			continue;

		expectShortcut(failures, expectedCommand, *actual._pShKey);
		const std::string id = shortcutId(*actual._pShKey);
		expectTrue(failures, std::string(expectedCommand.label) + " shortcut is unique", shortcuts.insert(id).second);
	}

	const wchar_t *russianNames[] =
	{
		L"\u0412\u044B\u0440\u043E\u0432\u043D\u044F\u0442\u044C \u0442\u0430\u0431\u043B\u0438\u0446\u0443 (\u0431\u0435\u0437 \u0438\u0437\u043C\u0435\u043D\u0435\u043D\u0438\u044F \u0448\u0438\u0440\u0438\u043D\u044B)",
		L"Light \u0410\u0432\u0442\u043E\u0432\u044B\u0440\u0430\u0432\u043D\u0438\u0432\u0430\u043D\u0438\u0435 \u043F\u043E\u0441\u043B\u0435 \u043F\u0440\u0430\u0432\u043A\u0438 (\u0431\u0435\u0437 \u0438\u0437\u043C\u0435\u043D\u0435\u043D\u0438\u044F \u0448\u0438\u0440\u0438\u043D\u044B)",
		L"\u041F\u043E\u0434\u043E\u0433\u043D\u0430\u0442\u044C \u0448\u0438\u0440\u0438\u043D\u0443 \u0442\u0430\u0431\u043B\u0438\u0446\u044B \u043F\u043E\u0434 \u043E\u043A\u043D\u043E",
		L"Power \u0410\u0432\u0442\u043E\u043F\u043E\u0434\u0433\u043E\u043D\u043A\u0430 \u0448\u0438\u0440\u0438\u043D\u044B \u0442\u0430\u0431\u043B\u0438\u0446\u044B \u043F\u043E\u0434 \u043E\u043A\u043D\u043E",
		L"\u0421\u043B\u0435\u0434\u0443\u044E\u0449\u0430\u044F \u044F\u0447\u0435\u0439\u043A\u0430",
		L"\u041F\u0440\u0435\u0434\u044B\u0434\u0443\u0449\u0430\u044F \u044F\u0447\u0435\u0439\u043A\u0430",
		L"\u0412\u0441\u0442\u0430\u0432\u0438\u0442\u044C \u0441\u0442\u0440\u043E\u043A\u0443 \u043D\u0438\u0436\u0435",
		L"\u0423\u0434\u0430\u043B\u0438\u0442\u044C \u0441\u0442\u0440\u043E\u043A\u0443",
		L"\u0412\u0441\u0442\u0430\u0432\u0438\u0442\u044C \u0441\u0442\u043E\u043B\u0431\u0435\u0446 \u0441\u043F\u0440\u0430\u0432\u0430",
		L"\u0423\u0434\u0430\u043B\u0438\u0442\u044C \u0441\u0442\u043E\u043B\u0431\u0435\u0446",
		L"\u0421\u0443\u0437\u0438\u0442\u044C \u0441\u0442\u043E\u043B\u0431\u0435\u0446",
		L"\u0420\u0430\u0441\u0448\u0438\u0440\u0438\u0442\u044C \u0441\u0442\u043E\u043B\u0431\u0435\u0446",
		L"\u041F\u0435\u0440\u0435\u043C\u0435\u0441\u0442\u0438\u0442\u044C \u0441\u0442\u0440\u043E\u043A\u0443 \u0432\u0432\u0435\u0440\u0445",
		L"\u041F\u0435\u0440\u0435\u043C\u0435\u0441\u0442\u0438\u0442\u044C \u0441\u0442\u0440\u043E\u043A\u0443 \u0432\u043D\u0438\u0437",
		L"\u041F\u0435\u0440\u0435\u043C\u0435\u0441\u0442\u0438\u0442\u044C \u0441\u0442\u043E\u043B\u0431\u0435\u0446 \u0432\u043B\u0435\u0432\u043E",
		L"\u041F\u0435\u0440\u0435\u043C\u0435\u0441\u0442\u0438\u0442\u044C \u0441\u0442\u043E\u043B\u0431\u0435\u0446 \u0432\u043F\u0440\u0430\u0432\u043E",
		L"\u0421\u043E\u0440\u0442\u0438\u0440\u043E\u0432\u0430\u0442\u044C \u0441\u0442\u0440\u043E\u043A\u0438 \u043F\u043E \u0432\u043E\u0437\u0440\u0430\u0441\u0442\u0430\u043D\u0438\u044E",
		L"\u0421\u043E\u0440\u0442\u0438\u0440\u043E\u0432\u0430\u0442\u044C \u0441\u0442\u0440\u043E\u043A\u0438 \u043F\u043E \u0443\u0431\u044B\u0432\u0430\u043D\u0438\u044E",
		L"\u041F\u0440\u0435\u043E\u0431\u0440\u0430\u0437\u043E\u0432\u0430\u0442\u044C CSV/TSV \u0432 \u0442\u0430\u0431\u043B\u0438\u0446\u0443",
		L"\u0412\u0441\u0442\u0430\u0432\u0438\u0442\u044C \u0442\u0430\u0431\u043B\u0438\u0446\u0443..."
	};
	MarkdownTablePluginTesting::applyNativeLangFileNameForTests("russian.xml");
	expectWideString(failures, "russian plugin menu name", MarkdownTablePluginTesting::pluginMenuNameForTests(), L"\u0420\u0435\u0434\u0430\u043A\u0442\u043E\u0440 Markdown-\u0442\u0430\u0431\u043B\u0438\u0446");
	for (std::size_t index = 0; index < expectedCount; ++index)
		expectWideString(failures, std::string("russian ") + expected[index].label + " name", MarkdownTablePluginTesting::commandTextForTests(expected[index].index), russianNames[index]);
	const std::wstring russianAlignMenu = std::wstring(russianNames[0]) + L"\tCtrl+Alt+Shift+1";
	const std::wstring russianAutoAlignMenu = std::wstring(russianNames[1]) + L"\tCtrl+Alt+Shift+A";
	const std::wstring russianFitMenu = std::wstring(russianNames[2]) + L"\tCtrl+Alt+Shift+W";
	const std::wstring russianAutoFitMenu = std::wstring(russianNames[3]) + L"\tCtrl+Alt+Shift+F";
	const std::wstring russianNarrowColumnMenu = std::wstring(russianNames[10]) + L"\tCtrl+Alt+Shift+,";
	const std::wstring russianWidenColumnMenu = std::wstring(russianNames[11]) + L"\tCtrl+Alt+Shift+.";
	expectWideString(failures, "russian align menu hotkey", MarkdownTablePluginTesting::commandMenuTextForTests(0), russianAlignMenu.c_str());
	expectWideString(failures, "russian auto align menu hotkey", MarkdownTablePluginTesting::commandMenuTextForTests(1), russianAutoAlignMenu.c_str());
	expectWideString(failures, "russian fit menu hotkey", MarkdownTablePluginTesting::commandMenuTextForTests(2), russianFitMenu.c_str());
	expectWideString(failures, "russian auto fit menu hotkey", MarkdownTablePluginTesting::commandMenuTextForTests(3), russianAutoFitMenu.c_str());
	expectWideString(failures, "russian narrow column menu hotkey", MarkdownTablePluginTesting::commandMenuTextForTests(10), russianNarrowColumnMenu.c_str());
	expectWideString(failures, "russian widen column menu hotkey", MarkdownTablePluginTesting::commandMenuTextForTests(11), russianWidenColumnMenu.c_str());

	struct LocalizedCommandSample
	{
		const char *nativeLangFileName;
		const char *label;
		const wchar_t *alignName;
	};
	const LocalizedCommandSample localizedSamples[] =
	{
		{ "chineseSimplified.xml", "mandarin chinese", L"\u5BF9\u9F50\u8868\u683C\uFF08\u4E0D\u6539\u53D8\u5BBD\u5EA6\uFF09" },
		{ "hindi.xml", "hindi", L"\u0924\u093E\u0932\u093F\u0915\u093E \u0938\u0902\u0930\u0947\u0916\u093F\u0924 \u0915\u0930\u0947\u0902 (\u091A\u094C\u0921\u093C\u093E\u0908 \u0928 \u092C\u0926\u0932\u0947\u0902)" },
		{ "spanish.xml", "spanish", L"Alinear tabla (sin cambiar ancho)" },
		{ "arabic.xml", "arabic", L"\u0645\u062D\u0627\u0630\u0627\u0629 \u0627\u0644\u062C\u062F\u0648\u0644 (\u0628\u062F\u0648\u0646 \u062A\u063A\u064A\u064A\u0631 \u0627\u0644\u0639\u0631\u0636)" },
		{ "french.xml", "french", L"Aligner le tableau (sans changer la largeur)" },
		{ "bengali.xml", "bengali", L"\u099F\u09C7\u09AC\u09BF\u09B2 \u09B8\u09BE\u09B0\u09BF\u09AC\u09A6\u09CD\u09A7 \u0995\u09B0\u09C1\u09A8 (\u09AA\u09CD\u09B0\u09B8\u09CD\u09A5 \u09A8\u09BE \u09AC\u09A6\u09B2\u09C7)" },
		{ "portuguese.xml", "portuguese", L"Alinhar tabela (sem alterar largura)" },
		{ "indonesian.xml", "indonesian", L"Ratakan tabel (tanpa mengubah lebar)" },
		{ "urdu.xml", "urdu", L"\u062C\u062F\u0648\u0644 \u0633\u06CC\u062F\u06BE\u0627 \u06A9\u0631\u06CC\u06BA (\u0686\u0648\u0691\u0627\u0626\u06CC \u0628\u062F\u0644\u06D2 \u0628\u063A\u06CC\u0631)" },
		{ "german.xml", "german", L"Tabelle ausrichten (Breite unveraendert)" },
		{ "japanese.xml", "japanese", L"\u30C6\u30FC\u30D6\u30EB\u3092\u6574\u5217\uFF08\u5E45\u3092\u5909\u66F4\u3057\u306A\u3044\uFF09" },
		{ "pidgin.xml", "nigerian pidgin", L"Arrange table (no change width)" },
		{ "marathi.xml", "marathi", L"\u0924\u0915\u094D\u0924\u093E \u0938\u0902\u0930\u0947\u0916\u093F\u0924 \u0915\u0930\u093E (\u0930\u0941\u0902\u0926\u0940 \u0928 \u092C\u0926\u0932\u0924\u093E)" },
		{ "telugu.xml", "telugu", L"\u0C2A\u0C1F\u0C4D\u0C1F\u0C3F\u0C15\u0C28\u0C41 \u0C38\u0C30\u0C3F\u0C2A\u0C30\u0C1A\u0C41 (\u0C35\u0C46\u0C21\u0C32\u0C4D\u0C2A\u0C41 \u0C2E\u0C3E\u0C30\u0C4D\u0C1A\u0C15\u0C41\u0C02\u0C21\u0C3E)" },
		{ "turkish.xml", "turkish", L"Tabloyu hizala (genisligi degistirme)" },
		{ "tamil.xml", "tamil", L"\u0B85\u0B9F\u0BCD\u0B9F\u0BB5\u0BA3\u0BC8\u0BAF\u0BC8 \u0B92\u0BB4\u0BC1\u0B99\u0BCD\u0B95\u0BC1\u0BAA\u0B9F\u0BC1\u0BA4\u0BCD\u0BA4\u0BC1 (\u0B85\u0B95\u0BB2\u0BA4\u0BCD\u0BA4\u0BC8 \u0BAE\u0BBE\u0BB1\u0BCD\u0BB1\u0BBE\u0BAE\u0BB2\u0BCD)" },
		{ "chineseTraditional.xml", "yue chinese", L"\u5C0D\u9F4A\u8868\u683C\uFF08\u4E0D\u6539\u8B8A\u95CA\u5EA6\uFF09" },
		{ "vietnamese.xml", "vietnamese", L"C\u0103n ch\u1EC9nh b\u1EA3ng (kh\u00F4ng \u0111\u1ED5i chi\u1EC1u r\u1ED9ng)" }
	};
	expectSize(failures, "additional localized command samples", sizeof(localizedSamples) / sizeof(localizedSamples[0]), static_cast<std::size_t>(18));
	const wchar_t *primaryCommandShortcutSuffixes[] =
	{
		L"\tCtrl+Alt+Shift+1",
		L"\tCtrl+Alt+Shift+A",
		L"\tCtrl+Alt+Shift+W",
		L"\tCtrl+Alt+Shift+F"
	};
	for (const LocalizedCommandSample &sample : localizedSamples)
	{
		MarkdownTablePluginTesting::applyNativeLangFileNameForTests(sample.nativeLangFileName);
		expectWideString(failures, std::string(sample.label) + " align name", MarkdownTablePluginTesting::commandTextForTests(0), sample.alignName);
		for (std::size_t commandIndex = 0; commandIndex < 4; ++commandIndex)
		{
			const wchar_t *localizedCommand = MarkdownTablePluginTesting::commandTextForTests(commandIndex);
			expectTrue(failures, std::string(sample.label) + " primary command text present " + std::to_string(commandIndex), localizedCommand != NULL && std::wcslen(localizedCommand) > 0);
			const std::wstring expectedMenuText = std::wstring(localizedCommand ? localizedCommand : L"") + primaryCommandShortcutSuffixes[commandIndex];
			expectWideString(failures, std::string(sample.label) + " primary command menu hotkey " + std::to_string(commandIndex), MarkdownTablePluginTesting::commandMenuTextForTests(commandIndex), expectedMenuText.c_str());
		}
	}

	MarkdownTablePluginTesting::applyNativeLangFileNameForTests("");
	expectWideString(failures, "english plugin menu fallback", MarkdownTablePluginTesting::pluginMenuNameForTests(), L"Markdown Table Editor");
	for (std::size_t index = 0; index < expectedCount; ++index)
		expectCommandName(failures, expected[index], funcItem[expected[index].index]);

	expectTrue(failures, "auto fit table starts enabled by default", MarkdownTablePluginTesting::autoFitTableEnabledForTests());
	expectTrue(failures, "fit table command starts disabled while auto fit is on by default", !MarkdownTablePluginTesting::fitTableToWindowCommandEnabledForTests());
	expectTrue(failures, "auto fit keeps auto align enabled by default", MarkdownTablePluginTesting::autoAlignTableEnabledForTests());
	expectTrue(failures, "auto align toggle starts disabled while auto fit is on by default", !MarkdownTablePluginTesting::autoAlignTableToggleEnabledForTests());
	MarkdownTablePluginTesting::setAutoAlignTableEnabledForTests(false);
	expectTrue(failures, "auto fit blocks disabling auto align", MarkdownTablePluginTesting::autoAlignTableEnabledForTests());
	expectTrue(failures, "auto fit table default does not fit align", !MarkdownTablePluginTesting::shouldApplyAutoFitAfterActionForTests(MarkdownTable::Action::Align));
	expectTrue(failures, "fit table command aligns before visible-width wrapping", MarkdownTablePluginTesting::coreActionForPluginActionForTests(MarkdownTable::Action::WrapLongCells) == MarkdownTable::Action::Align);
	expectTrue(failures, "fit table command always fits to visible width", MarkdownTablePluginTesting::shouldFitToWindowAfterActionForTests(MarkdownTable::Action::WrapLongCells));
	expectTrue(failures, "plain align never fits to visible width", !MarkdownTablePluginTesting::shouldFitToWindowAfterActionForTests(MarkdownTable::Action::Align));
	MarkdownTablePluginTesting::setAutoFitTableEnabledForTests(false);
	expectTrue(failures, "auto align toggle re-enabled after auto fit is disabled", MarkdownTablePluginTesting::autoAlignTableToggleEnabledForTests());
	MarkdownTablePluginTesting::setAutoAlignTableEnabledForTests(false);
	expectTrue(failures, "auto align can be disabled when auto fit is off", !MarkdownTablePluginTesting::autoAlignTableEnabledForTests());
	expectTrue(failures, "auto fit table starts disabled in test", !MarkdownTablePluginTesting::autoFitTableEnabledForTests());
	expectTrue(failures, "auto fit table disabled does not fit align", !MarkdownTablePluginTesting::shouldApplyAutoFitAfterActionForTests(MarkdownTable::Action::Align));
	toggleAutoFitTable();
	expectTrue(failures, "auto fit table toggles on", MarkdownTablePluginTesting::autoFitTableEnabledForTests());
	expectTrue(failures, "auto fit toggle forces auto align on", MarkdownTablePluginTesting::autoAlignTableEnabledForTests());
	expectTrue(failures, "auto align toggle disabled after auto fit toggles on", !MarkdownTablePluginTesting::autoAlignTableToggleEnabledForTests());
	expectTrue(failures, "auto fit table enabled does not change plain align", !MarkdownTablePluginTesting::shouldApplyAutoFitAfterActionForTests(MarkdownTable::Action::Align));
	expectTrue(failures, "auto fit table enabled keeps plain align width", !MarkdownTablePluginTesting::shouldFitToWindowAfterActionForTests(MarkdownTable::Action::Align));
	expectTrue(failures, "auto fit table enabled does not fit row insert", !MarkdownTablePluginTesting::shouldApplyAutoFitAfterActionForTests(MarkdownTable::Action::InsertRowBelow));
	expectTrue(failures, "auto fit table enabled does not fit row insert to visible width", !MarkdownTablePluginTesting::shouldFitToWindowAfterActionForTests(MarkdownTable::Action::InsertRowBelow));
	expectTrue(failures, "auto fit table does not auto-fit explicit wrap command", !MarkdownTablePluginTesting::shouldApplyAutoFitAfterActionForTests(MarkdownTable::Action::WrapLongCells));
	toggleAutoAlignTable();
	expectTrue(failures, "auto fit blocks auto align hotkey toggle", MarkdownTablePluginTesting::autoAlignTableEnabledForTests());
	toggleAutoFitTable();
	expectTrue(failures, "auto fit table toggles off", !MarkdownTablePluginTesting::autoFitTableEnabledForTests());
	expectTrue(failures, "auto fit leaves auto align on when disabled", MarkdownTablePluginTesting::autoAlignTableEnabledForTests());
	expectTrue(failures, "auto align toggle re-enabled after auto fit toggles off", MarkdownTablePluginTesting::autoAlignTableToggleEnabledForTests());
	expectTrue(failures, "auto fit table off after unpress does not fit align", !MarkdownTablePluginTesting::shouldApplyAutoFitAfterActionForTests(MarkdownTable::Action::Align));
	expectTrue(failures, "fit table command enabled after auto fit is off", MarkdownTablePluginTesting::fitTableToWindowCommandEnabledForTests());
	expectTrue(failures, "word wrap none does not count as enabled", !MarkdownTablePluginTesting::standardWordWrapEnabledForModeForTests(SC_WRAP_NONE));
	expectTrue(failures, "word wrap word counts as enabled", MarkdownTablePluginTesting::standardWordWrapEnabledForModeForTests(SC_WRAP_WORD));
	expectTrue(failures, "word wrap char counts as enabled", MarkdownTablePluginTesting::standardWordWrapEnabledForModeForTests(SC_WRAP_CHAR));
	expectTrue(failures, "word wrap whitespace counts as enabled", MarkdownTablePluginTesting::standardWordWrapEnabledForModeForTests(SC_WRAP_WHITESPACE));
	expectTrue(failures, "word wrap auto fit warning shows for active unsuppressed combo", MarkdownTablePluginTesting::shouldShowWordWrapAutoFitWarningForTests(true, true, false));
	expectTrue(failures, "word wrap auto fit warning ignores disabled auto fit", !MarkdownTablePluginTesting::shouldShowWordWrapAutoFitWarningForTests(false, true, false));
	expectTrue(failures, "word wrap auto fit warning ignores disabled word wrap", !MarkdownTablePluginTesting::shouldShowWordWrapAutoFitWarningForTests(true, false, false));
	expectTrue(failures, "word wrap auto fit warning respects suppression", !MarkdownTablePluginTesting::shouldShowWordWrapAutoFitWarningForTests(true, true, true));
	expectTrue(failures, "auto fit resize uses delayed debounce", MarkdownTablePluginTesting::fitToWindowResizeDelayMsForTests() > 0);
	expectTrue(failures, "auto fit resize ignores disabled mode", !MarkdownTablePluginTesting::shouldRunFitToWindowAfterResizeForTests(false, false, true, 100, 120));
	expectTrue(failures, "auto fit resize ignores reentrant fit", !MarkdownTablePluginTesting::shouldRunFitToWindowAfterResizeForTests(true, true, true, 100, 120));
	expectTrue(failures, "auto fit resize ignores inactive editor", !MarkdownTablePluginTesting::shouldRunFitToWindowAfterResizeForTests(true, false, false, 100, 120));
	expectTrue(failures, "auto fit resize runs first snapped width even before width is remembered", MarkdownTablePluginTesting::shouldRunFitToWindowAfterResizeForTests(true, false, true, 0, 120));
	expectTrue(failures, "auto fit resize ignores unchanged width", !MarkdownTablePluginTesting::shouldRunFitToWindowAfterResizeForTests(true, false, true, 120, 120));
	expectTrue(failures, "auto fit resize fits after active width change", MarkdownTablePluginTesting::shouldRunFitToWindowAfterResizeForTests(true, false, true, 120, 90));
	expectTrue(failures, "auto fit resize schedules on restored size", MarkdownTablePluginTesting::shouldScheduleFitToWindowAfterResizeMessageForTests(true, WM_SIZE, SIZE_RESTORED, 0));
	expectTrue(failures, "auto fit resize ignores minimized size", !MarkdownTablePluginTesting::shouldScheduleFitToWindowAfterResizeMessageForTests(true, WM_SIZE, SIZE_MINIMIZED, 0));
	expectTrue(failures, "auto fit resize schedules on snapped window position size", MarkdownTablePluginTesting::shouldScheduleFitToWindowAfterResizeMessageForTests(true, WM_WINDOWPOSCHANGED, 0, 0));
	expectTrue(failures, "auto fit resize ignores move-only window position", !MarkdownTablePluginTesting::shouldScheduleFitToWindowAfterResizeMessageForTests(true, WM_WINDOWPOSCHANGED, 0, SWP_NOSIZE));
	expectTrue(failures, "auto fit resize ignores snap messages while disabled", !MarkdownTablePluginTesting::shouldScheduleFitToWindowAfterResizeMessageForTests(false, WM_WINDOWPOSCHANGED, 0, 0));
	expectTrue(failures, "auto fit zoom ignores disabled mode", !MarkdownTablePluginTesting::shouldRunAutoFitAfterZoomForTests(false, false, true));
	expectTrue(failures, "auto fit zoom ignores reentrant fit", !MarkdownTablePluginTesting::shouldRunAutoFitAfterZoomForTests(true, true, true));
	expectTrue(failures, "auto fit zoom ignores inactive editor", !MarkdownTablePluginTesting::shouldRunAutoFitAfterZoomForTests(true, false, false));
	expectTrue(failures, "auto fit zoom does not run immediate fit before zoom settles", !MarkdownTablePluginTesting::shouldRunAutoFitAfterZoomForTests(true, false, true));
	expectTrue(failures, "auto fit zoom schedule ignores disabled mode", !MarkdownTablePluginTesting::shouldScheduleFitToWindowAfterZoomForTests(false, false, true));
	expectTrue(failures, "auto fit zoom schedule ignores reentrant fit", !MarkdownTablePluginTesting::shouldScheduleFitToWindowAfterZoomForTests(true, true, true));
	expectTrue(failures, "auto fit zoom schedule ignores inactive editor", !MarkdownTablePluginTesting::shouldScheduleFitToWindowAfterZoomForTests(true, false, false));
	expectTrue(failures, "auto fit zoom schedules delayed fit after active zoom", MarkdownTablePluginTesting::shouldScheduleFitToWindowAfterZoomForTests(true, false, true));
	expectTrue(failures, "auto fit toggle runs initial fit before enabling", MarkdownTablePluginTesting::shouldRunInitialFitWhenTogglingAutoFitTableForTests(false));
	expectTrue(failures, "auto fit toggle does not run initial fit when disabling", !MarkdownTablePluginTesting::shouldRunInitialFitWhenTogglingAutoFitTableForTests(true));
	MarkdownTablePluginTesting::setAutoFitTableEnabledForTests(false);
	toggleAutoFitTable();
	expectTrue(failures, "auto fit table toggles on for resize", MarkdownTablePluginTesting::autoFitTableEnabledForTests());
	expectTrue(failures, "fit table command disabled while auto fit is on", !MarkdownTablePluginTesting::fitTableToWindowCommandEnabledForTests());
	toggleAutoFitTable();
	expectTrue(failures, "auto fit table toggles off after resize mode", !MarkdownTablePluginTesting::autoFitTableEnabledForTests());
	expectTrue(failures, "fit table command re-enabled after auto fit is off", MarkdownTablePluginTesting::fitTableToWindowCommandEnabledForTests());
	expectTrue(failures, "auto align table starts enabled by default", MarkdownTablePluginTesting::autoAlignTableEnabledForTests());
	expectTrue(failures, "align table command starts disabled while auto align is on by default", !MarkdownTablePluginTesting::alignTableCommandEnabledForTests());
	expectTrue(failures, "auto input update ignores disabled modes", !MarkdownTablePluginTesting::shouldRunAutoTableFormatAfterUpdateForTests(false, false, false, false, false, true, true));
	expectTrue(failures, "auto input update ignores reentrant align", !MarkdownTablePluginTesting::shouldRunAutoTableFormatAfterUpdateForTests(true, true, true, false, false, true, true));
	expectTrue(failures, "auto input update ignores reentrant fit", !MarkdownTablePluginTesting::shouldRunAutoTableFormatAfterUpdateForTests(true, true, false, true, false, true, true));
	expectTrue(failures, "auto input update ignores plugin edit", !MarkdownTablePluginTesting::shouldRunAutoTableFormatAfterUpdateForTests(true, true, false, false, true, true, true));
	expectTrue(failures, "auto input update ignores inactive editor", !MarkdownTablePluginTesting::shouldRunAutoTableFormatAfterUpdateForTests(true, false, false, false, false, false, true));
	expectTrue(failures, "auto input update ignores selection-only update", !MarkdownTablePluginTesting::shouldRunAutoTableFormatAfterUpdateForTests(true, false, false, false, false, true, false));
	expectTrue(failures, "auto input update runs immediate auto align", MarkdownTablePluginTesting::shouldRunAutoTableFormatAfterUpdateForTests(true, false, false, false, false, true, true));
	expectTrue(failures, "auto input update runs immediate auto fit", MarkdownTablePluginTesting::shouldRunAutoTableFormatAfterUpdateForTests(false, true, false, false, false, true, true));
	expectTrue(failures, "auto input update runs immediate combined auto mode", MarkdownTablePluginTesting::shouldRunAutoTableFormatAfterUpdateForTests(true, true, false, false, false, true, true));
	expectTrue(failures, "auto input modified runs after inserted text", MarkdownTablePluginTesting::scintillaModificationShouldRunAutoTableFormatForTests(SC_MOD_INSERTTEXT));
	expectTrue(failures, "auto input modified runs after deleted text", MarkdownTablePluginTesting::scintillaModificationShouldRunAutoTableFormatForTests(SC_MOD_DELETETEXT));
	expectTrue(failures, "auto input modified runs after combined content edit", MarkdownTablePluginTesting::scintillaModificationShouldRunAutoTableFormatForTests(SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT));
	expectTrue(failures, "auto input modified runs after undo", MarkdownTablePluginTesting::scintillaModificationShouldRunAutoTableFormatForTests(SC_PERFORMED_UNDO));
	expectTrue(failures, "auto input modified runs after redo", MarkdownTablePluginTesting::scintillaModificationShouldRunAutoTableFormatForTests(SC_PERFORMED_REDO));
	expectTrue(failures, "auto input modified ignores indicator-only edits", !MarkdownTablePluginTesting::scintillaModificationShouldRunAutoTableFormatForTests(SC_MOD_CHANGEINDICATOR));
	expectTrue(failures, "global modified ignores disabled modes", !MarkdownTablePluginTesting::shouldRunAutoTableFormatAfterGlobalModifiedForTests(false, false, false, false, false));
	expectTrue(failures, "global modified ignores reentrant align", !MarkdownTablePluginTesting::shouldRunAutoTableFormatAfterGlobalModifiedForTests(true, false, true, false, false));
	expectTrue(failures, "global modified ignores reentrant fit", !MarkdownTablePluginTesting::shouldRunAutoTableFormatAfterGlobalModifiedForTests(true, true, false, true, false));
	expectTrue(failures, "global modified ignores plugin edit", !MarkdownTablePluginTesting::shouldRunAutoTableFormatAfterGlobalModifiedForTests(true, true, false, false, true));
	expectTrue(failures, "global modified runs auto align", MarkdownTablePluginTesting::shouldRunAutoTableFormatAfterGlobalModifiedForTests(true, false, false, false, false));
	expectTrue(failures, "global modified runs auto fit", MarkdownTablePluginTesting::shouldRunAutoTableFormatAfterGlobalModifiedForTests(false, true, false, false, false));
	expectTrue(failures, "initial open format ignores disabled modes", !MarkdownTablePluginTesting::shouldRunInitialAutoTableFormatForBufferForTests(false, false, false, false, true, false));
	expectTrue(failures, "initial open format ignores handled buffer", !MarkdownTablePluginTesting::shouldRunInitialAutoTableFormatForBufferForTests(true, true, false, false, true, true));
	expectTrue(failures, "initial open format ignores reentrant align", !MarkdownTablePluginTesting::shouldRunInitialAutoTableFormatForBufferForTests(true, false, true, false, true, false));
	expectTrue(failures, "initial open format ignores reentrant fit", !MarkdownTablePluginTesting::shouldRunInitialAutoTableFormatForBufferForTests(false, true, false, true, true, false));
	expectTrue(failures, "initial open format ignores inactive editor", !MarkdownTablePluginTesting::shouldRunInitialAutoTableFormatForBufferForTests(true, true, false, false, false, false));
	expectTrue(failures, "initial open format runs auto align", MarkdownTablePluginTesting::shouldRunInitialAutoTableFormatForBufferForTests(true, false, false, false, true, false));
	expectTrue(failures, "initial open format runs auto fit", MarkdownTablePluginTesting::shouldRunInitialAutoTableFormatForBufferForTests(false, true, false, false, true, false));
	expectTrue(failures, "file opened queues initial auto align", MarkdownTablePluginTesting::shouldQueueInitialAutoTableFormatForOpenedBufferForTests(true, false, false));
	expectTrue(failures, "file opened queues initial auto fit", MarkdownTablePluginTesting::shouldQueueInitialAutoTableFormatForOpenedBufferForTests(false, true, false));
	expectTrue(failures, "file opened ignores disabled initial auto format", !MarkdownTablePluginTesting::shouldQueueInitialAutoTableFormatForOpenedBufferForTests(false, false, false));
	expectTrue(failures, "file opened ignores handled initial auto format buffer", !MarkdownTablePluginTesting::shouldQueueInitialAutoTableFormatForOpenedBufferForTests(true, true, true));
	expectTrue(failures, "file opened queues without immediate initial format attempt", !MarkdownTablePluginTesting::shouldTryQueuedInitialAutoTableFormatForNotificationForTests(NPPN_FILEOPENED));
	expectTrue(failures, "buffer activated runs queued initial format", MarkdownTablePluginTesting::shouldTryQueuedInitialAutoTableFormatForNotificationForTests(NPPN_BUFFERACTIVATED));
	expectTrue(failures, "content update runs queued initial format", MarkdownTablePluginTesting::shouldTryQueuedInitialAutoTableFormatForNotificationForTests(SCN_UPDATEUI));
	expectTrue(failures, "initial open defers while scintilla text is still empty", MarkdownTablePluginTesting::shouldDeferInitialAutoTableFormatForDocumentLengthForTests(0));
	expectTrue(failures, "initial open can run after file text is loaded", !MarkdownTablePluginTesting::shouldDeferInitialAutoTableFormatForDocumentLengthForTests(1));
	const std::vector<std::string> openFileDocument =
	{
		"# title",
		"",
		"| Name | Age |",
		"| --- | ---: |",
		"| Anna | 20 |",
		"| Alexander | 7 |",
		"",
		"tail"
	};
	const std::vector<std::size_t> openFileTableRanges = MarkdownTablePluginTesting::documentMarkdownTableRangeLinesForTests(openFileDocument);
	expectSize(failures, "initial document auto format finds table below cursor range count", openFileTableRanges.size(), 2);
	if (openFileTableRanges.size() >= 2)
	{
		expectSize(failures, "initial document auto format range start", openFileTableRanges[0], 2);
		expectSize(failures, "initial document auto format range end", openFileTableRanges[1], 5);
	}
	const std::vector<std::string> formattedOpenFileDocument = MarkdownTablePluginTesting::autoFormatDocumentTablesForTests(openFileDocument, MarkdownTable::Action::Align, 80);
	expectString(failures, "initial document auto format aligns header below cursor", formattedOpenFileDocument[2], "| Name      | Age |");
	expectString(failures, "initial document auto format aligns separator below cursor", formattedOpenFileDocument[3], "| --------- | --: |");
	expectString(failures, "initial document auto format keeps prose before table", formattedOpenFileDocument[0], "# title");
	expectString(failures, "initial document auto format keeps prose after table", formattedOpenFileDocument[7], "tail");
	const std::vector<std::string> wideRegistryDocument =
	{
		"# registry",
		"",
		"| patch_id | date | component | target_path | change | evidence | rollback | status | notes |",
		"| --- | --- | --- | --- | --- | --- | --- | --- | --- |",
		"| codex-admin-launcher-2026-06-01 | 2026-06-01 | Codex launcher | C:/Users/KRT/.codex/launchers/Start-Codex-Elevated.ps1 | Created local launcher that fixes RunAs shortcuts and starts Codex with elevated rights | Service mark and log file confirm successful launch | Delete managed shortcuts and use stock package | active | prefers patched local shell |",
		"",
		"tail"
	};
	const std::vector<std::string> firstFitDocument = MarkdownTablePluginTesting::autoFormatDocumentTablesForTests(wideRegistryDocument, MarkdownTable::Action::WrapLongCells, 112);
	const std::vector<std::string> secondFitDocument = MarkdownTablePluginTesting::autoFormatDocumentTablesForTests(firstFitDocument, MarkdownTable::Action::WrapLongCells, 112);
	expectLines(failures, "manual fit document is idempotent after first pass", secondFitDocument, firstFitDocument);
	expectTrue(failures, "unchanged replacement moves caret to wrapped logical row", MarkdownTablePluginTesting::shouldMoveCaretToTargetForTests(24, 48));
	expectTrue(failures, "unchanged replacement leaves caret when target unchanged", !MarkdownTablePluginTesting::shouldMoveCaretToTargetForTests(48, 48));
	expectTrue(failures, "enter preserves column in active table line", MarkdownTablePluginTesting::shouldPreserveEnterColumnForTests(true, true, true, WM_KEYDOWN, VK_RETURN));
	expectTrue(failures, "enter column preserve ignores inactive editor", !MarkdownTablePluginTesting::shouldPreserveEnterColumnForTests(false, true, true, WM_KEYDOWN, VK_RETURN));
	expectTrue(failures, "enter column preserve ignores selection", !MarkdownTablePluginTesting::shouldPreserveEnterColumnForTests(true, false, true, WM_KEYDOWN, VK_RETURN));
	expectTrue(failures, "enter column preserve ignores non-table line", !MarkdownTablePluginTesting::shouldPreserveEnterColumnForTests(true, true, false, WM_KEYDOWN, VK_RETURN));
	expectTrue(failures, "enter column preserve ignores other keys", !MarkdownTablePluginTesting::shouldPreserveEnterColumnForTests(true, true, true, WM_KEYDOWN, 'A'));
	const std::string privet = "\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82";
	const std::string p = "\xD0\xBF";
	const std::string r = "\xD1\x80";
	const std::string iLetter = "\xD0\xB8";
	const std::string vLetter = "\xD0\xB2";
	const std::string eLetter = "\xD0\xB5";
	const std::string tLetter = "\xD1\x82";
	const std::string upperK = "\xD0\x9A";
	const std::string pr = "\xD0\xBF\xD1\x80";
	const std::string pri = "\xD0\xBF\xD1\x80\xD0\xB8";
	const std::string sourcePrLine = "| " + pr + " |";
	const std::string sourcePrivetLine = "| " + privet + " |";
	const std::string sourcePrivetSpaceLine = "| " + privet + "  |";
	const std::string wrappedPLine = "| " + p + " |";
	const std::string paddedPrivetLine = "| " + privet + "     |";
	const std::size_t caretAfterPr = sourcePrLine.find(pr) + pr.size();
	const std::size_t caretAfterPri = sourcePrivetLine.find(privet) + pri.size();
	const std::size_t caretAfterPrivetSpace = sourcePrivetSpaceLine.find(privet) + privet.size() + 1;
	expectSize(failures, "auto input preserves utf8 cell caret after same formatting",
		MarkdownTablePluginTesting::preservedCellCaretColumnOffsetForTests(sourcePrivetLine, 0, caretAfterPri, sourcePrivetLine),
		caretAfterPri);
	expectSize(failures, "auto input preserves utf8 cell caret after padding changes",
		MarkdownTablePluginTesting::preservedCellCaretColumnOffsetForTests(sourcePrivetLine, 0, caretAfterPri, paddedPrivetLine),
		paddedPrivetLine.find(privet) + pri.size());
	expectSize(failures, "auto input preserves utf8 trailing space between words",
		MarkdownTablePluginTesting::preservedCellCaretColumnOffsetForTests(sourcePrivetSpaceLine, 0, caretAfterPrivetSpace, sourcePrivetLine),
		sourcePrivetLine.find(privet) + privet.size() + 1);
	expectSize(failures, "auto input does not pad shortened utf8 wrapped segment",
		MarkdownTablePluginTesting::preservedCellCaretColumnOffsetForTests(sourcePrLine, 0, caretAfterPr, wrappedPLine),
		wrappedPLine.find(p) + p.size());
	const std::vector<std::string> sourceNarrowPri =
	{
		"| text | other |",
		"| --- | --- |",
		"| " + p + " | tail |",
		"| " + r + iLetter + " | |"
	};
	const std::vector<std::string> replacementNarrowPri =
	{
		"| text | other |",
		"| --- | --- |",
		"| " + p + " | tail |",
		"| " + r + " | |",
		"| " + iLetter + " | |"
	};
	const std::size_t caretAfterNarrowPri = sourceNarrowPri[3].find(r + iLetter) + r.size() + iLetter.size();
	const MarkdownTablePluginTesting::CellCaretPreview narrowPriCaret =
		MarkdownTablePluginTesting::preservedCellCaretPositionForTests(sourceNarrowPri, 3, 0, caretAfterNarrowPri, replacementNarrowPri);
	expectSize(failures, "auto input follows utf8 caret across narrow continuation rows", narrowPriCaret.row, 4);
	expectSize(failures, "auto input keeps utf8 caret after wrapped continuation letter", narrowPriCaret.columnOffset, replacementNarrowPri[4].find(iLetter) + iLetter.size());
	const std::vector<std::string> sourceNarrowNextWord =
	{
		"| text | other |",
		"| --- | --- |",
		"| " + p + " | tail |",
		"| " + r + " | |",
		"| " + iLetter + " | |",
		"| " + vLetter + " | |",
		"| " + eLetter + " | |",
		"| " + tLetter + " " + upperK + " | |"
	};
	const std::vector<std::string> replacementNarrowNextWord =
	{
		"| text | other |",
		"| --- | --- |",
		"| " + p + " | tail |",
		"| " + r + " | |",
		"| " + iLetter + " | |",
		"| " + vLetter + " | |",
		"| " + eLetter + " | |",
		"| " + tLetter + " | |",
		"| " + upperK + " | |"
	};
	const std::size_t caretAfterUpperK = sourceNarrowNextWord[7].find(upperK) + upperK.size();
	const MarkdownTablePluginTesting::CellCaretPreview narrowNextWordCaret =
		MarkdownTablePluginTesting::preservedCellCaretPositionForTests(sourceNarrowNextWord, 7, 0, caretAfterUpperK, replacementNarrowNextWord);
	expectSize(failures, "auto input follows utf8 caret after narrow wrapped word boundary", narrowNextWordCaret.row, 8);
	expectSize(failures, "auto input keeps utf8 caret after next narrow word letter", narrowNextWordCaret.columnOffset, replacementNarrowNextWord[8].find(upperK) + upperK.size());
	expectTrue(failures, "auto align toggle runs initial align before enabling", MarkdownTablePluginTesting::shouldRunInitialAlignWhenTogglingAutoAlignTableForTests(false));
	expectTrue(failures, "auto align toggle does not run initial align when disabling", !MarkdownTablePluginTesting::shouldRunInitialAlignWhenTogglingAutoAlignTableForTests(true));
	MarkdownTablePluginTesting::setAutoAlignTableEnabledForTests(false);
	toggleAutoAlignTable();
	expectTrue(failures, "auto align table toggles on", MarkdownTablePluginTesting::autoAlignTableEnabledForTests());
	expectTrue(failures, "align table command disabled while auto align is on", !MarkdownTablePluginTesting::alignTableCommandEnabledForTests());
	toggleAutoAlignTable();
	expectTrue(failures, "auto align table toggles off", !MarkdownTablePluginTesting::autoAlignTableEnabledForTests());
	expectTrue(failures, "align table command re-enabled after auto align is off", MarkdownTablePluginTesting::alignTableCommandEnabledForTests());
	expectTrue(failures, "align toolbar icons are created", MarkdownTablePluginTesting::ensureAlignToolbarIconsForTests());
	expectTrue(failures, "fit table width toolbar icons are created", MarkdownTablePluginTesting::ensureWrapLongCellsToolbarIconsForTests());
	expectTrue(failures, "auto fit table toolbar icons are created", MarkdownTablePluginTesting::ensureAutoFitTableToolbarIconsForTests());
	expectTrue(failures, "auto align table toolbar icons are created", MarkdownTablePluginTesting::ensureAutoAlignTableToolbarIconsForTests());
	expectTrue(failures, "narrow column toolbar icons are created", MarkdownTablePluginTesting::ensureNarrowColumnToolbarIconsForTests());
	expectTrue(failures, "widen column toolbar icons are created", MarkdownTablePluginTesting::ensureWidenColumnToolbarIconsForTests());
	expectTrue(failures, "all command toolbar icons are created", MarkdownTablePluginTesting::ensureAllToolbarIconsForTests());
	// issue #11: на панели Notepad++ остаются только четыре основные кнопки, остальные
	// команды доступны из меню плагина и по горячим клавишам.
	expectSize(failures, "toolbar keeps only the four main buttons",
		MarkdownTablePluginTesting::toolbarButtonCountForTests(), 4);
	// Индексы — те же, что в таблице expected выше: 0 align, 1 auto align, 2 fit width, 3 auto fit.
	for (std::size_t commandIndex = 0; commandIndex < static_cast<std::size_t>(nbFunc); ++commandIndex)
	{
		const bool expectedButton = commandIndex < 4;
		expectTrue(failures, "toolbar button presence for command " + std::to_string(commandIndex),
			MarkdownTablePluginTesting::commandHasToolbarButtonForTests(commandIndex) == expectedButton);
	}
	MarkdownTablePluginTesting::destroyAlignToolbarIconsForTests();
	MarkdownTablePluginTesting::destroyWrapLongCellsToolbarIconsForTests();
	MarkdownTablePluginTesting::destroyAutoFitTableToolbarIconsForTests();
	MarkdownTablePluginTesting::destroyAutoAlignTableToolbarIconsForTests();
	MarkdownTablePluginTesting::destroyNarrowColumnToolbarIconsForTests();
	MarkdownTablePluginTesting::destroyWidenColumnToolbarIconsForTests();
	MarkdownTablePluginTesting::destroyAllToolbarIconsForTests();

	expectString(failures, "plugin eol keeps crlf source", MarkdownTablePluginTesting::chooseEolFromTextForTests("A,B\r\n1,2", "\n"), "\r\n");
	expectString(failures, "plugin eol keeps cr source", MarkdownTablePluginTesting::chooseEolFromTextForTests("A,B\r1,2", "\n"), "\r");
	expectString(failures, "plugin eol keeps lf source", MarkdownTablePluginTesting::chooseEolFromTextForTests("A,B\n1,2", "\r\n"), "\n");
	expectString(failures, "plugin eol uses fallback without separator", MarkdownTablePluginTesting::chooseEolFromTextForTests("A,B", "\r\n"), "\r\n");

	const MarkdownTable::EditResult endReplacementEdit = MarkdownTable::apply(
		{
			"| H | V |",
			"| --- | --- |",
			"| aa | b |"
		},
		2,
		1,
		MarkdownTable::Action::Align);
	expectTrue(failures, "plugin replacement end edit ok", endReplacementEdit.ok);
	const MarkdownTablePluginTesting::ReplacementPreview endPreview =
		MarkdownTablePluginTesting::replacementPreviewForTests(endReplacementEdit, "\r\n");
	expectString(failures, "plugin replacement end keeps crlf", endPreview.text,
		"| H   | V   |\r\n| --- | --- |\r\n| aa  | b   |");
	expectSize(failures, "plugin replacement end caret", endPreview.caretOffset, endPreview.text.find("b   |"));

	const MarkdownTablePluginTesting::ReplacementPreview mixedEolPreview =
		MarkdownTablePluginTesting::replacementPreviewForTests(endReplacementEdit, "\n");
	expectString(failures, "plugin replacement mixed boundary uses selected eol", mixedEolPreview.text,
		"| H   | V   |\n| --- | --- |\n| aa  | b   |");
	expectSize(failures, "plugin replacement mixed boundary caret", mixedEolPreview.caretOffset, mixedEolPreview.text.find("b   |"));

	const MarkdownTable::EditResult singleLineCsv = MarkdownTable::convertDelimitedToTable("Name,Score");
	expectTrue(failures, "plugin single line csv edit ok", singleLineCsv.ok);
	const MarkdownTablePluginTesting::ReplacementPreview singleLineCsvPreview =
		MarkdownTablePluginTesting::delimitedReplacementPreviewForTests("Name,Score", "\r\n", singleLineCsv);
	expectString(failures, "plugin single line csv fallback crlf", singleLineCsvPreview.text,
		"| Name | Score |\r\n| ---- | ----- |");
	expectSize(failures, "plugin single line csv caret", singleLineCsvPreview.caretOffset, std::string("| ").size());
	for (const std::string &eol : { std::string("\n"), std::string("\r\n"), std::string("\r") })
	{
		for (const std::string &suffix : { eol, eol + eol, eol + eol + eol })
		{
			const std::string source = "Name,Score" + eol + "Anna,10" + suffix;
			const MarkdownTable::EditResult edit = MarkdownTable::convertDelimitedToTable(source);
			const MarkdownTablePluginTesting::ReplacementPreview preview =
				MarkdownTablePluginTesting::delimitedReplacementPreviewForTests(source, "\n", edit);
			expectString(failures, "csv selection keeps newline before following prose", preview.text + "Following paragraph",
				"| Name | Score |" + eol + "| ---- | ----- |" + eol + "| Anna | 10    |" + suffix + "Following paragraph");
			expectSize(failures, "csv trailing newline keeps caret in first cell", preview.caretOffset, 2);
		}
	}

	const MarkdownTable::EditResult shortColumnInsert = MarkdownTable::apply(
		{
			"| H |",
			"| - |",
			"| a |"
		},
		2,
		0,
		MarkdownTable::Action::InsertColumnRight);
	expectTrue(failures, "plugin short column insert edit ok", shortColumnInsert.ok);
	const MarkdownTablePluginTesting::ReplacementPreview shortColumnPreview =
		MarkdownTablePluginTesting::replacementPreviewForTests(shortColumnInsert, "\n");
	expectString(failures, "plugin short column replacement", shortColumnPreview.text,
		"| H   |     |\n| --- | --- |\n| a   |     |");
	expectSize(failures, "plugin short column replacement caret", shortColumnPreview.caretOffset, shortColumnPreview.text.find("    |", shortColumnPreview.text.find("| a")));

	testAutomaticWorkBounds(failures);
	testEditorNotificationTransitions(failures);
	testWarningSettingsPersistence(failures);
	return failures;
}
