//this file is part of notepad++
//Copyright (C)2022 Don HO <don.h@free.fr>
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef PLUGINDEFINITION_H
#define PLUGINDEFINITION_H

//
// All difinitions of plugin interface
//
#include "PluginInterface.h"

#ifdef MARKDOWN_TABLE_PLUGIN_TESTING
#include "MarkdownTableCore.h"
#include <cstddef>
#include <string>
#include <vector>
#endif

//-------------------------------------//
//-- STEP 1. DEFINE YOUR PLUGIN NAME --//
//-------------------------------------//
// Here define your plugin name
//
const TCHAR NPP_PLUGIN_NAME[] = TEXT("Markdown Table Editor");

//-----------------------------------------------//
//-- STEP 2. DEFINE YOUR PLUGIN COMMAND NUMBER --//
//-----------------------------------------------//
//
// Here define the number of your plugin commands
//
const int nbFunc = 18;


//
// Initialization of your plugin data
// It will be called while plugin loading
//
void pluginInit(HANDLE hModule);

//
// Cleaning of your plugin
// It will be called while plugin unloading
//
void pluginCleanUp();

//
//Initialization of your plugin commands
//
void commandMenuInit();

void refreshUiLanguageFromNotepad();
void registerToolbarIcons();
void refreshAutoFitTableUi();
void refreshAutoAlignTableUi();
void checkWordWrapAutoFitWarning();
void handleScintillaUpdateUi(const SCNotification *notification);
void handleScintillaModified(const SCNotification *notification);
void handleScintillaZoom(const SCNotification *notification);
void handleGlobalModified();
void handleInitialAutoTableFormatForBuffer(const SCNotification *notification);
void handleInitialAutoTableFormatForCurrentBuffer();
void forgetInitialAutoTableFormatForBuffer(const SCNotification *notification);
void installFitToWindowResizeHooks();
void removeFitToWindowResizeHooks();

//
//Clean up your plugin commands allocation (if any)
//
void commandMenuCleanUp();

//
// Function which sets your command 
//
bool setCommand(size_t index, const TCHAR *cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey *sk = NULL, bool check0nInit = false);


//
// Your plugin command functions
//
void alignTable();
void nextCell();
void previousCell();
void insertRowBelow();
void deleteRow();
void insertColumnRight();
void deleteColumn();
void moveRowUp();
void moveRowDown();
void moveColumnLeft();
void moveColumnRight();
void sortRowsAscending();
void sortRowsDescending();
void convertCsvTsvSelectionToTable();
void insertTable();
void wrapLongCells();
void toggleAutoFitTable();
void toggleAutoAlignTable();

#ifdef MARKDOWN_TABLE_PLUGIN_TESTING
namespace MarkdownTablePluginTesting
{
struct ReplacementPreview
{
	std::string text;
	std::size_t caretOffset = 0;
};

struct CellCaretPreview
{
	std::size_t row = 0;
	std::size_t columnOffset = 0;
};

std::string chooseEolFromTextForTests(const std::string &text, const std::string &fallback);
ReplacementPreview replacementPreviewForTests(const MarkdownTable::EditResult &edit, const std::string &eol);
ReplacementPreview delimitedReplacementPreviewForTests(const std::string &source, const std::string &fallback, const MarkdownTable::EditResult &edit);
void applyNativeLangFileNameForTests(const std::string &nativeLangFileName);
const wchar_t *pluginMenuNameForTests();
const wchar_t *commandTextForTests(std::size_t index);
const wchar_t *commandMenuTextForTests(std::size_t index);
bool autoFitTableEnabledForTests();
void setAutoFitTableEnabledForTests(bool enabled);
bool autoAlignTableEnabledForTests();
void setAutoAlignTableEnabledForTests(bool enabled);
MarkdownTable::Action coreActionForPluginActionForTests(MarkdownTable::Action action);
bool shouldApplyAutoFitAfterActionForTests(MarkdownTable::Action action);
bool shouldFitToWindowAfterActionForTests(MarkdownTable::Action action);
bool alignTableCommandEnabledForTests();
bool fitTableToWindowCommandEnabledForTests();
bool autoAlignTableToggleEnabledForTests();
bool standardWordWrapEnabledForModeForTests(long wrapMode);
bool shouldShowWordWrapAutoFitWarningForTests(bool autoFitEnabled, bool wordWrapEnabled, bool warningSuppressed);
bool shouldRunFitToWindowAfterResizeForTests(bool enabled, bool inProgress, bool activeEditor, std::size_t previousColumns, std::size_t currentColumns);
bool shouldScheduleFitToWindowAfterResizeMessageForTests(bool enabled, UINT message, WPARAM wParam, UINT windowPosFlags);
bool shouldRunInitialFitWhenTogglingAutoFitTableForTests(bool currentlyEnabled);
bool shouldRunAutoTableFormatAfterUpdateForTests(bool autoAlignEnabled, bool autoFitEnabled, bool alignInProgress, bool fitInProgress, bool activeEditor, bool contentUpdated);
bool scintillaModificationShouldRunAutoTableFormatForTests(int modificationType);
bool shouldRunAutoFitAfterZoomForTests(bool autoFitEnabled, bool fitInProgress, bool activeEditor);
bool shouldScheduleFitToWindowAfterZoomForTests(bool autoFitEnabled, bool fitInProgress, bool activeEditor);
bool shouldRunAutoTableFormatAfterGlobalModifiedForTests(bool autoAlignEnabled, bool autoFitEnabled, bool alignInProgress, bool fitInProgress);
bool shouldRunInitialAlignWhenTogglingAutoAlignTableForTests(bool currentlyEnabled);
bool shouldRunInitialAutoTableFormatForBufferForTests(bool autoAlignEnabled, bool autoFitEnabled, bool alignInProgress, bool fitInProgress, bool activeEditor, bool alreadyHandled);
bool shouldQueueInitialAutoTableFormatForOpenedBufferForTests(bool autoAlignEnabled, bool autoFitEnabled, bool alreadyHandled);
std::vector<std::size_t> documentMarkdownTableRangeLinesForTests(const std::vector<std::string> &lines);
std::vector<std::string> autoFormatDocumentTablesForTests(const std::vector<std::string> &lines, MarkdownTable::Action action, std::size_t maxTableWidth);
bool shouldMoveCaretToTargetForTests(std::size_t currentPosition, std::size_t targetPosition);
bool shouldPreserveEnterColumnForTests(bool activeEditor, bool emptySelection, bool tableLine, UINT message, WPARAM key);
UINT fitToWindowResizeDelayMsForTests();
std::size_t preservedCellCaretColumnOffsetForTests(const std::string &sourceLine, std::size_t column, std::size_t byteColumn, const std::string &replacementLine);
CellCaretPreview preservedCellCaretPositionForTests(const std::vector<std::string> &sourceLines, std::size_t row, std::size_t column, std::size_t byteColumn, const std::vector<std::string> &replacementLines);
bool ensureAlignToolbarIconsForTests();
void destroyAlignToolbarIconsForTests();
bool ensureWrapLongCellsToolbarIconsForTests();
void destroyWrapLongCellsToolbarIconsForTests();
bool ensureAutoFitTableToolbarIconsForTests();
void destroyAutoFitTableToolbarIconsForTests();
bool ensureAutoAlignTableToolbarIconsForTests();
void destroyAutoAlignTableToolbarIconsForTests();
}
#endif

#endif //PLUGINDEFINITION_H
