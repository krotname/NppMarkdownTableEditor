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
	expectSize("escaped pipe cursor", MarkdownTable::columnFromCursor(escaped, escaped.find("c")), 1);
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

	if (g_failures != 0)
	{
		std::cerr << g_failures << " test(s) failed\n";
		return 1;
	}

	std::cout << "Core smoke tests passed\n";
	return 0;
}
