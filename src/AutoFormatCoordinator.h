#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace MarkdownTablePlugin
{
using BufferId = std::uintptr_t;
enum class EditorView : std::size_t { Main, Secondary };
enum class FormatMode { None, Align, Fit };

// UI-thread state only. The adapter supplies editor facts and executes returned work.
// Buffer state survives view changes; view state never follows a different buffer.
class AutoFormatCoordinator
{
public:
	bool autoFitEnabled() const { return autoFit_; }
	bool autoAlignEnabled() const { return autoAlign_; }
	bool fitInProgress() const { return operation_ == FormatMode::Fit; }
	bool alignInProgress() const { return operation_ == FormatMode::Align; }
	void setAutoFitEnabled(bool enabled);
	void setAutoAlignEnabled(bool enabled);

	bool beginOperation(FormatMode mode);
	void endOperation();
	void beginPluginEdit(BufferId buffer);
	void endPluginEdit();

	void bindView(EditorView view, BufferId buffer);
	void rememberWidth(EditorView view, BufferId buffer, std::size_t columns);
	bool scheduleResize(EditorView view, BufferId buffer);
	void cancelResize(EditorView view);
	FormatMode takeResize(EditorView view, BufferId buffer, bool active, std::size_t columns, bool emptySelection);
	FormatMode onContentUpdate(EditorView view, BufferId buffer, bool active, bool contentUpdated, bool emptySelection);
	FormatMode onGlobalModified(EditorView view, BufferId buffer, bool active, bool emptySelection);
	void queueInitial(BufferId buffer);
	bool initialPending(BufferId buffer) const;
	FormatMode takeInitial(EditorView view, BufferId buffer, bool active, bool hasText, bool emptySelection);
	void forgetBuffer(BufferId buffer);

private:
	enum class InitialState { Unseen, Pending, Handled };
	enum class Notification { Update, Global, Resize };
	struct BufferState
	{
		InitialState initial = InitialState::Unseen;
		int updateSkips = 0;
		int globalSkips = 0;
		int resizeSkips = 0;
	};
	struct ViewState
	{
		BufferId buffer = 0;
		std::size_t lastColumns = 0;
		bool resizePending = false;
	};
	ViewState &viewState(EditorView view);
	bool consumeEditNotification(BufferId buffer, Notification notification);
	FormatMode requestedMode() const;
	bool canFormat() const;
	bool autoFit_ = true;
	bool autoAlign_ = true;
	FormatMode operation_ = FormatMode::None;
	int pluginEditDepth_ = 0;
	std::unordered_map<BufferId, BufferState> buffers_;
	std::array<ViewState, 2> views_ = {};
};

// Keeps re-entrant notifications blocked until editor work has unwound.
class AutoFormatOperation
{
public:
	AutoFormatOperation(AutoFormatCoordinator &coordinator, FormatMode mode)
		: coordinator_(coordinator), started_(coordinator.beginOperation(mode)) {}
	~AutoFormatOperation() { if (started_) coordinator_.endOperation(); }
	explicit operator bool() const { return started_; }
	AutoFormatOperation(const AutoFormatOperation &) = delete;
	AutoFormatOperation &operator=(const AutoFormatOperation &) = delete;
private:
	AutoFormatCoordinator &coordinator_;
	bool started_;
};

bool shouldRunFitToWindowAfterResize(bool enabled, bool inProgress, bool activeEditor, std::size_t previousColumns, std::size_t currentColumns);
bool shouldRunInitialFitWhenTogglingAutoFitTable(bool currentlyEnabled);
bool shouldRunAutoTableFormatAfterUpdate(bool autoAlignEnabled, bool autoFitEnabled, bool alignInProgress, bool fitInProgress, bool pluginEditInProgress, bool activeEditor, bool contentUpdated);
bool shouldRunAutoFitAfterZoom(bool autoFitEnabled, bool fitInProgress, bool activeEditor);
bool shouldScheduleFitToWindowAfterZoom(bool autoFitEnabled, bool fitInProgress, bool activeEditor);
bool shouldRunAutoTableFormatAfterGlobalModified(bool autoAlignEnabled, bool autoFitEnabled, bool alignInProgress, bool fitInProgress, bool pluginEditInProgress);
bool shouldRunInitialAlignWhenTogglingAutoAlignTable(bool currentlyEnabled);
bool shouldRunInitialAutoTableFormatForBuffer(bool autoAlignEnabled, bool autoFitEnabled, bool alignInProgress, bool fitInProgress, bool activeEditor, bool alreadyHandled);
bool shouldQueueInitialAutoTableFormatForOpenedBuffer(bool autoAlignEnabled, bool autoFitEnabled, bool alreadyHandled);
}
