#include "AutoFormatCoordinator.h"

#include <algorithm>

namespace MarkdownTablePlugin
{
void AutoFormatCoordinator::setAutoFitEnabled(bool enabled)
{
	autoFit_ = enabled;
	if (enabled)
		autoAlign_ = true;
	else
		for (ViewState &view : views_)
		{
			view.lastColumns = 0;
			view.resizePending = false;
		}
}

void AutoFormatCoordinator::setAutoAlignEnabled(bool enabled)
{
	if (!autoFit_ || enabled)
		autoAlign_ = enabled;
}

bool AutoFormatCoordinator::beginOperation(FormatMode mode)
{
	if (mode == FormatMode::None || operation_ != FormatMode::None || pluginEditDepth_ > 0)
		return false;
	operation_ = mode;
	return true;
}

void AutoFormatCoordinator::endOperation()
{
	operation_ = FormatMode::None;
}

void AutoFormatCoordinator::beginPluginEdit(BufferId buffer)
{
	++pluginEditDepth_;
	BufferState &state = buffers_[buffer];
	state.updateSkips = (std::max)(state.updateSkips, 2);
	state.globalSkips = (std::max)(state.globalSkips, 2);
	state.resizeSkips = (std::max)(state.resizeSkips, 1);
	for (ViewState &view : views_)
		view.resizePending = false;
}

void AutoFormatCoordinator::endPluginEdit()
{
	if (pluginEditDepth_ > 0)
		--pluginEditDepth_;
	for (ViewState &view : views_)
		view.resizePending = false;
}

AutoFormatCoordinator::ViewState &AutoFormatCoordinator::viewState(EditorView view)
{
	return views_[static_cast<std::size_t>(view)];
}

void AutoFormatCoordinator::bindView(EditorView view, BufferId buffer)
{
	ViewState &state = viewState(view);
	if (state.buffer != buffer)
	{
		state = ViewState();
		state.buffer = buffer;
	}
}

void AutoFormatCoordinator::rememberWidth(EditorView view, BufferId buffer, std::size_t columns)
{
	bindView(view, buffer);
	viewState(view).lastColumns = columns;
}

bool AutoFormatCoordinator::scheduleResize(EditorView view, BufferId buffer)
{
	if (!autoFit_ || !canFormat())
		return false;
	bindView(view, buffer);
	viewState(view).resizePending = true;
	return true;
}

void AutoFormatCoordinator::cancelResize(EditorView view)
{
	viewState(view).resizePending = false;
}

bool AutoFormatCoordinator::consumeEditNotification(BufferId buffer, Notification notification)
{
	if (pluginEditDepth_ > 0)
		return true;
	const auto found = buffers_.find(buffer);
	if (found == buffers_.end())
		return false;
	BufferState &state = found->second;
	int &skips = notification == Notification::Update ? state.updateSkips :
		(notification == Notification::Global ? state.globalSkips : state.resizeSkips);
	if (skips == 0)
		return false;
	--skips;
	return true;
}

bool AutoFormatCoordinator::canFormat() const
{
	return (autoFit_ || autoAlign_) && operation_ == FormatMode::None && pluginEditDepth_ == 0;
}

FormatMode AutoFormatCoordinator::requestedMode() const
{
	return autoFit_ ? FormatMode::Fit : FormatMode::Align;
}

FormatMode AutoFormatCoordinator::takeResize(EditorView view, BufferId buffer, bool active, std::size_t columns, bool emptySelection)
{
	ViewState &state = viewState(view);
	const bool pending = state.resizePending && state.buffer == buffer;
	state.resizePending = false;
	if (!active)
		return FormatMode::None;
	bindView(view, buffer);
	if (!pending || !autoFit_ || !canFormat() || consumeEditNotification(buffer, Notification::Resize) || !emptySelection)
		return FormatMode::None;
	const std::size_t previousColumns = state.lastColumns;
	state.lastColumns = columns;
	return shouldRunFitToWindowAfterResize(true, false, true, previousColumns, columns) ? FormatMode::Fit : FormatMode::None;
}

FormatMode AutoFormatCoordinator::onContentUpdate(EditorView view, BufferId buffer, bool active, bool contentUpdated, bool emptySelection)
{
	if (!active)
		return FormatMode::None;
	bindView(view, buffer);
	if (!canFormat() || !contentUpdated || consumeEditNotification(buffer, Notification::Update) || !emptySelection)
		return FormatMode::None;
	return requestedMode();
}

FormatMode AutoFormatCoordinator::onGlobalModified(EditorView view, BufferId buffer, bool active, bool emptySelection)
{
	if (!active)
		return FormatMode::None;
	bindView(view, buffer);
	if (!canFormat() || consumeEditNotification(buffer, Notification::Global) || !emptySelection)
		return FormatMode::None;
	return requestedMode();
}

void AutoFormatCoordinator::queueInitial(BufferId buffer)
{
	if (buffer == 0 || (!autoFit_ && !autoAlign_))
		return;
	BufferState &state = buffers_[buffer];
	if (state.initial == InitialState::Unseen)
		state.initial = InitialState::Pending;
}

bool AutoFormatCoordinator::initialPending(BufferId buffer) const
{
	const auto found = buffers_.find(buffer);
	return found != buffers_.end() && found->second.initial == InitialState::Pending;
}

FormatMode AutoFormatCoordinator::takeInitial(EditorView view, BufferId buffer, bool active, bool hasText, bool emptySelection)
{
	if (!active)
		return FormatMode::None;
	bindView(view, buffer);
	if (!canFormat() || !hasText || !emptySelection || !initialPending(buffer))
		return FormatMode::None;
	// Mark before entering the editor: synchronous notifications must not reopen it.
	buffers_[buffer].initial = InitialState::Handled;
	return requestedMode();
}

void AutoFormatCoordinator::forgetBuffer(BufferId buffer)
{
	buffers_.erase(buffer);
	for (ViewState &view : views_)
		if (view.buffer == buffer)
			view = ViewState();
}

bool shouldRunFitToWindowAfterResize(bool enabled, bool inProgress, bool activeEditor, std::size_t previousColumns, std::size_t currentColumns)
{
	return enabled && !inProgress && activeEditor && currentColumns != 0 && (previousColumns == 0 || previousColumns != currentColumns);
}

bool shouldRunInitialFitWhenTogglingAutoFitTable(bool currentlyEnabled)
{
	return !currentlyEnabled;
}

bool shouldRunAutoTableFormatAfterUpdate(bool autoAlignEnabled, bool autoFitEnabled, bool alignInProgress, bool fitInProgress, bool pluginEditInProgress, bool activeEditor, bool contentUpdated)
{
	return (autoAlignEnabled || autoFitEnabled) && !alignInProgress && !fitInProgress && !pluginEditInProgress && activeEditor && contentUpdated;
}

bool shouldRunAutoFitAfterZoom(bool autoFitEnabled, bool fitInProgress, bool activeEditor)
{
	(void)autoFitEnabled;
	(void)fitInProgress;
	(void)activeEditor;
	return false;
}

bool shouldScheduleFitToWindowAfterZoom(bool autoFitEnabled, bool fitInProgress, bool activeEditor)
{
	return autoFitEnabled && !fitInProgress && activeEditor;
}

bool shouldRunAutoTableFormatAfterGlobalModified(bool autoAlignEnabled, bool autoFitEnabled, bool alignInProgress, bool fitInProgress, bool pluginEditInProgress)
{
	return (autoAlignEnabled || autoFitEnabled) && !alignInProgress && !fitInProgress && !pluginEditInProgress;
}

bool shouldRunInitialAlignWhenTogglingAutoAlignTable(bool currentlyEnabled)
{
	return !currentlyEnabled;
}

bool shouldRunInitialAutoTableFormatForBuffer(bool autoAlignEnabled, bool autoFitEnabled, bool alignInProgress, bool fitInProgress, bool activeEditor, bool alreadyHandled)
{
	return (autoAlignEnabled || autoFitEnabled) && !alignInProgress && !fitInProgress && activeEditor && !alreadyHandled;
}

bool shouldQueueInitialAutoTableFormatForOpenedBuffer(bool autoAlignEnabled, bool autoFitEnabled, bool alreadyHandled)
{
	return (autoAlignEnabled || autoFitEnabled) && !alreadyHandled;
}
}
