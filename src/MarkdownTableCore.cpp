#include "MarkdownTableCore.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <utility>

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
	std::size_t id = 0;
};

struct Table
{
	std::vector<Row> rows;
	std::vector<Align> alignments;
	std::size_t columns = 0;
	std::size_t separatorRow = static_cast<std::size_t>(-1);
	bool leadingPipe = true;
	bool trailingPipe = true;
};

struct FormatResult
{
	std::vector<std::string> lines;
	std::size_t targetRow = 0;
	std::size_t targetColumn = 0;
	std::size_t targetColumnOffset = 0;
};

struct SortKey
{
	bool numeric = false;
	double number = 0;
	std::string foldedText;
	std::string text;
};

struct SortEntry
{
	Row row;
	SortKey key;
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
	std::size_t leadingPipeRows = 0;
	std::size_t trailingPipeRows = 0;
	for (std::size_t i = 0; i < lines.size(); ++i)
	{
		Row row;
		row.id = i;
		row.cells = splitCells(lines[i]);
		table.columns = (std::max)(table.columns, row.cells.size());
		table.rows.push_back(row);
		if (startsWithUnescapedPipe(lines[i]))
			++leadingPipeRows;
		if (endsWithUnescapedPipe(trim(lines[i])))
			++trailingPipeRows;
	}

