#include "MarkdownTableCore.h"

#include <algorithm>
#include <cctype>

namespace MarkdownTable
{
namespace
{
enum class Align
{
	None,
	Left,
	Center,
	Right
};

struct Row
{
	std::vector<std::string> cells;
	bool separator = false;
};

struct Table
{
	std::vector<Row> rows;
	std::vector<Align> alignments;
	std::size_t columns = 0;
	std::size_t separatorRow = static_cast<std::size_t>(-1);
};

struct FormatResult
{
	std::vector<std::string> lines;
	std::vector<std::vector<std::size_t> > starts;
};

bool isSpace(unsigned char ch)
{
	return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

std::string trim(const std::string &value)
{
	std::size_t first = 0;
	while (first < value.size() && isSpace(static_cast<unsigned char>(value[first])))
		++first;

	std::size_t last = value.size();
	while (last > first && isSpace(static_cast<unsigned char>(value[last - 1])))
		--last;

	return value.substr(first, last - first);
}

bool isEscaped(const std::string &line, std::size_t pos)
{
	std::size_t slashCount = 0;
	while (pos > slashCount && line[pos - slashCount - 1] == '\\')
		++slashCount;
	return (slashCount % 2) == 1;
}

bool endsWithUnescapedPipe(const std::string &line)
{
	if (line.empty() || line[line.size() - 1] != '|')
		return false;
	return !isEscaped(line, line.size() - 1);
}

bool startsWithUnescapedPipe(const std::string &line)
{
	std::size_t pos = 0;
	while (pos < line.size() && isSpace(static_cast<unsigned char>(line[pos])))
		++pos;
	return pos < line.size() && line[pos] == '|' && !isEscaped(line, pos);
}

std::vector<std::string> splitCells(const std::string &line)
{
	std::string body = trim(line);
	if (!body.empty() && body[0] == '|')
		body.erase(body.begin());
	if (endsWithUnescapedPipe(body))
		body.erase(body.end() - 1);

	std::vector<std::string> cells;
	std::size_t start = 0;
	for (std::size_t i = 0; i < body.size(); ++i)
	{
		if (body[i] == '|' && !isEscaped(body, i))
		{
			cells.push_back(trim(body.substr(start, i - start)));
			start = i + 1;
		}
	}
	cells.push_back(trim(body.substr(start)));
	return cells;
}

bool isSeparatorCell(const std::string &cell)
{
	const std::string value = trim(cell);
	if (value.empty())
		return false;

	bool hasDash = false;
	for (std::size_t i = 0; i < value.size(); ++i)
	{
		const char ch = value[i];
		if (ch == '-')
		{
			hasDash = true;
			continue;
		}
		if (ch == ':' || isSpace(static_cast<unsigned char>(ch)))
			continue;
		return false;
	}
	return hasDash;
}

bool isSeparatorRow(const Row &row)
{
	if (row.cells.empty())
		return false;
	for (std::size_t i = 0; i < row.cells.size(); ++i)
	{
		if (!isSeparatorCell(row.cells[i]))
			return false;
	}
	return true;
}

bool isShortSeparatorLine(const std::string &line)
{
	const std::string value = trim(line);
	if (value.empty() || value[0] != '|')
		return false;

	for (std::size_t i = 1; i < value.size(); ++i)
	{
		const char ch = value[i];
		if (ch != '-' && ch != '=' && ch != '|' && ch != ':' && !isSpace(static_cast<unsigned char>(ch)))
			return false;
	}
	return true;
}

Align parseAlignment(const std::string &cell)
{
	const std::string value = trim(cell);
	const bool left = !value.empty() && value[0] == ':';
	const bool right = !value.empty() && value[value.size() - 1] == ':';

	if (left && right)
		return Align::Center;
	if (left)
		return Align::Left;
	if (right)
		return Align::Right;
	return Align::None;
}

Table parseTable(const std::vector<std::string> &lines)
{
	Table table;
	for (std::size_t i = 0; i < lines.size(); ++i)
	{
		Row row;
		row.cells = splitCells(lines[i]);
		table.columns = (std::max)(table.columns, row.cells.size());
		table.rows.push_back(row);
	}

	for (std::size_t i = 0; i < table.rows.size(); ++i)
	{
		if (isSeparatorRow(table.rows[i]) || (i == 1 && isShortSeparatorLine(lines[i])))
		{
			table.separatorRow = i;
			table.rows[i].separator = true;
			break;
		}
	}

	if (table.columns == 0)
		table.columns = 1;

	for (std::size_t i = 0; i < table.rows.size(); ++i)
	{
		while (table.rows[i].cells.size() < table.columns)
			table.rows[i].cells.push_back(std::string());
	}

	table.alignments.assign(table.columns, Align::None);
	if (table.separatorRow != static_cast<std::size_t>(-1))
	{
		const Row &separator = table.rows[table.separatorRow];
		for (std::size_t i = 0; i < table.columns; ++i)
			table.alignments[i] = parseAlignment(separator.cells[i]);
	}

	return table;
}

bool isCjkCodePoint(unsigned int cp)
{
	return (cp >= 0x1100 && cp <= 0x115F) ||
		(cp >= 0x2E80 && cp <= 0xA4CF) ||
		(cp >= 0xAC00 && cp <= 0xD7A3) ||
		(cp >= 0xF900 && cp <= 0xFAFF) ||
		(cp >= 0xFE10 && cp <= 0xFE6F) ||
		(cp >= 0xFF00 && cp <= 0xFF60) ||
		(cp >= 0xFFE0 && cp <= 0xFFE6);
}

std::size_t displayWidth(const std::string &text)
{
	std::size_t width = 0;
	for (std::size_t i = 0; i < text.size();)
	{
		const unsigned char ch = static_cast<unsigned char>(text[i]);
		unsigned int cp = ch;
		std::size_t advance = 1;

		if ((ch & 0x80) == 0)
		{
			cp = ch;
			advance = 1;
		}
		else if ((ch & 0xE0) == 0xC0 && i + 1 < text.size())
		{
			cp = static_cast<unsigned int>(((ch & 0x1F) << 6) | (static_cast<unsigned char>(text[i + 1]) & 0x3F));
			advance = 2;
		}
		else if ((ch & 0xF0) == 0xE0 && i + 2 < text.size())
		{
			cp = static_cast<unsigned int>(((ch & 0x0F) << 12) |
				((static_cast<unsigned char>(text[i + 1]) & 0x3F) << 6) |
				(static_cast<unsigned char>(text[i + 2]) & 0x3F));
			advance = 3;
		}
		else if ((ch & 0xF8) == 0xF0 && i + 3 < text.size())
		{
			cp = static_cast<unsigned int>(((ch & 0x07) << 18) |
				((static_cast<unsigned char>(text[i + 1]) & 0x3F) << 12) |
				((static_cast<unsigned char>(text[i + 2]) & 0x3F) << 6) |
				(static_cast<unsigned char>(text[i + 3]) & 0x3F));
			advance = 4;
		}

		width += isCjkCodePoint(cp) ? 2 : 1;
		i += advance;
	}
	return width;
}

std::string spaces(std::size_t count)
{
	return std::string(count, ' ');
}

std::string separatorCell(Align align, std::size_t width)
{
	const std::size_t target = (std::max)(width, static_cast<std::size_t>(3));
	if (align == Align::Center)
		return std::string(":") + std::string(target - 2, '-') + ":";
	if (align == Align::Left)
		return std::string(":") + std::string(target - 1, '-');
	if (align == Align::Right)
		return std::string(target - 1, '-') + ":";
	return std::string(target, '-');
}

std::string paddedCell(const std::string &cell, Align align, std::size_t width, std::size_t *contentOffset)
{
	const std::size_t current = displayWidth(cell);
	const std::size_t pad = width > current ? width - current : 0;
	std::size_t leftPad = 0;
	std::size_t rightPad = pad;

	if (align == Align::Right)
	{
		leftPad = pad;
		rightPad = 0;
	}
	else if (align == Align::Center)
	{
		leftPad = pad / 2;
		rightPad = pad - leftPad;
	}

	if (contentOffset)
		*contentOffset = leftPad;
	return spaces(leftPad) + cell + spaces(rightPad);
}

FormatResult formatTable(const Table &table)
{
	std::vector<std::size_t> widths(table.columns, 3);
	for (std::size_t rowIndex = 0; rowIndex < table.rows.size(); ++rowIndex)
	{
		const Row &row = table.rows[rowIndex];
		if (row.separator)
			continue;

		for (std::size_t column = 0; column < table.columns; ++column)
			widths[column] = (std::max)(widths[column], displayWidth(row.cells[column]));
	}

	FormatResult result;
	result.lines.reserve(table.rows.size());
	result.starts.reserve(table.rows.size());

	for (std::size_t rowIndex = 0; rowIndex < table.rows.size(); ++rowIndex)
	{
		const Row &row = table.rows[rowIndex];
		std::string line = "|";
		std::vector<std::size_t> starts(table.columns, 0);

		for (std::size_t column = 0; column < table.columns; ++column)
		{
			if (row.separator)
			{
				const std::string value = separatorCell(table.alignments[column], widths[column]);
				line += " " + value + " |";
				starts[column] = line.size() >= value.size() + 2 ? line.size() - value.size() - 2 : 0;
			}
			else
			{
				std::size_t contentOffset = 0;
				const std::string value = paddedCell(row.cells[column], table.alignments[column], widths[column], &contentOffset);
				const std::size_t cellStart = line.size() + 1;
				line += " " + value + " |";
				starts[column] = cellStart + contentOffset;
			}
		}

		result.lines.push_back(line);
		result.starts.push_back(starts);
	}

	return result;
}

std::size_t nextEditableRow(const Table &table, std::size_t row)
{
	std::size_t next = row + 1;
	while (next < table.rows.size() && table.rows[next].separator)
		++next;
	return next;
}

std::size_t previousEditableRow(const Table &table, std::size_t row)
{
	if (row == 0)
		return 0;

	std::size_t previous = row - 1;
	while (previous > 0 && table.rows[previous].separator)
		--previous;
	return table.rows[previous].separator ? row : previous;
}

std::size_t editableRowCount(const Table &table)
{
	std::size_t count = 0;
	for (std::size_t i = 0; i < table.rows.size(); ++i)
	{
		if (!table.rows[i].separator)
			++count;
	}
	return count;
}

std::size_t closestEditableRow(const Table &table, std::size_t row)
{
	if (table.rows.empty())
		return 0;

	if (row >= table.rows.size())
		row = table.rows.size() - 1;
	if (!table.rows[row].separator)
		return row;

	for (std::size_t next = row + 1; next < table.rows.size(); ++next)
	{
		if (!table.rows[next].separator)
			return next;
	}

	while (row > 0)
	{
		--row;
		if (!table.rows[row].separator)
			return row;
	}

	return 0;
}

void insertEmptyRow(Table &table, std::size_t index)
{
	Row row;
	row.cells.assign(table.columns, std::string());
	if (index > table.rows.size())
		index = table.rows.size();
	table.rows.insert(table.rows.begin() + static_cast<std::ptrdiff_t>(index), row);
	if (table.separatorRow != static_cast<std::size_t>(-1) && index <= table.separatorRow)
		++table.separatorRow;
}

void removeColumn(Table &table, std::size_t column)
{
	if (table.columns <= 1 || column >= table.columns)
		return;

	for (std::size_t i = 0; i < table.rows.size(); ++i)
		table.rows[i].cells.erase(table.rows[i].cells.begin() + static_cast<std::ptrdiff_t>(column));
	table.alignments.erase(table.alignments.begin() + static_cast<std::ptrdiff_t>(column));
	--table.columns;
}

void insertColumn(Table &table, std::size_t column)
{
	if (column > table.columns)
		column = table.columns;

	for (std::size_t i = 0; i < table.rows.size(); ++i)
		table.rows[i].cells.insert(table.rows[i].cells.begin() + static_cast<std::ptrdiff_t>(column), std::string());
	table.alignments.insert(table.alignments.begin() + static_cast<std::ptrdiff_t>(column), Align::None);
	++table.columns;
}

void moveColumn(Table &table, std::size_t from, std::size_t to)
{
	if (from >= table.columns || to >= table.columns || from == to)
		return;

	for (std::size_t i = 0; i < table.rows.size(); ++i)
	{
		std::string value = table.rows[i].cells[from];
		table.rows[i].cells.erase(table.rows[i].cells.begin() + static_cast<std::ptrdiff_t>(from));
		table.rows[i].cells.insert(table.rows[i].cells.begin() + static_cast<std::ptrdiff_t>(to), value);
	}

	const Align align = table.alignments[from];
	table.alignments.erase(table.alignments.begin() + static_cast<std::ptrdiff_t>(from));
	table.alignments.insert(table.alignments.begin() + static_cast<std::ptrdiff_t>(to), align);
}

void setResultFromFormat(EditResult &result, const FormatResult &formatted, std::size_t row, std::size_t column)
{
	result.lines = formatted.lines;
	result.targetRow = row < formatted.starts.size() ? row : 0;
	result.targetColumn = column;
	if (!formatted.starts.empty() && result.targetRow < formatted.starts.size())
	{
		if (result.targetColumn >= formatted.starts[result.targetRow].size())
			result.targetColumn = formatted.starts[result.targetRow].empty() ? 0 : formatted.starts[result.targetRow].size() - 1;
		result.targetColumnOffset = formatted.starts[result.targetRow][result.targetColumn];
	}
}
}

bool isPotentialTableLine(const std::string &line)
{
	for (std::size_t i = 0; i < line.size(); ++i)
	{
		if (line[i] == '|' && !isEscaped(line, i))
			return true;
	}
	return false;
}

std::size_t columnFromCursor(const std::string &line, std::size_t byteColumn)
{
	std::size_t column = 0;
	bool skippedLeadingPipe = false;
	const bool hasLeadingPipe = startsWithUnescapedPipe(line);
	const std::size_t limit = (std::min)(byteColumn, line.size());

	for (std::size_t i = 0; i < limit; ++i)
	{
		if (line[i] == '|' && !isEscaped(line, i))
		{
			if (hasLeadingPipe && !skippedLeadingPipe)
				skippedLeadingPipe = true;
			else
				++column;
		}
	}
	return column;
}

EditResult apply(const std::vector<std::string> &lines, std::size_t row, std::size_t column, Action action)
{
	EditResult result;
	if (lines.empty())
	{
		result.message = "No table found";
		return result;
	}

	Table table = parseTable(lines);
	if (row >= table.rows.size())
		row = table.rows.size() - 1;
	if (column >= table.columns)
		column = table.columns - 1;

	std::size_t targetRow = row;
	std::size_t targetColumn = column;

	switch (action)
	{
		case Action::NextCell:
			if (targetColumn + 1 < table.columns)
			{
				++targetColumn;
			}
			else
			{
				targetColumn = 0;
				targetRow = nextEditableRow(table, targetRow);
				if (targetRow >= table.rows.size())
					insertEmptyRow(table, table.rows.size());
			}
			break;

		case Action::PreviousCell:
			if (targetColumn > 0)
			{
				--targetColumn;
			}
			else
			{
				targetColumn = table.columns - 1;
				targetRow = previousEditableRow(table, targetRow);
			}
			break;

		case Action::InsertRowBelow:
			targetRow = nextEditableRow(table, row);
			insertEmptyRow(table, targetRow);
			break;

		case Action::DeleteRow:
			if (!table.rows[row].separator && editableRowCount(table) > 1)
			{
				table.rows.erase(table.rows.begin() + static_cast<std::ptrdiff_t>(row));
				if (table.separatorRow != static_cast<std::size_t>(-1) && row < table.separatorRow)
					--table.separatorRow;
				targetRow = closestEditableRow(table, row);
			}
			break;

		case Action::InsertColumnRight:
			insertColumn(table, column + 1);
			targetColumn = column + 1;
			break;

		case Action::DeleteColumn:
			removeColumn(table, column);
			targetColumn = column >= table.columns ? table.columns - 1 : column;
			break;

		case Action::MoveRowUp:
			if (row > 0 && !table.rows[row].separator && !table.rows[row - 1].separator)
			{
				std::swap(table.rows[row], table.rows[row - 1]);
				targetRow = row - 1;
			}
			break;

		case Action::MoveRowDown:
			if (row + 1 < table.rows.size() && !table.rows[row].separator && !table.rows[row + 1].separator)
			{
				std::swap(table.rows[row], table.rows[row + 1]);
				targetRow = row + 1;
			}
			break;

		case Action::MoveColumnLeft:
			if (column > 0)
			{
				moveColumn(table, column, column - 1);
				targetColumn = column - 1;
			}
			break;

		case Action::MoveColumnRight:
			if (column + 1 < table.columns)
			{
				moveColumn(table, column, column + 1);
				targetColumn = column + 1;
			}
			break;

		case Action::Align:
			break;
	}

	const FormatResult formatted = formatTable(table);
	setResultFromFormat(result, formatted, targetRow, targetColumn);
	result.ok = true;
	result.changed = true;
	return result;
}
}
