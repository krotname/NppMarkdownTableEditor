// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 krotname

#include "GoldenFixtureTests.h"
#include "../src/MarkdownTableCore.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
struct JsonValue
{
	enum Type
	{
		Null,
		Boolean,
		Number,
		String,
		Array,
		Object
	};

	Type type = Null;
	bool booleanValue = false;
	double numberValue = 0;
	std::string stringValue;
	std::vector<JsonValue> arrayValue;
	std::map<std::string, JsonValue> objectValue;
};

class JsonParser
{
public:
	explicit JsonParser(const std::string &source)
		: source_(source)
	{
	}

	JsonValue parse()
	{
		JsonValue value = parseValue();
		skipWhitespace();
		if (offset_ != source_.size())
			throw error("Unexpected trailing data");
		return value;
	}

private:
	JsonValue parseValue()
	{
		skipWhitespace();
		if (offset_ >= source_.size())
			throw error("Unexpected end of JSON");

		const char ch = source_[offset_];
		if (ch == '{')
			return parseObject();
		if (ch == '[')
			return parseArray();
		if (ch == '"')
		{
			JsonValue value;
			value.type = JsonValue::String;
			value.stringValue = parseString();
			return value;
		}
		if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch)) != 0)
			return parseNumber();
		if (source_.compare(offset_, 4, "true") == 0)
		{
			offset_ += 4;
			JsonValue value;
			value.type = JsonValue::Boolean;
			value.booleanValue = true;
			return value;
		}
		if (source_.compare(offset_, 5, "false") == 0)
		{
			offset_ += 5;
			JsonValue value;
			value.type = JsonValue::Boolean;
			return value;
		}
		if (source_.compare(offset_, 4, "null") == 0)
		{
			offset_ += 4;
			return JsonValue();
		}
		throw error("Unexpected value");
	}

	JsonValue parseObject()
	{
		expect('{');
		JsonValue result;
		result.type = JsonValue::Object;
		skipWhitespace();
		if (peek('}'))
		{
			++offset_;
			return result;
		}

		for (;;)
		{
			const std::string key = parseString();
			expect(':');
			result.objectValue[key] = parseValue();
			skipWhitespace();
			if (peek('}'))
			{
				++offset_;
				return result;
			}
			expect(',');
		}
	}

	JsonValue parseArray()
	{
		expect('[');
		JsonValue result;
		result.type = JsonValue::Array;
		skipWhitespace();
		if (peek(']'))
		{
			++offset_;
			return result;
		}

		for (;;)
		{
			result.arrayValue.push_back(parseValue());
			skipWhitespace();
			if (peek(']'))
			{
				++offset_;
				return result;
			}
			expect(',');
		}
	}

	std::string parseString()
	{
		expect('"');
		std::string result;
		while (offset_ < source_.size())
		{
			const char ch = source_[offset_++];
			if (ch == '"')
				return result;
			if (ch != '\\')
			{
				result += ch;
				continue;
			}
			if (offset_ >= source_.size())
				throw error("Unterminated escape sequence");
			const char escaped = source_[offset_++];
			switch (escaped)
			{
			case '"':
			case '\\':
			case '/':
				result += escaped;
				break;
			case 'b':
				result += '\b';
				break;
			case 'f':
				result += '\f';
				break;
			case 'n':
				result += '\n';
				break;
			case 'r':
				result += '\r';
				break;
			case 't':
				result += '\t';
				break;
			default:
				throw error("Unsupported escape sequence");
			}
		}
		throw error("Unterminated string");
	}

	JsonValue parseNumber()
	{
		const std::size_t start = offset_;
		if (peek('-'))
			++offset_;
		while (offset_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[offset_])) != 0)
			++offset_;
		if (peek('.'))
		{
			++offset_;
			while (offset_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[offset_])) != 0)
				++offset_;
		}
		if (peek('e') || peek('E'))
		{
			++offset_;
			if (peek('+') || peek('-'))
				++offset_;
			while (offset_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[offset_])) != 0)
				++offset_;
		}

		JsonValue result;
		result.type = JsonValue::Number;
		result.numberValue = std::strtod(source_.substr(start, offset_ - start).c_str(), 0);
		return result;
	}

	void expect(char expected)
	{
		skipWhitespace();
		if (offset_ >= source_.size() || source_[offset_] != expected)
			throw error(std::string("Expected '") + expected + "'");
		++offset_;
	}

	bool peek(char expected) const
	{
		return offset_ < source_.size() && source_[offset_] == expected;
	}

	void skipWhitespace()
	{
		while (offset_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[offset_])) != 0)
			++offset_;
	}

	std::runtime_error error(const std::string &message) const
	{
		std::ostringstream stream;
		stream << message << " at offset " << offset_;
		return std::runtime_error(stream.str());
	}

	const std::string &source_;
	std::size_t offset_ = 0;
};

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
	std::map<std::string, JsonValue>::const_iterator found = object.objectValue.find(key);
	if (found == object.objectValue.end())
		throw std::runtime_error("Missing JSON property: " + key);
	return found->second;
}

bool hasMember(const JsonValue &object, const std::string &key)
{
	return object.objectValue.find(key) != object.objectValue.end();
}

std::string asString(const JsonValue &value)
{
	if (value.type != JsonValue::String)
		throw std::runtime_error("Expected JSON string");
	return value.stringValue;
}

std::size_t asSize(const JsonValue &value)
{
	if (value.type != JsonValue::Number)
		throw std::runtime_error("Expected JSON number");
	return static_cast<std::size_t>(value.numberValue);
}

std::vector<std::string> asStringVector(const JsonValue &value)
{
	if (value.type != JsonValue::Array)
		throw std::runtime_error("Expected JSON array");
	std::vector<std::string> result;
	for (std::size_t i = 0; i < value.arrayValue.size(); ++i)
		result.push_back(asString(value.arrayValue[i]));
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
	if (scenarios.type != JsonValue::Array)
		throw std::runtime_error("conversion must be an array");

	for (std::size_t i = 0; i < scenarios.arrayValue.size(); ++i)
	{
		const JsonValue &scenario = scenarios.arrayValue[i];
		const std::string name = asString(member(scenario, "name"));
		const MarkdownTable::EditResult result = MarkdownTable::convertDelimitedToTable(asString(member(scenario, "input")));
		expectTrue(name, result.ok, std::string("should convert: ") + result.message);
		expectLines(name, result.lines, asStringVector(member(scenario, "lines")));
	}
}

void runEditScenarios(const JsonValue &scenarios)
{
	if (scenarios.type != JsonValue::Array)
		throw std::runtime_error("edits must be an array");

	for (std::size_t i = 0; i < scenarios.arrayValue.size(); ++i)
	{
		const JsonValue &scenario = scenarios.arrayValue[i];
		const std::string name = asString(member(scenario, "name"));
		const MarkdownTable::EditResult result = MarkdownTable::apply(
			asStringVector(member(scenario, "input")),
			asSize(member(scenario, "row")),
			asSize(member(scenario, "column")),
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
		JsonParser parser(text);
		const JsonValue root = parser.parse();
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
