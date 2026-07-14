// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 krotname

#include "../src/MarkdownTableCore.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
const int WarmupRuns = 3;
const int MeasuredRuns = 5;
volatile std::size_t g_sink = 0;

struct BenchmarkCase
{
	std::string name;
	double thresholdMillis;
	std::function<void()> action;
};

double thresholdScale()
{
#ifdef _MSC_VER
	char buffer[64];
	std::size_t required = 0;
	if (getenv_s(&required, buffer, sizeof(buffer), "CORE_PERFORMANCE_THRESHOLD_SCALE") != 0 || required == 0)
		return 1.0;
	const char *value = buffer;
#else
	const char *value = std::getenv("CORE_PERFORMANCE_THRESHOLD_SCALE");
	if (value == nullptr || *value == '\0')
		return 1.0;
#endif

	char *end = nullptr;
	const double parsed = std::strtod(value, &end);
	return parsed > 0.0 ? parsed : 1.0;
}

bool environmentFlag(const char *name)
{
#ifdef _MSC_VER
	char buffer[16];
	std::size_t required = 0;
	if (getenv_s(&required, buffer, sizeof(buffer), name) != 0 || required == 0)
		return false;
	const char *value = buffer;
#else
	const char *value = std::getenv(name);
	if (value == nullptr || *value == '\0')
		return false;
#endif
	return value[0] != '0';
}

void appendUtf8(std::string &result, unsigned int cp)
{
	if (cp <= 0x7F)
	{
		result.push_back(static_cast<char>(cp));
		return;
	}
	if (cp <= 0x7FF)
	{
		result.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
		result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
		return;
	}
	if (cp <= 0xFFFF)
	{
		result.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
		result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
		result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
		return;
	}

	result.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
	result.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
	result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
	result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
}

std::string codePoints(std::initializer_list<unsigned int> values)
{
	std::string result;
	for (std::initializer_list<unsigned int>::const_iterator it = values.begin(); it != values.end(); ++it)
		appendUtf8(result, *it);
	return result;
}

const std::vector<std::string> &unicodeValues()
{
	static const std::vector<std::string> values =
	{
		codePoints({ 0x8868 }),
		codePoints({ 0x30C6, 0x30B9, 0x30C8 }),
		codePoints({ 0x1F680 }),
		codePoints({ 0x0442, 0x0435, 0x0441, 0x0442 }),
		codePoints({ 0x03A3, 0x03C4, 0x03AE, 0x03BB, 0x03B7 }),
		codePoints({ 0x1F469, 0x200D, 0x1F4BB }),
		codePoints({ 0x0645, 0x0631, 0x062D, 0x0628, 0x0627 }),
		codePoints({ 0x65E5, 0x672C, 0x8A9E })
	};
	return values;
}

std::string unicodeToken(std::size_t row, std::size_t column)
{
	const std::vector<std::string> &values = unicodeValues();
	return values[(row + column) % values.size()];
}

std::string cell(std::size_t row, std::size_t column, bool unicode)
{
	std::string value;
	if (unicode)
	{
		value = unicodeToken(row, column) + "-" + std::to_string(row) + "-" + std::to_string(column);
	}
	else
	{
		value = "row-" + std::to_string(row) + "-column-" + std::to_string(column);
	}

	if ((row + column) % 13 == 0)
		value += " escaped \\| pipe";
	if (column % 5 == 0)
		value += " value-" + std::to_string(((row + 1) * (column + 3)) % 10000);
	return value;
}

std::string tableRow(int row, std::size_t columns, bool unicode)
{
	std::string line = "|";
	for (std::size_t column = 0; column < columns; ++column)
	{
		const std::string value = row < 0
			? "Column " + std::to_string(column + 1)
			: cell(static_cast<std::size_t>(row), column, unicode);
		line += " " + value + " |";
	}
	return line;
}

std::string separator(std::size_t columns)
{
	std::string line = "|";
	for (std::size_t column = 0; column < columns; ++column)
	{
		if (column % 4 == 1)
			line += " ---: |";
		else if (column % 4 == 2)
			line += " :---: |";
		else
			line += " --- |";
	}
	return line;
}

std::vector<std::string> table(std::size_t dataRows, std::size_t columns, bool unicode)
{
	std::vector<std::string> lines;
	lines.reserve(dataRows + 2);
	lines.push_back(tableRow(-1, columns, unicode));
	lines.push_back(separator(columns));
	for (std::size_t row = 0; row < dataRows; ++row)
		lines.push_back(tableRow(static_cast<int>(row), columns, unicode));
	return lines;
}

