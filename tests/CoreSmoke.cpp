// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 krotname

#include "../src/MarkdownTableCore.h"
#include "GoldenFixtureTests.h"
#include "ScenarioTests.h"

#include <iostream>
#include <string>
#include <vector>

namespace
{
int g_failures = 0;

void fail(const char *name, const std::string &message)
{
	++g_failures;
	std::cerr << name << " failed: " << message << "\n";
}

void expectTrue(const char *name, bool value)
{
	if (!value)
		fail(name, "expected true");
}

void expectSize(const char *name, std::size_t actual, std::size_t expected)
{
	if (actual == expected)
		return;

	++g_failures;
	std::cerr << name << " failed: expected " << expected << ", got " << actual << "\n";
}

void expectString(const char *name, const std::string &actual, const std::string &expected)
{
	if (actual == expected)
		return;

	++g_failures;
	std::cerr << name << " failed: expected \"" << expected << "\", got \"" << actual << "\"\n";
}

void expectLines(const char *name, const std::vector<std::string> &actual, const std::vector<std::string> &expected)
{
	if (actual == expected)
		return;

	++g_failures;
	std::cerr << name << " failed\n";
	std::cerr << "Expected:\n";
	for (std::size_t i = 0; i < expected.size(); ++i)
		std::cerr << expected[i] << "\n";
	std::cerr << "Actual:\n";
	for (std::size_t i = 0; i < actual.size(); ++i)
		std::cerr << actual[i] << "\n";
}

}

