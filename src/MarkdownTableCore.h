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
	MoveColumnRight
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

bool isPotentialTableLine(const std::string &line);
std::size_t columnFromCursor(const std::string &line, std::size_t byteColumn);
EditResult apply(const std::vector<std::string> &lines, std::size_t row, std::size_t column, Action action);
}

#endif