std::vector<std::string> sortableTable(std::size_t dataRows, std::size_t columns)
{
	std::vector<std::string> lines;
	lines.reserve(dataRows + 2);
	lines.push_back(tableRow(-1, columns, true));
	lines.push_back(separator(columns));
	for (std::size_t row = dataRows; row > 0; --row)
	{
		std::string line = "|";
		for (std::size_t column = 0; column < columns; ++column)
		{
			const std::string value = column == 0
				? unicodeToken(row, column) + "-" + std::to_string(row)
				: std::to_string((row * (column + 17)) % 100000);
			line += " " + value + " |";
		}
		lines.push_back(line);
	}
	return lines;
}

std::string delimited(std::size_t rows, std::size_t columns, char delimiter)
{
	std::string text;
	text.reserve(rows * columns * 14);
	for (std::size_t row = 0; row < rows; ++row)
	{
		if (row > 0)
			text.push_back('\n');
		for (std::size_t column = 0; column < columns; ++column)
		{
			if (column > 0)
				text.push_back(delimiter);
			const std::string value = row == 0
				? "Column " + std::to_string(column + 1)
				: unicodeToken(row, column) + " value " + std::to_string(row) + "-" + std::to_string(column);
			if (delimiter == ',' && (row + column) % 9 == 0)
			{
				text.push_back('"');
				text += value;
				text += ", quoted \"\"cell\"\"";
				text.push_back('"');
			}
			else
			{
				text += value;
			}
		}
	}
	return text;
}

void consume(const MarkdownTable::EditResult &result)
{
	if (!result.ok)
		throw std::runtime_error("core operation failed: " + result.message);

	std::size_t value = result.lines.size() + result.targetRow + result.targetColumn + result.targetColumnOffset;
	if (!result.lines.empty())
	{
		value += result.lines.front().size();
		value += result.lines.back().size();
	}
	g_sink = g_sink + value;
}

double runOnce(const std::function<void()> &action)
{
	const std::chrono::high_resolution_clock::time_point started = std::chrono::high_resolution_clock::now();
	action();
	const std::chrono::high_resolution_clock::time_point finished = std::chrono::high_resolution_clock::now();
	const std::chrono::duration<double, std::milli> elapsed = finished - started;
	return elapsed.count();
}

std::string samplesText(const std::vector<double> &samples)
{
	std::ostringstream output;
	output << "[";
	for (std::size_t i = 0; i < samples.size(); ++i)
	{
		if (i > 0)
			output << ", ";
		output << std::fixed << std::setprecision(2) << samples[i];
	}
	output << "] ms";
	return output.str();
}

bool runBenchmark(const BenchmarkCase &benchmark, double scale)
{
	for (int i = 0; i < WarmupRuns; ++i)
		benchmark.action();

	std::vector<double> samples;
	samples.reserve(MeasuredRuns);
	for (int i = 0; i < MeasuredRuns; ++i)
		samples.push_back(runOnce(benchmark.action));

	std::sort(samples.begin(), samples.end());
	const double median = samples[samples.size() / 2];
	const double threshold = benchmark.thresholdMillis * scale;
	std::cout << "core performance: " << std::left << std::setw(30) << benchmark.name
		<< " median=" << std::right << std::fixed << std::setprecision(2) << median
		<< " ms threshold=" << threshold
		<< " ms samples=" << samplesText(samples) << "\n";

	if (median <= threshold)
		return true;

	std::cerr << benchmark.name << " median " << median << " ms exceeded threshold "
		<< threshold << " ms\n";
	return false;
}

void printOperationBreakdown(const std::vector<std::string> &operationTable)
{
	const std::vector<BenchmarkCase> operations =
	{
		{
			"insert row below",
			0.0,
			[&operationTable]() { consume(MarkdownTable::apply(operationTable, 700, 6, MarkdownTable::Action::InsertRowBelow)); }
		},
		{
			"delete row",
			0.0,
			[&operationTable]() { consume(MarkdownTable::apply(operationTable, 700, 6, MarkdownTable::Action::DeleteRow)); }
		},
		{
			"insert column right",
			0.0,
			[&operationTable]() { consume(MarkdownTable::apply(operationTable, 700, 6, MarkdownTable::Action::InsertColumnRight)); }
		},
		{
			"delete column",
			0.0,
			[&operationTable]() { consume(MarkdownTable::apply(operationTable, 700, 6, MarkdownTable::Action::DeleteColumn)); }
		},
		{
			"move row up",
			0.0,
			[&operationTable]() { consume(MarkdownTable::apply(operationTable, 700, 6, MarkdownTable::Action::MoveRowUp)); }
		},
		{
			"move row down",
			0.0,
			[&operationTable]() { consume(MarkdownTable::apply(operationTable, 700, 6, MarkdownTable::Action::MoveRowDown)); }
		},
		{
			"move column left",
			0.0,
			[&operationTable]() { consume(MarkdownTable::apply(operationTable, 700, 6, MarkdownTable::Action::MoveColumnLeft)); }
		},
		{
			"move column right",
			0.0,
			[&operationTable]() { consume(MarkdownTable::apply(operationTable, 700, 6, MarkdownTable::Action::MoveColumnRight)); }
		}
	};

	std::cout << "core performance breakdown:\n";
	for (std::size_t i = 0; i < operations.size(); ++i)
	{
		for (int warmup = 0; warmup < 2; ++warmup)
			operations[i].action();
		std::vector<double> samples;
		samples.reserve(3);
		for (int sample = 0; sample < 3; ++sample)
			samples.push_back(runOnce(operations[i].action));
		std::sort(samples.begin(), samples.end());
		std::cout << "  " << std::left << std::setw(24) << operations[i].name
			<< " median=" << std::right << std::fixed << std::setprecision(2)
			<< samples[samples.size() / 2] << " ms samples=" << samplesText(samples) << "\n";
	}
}

}