int main()
{
	const MarkdownTable::EditResult emptyApply = MarkdownTable::apply(std::vector<std::string>(), 0, 0, MarkdownTable::Action::Align);
	expectTrue("empty apply rejected", !emptyApply.ok);
	expectString("empty apply message", emptyApply.message, "No table found");

	const std::vector<std::string> input =
	{
		"| Name | Age |",
		"| --- | ---: |",
		"| Anna | 20 |",
		"| Alexander | 7 |"
	};

	const MarkdownTable::EditResult aligned = MarkdownTable::apply(input, 2, 0, MarkdownTable::Action::Align);
	expectTrue("align ok", aligned.ok);

	expectLines("align", aligned.lines,
		{
			"| Name      | Age |",
			"| --------- | --: |",
			"| Anna      |  20 |",
			"| Alexander |   7 |"
		});

	const MarkdownTable::EditResult plainPipe = MarkdownTable::apply({ "Use A | B in text" }, 0, 0, MarkdownTable::Action::Align);
	expectTrue("plain pipe is not table", !plainPipe.ok);
	expectSize("plain pipe keeps empty result", plainPipe.lines.size(), 0);
	expectTrue("negative row range is rejected", !MarkdownTable::findTableRange(input, -1).found);
	expectLines("negative row and column clamp to first cell", MarkdownTable::apply(input, -1, -1, MarkdownTable::Action::Align).lines,
		{
			"| Name      | Age |",
			"| --------- | --: |",
			"| Anna      |  20 |",
			"| Alexander |   7 |"
		});
	expectTrue("pipe-only separator is not table", !MarkdownTable::findTableRange({ "| A | B |", "|   |   |", "| 1 | 2 |" }, 2).found);
	expectTrue("equals short separator is table", MarkdownTable::findTableRange({ "| A | B |", "| === | === |", "| 1 | 2 |" }, 2).found);

	const std::vector<std::string> adjacentPipeText =
	{
		"Use A | B in text",
		"| H | V |",
		"| --- | --- |",
		"| a | b |",
		"Next A | B"
	};
	const MarkdownTable::TableRange adjacentTable = MarkdownTable::findTableRange(adjacentPipeText, 3);
	expectTrue("adjacent pipe range found", adjacentTable.found);
	expectSize("adjacent pipe range start", adjacentTable.firstRow, 1);
	expectSize("adjacent pipe range end", adjacentTable.lastRow, 3);
	expectTrue("adjacent pipe text before rejected", !MarkdownTable::findTableRange(adjacentPipeText, 0).found);
	expectTrue("adjacent pipe text after rejected", !MarkdownTable::findTableRange(adjacentPipeText, 4).found);
	expectTrue("empty eof line after table rejected", !MarkdownTable::findTableRange({ "| A |", "| --- |", "| 1 |", "" }, 3).found);
	const MarkdownTable::EditResult adjacentApply = MarkdownTable::apply(adjacentPipeText, 3, 0, MarkdownTable::Action::Align);
	expectTrue("adjacent pipe apply ok", adjacentApply.ok);
	expectLines("adjacent pipe apply excludes text", adjacentApply.lines,
		{
			"| H   | V   |",
			"| --- | --- |",
			"| a   | b   |"
		});
	expectTrue("adjacent pipe text before apply rejected", !MarkdownTable::apply(adjacentPipeText, 0, 0, MarkdownTable::Action::Align).ok);
	expectTrue("adjacent pipe text after apply rejected", !MarkdownTable::apply(adjacentPipeText, 4, 0, MarkdownTable::Action::Align).ok);

	const MarkdownTable::EditResult next = MarkdownTable::apply(input, 3, 1, MarkdownTable::Action::NextCell);
	expectTrue("next cell ok", next.ok);
	expectSize("next cell row count", next.lines.size(), 5);
	expectSize("next cell target row", next.targetRow, 4);
	expectSize("next cell target column", next.targetColumn, 0);

	const MarkdownTable::EditResult previous = MarkdownTable::apply(input, 2, 0, MarkdownTable::Action::PreviousCell);
	expectTrue("previous cell ok", previous.ok);
	expectSize("previous cell skips separator", previous.targetRow, 0);
	expectSize("previous cell wraps to last column", previous.targetColumn, 1);

	const MarkdownTable::EditResult insertColumn = MarkdownTable::apply(input, 2, 0, MarkdownTable::Action::InsertColumnRight);
	expectTrue("insert column ok", insertColumn.ok);
	expectLines("insert column", insertColumn.lines,
		{
			"| Name      |     | Age |",
			"| --------- | --- | --: |",
			"| Anna      |     |  20 |",
			"| Alexander |     |   7 |"
		});

	expectLines("delete column preserves alignment", MarkdownTable::apply(input, 2, 0, MarkdownTable::Action::DeleteColumn).lines,
		{
			"| Age |",
			"| --: |",
			"|  20 |",
			"|   7 |"
		});

	const MarkdownTable::EditResult deleteLastColumn = MarkdownTable::apply(input, 2, 1, MarkdownTable::Action::DeleteColumn);
	expectTrue("delete last column ok", deleteLastColumn.ok);
	expectSize("delete last column target", deleteLastColumn.targetColumn, 0);
	expectLines("delete last column", deleteLastColumn.lines,
		{
			"| Name      |",
			"| --------- |",
			"| Anna      |",
			"| Alexander |"
		});

	expectLines("move column preserves alignment", MarkdownTable::apply(input, 2, 1, MarkdownTable::Action::MoveColumnLeft).lines,
		{
			"| Age | Name      |",
			"| --: | --------- |",
			"|  20 | Anna      |",
			"|   7 | Alexander |"
		});

	const std::vector<std::string> unwrapped =
	{
		"Name | Age",
		"--- | ---:",
		"Anna | 20"
	};
	expectSize("unwrapped cursor first cell", MarkdownTable::columnFromCursor(unwrapped[2], 1), 0);
	expectSize("unwrapped cursor second cell", MarkdownTable::columnFromCursor(unwrapped[2], 7), 1);
	expectLines("unwrapped align", MarkdownTable::apply(unwrapped, 2, 1, MarkdownTable::Action::Align).lines,
		{
			"Name | Age",
			"---- | --:",
			"Anna |  20"
		});

	const MarkdownTable::EditResult wrappedLongCells = MarkdownTable::apply(
		{
			"| Key | Value |",
			"| --- | --- |",
			"| row | alpha beta gamma delta epsilon zeta eta theta |"
		},
		2,
		1,
		MarkdownTable::Action::WrapLongCells);
	expectTrue("wrap long cells ok", wrappedLongCells.ok);
	expectSize("wrap long cells target row", wrappedLongCells.targetRow, 2);
	expectSize("wrap long cells target column", wrappedLongCells.targetColumn, 1);
	expectLines("wrap long cells", wrappedLongCells.lines,
		{
			"| Key | Value                  |",
			"| --- | ---------------------- |",
			"| row | alpha beta gamma delta |",
			"|     | epsilon zeta eta theta |"
		});

	const MarkdownTable::EditResult wrappedProtectedTokens = MarkdownTable::apply(
		{
			"| Key | Value | Other |",
			"| --- | --- | --- |",
			"| protected | before [Codex Desktop registry](<C:/tmp/patch registry.md>) after ``code span with spaces`` alpha beta gamma delta epsilon zeta | empty |",
			"| empty | | [broken bracket text keeps moving across words |"
		},
		2,
		1,
		MarkdownTable::Action::WrapLongCells);
	expectTrue("wrap protected tokens ok", wrappedProtectedTokens.ok);
	expectTrue("wrap protected tokens expands rows", wrappedProtectedTokens.lines.size() > 5);
	std::string wrappedProtectedText;
	for (std::size_t i = 0; i < wrappedProtectedTokens.lines.size(); ++i)
		wrappedProtectedText += wrappedProtectedTokens.lines[i] + "\n";
	expectTrue("wrap splits long markdown link token", wrappedProtectedText.find("[Codex Desktop registry](") != std::string::npos);
	expectTrue("wrap keeps markdown link remainder", wrappedProtectedText.find("/patch registry.md>)") != std::string::npos);
	expectTrue("wrap no longer keeps overwide markdown link token", wrappedProtectedText.find("[Codex Desktop registry](<C:/tmp/patch registry.md>)") == std::string::npos);
	expectTrue("wrap keeps code span token", wrappedProtectedText.find("``code span with spaces``") != std::string::npos);
	expectTrue("wrap keeps malformed bracket token text", wrappedProtectedText.find("[broken") != std::string::npos);

	const std::string escaped = "| a \\| b | c |";
	expectTrue("escaped pipe potential", MarkdownTable::isPotentialTableLine(escaped));
	expectTrue("only escaped pipe is not table", !MarkdownTable::isPotentialTableLine("a \\| b"));
	expectTrue("double escaped pipe is table", MarkdownTable::isPotentialTableLine("a \\\\| b"));
	expectSize("escaped pipe cursor", MarkdownTable::columnFromCursor(escaped, escaped.find("c")), 1);
	expectSize("cursor with leading spaces and pipe", MarkdownTable::columnFromCursor("  | a | b |", 8), 1);
	expectLines("escaped pipe align", MarkdownTable::apply(
		{
			"| H | X |",
			"| --- | --- |",
			"| a \\| b | c |"
		},
		2,
		0,
		MarkdownTable::Action::Align).lines,
		{
			"| H      | X   |",
			"| ------ | --- |",
			"| a \\| b | c   |"
		});

	expectLines("unicode width", MarkdownTable::apply(
		{
			"| Word | Value |",
			"| --- | --- |",
			"| тест | 1 |",
			"| 表 | 22 |"
		},
		2,
		0,
		MarkdownTable::Action::Align).lines,
		{
			"| Word | Value |",
			"| ---- | ----- |",
			"| тест | 1     |",
			"| 表   | 22    |"
		});

	expectLines("alignment variants", MarkdownTable::apply(
		{
			"| Left | Center | Right | Plain |",
			"| :--- | :---: | ---: | --- |",
			"| a | b | c | d |",
			"| long | wide | 123 | text |"
		},
		2,
		0,
		MarkdownTable::Action::Align).lines,
		{
			"| Left | Center | Right | Plain |",
			"| :--- | :----: | ----: | ----- |",
			"| a    |   b    |     c | d     |",
			"| long |  wide  |   123 | text  |"
		});

	const MarkdownTable::EditResult deleteLast = MarkdownTable::apply(
		{
			"| H | V |",
			"| --- | --- |",
			"| a | b |"
		},
		2,
		0,
		MarkdownTable::Action::DeleteRow);
	expectTrue("delete last data row ok", deleteLast.ok);
	expectSize("delete last data row target", deleteLast.targetRow, 0);
	expectLines("delete last data row", deleteLast.lines,
		{
			"| H   | V   |",
			"| --- | --- |"
		});

	expectLines("keep only editable row", MarkdownTable::apply(
		{
			"| H | V |",
			"| --- | --- |"
		},
		0,
		0,
		MarkdownTable::Action::DeleteRow).lines,
		{
			"| H   | V   |",
			"| --- | --- |"
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

	expectLines("move row up blocked by separator", MarkdownTable::apply(
		{
			"| A |",
			"| --- |",
			"| 1 |"
		},
		2,
		0,
		MarkdownTable::Action::MoveRowUp).lines,
		{
			"| A   |",
			"| --- |",
			"| 1   |"
		});

	expectLines("delete only column keeps table", MarkdownTable::apply(
		{
			"| A |",
			"| --- |",
			"| 1 |"
		},
		2,
		0,
		MarkdownTable::Action::DeleteColumn).lines,
		{
			"| A   |",
			"| --- |",
			"| 1   |"
		});

	const MarkdownTable::EditResult previousFromData = MarkdownTable::apply(
		{
			"| A | B |",
			"| --- | --- |",
			"| 1 | 2 |"
		},
		2,
		0,
		MarkdownTable::Action::PreviousCell);
	expectTrue("previous from first data ok", previousFromData.ok);
	expectSize("previous from first data row", previousFromData.targetRow, 0);
	expectSize("previous from first data column", previousFromData.targetColumn, 1);

	expectLines("sort rows ascending numeric", MarkdownTable::apply(
		{
			"| Name | Score |",
			"| --- | ---: |",
			"| Anna | 42 |",
			"| Dmitry | 7 |",
			"| Chen | 100 |"
		},
		2,
		1,
		MarkdownTable::Action::SortRowsAscending).lines,
		{
			"| Name   | Score |",
			"| ------ | ----: |",
			"| Dmitry |     7 |",
			"| Anna   |    42 |",
			"| Chen   |   100 |"
		});

	const MarkdownTable::EditResult sortTarget = MarkdownTable::apply(
		{
			"| Name | Score |",
			"| --- | ---: |",
			"| Anna | 42 |",
			"| Dmitry | 7 |",
			"| Chen | 100 |"
		},
		2,
		1,
		MarkdownTable::Action::SortRowsAscending);
	expectTrue("sort target ok", sortTarget.ok);
	expectSize("sort target follows current row", sortTarget.targetRow, 3);

	expectLines("sort rows ascending keeps stable ties", MarkdownTable::apply(
		{
			"| Name | Score |",
			"| --- | ---: |",
			"| Anna | 2 |",
			"| Boris | 10 |",
			"| Chen | 2 |"
		},
		2,
		1,
		MarkdownTable::Action::SortRowsAscending).lines,
		{
			"| Name  | Score |",
			"| ----- | ----: |",
			"| Anna  |     2 |",
			"| Chen  |     2 |",
			"| Boris |    10 |"
		});

	expectLines("sort rows descending text", MarkdownTable::apply(
		{
			"| Name | Score |",
			"| --- | ---: |",
			"| Anna | 42 |",
			"| Dmitry | 7 |",
			"| Chen | 100 |"
		},
		2,
		0,
		MarkdownTable::Action::SortRowsDescending).lines,
		{
			"| Name   | Score |",
			"| ------ | ----: |",
			"| Dmitry |     7 |",
			"| Chen   |   100 |",
			"| Anna   |    42 |"
		});

	expectLines("sort rows descending numeric decimals", MarkdownTable::apply(
		{
			"| Name | Score |",
			"| --- | ---: |",
			"| low | -1.5 |",
			"| mid | 2 |",
			"| high | 10.25 |"
		},
		2,
		1,
		MarkdownTable::Action::SortRowsDescending).lines,
		{
			"| Name | Score |",
			"| ---- | ----: |",
			"| high | 10.25 |",
			"| mid  |     2 |",
			"| low  |  -1.5 |"
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

	expectLines("sort table without data rows", MarkdownTable::apply(
		{
			"| Name | Score |",
			"| --- | ---: |"
		},
		0,
		1,
		MarkdownTable::Action::SortRowsAscending).lines,
		{
			"| Name | Score |",
			"| ---- | ----: |"
		});

	expectLines("csv to table", MarkdownTable::convertDelimitedToTable("Name,Role,Score\nAnna,Engineer,42\nDmitry,QA,7").lines,
		{
			"| Name   | Role     | Score |",
			"| ------ | -------- | ----- |",
			"| Anna   | Engineer | 42    |",
			"| Dmitry | QA       | 7     |"
		});

	expectLines("quoted csv to table", MarkdownTable::convertDelimitedToTable("Name,Note\nAnna,\"A, B\"\nBob,\"said \"\"hi\"\"\"").lines,
		{
			"| Name | Note      |",
			"| ---- | --------- |",
			"| Anna | A, B      |",
			"| Bob  | said \"hi\" |"
		});

	expectLines("csv escaped quotes and crlf", MarkdownTable::convertDelimitedToTable(
		"Name,Quote,Raw\r\n"
		"Anna,\"He said \"\"Hi\"\"\",a\"b\r\n"
		"\r\n"
		"Bob,\"line\r\nbreak\",x").lines,
		{
			"| Name | Quote        | Raw |",
			"| ---- | ------------ | --- |",
			"| Anna | He said \"Hi\" | a\"b |",
			"| Bob  | line break   | x   |"
		});

	expectLines("csv crlf and multiline quoted cell", MarkdownTable::convertDelimitedToTable("Name,Note\r\nAnna,\"line 1\r\nline 2\"\r\nBob,done\r\n").lines,
		{
			"| Name | Note          |",
			"| ---- | ------------- |",
			"| Anna | line 1 line 2 |",
			"| Bob  | done          |"
		});

	expectLines("delimiter ignores commas inside quotes", MarkdownTable::convertDelimitedToTable("Name\tNote\nAnna\t\"A,B\"").lines,
		{
			"| Name | Note |",
			"| ---- | ---- |",
			"| Anna | A,B  |"
		});

	expectLines("tsv to table escapes pipes", MarkdownTable::convertDelimitedToTable("Name\tNote\nAnna\tA|B").lines,
		{
			"| Name | Note |",
			"| ---- | ---- |",
			"| Anna | A\\|B |"
		});

	expectLines("tsv to table pads uneven rows", MarkdownTable::convertDelimitedToTable("A\tB\tC\n1\t2\n3\t4\t5").lines,
		{
			"| A   | B   | C   |",
			"| --- | --- | --- |",
			"| 1   | 2   |     |",
			"| 3   | 4   | 5   |"
		});
	expectLines("csv trims surrounding blank lines", MarkdownTable::convertDelimitedToTable("  \r\nName,Role\r\nAnna,Engineer\r\n  ").lines,
		{
			"| Name | Role     |",
			"| ---- | -------- |",
			"| Anna | Engineer |"
		});

	const MarkdownTable::EditResult created = MarkdownTable::createTable(3, 2);
	expectTrue("create table ok", created.ok);
	expectSize("create table target row", created.targetRow, 2);
	expectSize("create table target column", created.targetColumn, 0);
	expectLines("create table", created.lines,
		{
			"| Column 1 | Column 2 | Column 3 |",
			"| -------- | -------- | -------- |",
			"|          |          |          |",
			"|          |          |          |"
		});

	expectLines("create table without data rows", MarkdownTable::createTable(1, 0).lines,
		{
			"| Column 1 |",
			"| -------- |"
		});
	expectSize("create table without data target row", MarkdownTable::createTable(1, 0).targetRow, 0);
	const MarkdownTable::EditResult invalidTableSize = MarkdownTable::createTable(0, 0);
	expectTrue("invalid table size is rejected", !invalidTableSize.ok);
	expectString("invalid table size message", invalidTableSize.message, "Invalid table size");
	expectTrue("negative column count is rejected", !MarkdownTable::createTable(-1, 2).ok);
	expectTrue("negative table size is rejected", !MarkdownTable::createTable(2, -1).ok);

	expectTrue("plain text is not csv", !MarkdownTable::convertDelimitedToTable("just a note").ok);
	expectString("plain text csv message", MarkdownTable::convertDelimitedToTable("just a note").message, "No CSV or TSV data found");
	expectTrue("plain multiline text is not csv", !MarkdownTable::convertDelimitedToTable("first line\nsecond line").ok);
	expectTrue("single quoted comma cell is not csv", !MarkdownTable::convertDelimitedToTable("\"just, a note\"").ok);
	expectTrue("empty csv rejected", !MarkdownTable::convertDelimitedToTable(" \r\n\t").ok);

	expectLines("keep markdown header row", MarkdownTable::apply(
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

	expectLines("keep separator row", MarkdownTable::apply(
		{
			"| H | V |",
			"| --- | --- |",
			"| a | b |"
		},
		1,
		0,
		MarkdownTable::Action::DeleteRow).lines,
		{
			"| H   | V   |",
			"| --- | --- |",
			"| a   | b   |"
		});

	const MarkdownTable::EditResult insertRowBelowHeader = MarkdownTable::apply(
		{
			"| H | V |",
			"| --- | --- |"
		},
		0,
		0,
		MarkdownTable::Action::InsertRowBelow);
	expectTrue("insert row below header ok", insertRowBelowHeader.ok);
	expectSize("insert row below header target", insertRowBelowHeader.targetRow, 2);
	expectLines("insert row below header", insertRowBelowHeader.lines,
		{
			"| H   | V   |",
			"| --- | --- |",
			"|     |     |"
		});

	expectLines("keep header above separator", MarkdownTable::apply(
		{
			"| A |",
			"| --- |",
			"| 1 |"
		},
		0,
		0,
		MarkdownTable::Action::MoveRowDown).lines,
		{
			"| A   |",
			"| --- |",
			"| 1   |"
		});

	g_failures += runScenarioUnitTests();
	g_failures += runGoldenFixtureTests();

	if (g_failures != 0)
	{
		std::cerr << g_failures << " test(s) failed\n";
		return 1;
	}

	std::cout << "Core smoke tests passed\n";
	return 0;
}