	if (!lines.empty())
	{
		table.leadingPipe = leadingPipeRows * 2 >= lines.size();
		table.trailingPipe = trailingPipeRows * 2 >= lines.size();
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

bool isMarkdownTable(const Table &table)
{
	return table.separatorRow != static_cast<std::size_t>(-1) && table.separatorRow > 0;
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

FormatResult formatTable(const Table &table, std::size_t targetRow, std::size_t targetColumn)
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
	result.targetRow = targetRow < table.rows.size() ? targetRow : 0;
	result.targetColumn = targetColumn < table.columns ? targetColumn : 0;

	for (std::size_t rowIndex = 0; rowIndex < table.rows.size(); ++rowIndex)
	{
		const Row &row = table.rows[rowIndex];
		std::string line = table.leadingPipe ? "|" : "";

		for (std::size_t column = 0; column < table.columns; ++column)
		{
			if (column > 0)
				line += " |";
			if (table.leadingPipe || column > 0)
				line.push_back(' ');

			if (row.separator)
			{
				const std::string value = separatorCell(table.alignments[column], widths[column]);
				const std::size_t valueStart = line.size();
				line += value;
				if (rowIndex == result.targetRow && column == result.targetColumn)
					result.targetColumnOffset = valueStart;
			}
			else
			{
				std::size_t contentOffset = 0;
				const std::string value = paddedCell(row.cells[column], table.alignments[column], widths[column], &contentOffset);
				const std::size_t cellStart = line.size();
				line += value;
				if (rowIndex == result.targetRow && column == result.targetColumn)
					result.targetColumnOffset = cellStart + contentOffset;
			}

			if (table.trailingPipe && column + 1 == table.columns)
				line += " |";
		}

		result.lines.push_back(line);
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

bool canDeleteRow(const Table &table, std::size_t row)
{
	if (row >= table.rows.size() || table.rows[row].separator || editableRowCount(table) <= 1)
		return false;
	return table.separatorRow == static_cast<std::size_t>(-1) || row + 1 != table.separatorRow;
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

std::size_t nextRowId(const Table &table)
{
	std::size_t id = 0;
	for (std::size_t i = 0; i < table.rows.size(); ++i)
		id = (std::max)(id, table.rows[i].id + 1);
	return id;
}

std::size_t rowById(const Table &table, std::size_t id, std::size_t fallback)
{
	for (std::size_t i = 0; i < table.rows.size(); ++i)
	{
		if (table.rows[i].id == id)
			return i;
	}
	return closestEditableRow(table, fallback);
}

bool tryParseNumber(const std::string &value, double &number)
{
	const std::string trimmed = trim(value);
	if (trimmed.empty())
		return false;

	char *end = NULL;
	number = std::strtod(trimmed.c_str(), &end);
	while (end && *end != '\0' && isSpace(static_cast<unsigned char>(*end)))
		++end;
	return end && *end == '\0';
}

std::string lowerAscii(const std::string &value)
{
	std::string lowered = value;
	for (std::size_t i = 0; i < lowered.size(); ++i)
		lowered[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowered[i])));
	return lowered;
}

SortKey makeSortKey(const std::string &value)
{
	SortKey key;
	key.text = trim(value);
	key.foldedText = lowerAscii(key.text);
	key.numeric = tryParseNumber(value, key.number);
	return key;
}

int compareSortKeys(const SortKey &left, const SortKey &right)
{
	if (left.numeric && right.numeric)
	{
		if (left.number < right.number)
			return -1;
		if (left.number > right.number)
			return 1;
	}

	if (left.foldedText < right.foldedText)
		return -1;
	if (left.foldedText > right.foldedText)
		return 1;
	if (left.text < right.text)
		return -1;
	if (left.text > right.text)
		return 1;
	return 0;
}

std::size_t sortRows(Table &table, std::size_t column, bool ascending, std::size_t currentRowId, std::size_t fallbackRow)
{
	if (column >= table.columns || table.rows.empty())
		return closestEditableRow(table, fallbackRow);

	const std::size_t firstDataRow = table.separatorRow != static_cast<std::size_t>(-1) ? table.separatorRow + 1 : 0;
	if (firstDataRow >= table.rows.size())
		return rowById(table, currentRowId, fallbackRow);

	std::vector<SortEntry> entries;
	entries.reserve(table.rows.size() - firstDataRow);
	for (std::size_t i = firstDataRow; i < table.rows.size(); ++i)
	{
		SortEntry entry;
		entry.row = table.rows[i];
		entry.key = makeSortKey(entry.row.cells[column]);
		entries.push_back(entry);
	}

	std::stable_sort(entries.begin(), entries.end(),
		[ascending](const SortEntry &left, const SortEntry &right)
		{
			const int compared = compareSortKeys(left.key, right.key);
			if (compared == 0)
				return false;
			return ascending ? compared < 0 : compared > 0;
		});

	for (std::size_t i = 0; i < entries.size(); ++i)
		table.rows[firstDataRow + i] = entries[i].row;

	return rowById(table, currentRowId, fallbackRow);
}

void insertEmptyRow(Table &table, std::size_t index)
{
	Row row;
	row.id = nextRowId(table);
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

void setResultFromFormat(EditResult &result, FormatResult &formatted)
{
	result.lines = std::move(formatted.lines);
	result.targetRow = formatted.targetRow;
	result.targetColumn = formatted.targetColumn;
	result.targetColumnOffset = formatted.targetColumnOffset;
}

char detectDelimiter(const std::string &text)
{
	std::size_t tabs = 0;
	std::size_t commas = 0;
	bool inQuotes = false;
	bool cellBlank = true;

	for (std::size_t i = 0; i < text.size(); ++i)
	{
		const char ch = text[i];
		if (inQuotes)
		{
			if (ch == '"' && i + 1 < text.size() && text[i + 1] == '"')
				++i;
			else if (ch == '"')
				inQuotes = false;
		}
		else if (ch == '"' && cellBlank)
		{
			inQuotes = true;
		}
		else if (ch == '\t')
		{
			++tabs;
			cellBlank = true;
		}
		else if (ch == ',')
		{
			++commas;
			cellBlank = true;
		}
		else if (ch == '\r' || ch == '\n')
		{
			cellBlank = true;
		}
		else if (!isSpace(static_cast<unsigned char>(ch)))
		{
			cellBlank = false;
		}
	}

	return tabs > commas ? '\t' : ',';
}

bool hasDelimitedStructure(const std::string &text)
{
	const std::string value = trim(text);
	if (value.empty())
		return false;
	for (std::size_t i = 0; i < value.size(); ++i)
	{
		if (value[i] == ',' || value[i] == '\t' || value[i] == '\r' || value[i] == '\n')
			return true;
	}
	return false;
}

bool hasCellContent(const std::vector<std::string> &row)
{
	for (std::size_t i = 0; i < row.size(); ++i)
	{
		if (!trim(row[i]).empty())
			return true;
	}
	return false;
}

std::vector<std::vector<std::string> > parseDelimitedRows(const std::string &text, char delimiter)
{
	std::vector<std::vector<std::string> > rows;
	std::vector<std::string> row;
	std::string cell;
	bool inQuotes = false;

	for (std::size_t i = 0; i < text.size(); ++i)
	{
		const char ch = text[i];
		if (inQuotes)
		{
			if (ch == '"')
			{
				if (i + 1 < text.size() && text[i + 1] == '"')
				{
					cell += '"';
					++i;
				}
				else
				{
					inQuotes = false;
				}
			}
			else if (ch == '\r' || ch == '\n')
			{
				cell += ' ';
				if (ch == '\r' && i + 1 < text.size() && text[i + 1] == '\n')
					++i;
			}
			else
			{
				cell += ch;
			}
			continue;
		}

		if (ch == '"' && trim(cell).empty())
		{
			cell.clear();
			inQuotes = true;
			continue;
		}

		if (ch == delimiter)
		{
			row.push_back(trim(cell));
			cell.clear();
			continue;
		}

		if (ch == '\r' || ch == '\n')
		{
			row.push_back(trim(cell));
			cell.clear();
			if (hasCellContent(row))
				rows.push_back(row);
			row.clear();
			if (ch == '\r' && i + 1 < text.size() && text[i + 1] == '\n')
				++i;
			continue;
		}

		cell += ch;
	}

	row.push_back(trim(cell));
	if (hasCellContent(row))
		rows.push_back(row);
	return rows;
}

std::string escapeMarkdownCell(const std::string &cell)
{
	std::string escaped;
	for (std::size_t i = 0; i < cell.size(); ++i)
	{
		const char ch = cell[i];
		if (ch == '|')
			escaped += "\\|";
		else if (ch == '\r' || ch == '\n')
			escaped += ' ';
		else
			escaped += ch;
	}
	return escaped;
}

Table tableFromRows(const std::vector<std::vector<std::string> > &rows)
{
	Table table;
	table.columns = 1;
	for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
		table.columns = (std::max)(table.columns, rows[rowIndex].size());

	for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
	{
		Row row;
		row.id = table.rows.size();
		row.cells.reserve(table.columns);
		for (std::size_t column = 0; column < table.columns; ++column)
			row.cells.push_back(column < rows[rowIndex].size() ? escapeMarkdownCell(rows[rowIndex][column]) : std::string());
		table.rows.push_back(row);

		if (rowIndex == 0)
		{
			Row separator;
			separator.separator = true;
			separator.id = table.rows.size();
			separator.cells.assign(table.columns, "---");
			table.separatorRow = table.rows.size();
			table.rows.push_back(separator);
		}
	}

	table.alignments.assign(table.columns, Align::None);
	return table;
}

Table emptyTable(std::size_t columns, std::size_t dataRows)
{
	Table table;
	table.columns = (std::max)(columns, static_cast<std::size_t>(1));
	table.separatorRow = 1;
	table.alignments.assign(table.columns, Align::None);

	Row header;
	header.id = table.rows.size();
	for (std::size_t column = 0; column < table.columns; ++column)
		header.cells.push_back(std::string("Column ") + std::to_string(column + 1));
	table.rows.push_back(header);

	Row separator;
	separator.separator = true;
	separator.id = table.rows.size();
	separator.cells.assign(table.columns, "---");
	table.rows.push_back(separator);

	for (std::size_t rowIndex = 0; rowIndex < dataRows; ++rowIndex)
	{
		Row row;
		row.id = table.rows.size();
		row.cells.assign(table.columns, std::string());
		table.rows.push_back(row);
	}

	return table;
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
	if (!isMarkdownTable(table))
	{
		result.message = "No Markdown table found";
		return result;
	}

	if (row >= table.rows.size())
		row = table.rows.size() - 1;
	if (column >= table.columns)
		column = table.columns - 1;

	std::size_t targetRow = row;
	std::size_t targetColumn = column;
	const std::size_t currentRowId = table.rows[row].id;

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
			if (canDeleteRow(table, row))
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

		case Action::SortRowsAscending:
			targetRow = sortRows(table, column, true, currentRowId, row);
			break;

		case Action::SortRowsDescending:
			targetRow = sortRows(table, column, false, currentRowId, row);
			break;

		case Action::Align:
			break;
	}

	FormatResult formatted = formatTable(table, targetRow, targetColumn);
	setResultFromFormat(result, formatted);
	result.ok = true;
	result.changed = true;
	return result;
}

EditResult convertDelimitedToTable(const std::string &text)
{
	EditResult result;
	if (!hasDelimitedStructure(text))
	{
		result.message = "No CSV or TSV data found";
		return result;
	}

	const std::vector<std::vector<std::string> > rows = parseDelimitedRows(text, detectDelimiter(text));
	if (rows.empty())
	{
		result.message = "No CSV or TSV data found";
		return result;
	}

	const Table table = tableFromRows(rows);
	FormatResult formatted = formatTable(table, 0, 0);
	setResultFromFormat(result, formatted);
	result.ok = true;
	result.changed = true;
	return result;
}

EditResult createTable(std::size_t columns, std::size_t dataRows)
{
	EditResult result;
	if (columns < 1)
	{
		result.message = "Invalid table size";
		return result;
	}

	const Table table = emptyTable(columns, dataRows);
	FormatResult formatted = formatTable(table, dataRows > 0 ? 2 : 0, 0);
	setResultFromFormat(result, formatted);
	result.ok = true;
	result.changed = true;
	return result;
}
}
