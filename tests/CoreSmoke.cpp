#include "../src/MarkdownTableCore.h"
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

void expectContains(const char *name, const std::string &actual, const std::string &expected)
{
	if (actual.find(expected) != std::string::npos)
		return;

	++g_failures;
	std::cerr << name << " failed: expected to contain \"" << expected << "\"\n";
}

std::string joinLinesForCheck(const std::vector<std::string> &lines)
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
			"| Name | Age |",
			"| ---- | --: |",
			"| Anna |  20 |"
		});

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

	const MarkdownTable::EditResult largeFishTable = MarkdownTable::apply(
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
	expectTrue("large fish table ok", largeFishTable.ok);
	expectSize("large fish table line count", largeFishTable.lines.size(), 8);
	const std::string largeFishText = joinLinesForCheck(largeFishTable.lines);
	expectContains("large fish keeps lorem", largeFishText, "Lorem ipsum dolor sit amet");
	expectContains("large fish keeps cyrillic", largeFishText, "Съешь еще этих мягких французских булок");
	expectContains("large fish keeps cjk", largeFishText, "表示確認");
	expectContains("large fish keeps escaped pipe", largeFishText, "Markdown \\| escaped pipe");
	expectContains("large fish keeps url", largeFishText, "https://example.com/path/to/a/very/long/resource");

	std::vector<std::string> hugeTableLines;
	hugeTableLines.push_back("| Id | Payload | Note |");
	hugeTableLines.push_back("| --- | --- | --- |");
	const std::string hugePayload =
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua; "
		"Съешь еще этих мягких французских булок для проверки длинной кириллицы; "
		"表示確認 and Markdown \\| escaped pipe remain stable.";
	for (std::size_t i = 0; i < 160; ++i)
	{
		hugeTableLines.push_back(
			std::string("| row-") + std::to_string(i) + " | " + hugePayload + " chunk-" + std::to_string(i) + " | note-" + std::to_string(i) + " |");
	}
	const MarkdownTable::EditResult hugeTable = MarkdownTable::apply(hugeTableLines, 80, 1, MarkdownTable::Action::Align);
	expectTrue("huge generated table ok", hugeTable.ok);
	expectSize("huge generated table line count", hugeTable.lines.size(), 162);
	const std::string hugeTableText = joinLinesForCheck(hugeTable.lines);
	expectContains("huge generated table keeps first row", hugeTableText, "row-0");
	expectContains("huge generated table keeps last row", hugeTableText, "row-159");
	expectContains("huge generated table keeps long payload", hugeTableText, "Lorem ipsum dolor sit amet");
	expectContains("huge generated table keeps escaped pipe", hugeTableText, "Markdown \\| escaped pipe");

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

	const MarkdownTable::EditResult largeFishCsv = MarkdownTable::convertDelimitedToTable(
		"Name,Story,Score\r\n"
		"Anna,\"Lorem ipsum dolor sit amet, consectetur adipiscing elit, with comma\",42\r\n"
		"Борис,\"Длинная рыба с переносом\r\nвнутри кавычек и pipe |\",7\r\n"
		"Chen,\"CJK 表 with comma, quote \"\"ok\"\", and more filler text\",100\r\n"
		"Delta,\"one two three four five six seven eight nine ten\",-12.5\r\n");
	expectTrue("large fish csv ok", largeFishCsv.ok);
	expectSize("large fish csv line count", largeFishCsv.lines.size(), 6);
	const std::string largeFishCsvText = joinLinesForCheck(largeFishCsv.lines);
	expectContains("large fish csv keeps lorem", largeFishCsvText, "Lorem ipsum dolor sit amet");
	expectContains("large fish csv flattens newline", largeFishCsvText, "переносом внутри кавычек");
	expectContains("large fish csv escapes pipe", largeFishCsvText, "pipe \\|");
	expectContains("large fish csv keeps quotes", largeFishCsvText, "quote \"ok\"");

	std::string hugeCsv = "Id,Text,Score\r\n";
	for (std::size_t i = 0; i < 220; ++i)
	{
		hugeCsv += std::string("row-") + std::to_string(i) + ",\""
			"Lorem ipsum dolor sit amet, consectetur adipiscing elit, with comma, "
			"quote \"\"ok\"\", pipe |, кириллица, 表, and generated chunk " + std::to_string(i) + "\"," +
			std::to_string(static_cast<int>(i) - 110) + "\r\n";
	}
	const MarkdownTable::EditResult hugeCsvTable = MarkdownTable::convertDelimitedToTable(hugeCsv);
	expectTrue("huge generated csv ok", hugeCsvTable.ok);
	expectSize("huge generated csv line count", hugeCsvTable.lines.size(), 222);
	const std::string hugeCsvText = joinLinesForCheck(hugeCsvTable.lines);
	expectContains("huge generated csv keeps first row", hugeCsvText, "row-0");
	expectContains("huge generated csv keeps last row", hugeCsvText, "row-219");
	expectContains("huge generated csv escapes pipe", hugeCsvText, "pipe \\|");
	expectContains("huge generated csv keeps quotes", hugeCsvText, "quote \"ok\"");

	const MarkdownTable::EditResult created = MarkdownTable::createTable(3, 2);
	expectTrue("create table ok", created.ok);
	expectSize("create table target row", created.targetRow, 0);
	expectSize("create table target column", created.targetColumn, 0);
	expectLines("create table", created.lines,
		{
			"| Column 1 | Column 2 | Column 3 |",
			"| -------- | -------- | -------- |",
			"|          |          |          |",
			"|          |          |          |"
		});

	expectLines("create minimum table", MarkdownTable::createTable(0, 0).lines,
		{
			"| Column 1 |",
			"| -------- |"
		});

	expectTrue("plain text is not csv", !MarkdownTable::convertDelimitedToTable("just a note").ok);
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

	if (g_failures != 0)
	{
		std::cerr << g_failures << " test(s) failed\n";
		return 1;
	}

	std::cout << "Core smoke tests passed\n";
	return 0;
}
