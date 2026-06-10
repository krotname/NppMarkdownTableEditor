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
		{ 0, "align", L"Align table", alignTable, false, true, true, true, true, static_cast<UCHAR>('1') },
		{ 1, "next cell", L"Next cell", nextCell, false, true, true, true, true, static_cast<UCHAR>('2') },
		{ 2, "previous cell", L"Previous cell", previousCell, false, true, true, true, true, static_cast<UCHAR>('3') },
		{ 3, "insert row", L"Insert row below", insertRowBelow, false, true, true, true, true, static_cast<UCHAR>('4') },
		{ 4, "delete row", L"Delete row", deleteRow, false, true, true, true, true, static_cast<UCHAR>('5') },
		{ 5, "insert column", L"Insert column right", insertColumnRight, false, true, true, true, true, static_cast<UCHAR>('6') },
		{ 6, "delete column", L"Delete column", deleteColumn, false, true, true, true, true, static_cast<UCHAR>('7') },
		{ 7, "move row up", L"Move row up", moveRowUp, false, true, true, true, true, static_cast<UCHAR>('8') },
		{ 8, "move row down", L"Move row down", moveRowDown, false, true, true, true, true, static_cast<UCHAR>('9') },
		{ 9, "move column left", L"Move column left", moveColumnLeft, false, true, true, true, true, static_cast<UCHAR>(VK_OEM_4) },
		{ 10, "move column right", L"Move column right", moveColumnRight, false, true, true, true, true, static_cast<UCHAR>(VK_OEM_6) },
		{ 11, "sort ascending", L"Sort rows ascending", sortRowsAscending, false, true, true, true, true, static_cast<UCHAR>(VK_OEM_PLUS) },
		{ 12, "sort descending", L"Sort rows descending", sortRowsDescending, false, true, true, true, true, static_cast<UCHAR>(VK_OEM_MINUS) },
		{ 13, "convert csv tsv", L"Convert CSV/TSV to table", convertCsvTsvSelectionToTable, false, true, true, true, true, static_cast<UCHAR>('0') },
		{ 14, "insert table", L"Insert table...", insertTable, false, true, true, true, true, static_cast<UCHAR>(VK_OEM_5) },
		{ 15, "tab", L"Tab: align table or indent (MD)", tabOrIndent, false, true, false, false, false, static_cast<UCHAR>(VK_TAB) },
		{ 16, "fit table to window", L"Fit table to window", wrapLongCells, false, true, true, true, true, static_cast<UCHAR>('W') },
		{ 17, "notepad word wrap", L"Notepad++ word wrap (MD)", toggleNotepadWordWrap, false, false, false, false, false, 0 },
		{ 18, "auto fit table", L"Auto fit table (MD)", toggleAutoFitTable, false, false, false, false, false, 0 },
		{ 19, "auto align table", L"Auto align table (MD)", toggleAutoAlignTable, false, false, false, false, false, 0 }
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
		L"\u0410\u0432\u0442\u043E\u043F\u043E\u0434\u0433\u043E\u043D\u043A\u0430 \u0442\u0430\u0431\u043B\u0438\u0446\u044B (MD)",
		L"\u0410\u0432\u0442\u043E\u0432\u044B\u0440\u0430\u0432\u043D\u0438\u0432\u0430\u043D\u0438\u0435 \u0442\u0430\u0431\u043B\u0438\u0446\u044B (MD)"
	};
	MarkdownTablePluginTesting::applyNativeLangFileNameForTests("russian.xml");
	expectWideString(failures, "russian plugin menu name", MarkdownTablePluginTesting::pluginMenuNameForTests(), L"\u0420\u0435\u0434\u0430\u043A\u0442\u043E\u0440 Markdown-\u0442\u0430\u0431\u043B\u0438\u0446");
	for (std::size_t index = 0; index < expectedCount; ++index)
		expectWideString(failures, std::string("russian ") + expected[index].label + " name", funcItem[expected[index].index]._itemName, russianNames[index]);

	struct LocalizedCommandSample
	{
		const char *nativeLangFileName;
		const char *label;
		const wchar_t *alignName;
	};
	const LocalizedCommandSample localizedSamples[] =
	{
		{ "chineseSimplified.xml", "mandarin chinese", L"\u5BF9\u9F50\u8868\u683C" },
		{ "hindi.xml", "hindi", L"\u0924\u093E\u0932\u093F\u0915\u093E \u0938\u0902\u0930\u0947\u0916\u093F\u0924 \u0915\u0930\u0947\u0902" },
		{ "spanish.xml", "spanish", L"Alinear tabla" },
		{ "arabic.xml", "arabic", L"\u0645\u062D\u0627\u0630\u0627\u0629 \u0627\u0644\u062C\u062F\u0648\u0644" },
		{ "french.xml", "french", L"Aligner le tableau" },
		{ "bengali.xml", "bengali", L"\u099F\u09C7\u09AC\u09BF\u09B2 \u09B8\u09BE\u09B0\u09BF\u09AC\u09A6\u09CD\u09A7 \u0995\u09B0\u09C1\u09A8" },
		{ "portuguese.xml", "portuguese", L"Alinhar tabela" },
		{ "indonesian.xml", "indonesian", L"Ratakan tabel" },
		{ "urdu.xml", "urdu", L"\u062C\u062F\u0648\u0644 \u0633\u06CC\u062F\u06BE\u0627 \u06A9\u0631\u06CC\u06BA" },
		{ "german.xml", "german", L"Tabelle ausrichten" },
		{ "japanese.xml", "japanese", L"\u30C6\u30FC\u30D6\u30EB\u3092\u6574\u5217" },
		{ "pidgin.xml", "nigerian pidgin", L"Arrange table" },
		{ "marathi.xml", "marathi", L"\u0924\u0915\u094D\u0924\u093E \u0938\u0902\u0930\u0947\u0916\u093F\u0924 \u0915\u0930\u093E" },
		{ "telugu.xml", "telugu", L"\u0C2A\u0C1F\u0C4D\u0C1F\u0C3F\u0C15\u0C28\u0C41 \u0C38\u0C30\u0C3F\u0C2A\u0C30\u0C1A\u0C41" },
		{ "turkish.xml", "turkish", L"Tabloyu hizala" },
		{ "tamil.xml", "tamil", L"\u0B85\u0B9F\u0BCD\u0B9F\u0BB5\u0BA3\u0BC8\u0BAF\u0BC8 \u0B92\u0BB4\u0BC1\u0B99\u0BCD\u0B95\u0BC1\u0BAA\u0B9F\u0BC1\u0BA4\u0BCD\u0BA4\u0BC1" },
		{ "chineseTraditional.xml", "yue chinese", L"\u5C0D\u9F4A\u8868\u683C" },
		{ "vietnamese.xml", "vietnamese", L"C\u0103n ch\u1EC9nh b\u1EA3ng" }
	};
	expectSize(failures, "additional localized command samples", sizeof(localizedSamples) / sizeof(localizedSamples[0]), static_cast<std::size_t>(18));
	for (const LocalizedCommandSample &sample : localizedSamples)
	{
		MarkdownTablePluginTesting::applyNativeLangFileNameForTests(sample.nativeLangFileName);
		expectWideString(failures, std::string(sample.label) + " align name", funcItem[0]._itemName, sample.alignName);
	}

	MarkdownTablePluginTesting::applyNativeLangFileNameForTests("");
	expectWideString(failures, "english plugin menu fallback", MarkdownTablePluginTesting::pluginMenuNameForTests(), L"Markdown Table Editor");
	for (std::size_t index = 0; index < expectedCount; ++index)
		expectCommandName(failures, expected[index], funcItem[expected[index].index]);

	expectTrue(failures, "auto fit table starts disabled by default", !MarkdownTablePluginTesting::autoFitTableEnabledForTests());
	expectTrue(failures, "auto fit table default does not fit align", !MarkdownTablePluginTesting::shouldApplyAutoFitAfterActionForTests(MarkdownTable::Action::Align));
	expectTrue(failures, "fit table command aligns before visible-width wrapping", MarkdownTablePluginTesting::coreActionForPluginActionForTests(MarkdownTable::Action::WrapLongCells) == MarkdownTable::Action::Align);
	expectTrue(failures, "fit table command always fits to visible width", MarkdownTablePluginTesting::shouldFitToWindowAfterActionForTests(MarkdownTable::Action::WrapLongCells));
	expectTrue(failures, "plain align does not fit to visible width while auto fit is off", !MarkdownTablePluginTesting::shouldFitToWindowAfterActionForTests(MarkdownTable::Action::Align));
	MarkdownTablePluginTesting::setAutoFitTableEnabledForTests(false);
	expectTrue(failures, "auto fit table starts disabled in test", !MarkdownTablePluginTesting::autoFitTableEnabledForTests());
	expectTrue(failures, "auto fit table disabled does not fit align", !MarkdownTablePluginTesting::shouldApplyAutoFitAfterActionForTests(MarkdownTable::Action::Align));
	toggleAutoFitTable();
	expectTrue(failures, "auto fit table toggles on", MarkdownTablePluginTesting::autoFitTableEnabledForTests());
	expectTrue(failures, "auto fit table enabled fits align", MarkdownTablePluginTesting::shouldApplyAutoFitAfterActionForTests(MarkdownTable::Action::Align));
	expectTrue(failures, "auto fit table enabled fits align to visible width", MarkdownTablePluginTesting::shouldFitToWindowAfterActionForTests(MarkdownTable::Action::Align));
	expectTrue(failures, "auto fit table enabled does not fit row insert", !MarkdownTablePluginTesting::shouldApplyAutoFitAfterActionForTests(MarkdownTable::Action::InsertRowBelow));
	expectTrue(failures, "auto fit table enabled does not fit row insert to visible width", !MarkdownTablePluginTesting::shouldFitToWindowAfterActionForTests(MarkdownTable::Action::InsertRowBelow));
	expectTrue(failures, "auto fit table does not auto-fit explicit wrap command", !MarkdownTablePluginTesting::shouldApplyAutoFitAfterActionForTests(MarkdownTable::Action::WrapLongCells));
	toggleAutoFitTable();
	expectTrue(failures, "auto fit table toggles off", !MarkdownTablePluginTesting::autoFitTableEnabledForTests());
	expectTrue(failures, "auto fit table off after unpress does not fit align", !MarkdownTablePluginTesting::shouldApplyAutoFitAfterActionForTests(MarkdownTable::Action::Align));
	expectTrue(failures, "fit table command starts enabled", MarkdownTablePluginTesting::fitTableToWindowCommandEnabledForTests());
	expectTrue(failures, "auto fit resize uses delayed debounce", MarkdownTablePluginTesting::fitToWindowResizeDelayMsForTests() > 0);
	expectTrue(failures, "auto fit resize ignores disabled mode", !MarkdownTablePluginTesting::shouldRunFitToWindowAfterResizeForTests(false, false, true, 100, 120));
	expectTrue(failures, "auto fit resize ignores reentrant fit", !MarkdownTablePluginTesting::shouldRunFitToWindowAfterResizeForTests(true, true, true, 100, 120));
	expectTrue(failures, "auto fit resize ignores inactive editor", !MarkdownTablePluginTesting::shouldRunFitToWindowAfterResizeForTests(true, false, false, 100, 120));
	expectTrue(failures, "auto fit resize records first width without fitting", !MarkdownTablePluginTesting::shouldRunFitToWindowAfterResizeForTests(true, false, true, 0, 120));
	expectTrue(failures, "auto fit resize ignores unchanged width", !MarkdownTablePluginTesting::shouldRunFitToWindowAfterResizeForTests(true, false, true, 120, 120));
	expectTrue(failures, "auto fit resize fits after active width change", MarkdownTablePluginTesting::shouldRunFitToWindowAfterResizeForTests(true, false, true, 120, 90));
	expectTrue(failures, "auto fit toggle runs initial fit before enabling", MarkdownTablePluginTesting::shouldRunInitialFitWhenTogglingAutoFitTableForTests(false));
	expectTrue(failures, "auto fit toggle does not run initial fit when disabling", !MarkdownTablePluginTesting::shouldRunInitialFitWhenTogglingAutoFitTableForTests(true));
	MarkdownTablePluginTesting::setAutoFitTableEnabledForTests(false);
	toggleAutoFitTable();
	expectTrue(failures, "auto fit table toggles on for resize", MarkdownTablePluginTesting::autoFitTableEnabledForTests());
	expectTrue(failures, "fit table command disabled while auto fit is on", !MarkdownTablePluginTesting::fitTableToWindowCommandEnabledForTests());
	toggleAutoFitTable();
	expectTrue(failures, "auto fit table toggles off after resize mode", !MarkdownTablePluginTesting::autoFitTableEnabledForTests());
	expectTrue(failures, "fit table command re-enabled after auto fit is off", MarkdownTablePluginTesting::fitTableToWindowCommandEnabledForTests());
	expectTrue(failures, "auto align table starts disabled", !MarkdownTablePluginTesting::autoAlignTableEnabledForTests());
	expectTrue(failures, "align table command starts enabled", MarkdownTablePluginTesting::alignTableCommandEnabledForTests());
	expectTrue(failures, "auto input update ignores disabled modes", !MarkdownTablePluginTesting::shouldRunAutoTableFormatAfterUpdateForTests(false, false, false, false, true, true));
	expectTrue(failures, "auto input update ignores reentrant align", !MarkdownTablePluginTesting::shouldRunAutoTableFormatAfterUpdateForTests(true, true, true, false, true, true));
	expectTrue(failures, "auto input update ignores reentrant fit", !MarkdownTablePluginTesting::shouldRunAutoTableFormatAfterUpdateForTests(true, true, false, true, true, true));
	expectTrue(failures, "auto input update ignores inactive editor", !MarkdownTablePluginTesting::shouldRunAutoTableFormatAfterUpdateForTests(true, false, false, false, false, true));
	expectTrue(failures, "auto input update ignores selection-only update", !MarkdownTablePluginTesting::shouldRunAutoTableFormatAfterUpdateForTests(true, false, false, false, true, false));
	expectTrue(failures, "auto input update runs immediate auto align", MarkdownTablePluginTesting::shouldRunAutoTableFormatAfterUpdateForTests(true, false, false, false, true, true));
	expectTrue(failures, "auto input update runs immediate auto fit", MarkdownTablePluginTesting::shouldRunAutoTableFormatAfterUpdateForTests(false, true, false, false, true, true));
	expectTrue(failures, "auto input update runs immediate combined auto mode", MarkdownTablePluginTesting::shouldRunAutoTableFormatAfterUpdateForTests(true, true, false, false, true, true));
	const std::string privet = "\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82";
	const std::string pri = "\xD0\xBF\xD1\x80\xD0\xB8";
	const std::string sourcePrivetLine = "| " + privet + " |";
	const std::string paddedPrivetLine = "| " + privet + "     |";
	const std::size_t caretAfterPri = sourcePrivetLine.find(privet) + pri.size();
	expectSize(failures, "auto input preserves utf8 cell caret after same formatting",
		MarkdownTablePluginTesting::preservedCellCaretColumnOffsetForTests(sourcePrivetLine, 0, caretAfterPri, sourcePrivetLine),
		caretAfterPri);
	expectSize(failures, "auto input preserves utf8 cell caret after padding changes",
		MarkdownTablePluginTesting::preservedCellCaretColumnOffsetForTests(sourcePrivetLine, 0, caretAfterPri, paddedPrivetLine),
		paddedPrivetLine.find(privet) + pri.size());
	expectTrue(failures, "auto align toggle runs initial align before enabling", MarkdownTablePluginTesting::shouldRunInitialAlignWhenTogglingAutoAlignTableForTests(false));
	expectTrue(failures, "auto align toggle does not run initial align when disabling", !MarkdownTablePluginTesting::shouldRunInitialAlignWhenTogglingAutoAlignTableForTests(true));
	MarkdownTablePluginTesting::setAutoAlignTableEnabledForTests(false);
	toggleAutoAlignTable();
	expectTrue(failures, "auto align table toggles on", MarkdownTablePluginTesting::autoAlignTableEnabledForTests());
	expectTrue(failures, "align table command disabled while auto align is on", !MarkdownTablePluginTesting::alignTableCommandEnabledForTests());
	toggleAutoAlignTable();
	expectTrue(failures, "auto align table toggles off", !MarkdownTablePluginTesting::autoAlignTableEnabledForTests());
	expectTrue(failures, "align table command re-enabled after auto align is off", MarkdownTablePluginTesting::alignTableCommandEnabledForTests());
	expectTrue(failures, "tab toolbar icons are created", MarkdownTablePluginTesting::ensureTabToolbarIconsForTests());
	expectTrue(failures, "wrap long cells toolbar icons are created", MarkdownTablePluginTesting::ensureWrapLongCellsToolbarIconsForTests());
	expectTrue(failures, "notepad word wrap toolbar icons are created", MarkdownTablePluginTesting::ensureNotepadWordWrapToolbarIconsForTests());
	expectTrue(failures, "auto fit table toolbar icons are created", MarkdownTablePluginTesting::ensureAutoFitTableToolbarIconsForTests());
	expectTrue(failures, "auto align table toolbar icons are created", MarkdownTablePluginTesting::ensureAutoAlignTableToolbarIconsForTests());
	MarkdownTablePluginTesting::destroyTabToolbarIconsForTests();
	MarkdownTablePluginTesting::destroyWrapLongCellsToolbarIconsForTests();
	MarkdownTablePluginTesting::destroyNotepadWordWrapToolbarIconsForTests();
	MarkdownTablePluginTesting::destroyAutoFitTableToolbarIconsForTests();
	MarkdownTablePluginTesting::destroyAutoAlignTableToolbarIconsForTests();

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

	return failures;
}
