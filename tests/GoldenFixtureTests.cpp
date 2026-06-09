// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 krotname

#include "GoldenFixtureTests.h"
#include "../src/MarkdownTableCore.h"
#include "third_party/nlohmann/json.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
typedef nlohmann::json JsonValue;

int g_checks = 0;
int g_failures = 0;

void fail(const std::string &name, const std::string &message)
{
	++g_failures;
	std::cerr << "golden " << name << " failed: " << message << "\n";
}

void expectTrue(const std::string &name, bool value, const std::string &message)
{
	++g_checks;
	if (!value)
		fail(name, message);
}

void expectSize(const std::string &name, std::size_t actual, std::size_t expected, const char *field)
{
	++g_checks;
	if (actual == expected)
		return;

	std::ostringstream stream;
	stream << field << " expected " << expected << ", got " << actual;
	fail(name, stream.str());
}

void expectLines(const std::string &name, const std::vector<std::string> &actual, const std::vector<std::string> &expected)
{
	++g_checks;
	if (actual == expected)
		return;

	++g_failures;
	std::cerr << "golden " << name << " failed\nExpected:\n";
	for (std::size_t i = 0; i < expected.size(); ++i)
		std::cerr << expected[i] << "\n";
	std::cerr << "Actual:\n";
	for (std::size_t i = 0; i < actual.size(); ++i)
		std::cerr << actual[i] << "\n";
}

const JsonValue &member(const JsonValue &object, const std::string &key)
{
	if (!object.is_object())
		throw std::runtime_error("Expected JSON object");
	JsonValue::const_iterator found = object.find(key);
	if (found == object.end())
		throw std::runtime_error("Missing JSON property: " + key);
	return *found;
}

bool hasMember(const JsonValue &object, const std::string &key)
{
	return object.is_object() && object.find(key) != object.end();
}

std::string asString(const JsonValue &value)
{
	if (!value.is_string())
		throw std::runtime_error("Expected JSON string");
	return value.get<std::string>();
}

std::size_t asSize(const JsonValue &value)
{
	if (!value.is_number_unsigned() && !value.is_number_integer())
		throw std::runtime_error("Expected JSON number");
	return value.get<std::size_t>();
}

std::vector<std::string> asStringVector(const JsonValue &value)
{
	if (!value.is_array())
		throw std::runtime_error("Expected JSON array");
	std::vector<std::string> result;
	for (JsonValue::const_iterator item = value.begin(); item != value.end(); ++item)
		result.push_back(asString(*item));
	return result;
}

MarkdownTable::Action actionFromString(const std::string &value)
{
	if (value == "ALIGN")
		return MarkdownTable::Action::Align;
	if (value == "NEXT_CELL")
		return MarkdownTable::Action::NextCell;
	if (value == "PREVIOUS_CELL")
		return MarkdownTable::Action::PreviousCell;
	if (value == "INSERT_ROW_BELOW")
		return MarkdownTable::Action::InsertRowBelow;
	if (value == "DELETE_ROW")
		return MarkdownTable::Action::DeleteRow;
	if (value == "INSERT_COLUMN_RIGHT")
		return MarkdownTable::Action::InsertColumnRight;
	if (value == "DELETE_COLUMN")
		return MarkdownTable::Action::DeleteColumn;
	if (value == "MOVE_ROW_UP")
		return MarkdownTable::Action::MoveRowUp;
	if (value == "MOVE_ROW_DOWN")
		return MarkdownTable::Action::MoveRowDown;
	if (value == "MOVE_COLUMN_LEFT")
		return MarkdownTable::Action::MoveColumnLeft;
	if (value == "MOVE_COLUMN_RIGHT")
		return MarkdownTable::Action::MoveColumnRight;
	if (value == "SORT_ASCENDING")
		return MarkdownTable::Action::SortRowsAscending;
	if (value == "SORT_DESCENDING")
		return MarkdownTable::Action::SortRowsDescending;
	if (value == "WRAP_LONG_CELLS")
		return MarkdownTable::Action::WrapLongCells;
	throw std::runtime_error("Unsupported action: " + value);
}

std::string readFile(const std::string &path)
{
	std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
	if (!input)
		throw std::runtime_error("Cannot open " + path);
	std::ostringstream stream;
	stream << input.rdbuf();
	return stream.str();
}

std::string findFixturePath()
{
	const char *candidates[] =
	{
		"test-fixtures\\markdown-table-core-golden.json",
		"..\\test-fixtures\\markdown-table-core-golden.json"
	};
	for (std::size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
	{
		std::ifstream input(candidates[i], std::ios::in | std::ios::binary);
		if (input)
			return candidates[i];
	}
	throw std::runtime_error("Cannot find test-fixtures\\markdown-table-core-golden.json");
}

void runConversionScenarios(const JsonValue &scenarios)
{
	if (!scenarios.is_array())
		throw std::runtime_error("conversion must be an array");

	for (JsonValue::const_iterator item = scenarios.begin(); item != scenarios.end(); ++item)
	{
		const JsonValue &scenario = *item;
		const std::string name = asString(member(scenario, "name"));
		const MarkdownTable::EditResult result = MarkdownTable::convertDelimitedToTable(asString(member(scenario, "input")));
		expectTrue(name, result.ok, std::string("should convert: ") + result.message);
		expectLines(name, result.lines, asStringVector(member(scenario, "lines")));
	}
}

void runEditScenarios(const JsonValue &scenarios)
{
	if (!scenarios.is_array())
		throw std::runtime_error("edits must be an array");

	for (JsonValue::const_iterator item = scenarios.begin(); item != scenarios.end(); ++item)
	{
		const JsonValue &scenario = *item;
		const std::string name = asString(member(scenario, "name"));
		const MarkdownTable::EditResult result = MarkdownTable::apply(
			asStringVector(member(scenario, "input")),
			static_cast<int>(asSize(member(scenario, "row"))),
			static_cast<int>(asSize(member(scenario, "column"))),
			actionFromString(asString(member(scenario, "action"))));
		expectTrue(name, result.ok, std::string("should apply: ") + result.message);
		expectLines(name, result.lines, asStringVector(member(scenario, "lines")));

		if (hasMember(scenario, "targetRow"))
			expectSize(name, result.targetRow, asSize(member(scenario, "targetRow")), "targetRow");
		if (hasMember(scenario, "targetColumn"))
			expectSize(name, result.targetColumn, asSize(member(scenario, "targetColumn")), "targetColumn");
	}
}
}

int runGoldenFixtureTests()
{
	try
	{
		const std::string path = findFixturePath();
		const std::string text = readFile(path);
		const JsonValue root = JsonValue::parse(text);
		runConversionScenarios(member(root, "conversion"));
		runEditScenarios(member(root, "edits"));
	}
	catch (const std::exception &error)
	{
		++g_failures;
		std::cerr << "golden fixtures failed: " << error.what() << "\n";
	}

	if (g_failures == 0)
		std::cout << "Golden fixture tests passed (" << g_checks << " checks)\n";
	return g_failures;
}
