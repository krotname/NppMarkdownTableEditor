#pragma once

#include <cstddef>

namespace MarkdownTablePlugin
{
constexpr std::size_t commandCount = 20;
const std::size_t alignCommandIndex = 0;
const std::size_t autoAlignTableCommandIndex = 1;
const std::size_t wrapLongCellsCommandIndex = 2;
const std::size_t autoFitTableCommandIndex = 3;
const std::size_t nextCellCommandIndex = 4;
const std::size_t previousCellCommandIndex = 5;
const std::size_t insertRowCommandIndex = 6;
const std::size_t deleteRowCommandIndex = 7;
const std::size_t insertColumnCommandIndex = 8;
const std::size_t deleteColumnCommandIndex = 9;
const std::size_t narrowColumnCommandIndex = 10;
const std::size_t widenColumnCommandIndex = 11;
const std::size_t moveRowUpCommandIndex = 12;
const std::size_t moveRowDownCommandIndex = 13;
const std::size_t moveColumnLeftCommandIndex = 14;
const std::size_t moveColumnRightCommandIndex = 15;
const std::size_t sortRowsAscendingCommandIndex = 16;
const std::size_t sortRowsDescendingCommandIndex = 17;
const std::size_t convertCsvTsvCommandIndex = 18;
const std::size_t insertTableCommandIndex = 19;
}
