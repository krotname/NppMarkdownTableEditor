// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 krotname

#ifndef MARKDOWN_TABLE_CORE_H
#define MARKDOWN_TABLE_CORE_H

#include <cstddef>
#include <string>
#include <vector>

namespace MarkdownTable
{
enum class Action
{
	Align,
	NextCell,
	PreviousCell,
	InsertRowBelow,
	DeleteRow,
	InsertColumnRight,
	DeleteColumn,
	MoveRowUp,
	MoveRowDown,
	MoveColumnLeft,
	MoveColumnRight,
	SortRowsAscending,
	SortRowsDescending,
	WrapLongCells
};

struct EditResult
{
	bool changed = false;
	bool ok = false;
	std::string message;
	std::vector<std::string> lines;
	std::size_t targetRow = 0;
	std::size_t targetColumn = 0;
	std::size_t targetColumnOffset = 0;
};

struct TableRange
{
	bool found = false;
	std::size_t firstRow = 0;
	std::size_t lastRow = 0;
};

bool isPotentialTableLine(const std::string &line);
std::size_t columnFromCursor(const std::string &line, std::size_t byteColumn);
TableRange findTableRange(const std::vector<std::string> &lines, int row);
EditResult apply(const std::vector<std::string> &lines, int row, int column, Action action);
EditResult applyWrappedToWidth(const std::vector<std::string> &lines, int row, int column, std::size_t maxTableWidth);
EditResult convertDelimitedToTable(const std::string &text);
EditResult createTable(int columns, int dataRows);
}

#endif
