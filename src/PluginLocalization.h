#pragma once

#include "PluginCommands.h"
#include <string>

namespace MarkdownTablePlugin
{
enum class UiLanguage
{
	English,
	MandarinChinese,
	Hindi,
	Spanish,
	Arabic,
	French,
	Bengali,
	Portuguese,
	Russian,
	Indonesian,
	Urdu,
	German,
	Japanese,
	NigerianPidgin,
	Marathi,
	Telugu,
	Turkish,
	Tamil,
	YueChinese,
	Vietnamese
};

struct UiText
{
	const wchar_t *pluginMenuName;
	const wchar_t *commands[commandCount];
	const wchar_t *tableSizeTitle;
	const wchar_t *tableSizeColumnsLabel;
	const wchar_t *tableSizeRowsLabel;
	const wchar_t *okButton;
	const wchar_t *cancelButton;
	const wchar_t *tableSizeRangeError;
	const wchar_t *putCaretInsideTable;
	const wchar_t *couldNotEditTable;
	const wchar_t *selectCsvTsv;
	const wchar_t *couldNotConvertCsvTsv;
	const wchar_t *wordWrapAutoFitWarningTitle;
	const wchar_t *wordWrapAutoFitWarningMessage;
	const wchar_t *doNotShowAgain;
};


UiLanguage languageFromNativeLangFileName(const std::string &nativeLangFileName);

class PluginLocalization
{
public:
	void setLanguage(UiLanguage language) { language_ = language; }
	const UiText &uiText() const;
	const wchar_t *commandText(std::size_t index);
	const wchar_t *commandMenuText(std::size_t index);
private:
	const wchar_t *localizedSpecialCommandText(std::size_t index) const;
	const wchar_t *autoModeCommandText(std::size_t index, const wchar_t *text);
	UiLanguage language_ = UiLanguage::English;
	std::wstring modeText_[commandCount];
	std::wstring menuText_[commandCount];
};
}