int main()
{
	try
	{
		const std::vector<std::string> largeTable = table(3000, 8, false);
		const std::vector<std::string> unicodeTable = table(1000, 24, true);
		const std::string csv = delimited(3000, 10, ',');
		const std::string tsv = delimited(5000, 8, '\t');
		const std::vector<std::string> sortable = sortableTable(5000, 8);
		const std::vector<std::string> operationTable = table(1500, 16, true);
		const std::vector<std::string> hugeHeaderTable =
		{
			"| " + std::string(1000000, 'x') + " |",
			"| --- |",
			"| value |"
		};

		const std::vector<BenchmarkCase> benchmarks =
		{
			{
				"align 3000x8 table",
				500.0,
				[&largeTable]() { consume(MarkdownTable::apply(largeTable, 1502, 3, MarkdownTable::Action::Align)); }
			},
			{
				"align 1000x24 unicode table",
				700.0,
				[&unicodeTable]() { consume(MarkdownTable::apply(unicodeTable, 502, 12, MarkdownTable::Action::Align)); }
			},
			{
				"convert 3000x10 CSV",
				800.0,
				[&csv]() { consume(MarkdownTable::convertDelimitedToTable(csv)); }
			},
			{
				"convert 5000x8 TSV",
				900.0,
				[&tsv]() { consume(MarkdownTable::convertDelimitedToTable(tsv)); }
			},
			{
				"sort 5000 rows",
				800.0,
				[&sortable]()
				{
					consume(MarkdownTable::apply(sortable, 2500, 0, MarkdownTable::Action::SortRowsAscending));
					consume(MarkdownTable::apply(sortable, 2500, 1, MarkdownTable::Action::SortRowsDescending));
				}
			},
			{
				"fit 1000000-char header",
				1500.0,
				[&hugeHeaderTable]() { consume(MarkdownTable::applyWrappedToWidth(hugeHeaderTable, 2, 0, 80)); }
			},
			{
				"row and column operations",
				1000.0,
				[&operationTable]()
				{
					consume(MarkdownTable::apply(operationTable, 700, 6, MarkdownTable::Action::InsertRowBelow));
					consume(MarkdownTable::apply(operationTable, 700, 6, MarkdownTable::Action::DeleteRow));
					consume(MarkdownTable::apply(operationTable, 700, 6, MarkdownTable::Action::InsertColumnRight));
					consume(MarkdownTable::apply(operationTable, 700, 6, MarkdownTable::Action::DeleteColumn));
					consume(MarkdownTable::apply(operationTable, 700, 6, MarkdownTable::Action::MoveRowUp));
					consume(MarkdownTable::apply(operationTable, 700, 6, MarkdownTable::Action::MoveRowDown));
					consume(MarkdownTable::apply(operationTable, 700, 6, MarkdownTable::Action::MoveColumnLeft));
					consume(MarkdownTable::apply(operationTable, 700, 6, MarkdownTable::Action::MoveColumnRight));
				}
			}
		};

		bool ok = true;
		const double scale = thresholdScale();
		if (environmentFlag("CORE_PERFORMANCE_BREAKDOWN"))
			printOperationBreakdown(operationTable);
		for (std::size_t i = 0; i < benchmarks.size(); ++i)
		{
			ok = runBenchmark(benchmarks[i], scale) && ok;
		}
		if (g_sink == 0)
			throw std::runtime_error("benchmark results were not consumed");
		return ok ? 0 : 1;
	}
	catch (const std::exception &ex)
	{
		std::cerr << "core performance benchmark failed: " << ex.what() << "\n";
		return 1;
	}
}
