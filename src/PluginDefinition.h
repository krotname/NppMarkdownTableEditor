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
const int nbFunc = 20;


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
void refreshNotepadWordWrapUi();
void refreshAutoWrapLongCellsUi();
void refreshFitToWindowOnResizeUi();
void maybeFitToWindowAfterUiUpdate();

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
void tabOrIndent();
void wrapLongCells();
void toggleNotepadWordWrap();
void toggleAutoWrapLongCells();
void toggleFitToWindowOnResize();

#ifdef MARKDOWN_TABLE_PLUGIN_TESTING
namespace MarkdownTablePluginTesting
{
struct ReplacementPreview
{
	std::string text;
	std::size_t caretOffset = 0;
};

std::string chooseEolFromTextForTests(const std::string &text, const std::string &fallback);
ReplacementPreview replacementPreviewForTests(const MarkdownTable::EditResult &edit, const std::string &eol);
ReplacementPreview delimitedReplacementPreviewForTests(const std::string &source, const std::string &fallback, const MarkdownTable::EditResult &edit);
void applyNativeLangFileNameForTests(const std::string &nativeLangFileName);
const wchar_t *pluginMenuNameForTests();
bool autoWrapLongCellsEnabledForTests();
void setAutoWrapLongCellsEnabledForTests(bool enabled);
MarkdownTable::Action coreActionForPluginActionForTests(MarkdownTable::Action action);
bool shouldApplyAutoWrapAfterActionForTests(MarkdownTable::Action action);
bool shouldFitToWindowAfterActionForTests(MarkdownTable::Action action);
bool fitToWindowOnResizeEnabledForTests();
void setFitToWindowOnResizeEnabledForTests(bool enabled);
bool ensureTabToolbarIconsForTests();
void destroyTabToolbarIconsForTests();
bool ensureWrapLongCellsToolbarIconsForTests();
void destroyWrapLongCellsToolbarIconsForTests();
bool ensureNotepadWordWrapToolbarIconsForTests();
void destroyNotepadWordWrapToolbarIconsForTests();
bool ensureAutoWrapToolbarIconsForTests();
void destroyAutoWrapToolbarIconsForTests();
bool ensureFitToWindowOnResizeToolbarIconsForTests();
void destroyFitToWindowOnResizeToolbarIconsForTests();
}
#endif

#endif //PLUGINDEFINITION_H
