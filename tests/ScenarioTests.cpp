// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 krotname

#include "ScenarioTests.h"
#include "../src/MarkdownTableCore.h"

#include <iostream>
#include <string>
#include <vector>

namespace
{
int g_checks = 0;
int g_failures = 0;

void fail(const char *name, const std::string &message)
{
	++g_failures;
	std::cerr << "scenario " << name << " failed: " << message << "\n";
}

void expectTrue(const char *name, bool value)
{
	++g_checks;
	if (!value)
		fail(name, "expected true");
}

void expectSize(const char *name, std::size_t actual, std::size_t expected)
{
	++g_checks;
	if (actual != expected)
		fail(name, std::string("expected ") + std::to_string(expected) + ", got " + std::to_string(actual));
}

void expectContains(const char *name, const std::string &actual, const std::string &expected)
{
	++g_checks;
	if (actual.find(expected) == std::string::npos)
		fail(name, std::string("expected to contain \"") + expected + "\"");
}

void expectLines(const char *name, const std::vector<std::string> &actual, const std::vector<std::string> &expected)
{
	++g_checks;
	if (actual == expected)
		return;

	++g_failures;
	std::cerr << "scenario " << name << " failed\nExpected:\n";
	for (std::size_t i = 0; i < expected.size(); ++i)
		std::cerr << expected[i] << "\n";
	std::cerr << "Actual:\n";
	for (std::size_t i = 0; i < actual.size(); ++i)
		std::cerr << actual[i] << "\n";
}

std::string joinLines(const std::vector<std::string> &lines)
{
	std::string result;
	for (std::size_t i = 0; i < lines.size(); ++i)
	{
		if (i != 0)
			result += "\n";
		result += lines[i];
	}
	return result;
}

void runEditorCommandScenarios()
{
	const std::vector<std::string> table =
	{
		"| Name | Age |",
		"| --- | ---: |",
		"| Anna | 20 |",
		"| Alexander | 7 |"
	};

	expectLines("align table", MarkdownTable::apply(table, 2, 0, MarkdownTable::Action::Align).lines,
		{
			"| Name      | Age |",
			"| --------- | --: |",
			"| Anna      |  20 |",
			"| Alexander |   7 |"
		});

	expectTrue("plain pipe rejected", !MarkdownTable::apply({ "Use A | B in text" }, 0, 0, MarkdownTable::Action::Align).ok);

	expectLines("next cell appends row", MarkdownTable::apply(table, 3, 1, MarkdownTable::Action::NextCell).lines,
		{
			"| Name      | Age |",
			"| --------- | --: |",
			"| Anna      |  20 |",
			"| Alexander |   7 |",
			"|           |     |"
		});

	expectLines("previous cell realigns", MarkdownTable::apply(table, 2, 0, MarkdownTable::Action::PreviousCell).lines,
		{
			"| Name      | Age |",
			"| --------- | --: |",
			"| Anna      |  20 |",
			"| Alexander |   7 |"
		});

	expectLines("insert row below", MarkdownTable::apply(
		{
			"| Name | Age |",
			"| --- | --- |",
			"| Anna | 20 |"
		},
		2,
		0,
		MarkdownTable::Action::InsertRowBelow).lines,
		{
			"| Name | Age |",
			"| ---- | --- |",
			"| Anna | 20  |",
			"|      |     |"
		});

	expectLines("delete data row", MarkdownTable::apply(
		{
			"| Name | Age |",
			"| --- | --- |",
			"| Anna | 20 |",
			"| Bob | 7 |"
		},
		2,
		0,
		MarkdownTable::Action::DeleteRow).lines,
		{
			"| Name | Age |",
			"| ---- | --- |",
			"| Bob  | 7   |"
		});

	expectLines("delete header is blocked", MarkdownTable::apply(
		{
			"| H | V |",
			"| --- | --- |",
			"| a | b |"
		},
		0,
		0,
		MarkdownTable::Action::DeleteRow).lines,
		{
			"| H   | V   |",
			"| --- | --- |",
			"| a   | b   |"
		});

	expectLines("insert column right", MarkdownTable::apply(table, 2, 0, MarkdownTable::Action::InsertColumnRight).lines,
		{
			"| Name      |     | Age |",
			"| --------- | --- | --: |",
			"| Anna      |     |  20 |",
			"| Alexander |     |   7 |"
		});

	expectLines("delete column", MarkdownTable::apply(table, 2, 0, MarkdownTable::Action::DeleteColumn).lines,
		{
			"| Age |",
			"| --: |",
			"|  20 |",
			"|   7 |"
		});

	expectLines("move row up", MarkdownTable::apply(
		{
			"| A |",
			"| --- |",
			"| 1 |",
			"| 2 |"
		},
		3,
		0,
		MarkdownTable::Action::MoveRowUp).lines,
		{
			"| A   |",
			"| --- |",
			"| 2   |",
			"| 1   |"
		});

	expectLines("move row down", MarkdownTable::apply(
		{
			"| A |",
			"| --- |",
			"| 1 |",
			"| 2 |"
		},
		2,
		0,
		MarkdownTable::Action::MoveRowDown).lines,
		{
			"| A   |",
			"| --- |",
			"| 2   |",
			"| 1   |"
		});

	expectLines("move column left", MarkdownTable::apply(
		{
			"| A | B | C |",
			"| --- | --- | --- |",
			"| 1 | 2 | 3 |"
		},
		2,
		2,
		MarkdownTable::Action::MoveColumnLeft).lines,
		{
			"| A   | C   | B   |",
			"| --- | --- | --- |",
			"| 1   | 3   | 2   |"
		});

	expectLines("move column right", MarkdownTable::apply(
		{
			"| A | B | C |",
			"| --- | --- | --- |",
			"| 1 | 2 | 3 |"
		},
		2,
		0,
		MarkdownTable::Action::MoveColumnRight).lines,
		{
			"| B   | A   | C   |",
			"| --- | --- | --- |",
			"| 2   | 1   | 3   |"
		});

	const std::vector<std::string> sortTable =
	{
		"| Name | Score |",
		"| --- | ---: |",
		"| Anna | 42 |",
		"| Dmitry | 7 |",
		"| Chen | 100 |"
	};
	expectLines("sort rows ascending by score", MarkdownTable::apply(sortTable, 2, 1, MarkdownTable::Action::SortRowsAscending).lines,
		{
			"| Name   | Score |",
			"| ------ | ----: |",
			"| Dmitry |     7 |",
			"| Anna   |    42 |",
			"| Chen   |   100 |"
		});
	expectLines("sort rows descending by name", MarkdownTable::apply(sortTable, 2, 0, MarkdownTable::Action::SortRowsDescending).lines,
		{
			"| Name   | Score |",
			"| ------ | ----: |",
			"| Dmitry |     7 |",
			"| Chen   |   100 |",
			"| Anna   |    42 |"
		});
	expectLines("sort rows ascending cyrillic case fold", MarkdownTable::apply(
		{
			"| Name |",
			"| --- |",
			"| Яна |",
			"| борис |",
			"| Анна |"
		},
		2,
		0,
		MarkdownTable::Action::SortRowsAscending).lines,
		{
			"| Name  |",
			"| ----- |",
			"| Анна  |",
			"| борис |",
			"| Яна   |"
		});

	expectLines("insert table dialog result", MarkdownTable::createTable(2, 1).lines,
		{
			"| Column 1 | Column 2 |",
			"| -------- | -------- |",
			"|          |          |"
		});

	const std::vector<std::string> unwrapped =
	{
		"Name | Age",
		"--- | ---:",
		"Anna | 20"
	};
	expectLines("unwrapped align preserves style", MarkdownTable::apply(unwrapped, 2, 1, MarkdownTable::Action::Align).lines,
		{
			"Name | Age",
			"---- | --:",
			"Anna |  20"
		});
}

void runMalformedTableScenarios()
{
	expectLines("uneven malformed rows align", MarkdownTable::apply(
		{
			"| H | V | Tail |",
			"| --- | --- | --- |",
			"| |",
			"| short |",
			"| a | b | c | d |"
		},
		3,
		0,
		MarkdownTable::Action::Align).lines,
		{
			"| H     | V   | Tail |     |",
			"| ----- | --- | ---- | --- |",
			"|       |     |      |     |",
			"| short |     |      |     |",
			"| a     | b   | c    | d   |"
		});

	const std::vector<std::string> mixedPipeStyle =
	{
		"A | B",
		"| --- | --- |",
		"| 1 | 2 |",
		"3 | 4"
	};
	const MarkdownTable::TableRange mixedPipeRange = MarkdownTable::findTableRange(mixedPipeStyle, 2);
	expectTrue("mixed pipe style range found", mixedPipeRange.found);
	expectSize("mixed pipe style range start", mixedPipeRange.firstRow, 0);
	expectSize("mixed pipe style range end", mixedPipeRange.lastRow, 3);
	expectLines("mixed pipe style normalizes majority wrapper", MarkdownTable::apply(mixedPipeStyle, 2, 0, MarkdownTable::Action::Align).lines,
		{
			"| A   | B   |",
			"| --- | --- |",
			"| 1   | 2   |",
			"| 3   | 4   |"
		});

	const std::vector<std::string> invalidAlignment =
	{
		"| H | V |",
		"| :---x | --- |",
		"| a | b |"
	};
	expectTrue("invalid alignment row range rejected", !MarkdownTable::findTableRange(invalidAlignment, 2).found);
	expectTrue("invalid alignment row apply rejected", !MarkdownTable::apply(invalidAlignment, 2, 0, MarkdownTable::Action::Align).ok);

	const std::vector<std::string> blankBreaksTable =
	{
		"| H | V |",
		"| --- | --- |",
		"| a | b |",
		"",
		"| orphan | row |"
	};
	const MarkdownTable::TableRange beforeBlank = MarkdownTable::findTableRange(blankBreaksTable, 2);
	expectTrue("blank line ends malformed table", beforeBlank.found);
	expectSize("blank line range end", beforeBlank.lastRow, 2);
	expectTrue("orphan row after blank rejected", !MarkdownTable::findTableRange(blankBreaksTable, 4).found);
}

void runShortColumnScenarios()
{
	const MarkdownTable::EditResult singleInsert = MarkdownTable::apply(
		{
			"| H |",
			"| - |",
			"| a |"
		},
		2,
		0,
		MarkdownTable::Action::InsertColumnRight);
	expectTrue("single short column insert ok", singleInsert.ok);
	expectSize("single short column insert target column", singleInsert.targetColumn, 1);
	expectSize("single short column insert target offset", singleInsert.targetColumnOffset, 8);
	expectLines("single short column insert", singleInsert.lines,
		{
			"| H   |     |",
			"| --- | --- |",
			"| a   |     |"
		});

	const MarkdownTable::EditResult twoColumnInsert = MarkdownTable::apply(
		{
			"| A | B |",
			"| - | - |",
			"| x | |"
		},
		2,
		1,
		MarkdownTable::Action::InsertColumnRight);
	expectTrue("two short columns insert ok", twoColumnInsert.ok);
	expectSize("two short columns insert target column", twoColumnInsert.targetColumn, 2);
	expectSize("two short columns insert target offset", twoColumnInsert.targetColumnOffset, 14);
	expectLines("two short columns insert", twoColumnInsert.lines,
		{
			"| A   | B   |     |",
			"| --- | --- | --- |",
			"| x   |     |     |"
		});

	const MarkdownTable::EditResult twoColumnDelete = MarkdownTable::apply(
		{
			"| A | B |",
			"| - | - |",
			"| x | |"
		},
		2,
		1,
		MarkdownTable::Action::DeleteColumn);
	expectTrue("two short columns delete ok", twoColumnDelete.ok);
	expectSize("two short columns delete target column", twoColumnDelete.targetColumn, 0);
	expectSize("two short columns delete target offset", twoColumnDelete.targetColumnOffset, 2);
	expectLines("two short columns delete", twoColumnDelete.lines,
		{
			"| A   |",
			"| --- |",
			"| x   |"
		});
}

void runDelimitedScenarios()
{
	expectLines("convert csv selection", MarkdownTable::convertDelimitedToTable("Name,Role,Score\nAnna,Engineer,42\nDmitry,QA,7").lines,
		{
			"| Name   | Role     | Score |",
			"| ------ | -------- | ----- |",
			"| Anna   | Engineer | 42    |",
			"| Dmitry | QA       | 7     |"
		});
	expectLines("convert tsv selection", MarkdownTable::convertDelimitedToTable("Name\tNote\nAnna\tA|B").lines,
		{
			"| Name | Note |",
			"| ---- | ---- |",
			"| Anna | A\\|B |"
		});
}

void runLargeDataScenarios()
{
	const MarkdownTable::EditResult largeTable = MarkdownTable::apply(
		{
			"| Topic | Description | Note |",
			"| --- | --- | --- |",
			"| Lorem | Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. | alpha |",
			"| Рыба | Съешь еще этих мягких французских булок, да выпей чаю; длинная проверка ширины кириллицы. | beta \\| pipe |",
			"| CJK | 表示確認のための長いセルと punctuation, commas, and quotes \"ok\". | gamma |",
			"| Mixed | Markdown \\| escaped pipe stays in one cell while text keeps going for a wider table. | delta |",
			"| Numbers | 1234567890 9876543210 3.1415926535 -42.75 and padded numeric-looking prose. | epsilon |",
			"| URL | https://example.com/path/to/a/very/long/resource?query=markdown-table-editor&fixture=fish | zeta |"
		},
		2,
		0,
		MarkdownTable::Action::Align);
	expectTrue("large fish table ok", largeTable.ok);
	expectSize("large fish table line count", largeTable.lines.size(), 8);
	const std::string largeText = joinLines(largeTable.lines);
	expectContains("large fish table keeps cyrillic", largeText, "Съешь еще этих мягких французских булок");
	expectContains("large fish table keeps cjk", largeText, "表示確認");
	expectContains("large fish table keeps escaped pipe", largeText, "Markdown \\| escaped pipe");

	std::vector<std::string> hugeLines;
	hugeLines.push_back("| Id | Payload | Note |");
	hugeLines.push_back("| --- | --- | --- |");
	const std::string payload =
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit; "
		"Съешь еще этих мягких французских булок; "
		"表示確認 and Markdown \\| escaped pipe remain stable.";
	for (std::size_t i = 0; i < 160; ++i)
	{
		hugeLines.push_back(std::string("| row-") + std::to_string(i) + " | " + payload + " chunk-" + std::to_string(i) + " | note-" + std::to_string(i) + " |");
	}
	const MarkdownTable::EditResult hugeTable = MarkdownTable::apply(hugeLines, 80, 1, MarkdownTable::Action::Align);
	expectTrue("huge generated table ok", hugeTable.ok);
	expectSize("huge generated table line count", hugeTable.lines.size(), 162);
	const std::string hugeText = joinLines(hugeTable.lines);
	expectContains("huge generated table keeps first row", hugeText, "row-0");
	expectContains("huge generated table keeps last row", hugeText, "row-159");
	expectContains("huge generated table keeps escaped pipe", hugeText, "Markdown \\| escaped pipe");

	const MarkdownTable::EditResult largeCsv = MarkdownTable::convertDelimitedToTable(
		"Name,Story,Score\r\n"
		"Anna,\"Lorem ipsum dolor sit amet, consectetur adipiscing elit, with comma\",42\r\n"
		"Борис,\"Длинная рыба с переносом\r\nвнутри кавычек и pipe |\",7\r\n"
		"Chen,\"CJK 表 with comma, quote \"\"ok\"\", and more filler text\",100\r\n"
		"Delta,\"one two three four five six seven eight nine ten\",-12.5\r\n");
	expectTrue("large fish csv ok", largeCsv.ok);
	expectSize("large fish csv line count", largeCsv.lines.size(), 6);
	const std::string largeCsvText = joinLines(largeCsv.lines);
	expectContains("large fish csv flattens newline", largeCsvText, "переносом внутри кавычек");
	expectContains("large fish csv escapes pipe", largeCsvText, "pipe \\|");
	expectContains("large fish csv keeps quotes", largeCsvText, "quote \"ok\"");

	std::string hugeCsv = "Id,Text,Score\r\n";
	for (std::size_t i = 0; i < 220; ++i)
	{
		hugeCsv += std::string("row-") + std::to_string(i) + ",\""
			"Lorem ipsum dolor sit amet, with comma, "
			"quote \"\"ok\"\", pipe |, кириллица, 表, and generated chunk " + std::to_string(i) + "\"," +
			std::to_string(static_cast<int>(i) - 110) + "\r\n";
	}
	const MarkdownTable::EditResult hugeCsvTable = MarkdownTable::convertDelimitedToTable(hugeCsv);
	expectTrue("huge generated csv ok", hugeCsvTable.ok);
	expectSize("huge generated csv line count", hugeCsvTable.lines.size(), 222);
	const std::string hugeCsvText = joinLines(hugeCsvTable.lines);
	expectContains("huge generated csv keeps first row", hugeCsvText, "row-0");
	expectContains("huge generated csv keeps last row", hugeCsvText, "row-219");
	expectContains("huge generated csv escapes pipe", hugeCsvText, "pipe \\|");
	expectContains("huge generated csv keeps quotes", hugeCsvText, "quote \"ok\"");
}
}

int runScenarioUnitTests()
{
	g_checks = 0;
	g_failures = 0;

	runEditorCommandScenarios();
	runMalformedTableScenarios();
	runShortColumnScenarios();
	runDelimitedScenarios();
	runLargeDataScenarios();

	if (g_failures == 0)
		std::cout << "Scenario unit tests passed (" << g_checks << " checks)\n";
	else
		std::cerr << g_failures << " scenario test(s) failed\n";

	return g_failures;
}
