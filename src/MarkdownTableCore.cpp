// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 krotname

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

struct CodePointRange
{
	unsigned int first;
	unsigned int last;
};

const std::size_t hardWrapCellWidth = 32;

std::size_t nextRowId(const Table &table);

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

std::string trimRange(const std::string &value, std::size_t first, std::size_t last)
{
	while (first < last && isSpace(static_cast<unsigned char>(value[first])))
		++first;
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

bool endsWithUnescapedPipeTrimmed(const std::string &line)
{
	std::size_t last = line.size();
	while (last > 0 && isSpace(static_cast<unsigned char>(line[last - 1])))
		--last;
	return last > 0 && line[last - 1] == '|' && !isEscaped(line, last - 1);
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
	std::size_t first = 0;
	while (first < line.size() && isSpace(static_cast<unsigned char>(line[first])))
		++first;

	std::size_t last = line.size();
	while (last > first && isSpace(static_cast<unsigned char>(line[last - 1])))
		--last;

	if (first < last && line[first] == '|')
		++first;
	if (last > first && line[last - 1] == '|' && !isEscaped(line, last - 1))
		--last;

	std::vector<std::string> cells;
	cells.reserve(8);
	std::size_t start = first;
	for (std::size_t i = first; i < last; ++i)
	{
		if (line[i] == '|' && !isEscaped(line, i))
		{
			cells.push_back(trimRange(line, start, i));
			start = i + 1;
		}
	}
	cells.push_back(trimRange(line, start, last));
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

	bool hasRule = false;
	for (std::size_t i = 1; i < value.size(); ++i)
	{
		const char ch = value[i];
		if (ch == '-' || ch == '=')
			hasRule = true;
		if (ch != '-' && ch != '=' && ch != '|' && ch != ':' && !isSpace(static_cast<unsigned char>(ch)))
			return false;
	}
	return hasRule;
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

std::size_t tableRangeEnd(const std::vector<std::string> &lines, std::size_t firstRow, std::size_t separatorRow)
{
	const bool requireLeadingPipe = startsWithUnescapedPipe(lines[firstRow]) && startsWithUnescapedPipe(lines[separatorRow]);
	std::size_t lastRow = separatorRow;
	for (std::size_t row = separatorRow + 1; row < lines.size(); ++row)
	{
		if (!isPotentialTableLine(lines[row]))
			break;
		if (requireLeadingPipe && !startsWithUnescapedPipe(lines[row]))
			break;
		lastRow = row;
	}
	return lastRow;
}

Table parseTable(const std::vector<std::string> &lines, std::size_t firstRow, std::size_t lastRow)
{
	Table table;
	if (lines.empty() || firstRow >= lines.size() || lastRow < firstRow)
		return table;
	if (lastRow >= lines.size())
		lastRow = lines.size() - 1;

	const std::size_t rowCount = lastRow - firstRow + 1;
	table.rows.reserve(rowCount);
	std::size_t leadingPipeRows = 0;
	std::size_t trailingPipeRows = 0;
	for (std::size_t rowOffset = 0; rowOffset < rowCount; ++rowOffset)
	{
		const std::string &line = lines[firstRow + rowOffset];
		Row row;
		row.id = rowOffset;
		row.cells = splitCells(line);
		table.columns = (std::max)(table.columns, row.cells.size());
		table.rows.push_back(std::move(row));
		if (startsWithUnescapedPipe(line))
			++leadingPipeRows;
		if (endsWithUnescapedPipeTrimmed(line))
			++trailingPipeRows;
	}

	if (rowCount > 0)
	{
		table.leadingPipe = leadingPipeRows * 2 >= rowCount;
		table.trailingPipe = trailingPipeRows * 2 >= rowCount;
	}

	for (std::size_t i = 0; i < table.rows.size(); ++i)
	{
		if (isSeparatorRow(table.rows[i]) || (i == 1 && isShortSeparatorLine(lines[firstRow + i])))
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

bool isUtf8Continuation(unsigned char ch);
unsigned int readUtf8CodePoint(const std::string &value, std::size_t &offset);

bool isWideCodePoint(unsigned int cp)
{
	return cp == 0x2329 || cp == 0x232A ||
		(cp >= 0x1100 && cp <= 0x115F) ||
		(cp >= 0x2E80 && cp <= 0xA4CF && cp != 0x303F) ||
		(cp >= 0xAC00 && cp <= 0xD7A3) ||
		(cp >= 0xF900 && cp <= 0xFAFF) ||
		(cp >= 0xFE10 && cp <= 0xFE19) ||
		(cp >= 0xFE30 && cp <= 0xFE6F) ||
		(cp >= 0xFF00 && cp <= 0xFF60) ||
		(cp >= 0xFFE0 && cp <= 0xFFE6) ||
		(cp >= 0x20000 && cp <= 0x3FFFD);
}

bool isEmojiPresentationCodePoint(unsigned int cp)
{
	return (cp >= 0x1F000 && cp <= 0x1FAFF) ||
		(cp >= 0x231A && cp <= 0x231B) ||
		(cp >= 0x23E9 && cp <= 0x23EC) ||
		cp == 0x23F0 || cp == 0x23F3 ||
		(cp >= 0x25FD && cp <= 0x25FE) ||
		(cp >= 0x2614 && cp <= 0x2615) ||
		(cp >= 0x2648 && cp <= 0x2653) ||
		cp == 0x267F || cp == 0x2693 || cp == 0x26A1 ||
		(cp >= 0x26AA && cp <= 0x26AB) ||
		(cp >= 0x26BD && cp <= 0x26BE) ||
		(cp >= 0x26C4 && cp <= 0x26C5) ||
		cp == 0x26CE || cp == 0x26D4 || cp == 0x26EA ||
		(cp >= 0x26F2 && cp <= 0x26F3) ||
		cp == 0x26F5 || cp == 0x26FA || cp == 0x26FD ||
		cp == 0x2705 || (cp >= 0x270A && cp <= 0x270B) ||
		cp == 0x2728 || cp == 0x274C || cp == 0x274E ||
		(cp >= 0x2753 && cp <= 0x2755) ||
		cp == 0x2757 || (cp >= 0x2795 && cp <= 0x2797) ||
		cp == 0x27B0 || cp == 0x27BF ||
		(cp >= 0x2B1B && cp <= 0x2B1C) ||
		cp == 0x2B50 || cp == 0x2B55 ||
		cp == 0x3030 || cp == 0x303D || cp == 0x3297 || cp == 0x3299;
}

bool isEmojiVariationBase(unsigned int cp)
{
	return isEmojiPresentationCodePoint(cp) ||
		cp == 0x00A9 || cp == 0x00AE || cp == 0x203C || cp == 0x2049 ||
		cp == 0x2122 || cp == 0x2139 ||
		(cp >= 0x2194 && cp <= 0x2199) ||
		(cp >= 0x21A9 && cp <= 0x21AA) ||
		cp == 0x2328 || cp == 0x23CF ||
		(cp >= 0x23ED && cp <= 0x23EF) ||
		(cp >= 0x23F1 && cp <= 0x23F2) ||
		(cp >= 0x23F8 && cp <= 0x23FA) ||
		cp == 0x24C2 || (cp >= 0x25AA && cp <= 0x25AB) ||
		cp == 0x25B6 || cp == 0x25C0 ||
		(cp >= 0x25FB && cp <= 0x25FE) ||
		(cp >= 0x2600 && cp <= 0x27BF) ||
		(cp >= 0x2934 && cp <= 0x2935) ||
		(cp >= 0x2B05 && cp <= 0x2B07) ||
		(cp >= 0x2B1B && cp <= 0x2B1C) ||
		cp == 0x2B50 || cp == 0x2B55 ||
		cp == 0x3030 || cp == 0x303D || cp == 0x3297 || cp == 0x3299;
}

bool isVariationSelector(unsigned int cp)
{
	return (cp >= 0xFE00 && cp <= 0xFE0F) || (cp >= 0xE0100 && cp <= 0xE01EF);
}

bool isEmojiModifier(unsigned int cp)
{
	return cp >= 0x1F3FB && cp <= 0x1F3FF;
}

bool isEmojiTag(unsigned int cp)
{
	return cp >= 0xE0020 && cp <= 0xE007F;
}

bool isRegionalIndicator(unsigned int cp)
{
	return cp >= 0x1F1E6 && cp <= 0x1F1FF;
}

bool isKeycapBase(unsigned int cp)
{
	return cp == '#' || cp == '*' || (cp >= '0' && cp <= '9');
}

bool inCodePointRanges(unsigned int cp, const CodePointRange *ranges, std::size_t count)
{
	for (std::size_t i = 0; i < count; ++i)
	{
		if (cp < ranges[i].first)
			return false;
		if (cp <= ranges[i].last)
			return true;
	}
	return false;
}

bool isCombiningCodePoint(unsigned int cp)
{
	static const CodePointRange ranges[] =
	{
		{0x0300, 0x036F}, {0x0483, 0x0489}, {0x0591, 0x05BD}, {0x05BF, 0x05BF},
		{0x05C1, 0x05C2}, {0x05C4, 0x05C5}, {0x05C7, 0x05C7}, {0x0610, 0x061A},
		{0x064B, 0x065F}, {0x0670, 0x0670}, {0x06D6, 0x06DC}, {0x06DF, 0x06E4},
		{0x06E7, 0x06E8}, {0x06EA, 0x06ED}, {0x0711, 0x0711}, {0x0730, 0x074A},
		{0x07A6, 0x07B0}, {0x07EB, 0x07F3}, {0x0816, 0x0819}, {0x081B, 0x0823},
		{0x0825, 0x0827}, {0x0829, 0x082D}, {0x0859, 0x085B}, {0x08D3, 0x08E1},
		{0x08E3, 0x0903}, {0x093A, 0x093C}, {0x093E, 0x094F}, {0x0951, 0x0957},
		{0x0962, 0x0963}, {0x0981, 0x0983}, {0x09BC, 0x09BC}, {0x09BE, 0x09C4},
		{0x09C7, 0x09C8}, {0x09CB, 0x09CD}, {0x09D7, 0x09D7}, {0x09E2, 0x09E3},
		{0x0A01, 0x0A03}, {0x0A3C, 0x0A3C}, {0x0A3E, 0x0A42}, {0x0A47, 0x0A48},
		{0x0A4B, 0x0A4D}, {0x0A51, 0x0A51}, {0x0A70, 0x0A71}, {0x0A75, 0x0A75},
		{0x0A81, 0x0A83}, {0x0ABC, 0x0ABC}, {0x0ABE, 0x0AC5}, {0x0AC7, 0x0AC9},
		{0x0ACB, 0x0ACD}, {0x0AE2, 0x0AE3}, {0x0B01, 0x0B03}, {0x0B3C, 0x0B3C},
		{0x0B3E, 0x0B44}, {0x0B47, 0x0B48}, {0x0B4B, 0x0B4D}, {0x0B56, 0x0B57},
		{0x0B62, 0x0B63}, {0x0B82, 0x0B82}, {0x0BBE, 0x0BC2}, {0x0BC6, 0x0BC8},
		{0x0BCA, 0x0BCD}, {0x0BD7, 0x0BD7}, {0x0C00, 0x0C04}, {0x0C3E, 0x0C44},
		{0x0C46, 0x0C48}, {0x0C4A, 0x0C4D}, {0x0C55, 0x0C56}, {0x0C62, 0x0C63},
		{0x0C81, 0x0C83}, {0x0CBC, 0x0CBC}, {0x0CBE, 0x0CC4}, {0x0CC6, 0x0CC8},
		{0x0CCA, 0x0CCD}, {0x0CD5, 0x0CD6}, {0x0CE2, 0x0CE3}, {0x0D00, 0x0D03},
		{0x0D3B, 0x0D3C}, {0x0D3E, 0x0D44}, {0x0D46, 0x0D48}, {0x0D4A, 0x0D4D},
		{0x0D57, 0x0D57}, {0x0D62, 0x0D63}, {0x0D82, 0x0D83}, {0x0DCA, 0x0DCA},
		{0x0DCF, 0x0DD4}, {0x0DD6, 0x0DD6}, {0x0DD8, 0x0DDF}, {0x0DF2, 0x0DF3},
		{0x0E31, 0x0E31}, {0x0E34, 0x0E3A}, {0x0E47, 0x0E4E}, {0x0EB1, 0x0EB1},
		{0x0EB4, 0x0EBC}, {0x0EC8, 0x0ECD}, {0x0F18, 0x0F19}, {0x0F35, 0x0F35},
		{0x0F37, 0x0F37}, {0x0F39, 0x0F39}, {0x0F3E, 0x0F3F}, {0x0F71, 0x0F84},
		{0x0F86, 0x0F87}, {0x0F8D, 0x0F97}, {0x0F99, 0x0FBC}, {0x0FC6, 0x0FC6},
		{0x102B, 0x103E}, {0x1056, 0x1059}, {0x105E, 0x1060}, {0x1062, 0x1064},
		{0x1067, 0x106D}, {0x1071, 0x1074}, {0x1082, 0x108D}, {0x108F, 0x108F},
		{0x109A, 0x109D}, {0x135D, 0x135F}, {0x1712, 0x1715}, {0x1732, 0x1734},
		{0x1752, 0x1753}, {0x1772, 0x1773}, {0x17B4, 0x17D3}, {0x17DD, 0x17DD},
		{0x180B, 0x180D}, {0x1885, 0x1886}, {0x18A9, 0x18A9}, {0x1920, 0x192B},
		{0x1930, 0x193B}, {0x1A17, 0x1A1B}, {0x1A55, 0x1A5E}, {0x1A60, 0x1A7C},
		{0x1A7F, 0x1A7F}, {0x1AB0, 0x1ACE}, {0x1B00, 0x1B04}, {0x1B34, 0x1B44},
		{0x1B6B, 0x1B73}, {0x1B80, 0x1B82}, {0x1BA1, 0x1BAD}, {0x1BE6, 0x1BF3},
		{0x1C24, 0x1C37}, {0x1CD0, 0x1CD2}, {0x1CD4, 0x1CE8}, {0x1CED, 0x1CED},
		{0x1CF4, 0x1CF4}, {0x1CF7, 0x1CF9}, {0x1DC0, 0x1DFF}, {0x20D0, 0x20FF},
		{0x2CEF, 0x2CF1}, {0x2D7F, 0x2D7F}, {0x2DE0, 0x2DFF}, {0x302A, 0x302F},
		{0x3099, 0x309A}, {0xA66F, 0xA672}, {0xA674, 0xA67D}, {0xA69E, 0xA69F},
		{0xA6F0, 0xA6F1}, {0xA802, 0xA802}, {0xA806, 0xA806}, {0xA80B, 0xA80B},
		{0xA823, 0xA827}, {0xA880, 0xA881}, {0xA8B4, 0xA8C5}, {0xA8E0, 0xA8F1},
		{0xA926, 0xA92D}, {0xA947, 0xA953}, {0xA980, 0xA983}, {0xA9B3, 0xA9C0},
		{0xA9E5, 0xA9E5}, {0xAA29, 0xAA36}, {0xAA43, 0xAA43}, {0xAA4C, 0xAA4D},
		{0xAA7B, 0xAA7D}, {0xAAB0, 0xAAB0}, {0xAAB2, 0xAAB4}, {0xAAB7, 0xAAB8},
		{0xAABE, 0xAABF}, {0xAAC1, 0xAAC1}, {0xAAEB, 0xAAEF}, {0xAAF5, 0xAAF6},
		{0xABE3, 0xABEA}, {0xABEC, 0xABED}, {0xFB1E, 0xFB1E}, {0xFE00, 0xFE0F},
		{0xFE20, 0xFE2F}, {0x101FD, 0x101FD}, {0x102E0, 0x102E0}, {0x10376, 0x1037A},
		{0x10A01, 0x10A03}, {0x10A05, 0x10A06}, {0x10A0C, 0x10A0F}, {0x10A38, 0x10A3A},
		{0x10A3F, 0x10A3F}, {0x10AE5, 0x10AE6}, {0x10D24, 0x10D27}, {0x10EAB, 0x10EAC},
		{0x10F46, 0x10F50}, {0x11000, 0x11002}, {0x11038, 0x11046}, {0x11070, 0x11070},
		{0x11073, 0x11074}, {0x1107F, 0x11082}, {0x110B0, 0x110BA}, {0x11100, 0x11102},
		{0x11127, 0x11134}, {0x11145, 0x11146}, {0x11173, 0x11173}, {0x11180, 0x11182},
		{0x111B3, 0x111C0}, {0x111C9, 0x111CC}, {0x1122C, 0x11237}, {0x1123E, 0x1123E},
		{0x112DF, 0x112EA}, {0x11300, 0x11303}, {0x1133B, 0x1133C}, {0x1133E, 0x11344},
		{0x11347, 0x11348}, {0x1134B, 0x1134D}, {0x11357, 0x11357}, {0x11362, 0x11363},
		{0x11366, 0x1136C}, {0x11370, 0x11374}, {0x11435, 0x11446}, {0x1145E, 0x1145E},
		{0x114B0, 0x114C3}, {0x115AF, 0x115B5}, {0x115B8, 0x115C0}, {0x11630, 0x11640},
		{0x116AB, 0x116B7}, {0x1171D, 0x1172B}, {0x1182C, 0x1183A}, {0x11930, 0x11935},
		{0x11937, 0x11938}, {0x1193B, 0x1193E}, {0x11940, 0x11940}, {0x11942, 0x11943},
		{0x119D1, 0x119D7}, {0x119DA, 0x119E0}, {0x119E4, 0x119E4}, {0x11A01, 0x11A0A},
		{0x11A33, 0x11A39}, {0x11A3B, 0x11A3E}, {0x11A47, 0x11A47}, {0x11A51, 0x11A5B},
		{0x11A8A, 0x11A99}, {0x11C2F, 0x11C36}, {0x11C38, 0x11C3F}, {0x11C92, 0x11CA7},
		{0x11CA9, 0x11CB6}, {0x11D31, 0x11D36}, {0x11D3A, 0x11D3A}, {0x11D3C, 0x11D3D},
		{0x11D3F, 0x11D45}, {0x11D47, 0x11D47}, {0x11D8A, 0x11D8E}, {0x11D90, 0x11D91},
		{0x11D93, 0x11D97}, {0x11EF3, 0x11EF6}, {0x16AF0, 0x16AF4}, {0x16B30, 0x16B36},
		{0x16F4F, 0x16F4F}, {0x16F51, 0x16F87}, {0x16F8F, 0x16F92}, {0x16FE4, 0x16FE4},
		{0x1BC9D, 0x1BC9E}, {0x1D165, 0x1D169}, {0x1D16D, 0x1D172}, {0x1D17B, 0x1D182},
		{0x1D185, 0x1D18B}, {0x1D1AA, 0x1D1AD}, {0x1D242, 0x1D244}, {0x1DA00, 0x1DA36},
		{0x1DA3B, 0x1DA6C}, {0x1DA75, 0x1DA75}, {0x1DA84, 0x1DA84}, {0x1DA9B, 0x1DA9F},
		{0x1DAA1, 0x1DAAF}, {0x1E000, 0x1E006}, {0x1E008, 0x1E018}, {0x1E01B, 0x1E021},
		{0x1E023, 0x1E024}, {0x1E026, 0x1E02A}, {0x1E130, 0x1E136}, {0x1E2EC, 0x1E2EF},
		{0x1E8D0, 0x1E8D6}, {0x1E944, 0x1E94A}, {0xE0100, 0xE01EF}
	};
	return inCodePointRanges(cp, ranges, sizeof(ranges) / sizeof(ranges[0]));
}

bool isFormatCodePoint(unsigned int cp)
{
	return cp == 0x00AD || cp == 0x061C || cp == 0x180E ||
		(cp >= 0x200B && cp <= 0x200F) ||
		(cp >= 0x202A && cp <= 0x202E) ||
		(cp >= 0x2060 && cp <= 0x206F) ||
		cp == 0xFEFF ||
		(cp >= 0xFFF9 && cp <= 0xFFFB) ||
		cp == 0x110BD || cp == 0x110CD ||
		(cp >= 0xE0000 && cp <= 0xE007F);
}

bool isZeroWidthCodePoint(unsigned int cp)
{
	return cp == 0 ||
		cp == 0x200C || cp == 0x200D ||
		cp == 0x20E3 ||
		isFormatCodePoint(cp) ||
		isCombiningCodePoint(cp) ||
		isVariationSelector(cp) ||
		isEmojiModifier(cp) ||
		isEmojiTag(cp);
}

std::size_t codePointDisplayWidth(unsigned int cp)
{
	if (isZeroWidthCodePoint(cp))
		return 0;
	if (cp < 0x1100)
		return 1;
	return isWideCodePoint(cp) || isEmojiPresentationCodePoint(cp) ? 2 : 1;
}

std::size_t skipClusterModifiers(const std::string &text, std::size_t offset, unsigned int baseCodePoint, std::size_t &clusterWidth)
{
	while (offset < text.size())
	{
		std::size_t nextOffset = offset;
		const unsigned int cp = readUtf8CodePoint(text, nextOffset);
		if (!isVariationSelector(cp) && !isCombiningCodePoint(cp) && !isEmojiModifier(cp) && !isEmojiTag(cp))
			break;
		if (cp == 0xFE0F && isEmojiVariationBase(baseCodePoint))
			clusterWidth = (std::max)(clusterWidth, static_cast<std::size_t>(2));
		offset = nextOffset;
	}
	return offset;
}

std::size_t skipKeycapCluster(const std::string &text, std::size_t offset)
{
	std::size_t next = offset;
	while (next < text.size())
	{
		std::size_t after = next;
		const unsigned int cp = readUtf8CodePoint(text, after);
		if (!isVariationSelector(cp))
			break;
		next = after;
	}

	if (next < text.size())
	{
		std::size_t after = next;
		const unsigned int cp = readUtf8CodePoint(text, after);
		if (cp == 0x20E3)
			return after;
	}
	return static_cast<std::size_t>(-1);
}

std::size_t displayWidth(const std::string &text)
{
	std::size_t width = 0;
	for (std::size_t offset = 0; offset < text.size();)
	{
		const unsigned int cp = readUtf8CodePoint(text, offset);

		if (isZeroWidthCodePoint(cp))
			continue;

		if (isKeycapBase(cp))
		{
			const std::size_t keycapEnd = skipKeycapCluster(text, offset);
			if (keycapEnd != static_cast<std::size_t>(-1))
			{
				width += 2;
				offset = keycapEnd;
				continue;
			}
		}

		if (isRegionalIndicator(cp) && offset < text.size())
		{
			std::size_t nextOffset = offset;
			const unsigned int next = readUtf8CodePoint(text, nextOffset);
			if (isRegionalIndicator(next))
			{
				width += 2;
				offset = nextOffset;
				continue;
			}
		}

		std::size_t clusterWidth = codePointDisplayWidth(cp);
		offset = skipClusterModifiers(text, offset, cp, clusterWidth);
		bool joined = false;
		while (offset < text.size())
		{
			std::size_t joinerOffset = offset;
			const unsigned int joiner = readUtf8CodePoint(text, joinerOffset);
			if (joiner != 0x200D)
				break;

			joined = true;
			offset = joinerOffset;
			if (offset >= text.size())
				break;

			const unsigned int joinedCp = readUtf8CodePoint(text, offset);
			clusterWidth = (std::max)(clusterWidth, codePointDisplayWidth(joinedCp));
			offset = skipClusterModifiers(text, offset, joinedCp, clusterWidth);
		}

		width += joined && clusterWidth > 0 ? (std::max)(clusterWidth, static_cast<std::size_t>(2)) : clusterWidth;
	}
	return width;
}

std::size_t nextUtf8Offset(const std::string &text, std::size_t offset)
{
	readUtf8CodePoint(text, offset);
	return offset;
}

bool startsMarkdownLinkAt(const std::string &text, std::size_t offset)
{
	if (offset >= text.size() || text[offset] != '[')
		return false;

	std::size_t closeBracket = offset + 1;
	while (closeBracket < text.size())
	{
		if (text[closeBracket] == ']' && !isEscaped(text, closeBracket))
			break;
		closeBracket = nextUtf8Offset(text, closeBracket);
	}
	return closeBracket + 1 < text.size() && text[closeBracket + 1] == '(';
}

std::size_t markdownLinkEnd(const std::string &text, std::size_t offset)
{
	std::size_t closeBracket = offset + 1;
	while (closeBracket < text.size())
	{
		if (text[closeBracket] == ']' && !isEscaped(text, closeBracket))
			break;
		closeBracket = nextUtf8Offset(text, closeBracket);
	}
	if (closeBracket + 1 >= text.size() || text[closeBracket + 1] != '(')
		return offset + 1;

	std::size_t pos = closeBracket + 2;
	int depth = 1;
	while (pos < text.size())
	{
		if (text[pos] == '(' && !isEscaped(text, pos))
			++depth;
		else if (text[pos] == ')' && !isEscaped(text, pos))
		{
			--depth;
			if (depth == 0)
				return pos + 1;
		}
		pos = nextUtf8Offset(text, pos);
	}
	return offset + 1;
}

std::size_t markdownCodeSpanEnd(const std::string &text, std::size_t offset)
{
	if (offset >= text.size() || text[offset] != '`')
		return offset + 1;

	std::size_t tickCount = 0;
	while (offset + tickCount < text.size() && text[offset + tickCount] == '`')
		++tickCount;

	std::size_t pos = offset + tickCount;
	while (pos < text.size())
	{
		if (text[pos] == '`')
		{
			std::size_t closingTicks = 0;
			while (pos + closingTicks < text.size() && text[pos + closingTicks] == '`')
				++closingTicks;
			if (closingTicks == tickCount)
				return pos + closingTicks;
			pos += closingTicks;
		}
		else
		{
			pos = nextUtf8Offset(text, pos);
		}
	}
	return offset + tickCount;
}

std::size_t longTokenChunkEnd(const std::string &token, std::size_t offset, std::size_t width)
{
	const std::size_t targetWidth = (std::max)(static_cast<std::size_t>(1), width);
	std::size_t end = offset;
	std::size_t chunkWidth = 0;
	while (end < token.size())
	{
		const std::size_t before = end;
		const std::size_t after = nextUtf8Offset(token, before);
		const std::size_t codePointWidth = displayWidth(token.substr(before, after - before));
		if (before > offset && chunkWidth + codePointWidth > targetWidth)
			return before;
		chunkWidth += codePointWidth;
		end = after;
		if (chunkWidth >= targetWidth)
			return end;
	}
	return end > offset ? end : nextUtf8Offset(token, offset);
}

void appendWrappedToken(std::vector<std::string> &segments, std::string &current, const std::string &token, std::size_t width)
{
	const std::size_t tokenWidth = displayWidth(token);
	const std::size_t currentWidth = displayWidth(current);
	const std::size_t candidateWidth = current.empty() ? tokenWidth : currentWidth + 1 + tokenWidth;
	if (tokenWidth <= width)
	{
		if (!current.empty() && candidateWidth > width)
		{
			segments.push_back(current);
			current.clear();
		}
		if (!current.empty())
			current.push_back(' ');
		current += token;
		return;
	}

	if (!current.empty())
	{
		segments.push_back(current);
		current.clear();
	}

	std::size_t offset = 0;
	while (offset < token.size())
	{
		const std::size_t end = longTokenChunkEnd(token, offset, width);
		const std::string chunk = token.substr(offset, end - offset);
		if (end < token.size())
		{
			segments.push_back(chunk);
		}
		else
		{
			current = chunk;
		}
		offset = end;
	}
}

std::vector<std::string> wrapCellSegments(const std::string &cell, std::size_t width)
{
	const std::string value = trim(cell);
	if (value.empty())
		return { "" };

	std::vector<std::string> segments;
	std::string current;
	std::size_t offset = 0;
	while (offset < value.size())
	{
		while (offset < value.size() && isSpace(static_cast<unsigned char>(value[offset])))
			++offset;
		if (offset >= value.size())
			break;

		std::size_t end = offset;
		if (value[offset] == '`')
		{
			end = markdownCodeSpanEnd(value, offset);
		}
		else if (startsMarkdownLinkAt(value, offset))
		{
			end = markdownLinkEnd(value, offset);
		}
		else
		{
			while (end < value.size() && !isSpace(static_cast<unsigned char>(value[end])))
				end = nextUtf8Offset(value, end);
		}

		appendWrappedToken(segments, current, value.substr(offset, end - offset), width);
		offset = end;
	}

	if (!current.empty())
		segments.push_back(current);
	if (segments.empty())
		segments.push_back("");
	return segments;
}

std::size_t wrapLongCells(Table &table, std::size_t originalTargetRow)
{
	if (table.separatorRow == static_cast<std::size_t>(-1))
		return originalTargetRow;

	std::vector<Row> wrappedRows;
	wrappedRows.reserve(table.rows.size());
	std::size_t wrappedTargetRow = originalTargetRow;
	std::size_t nextId = nextRowId(table);

	for (std::size_t rowIndex = 0; rowIndex < table.rows.size(); ++rowIndex)
	{
		const Row &row = table.rows[rowIndex];
		if (row.separator || rowIndex <= table.separatorRow)
		{
			if (rowIndex == originalTargetRow)
				wrappedTargetRow = wrappedRows.size();
			wrappedRows.push_back(row);
			continue;
		}

		std::vector<std::vector<std::string>> cellSegments;
		cellSegments.reserve(table.columns);
		std::size_t segmentCount = 1;
		for (std::size_t column = 0; column < table.columns; ++column)
		{
			cellSegments.push_back(wrapCellSegments(row.cells[column], hardWrapCellWidth));
			segmentCount = (std::max)(segmentCount, cellSegments.back().size());
		}

		if (rowIndex == originalTargetRow)
			wrappedTargetRow = wrappedRows.size();

		for (std::size_t segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
		{
			Row wrapped;
			wrapped.id = segmentIndex == 0 ? row.id : nextId++;
			wrapped.cells.reserve(table.columns);
			for (std::size_t column = 0; column < table.columns; ++column)
			{
				const std::vector<std::string> &segments = cellSegments[column];
				wrapped.cells.push_back(segmentIndex < segments.size() ? segments[segmentIndex] : "");
			}
			wrappedRows.push_back(std::move(wrapped));
		}
	}

	table.rows = std::move(wrappedRows);
	return wrappedTargetRow;
}

void appendSeparatorCell(std::string &line, Align align, std::size_t width)
{
	const std::size_t target = (std::max)(width, static_cast<std::size_t>(3));
	if (align == Align::Center)
	{
		line.push_back(':');
		line.append(target - 2, '-');
		line.push_back(':');
	}
	else if (align == Align::Left)
	{
		line.push_back(':');
		line.append(target - 1, '-');
	}
	else if (align == Align::Right)
	{
		line.append(target - 1, '-');
		line.push_back(':');
	}
	else
	{
		line.append(target, '-');
	}
}

void appendPaddedCell(std::string &line, const std::string &cell, Align align, std::size_t width, std::size_t current, std::size_t *contentOffset)
{
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
	line.append(leftPad, ' ');
	line += cell;
	line.append(rightPad, ' ');
}

FormatResult formatTable(const Table &table, std::size_t targetRow, std::size_t targetColumn)
{
	std::vector<std::size_t> widths(table.columns, 3);
	std::vector<std::size_t> cellWidths(table.rows.size() * table.columns, 0);
	for (std::size_t rowIndex = 0; rowIndex < table.rows.size(); ++rowIndex)
	{
		const Row &row = table.rows[rowIndex];
		if (row.separator)
			continue;

		for (std::size_t column = 0; column < table.columns; ++column)
		{
			const std::size_t currentWidth = displayWidth(row.cells[column]);
			cellWidths[rowIndex * table.columns + column] = currentWidth;
			widths[column] = (std::max)(widths[column], currentWidth);
		}
	}

	FormatResult result;
	result.lines.reserve(table.rows.size());
	result.targetRow = targetRow < table.rows.size() ? targetRow : 0;
	result.targetColumn = targetColumn < table.columns ? targetColumn : 0;

	for (std::size_t rowIndex = 0; rowIndex < table.rows.size(); ++rowIndex)
	{
		const Row &row = table.rows[rowIndex];
		std::string line = table.leadingPipe ? "|" : "";
		std::size_t lineCapacity = table.leadingPipe ? 1 : 0;
		for (std::size_t column = 0; column < table.columns; ++column)
		{
			if (column > 0)
				lineCapacity += 2;
			if (table.leadingPipe || column > 0)
				++lineCapacity;
			if (row.separator)
			{
				lineCapacity += (std::max)(widths[column], static_cast<std::size_t>(3));
			}
			else
			{
				const std::size_t currentWidth = cellWidths[rowIndex * table.columns + column];
				lineCapacity += row.cells[column].size() + (widths[column] > currentWidth ? widths[column] - currentWidth : 0);
			}
			if (table.trailingPipe && column + 1 == table.columns)
				lineCapacity += 2;
		}
		line.reserve(lineCapacity);

		for (std::size_t column = 0; column < table.columns; ++column)
		{
			if (column > 0)
				line += " |";
			if (table.leadingPipe || column > 0)
				line.push_back(' ');

			if (row.separator)
			{
				const std::size_t valueStart = line.size();
				appendSeparatorCell(line, table.alignments[column], widths[column]);
				if (rowIndex == result.targetRow && column == result.targetColumn)
					result.targetColumnOffset = valueStart;
			}
			else
			{
				std::size_t contentOffset = 0;
				const std::size_t cellStart = line.size();
				appendPaddedCell(line, row.cells[column], table.alignments[column], widths[column], cellWidths[rowIndex * table.columns + column], &contentOffset);
				if (rowIndex == result.targetRow && column == result.targetColumn)
					result.targetColumnOffset = cellStart + contentOffset;
			}

			if (table.trailingPipe && column + 1 == table.columns)
				line += " |";
		}

		result.lines.push_back(std::move(line));
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

bool isUtf8Continuation(unsigned char ch)
{
	return (ch & 0xC0) == 0x80;
}

unsigned int readUtf8CodePoint(const std::string &value, std::size_t &offset)
{
	const unsigned char ch = static_cast<unsigned char>(value[offset]);
	if ((ch & 0x80) == 0)
	{
		++offset;
		return ch;
	}

	if ((ch & 0xE0) == 0xC0 && offset + 1 < value.size() && isUtf8Continuation(static_cast<unsigned char>(value[offset + 1])))
	{
		const unsigned int cp = static_cast<unsigned int>(((ch & 0x1F) << 6) |
			(static_cast<unsigned char>(value[offset + 1]) & 0x3F));
		offset += 2;
		return cp;
	}

	if ((ch & 0xF0) == 0xE0 && offset + 2 < value.size() &&
		isUtf8Continuation(static_cast<unsigned char>(value[offset + 1])) &&
		isUtf8Continuation(static_cast<unsigned char>(value[offset + 2])))
	{
		const unsigned int cp = static_cast<unsigned int>(((ch & 0x0F) << 12) |
			((static_cast<unsigned char>(value[offset + 1]) & 0x3F) << 6) |
			(static_cast<unsigned char>(value[offset + 2]) & 0x3F));
		offset += 3;
		return cp;
	}

	if ((ch & 0xF8) == 0xF0 && offset + 3 < value.size() &&
		isUtf8Continuation(static_cast<unsigned char>(value[offset + 1])) &&
		isUtf8Continuation(static_cast<unsigned char>(value[offset + 2])) &&
		isUtf8Continuation(static_cast<unsigned char>(value[offset + 3])))
	{
		const unsigned int cp = static_cast<unsigned int>(((ch & 0x07) << 18) |
			((static_cast<unsigned char>(value[offset + 1]) & 0x3F) << 12) |
			((static_cast<unsigned char>(value[offset + 2]) & 0x3F) << 6) |
			(static_cast<unsigned char>(value[offset + 3]) & 0x3F));
		offset += 4;
		return cp;
	}

	++offset;
	return ch;
}

void appendUtf8(std::string &result, unsigned int cp)
{
	if (cp <= 0x7F)
	{
		result.push_back(static_cast<char>(cp));
	}
	else if (cp <= 0x7FF)
	{
		result.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
		result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
	}
	else if (cp <= 0xFFFF)
	{
		result.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
		result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
		result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
	}
	else
	{
		result.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
		result.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
		result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
		result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
	}
}

unsigned int foldCaseCodePoint(unsigned int cp)
{
	if (cp >= 'A' && cp <= 'Z')
		return cp + ('a' - 'A');
	if (cp >= 0x00C0 && cp <= 0x00D6)
		return cp + 0x20;
	if (cp >= 0x00D8 && cp <= 0x00DE)
		return cp + 0x20;
	if (cp == 0x0178)
		return 0x00FF;
	if (cp >= 0x0391 && cp <= 0x03A1)
		return cp + 0x20;
	if (cp >= 0x03A3 && cp <= 0x03AB)
		return cp + 0x20;
	if (cp >= 0x0400 && cp <= 0x040F)
		return cp + 0x50;
	if (cp >= 0x0410 && cp <= 0x042F)
		return cp + 0x20;

	switch (cp)
	{
		case 0x0386:
			return 0x03AC;
		case 0x0388:
			return 0x03AD;
		case 0x0389:
			return 0x03AE;
		case 0x038A:
			return 0x03AF;
		case 0x038C:
			return 0x03CC;
		case 0x038E:
			return 0x03CD;
		case 0x038F:
			return 0x03CE;
		default:
			return cp;
	}
}

std::string foldCaseForSort(const std::string &value)
{
	std::string folded;
	folded.reserve(value.size());
	for (std::size_t offset = 0; offset < value.size();)
	{
		const unsigned int cp = readUtf8CodePoint(value, offset);
		if (isCombiningCodePoint(cp) || isVariationSelector(cp) || isEmojiModifier(cp) || isEmojiTag(cp) || isFormatCodePoint(cp))
			continue;

		switch (cp)
		{
			case 0x00C0: case 0x00C1: case 0x00C2: case 0x00C3: case 0x00C4: case 0x00C5:
			case 0x00E0: case 0x00E1: case 0x00E2: case 0x00E3: case 0x00E4: case 0x00E5:
				appendUtf8(folded, 'a');
				break;
			case 0x00C6: case 0x00E6:
				appendUtf8(folded, 'a');
				appendUtf8(folded, 'e');
				break;
			case 0x00C7: case 0x00E7:
				appendUtf8(folded, 'c');
				break;
			case 0x00D0: case 0x00F0:
				appendUtf8(folded, 'd');
				break;
			case 0x00C8: case 0x00C9: case 0x00CA: case 0x00CB:
			case 0x00E8: case 0x00E9: case 0x00EA: case 0x00EB:
				appendUtf8(folded, 'e');
				break;
			case 0x00CC: case 0x00CD: case 0x00CE: case 0x00CF:
			case 0x00EC: case 0x00ED: case 0x00EE: case 0x00EF:
				appendUtf8(folded, 'i');
				break;
			case 0x00D1: case 0x00F1:
				appendUtf8(folded, 'n');
				break;
			case 0x00D2: case 0x00D3: case 0x00D4: case 0x00D5: case 0x00D6: case 0x00D8:
			case 0x00F2: case 0x00F3: case 0x00F4: case 0x00F5: case 0x00F6: case 0x00F8:
				appendUtf8(folded, 'o');
				break;
			case 0x00D9: case 0x00DA: case 0x00DB: case 0x00DC:
			case 0x00F9: case 0x00FA: case 0x00FB: case 0x00FC:
				appendUtf8(folded, 'u');
				break;
			case 0x00DD: case 0x00FD: case 0x00FF:
				appendUtf8(folded, 'y');
				break;
			case 0x00DE: case 0x00FE:
				appendUtf8(folded, 't');
				appendUtf8(folded, 'h');
				break;
			case 0x00DF:
				appendUtf8(folded, 's');
				appendUtf8(folded, 's');
				break;
			default:
				appendUtf8(folded, foldCaseCodePoint(cp));
				break;
		}
	}
	return folded;
}

int compareCodePointOrder(const std::string &left, const std::string &right)
{
	std::size_t leftOffset = 0;
	std::size_t rightOffset = 0;
	while (leftOffset < left.size() && rightOffset < right.size())
	{
		const unsigned int leftCp = readUtf8CodePoint(left, leftOffset);
		const unsigned int rightCp = readUtf8CodePoint(right, rightOffset);
		if (leftCp < rightCp)
			return -1;
		if (leftCp > rightCp)
			return 1;
	}
	if (leftOffset < left.size())
		return 1;
	if (rightOffset < right.size())
		return -1;
	return 0;
}

SortKey makeSortKey(const std::string &value)
{
	SortKey key;
	key.text = trim(value);
	key.foldedText = foldCaseForSort(key.text);
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

	const int text = compareCodePointOrder(left.foldedText, right.foldedText);
	if (text != 0)
		return text;
	return compareCodePointOrder(left.text, right.text);
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
		entry.key = makeSortKey(table.rows[i].cells[column]);
		entry.row = std::move(table.rows[i]);
		entries.push_back(std::move(entry));
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
		table.rows[firstDataRow + i] = std::move(entries[i].row);

	return rowById(table, currentRowId, fallbackRow);
}

void insertEmptyRow(Table &table, std::size_t index)
{
	Row row;
	row.id = nextRowId(table);
	row.cells.assign(table.columns, std::string());
	if (index > table.rows.size())
		index = table.rows.size();
	table.rows.insert(table.rows.begin() + static_cast<std::ptrdiff_t>(index), std::move(row));
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
		std::string value = std::move(table.rows[i].cells[from]);
		table.rows[i].cells.erase(table.rows[i].cells.begin() + static_cast<std::ptrdiff_t>(from));
		table.rows[i].cells.insert(table.rows[i].cells.begin() + static_cast<std::ptrdiff_t>(to), std::move(value));
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
	bool inQuotes = false;
	bool cellBlank = true;
	for (std::size_t i = 0; i < value.size(); ++i)
	{
		const char ch = value[i];
		if (inQuotes)
		{
			if (ch == '"' && i + 1 < value.size() && value[i + 1] == '"')
				++i;
			else if (ch == '"')
				inQuotes = false;
		}
		else if (ch == '"' && cellBlank)
		{
			inQuotes = true;
		}
		else if (ch == ',' || ch == '\t')
		{
			return true;
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

TableRange findTableRange(const std::vector<std::string> &lines, int row)
{
	TableRange result;
	if (lines.empty() || row < 0)
		return result;

	const std::size_t targetRow = static_cast<std::size_t>(row);
	if (targetRow >= lines.size())
		return result;

	for (std::size_t separatorRow = 1; separatorRow < lines.size(); ++separatorRow)
	{
		const std::size_t firstRow = separatorRow - 1;
		if (!isPotentialTableLine(lines[firstRow]))
			continue;

		Row separator;
		separator.cells = splitCells(lines[separatorRow]);
		if (!isSeparatorRow(separator) && !isShortSeparatorLine(lines[separatorRow]))
			continue;

		const std::size_t lastRow = tableRangeEnd(lines, firstRow, separatorRow);
		if (targetRow >= firstRow && targetRow <= lastRow)
		{
			result.found = true;
			result.firstRow = firstRow;
			result.lastRow = lastRow;
			return result;
		}
	}

	return result;
}

EditResult apply(const std::vector<std::string> &lines, int row, int column, Action action)
{
	EditResult result;
	if (lines.empty())
	{
		result.message = "No table found";
		return result;
	}

	std::size_t sourceRow = row < 0 ? 0 : static_cast<std::size_t>(row);
	if (sourceRow >= lines.size())
		sourceRow = lines.size() - 1;

	const TableRange tableRange = findTableRange(lines, static_cast<int>(sourceRow));
	if (!tableRange.found)
	{
		result.message = "No Markdown table found";
		return result;
	}

	std::size_t tableRow = sourceRow - tableRange.firstRow;

	Table table = parseTable(lines, tableRange.firstRow, tableRange.lastRow);
	if (!isMarkdownTable(table))
	{
		result.message = "No Markdown table found";
		return result;
	}

	if (tableRow >= table.rows.size())
		tableRow = table.rows.size() - 1;
	std::size_t tableColumn = column < 0 ? 0 : static_cast<std::size_t>(column);
	if (tableColumn >= table.columns)
		tableColumn = table.columns - 1;

	std::size_t targetRow = tableRow;
	std::size_t targetColumn = tableColumn;
	const std::size_t currentRowId = table.rows[tableRow].id;

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
			targetRow = nextEditableRow(table, tableRow);
			insertEmptyRow(table, targetRow);
			break;

		case Action::DeleteRow:
			if (canDeleteRow(table, tableRow))
			{
				table.rows.erase(table.rows.begin() + static_cast<std::ptrdiff_t>(tableRow));
				if (table.separatorRow != static_cast<std::size_t>(-1) && tableRow < table.separatorRow)
					--table.separatorRow;
				targetRow = closestEditableRow(table, tableRow);
			}
			break;

		case Action::InsertColumnRight:
			insertColumn(table, tableColumn + 1);
			targetColumn = tableColumn + 1;
			break;

		case Action::DeleteColumn:
			removeColumn(table, tableColumn);
			targetColumn = tableColumn >= table.columns ? table.columns - 1 : tableColumn;
			break;

		case Action::MoveRowUp:
			if (tableRow > 0 && !table.rows[tableRow].separator && !table.rows[tableRow - 1].separator)
			{
				std::swap(table.rows[tableRow], table.rows[tableRow - 1]);
				targetRow = tableRow - 1;
			}
			break;

		case Action::MoveRowDown:
			if (tableRow + 1 < table.rows.size() && !table.rows[tableRow].separator && !table.rows[tableRow + 1].separator)
			{
				std::swap(table.rows[tableRow], table.rows[tableRow + 1]);
				targetRow = tableRow + 1;
			}
			break;

		case Action::MoveColumnLeft:
			if (tableColumn > 0)
			{
				moveColumn(table, tableColumn, tableColumn - 1);
				targetColumn = tableColumn - 1;
			}
			break;

		case Action::MoveColumnRight:
			if (tableColumn + 1 < table.columns)
			{
				moveColumn(table, tableColumn, tableColumn + 1);
				targetColumn = tableColumn + 1;
			}
			break;

		case Action::SortRowsAscending:
			targetRow = sortRows(table, tableColumn, true, currentRowId, tableRow);
			break;

		case Action::SortRowsDescending:
			targetRow = sortRows(table, tableColumn, false, currentRowId, tableRow);
			break;

		case Action::WrapLongCells:
			targetRow = wrapLongCells(table, tableRow);
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
	const std::string value = trim(text);
	if (!hasDelimitedStructure(value))
	{
		result.message = "No CSV or TSV data found";
		return result;
	}

	const std::vector<std::vector<std::string> > rows = parseDelimitedRows(value, detectDelimiter(value));
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

EditResult createTable(int columns, int dataRows)
{
	EditResult result;
	if (columns < 1 || dataRows < 0)
	{
		result.message = "Invalid table size";
		return result;
	}

	const Table table = emptyTable(static_cast<std::size_t>(columns), static_cast<std::size_t>(dataRows));
	FormatResult formatted = formatTable(table, dataRows > 0 ? 2 : 0, 0);
	setResultFromFormat(result, formatted);
	result.ok = true;
	result.changed = true;
	return result;
}
}
