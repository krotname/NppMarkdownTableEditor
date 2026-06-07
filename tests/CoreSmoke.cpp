#include "../src/MarkdownTableCore.h"

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

	const MarkdownTable::EditResult next = MarkdownTable::apply(input, 3, 1, MarkdownTable::Action::NextCell);
	expectTrue("next cell ok", next.ok);
	expectSize("next cell row count", next.lines.size(), 5);
	expectSize("next cell target row", next.targetRow, 4);
	expectSize("next cell target column", next.targetColumn, 0);

	const MarkdownTable::EditResult insertColumn = MarkdownTable::apply(input, 2, 0, MarkdownTable::Action::InsertColumnRight);
	expectTrue("insert column ok", insertColumn.ok);
	expectLines("insert column", insertColumn.lines,
		{
			"| Name      |     | Age |",
			"| --------- | --- | --: |",
			"| Anna      |     |  20 |",
			"| Alexander |     |   7 |"
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

	expectTrue("empty csv rejected", !MarkdownTable::convertDelimitedToTable(" \r\n\t").ok);

	if (g_failures != 0)
	{
		std::cerr << g_failures << " test(s) failed\n";
		return 1;
	}

	std::cout << "Core smoke tests passed\n";
	return 0;
}
