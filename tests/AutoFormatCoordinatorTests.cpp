#include "AutoFormatCoordinatorTests.h"
#include "../src/AutoFormatCoordinator.h"

#include <iostream>

namespace
{
using namespace MarkdownTablePlugin;

void expect(int &failures, const char *name, bool value)
{
	if (!value)
	{
		++failures;
		std::cerr << name << " failed\n";
	}
}

void testBufferLifetime(int &failures)
{
	AutoFormatCoordinator state;
	state.queueInitial(10);
	expect(failures, "opening background buffer only queues work", state.initialPending(10) &&
		state.takeInitial(EditorView::Main, 10, false, true, true) == FormatMode::None);
	expect(failures, "empty buffer defers until loaded", state.takeInitial(EditorView::Main, 10, true, false, true) == FormatMode::None && state.initialPending(10));
	expect(failures, "selected text defers initial formatting", state.takeInitial(EditorView::Main, 10, true, true, false) == FormatMode::None && state.initialPending(10));
	expect(failures, "activation formats loaded buffer", state.takeInitial(EditorView::Main, 10, true, true, true) == FormatMode::Fit);
	state.queueInitial(10);
	expect(failures, "cloned buffer is initialized only once", !state.initialPending(10) &&
		state.takeInitial(EditorView::Secondary, 10, true, true, true) == FormatMode::None);
	state.forgetBuffer(10);
	state.queueInitial(10);
	expect(failures, "reopened buffer has a new lifetime", state.takeInitial(EditorView::Secondary, 10, true, true, true) == FormatMode::Fit);
}

void testViewTimers(int &failures)
{
	AutoFormatCoordinator state;
	state.rememberWidth(EditorView::Main, 10, 80);
	state.rememberWidth(EditorView::Secondary, 20, 40);
	state.scheduleResize(EditorView::Main, 10);
	expect(failures, "main view keeps its own width", state.takeResize(EditorView::Main, 10, true, 80, true) == FormatMode::None);
	state.scheduleResize(EditorView::Secondary, 20);
	expect(failures, "secondary view width change schedules fit", state.takeResize(EditorView::Secondary, 20, true, 80, true) == FormatMode::Fit);
	state.scheduleResize(EditorView::Main, 10);
	expect(failures, "timer cannot format newly activated buffer", state.takeResize(EditorView::Main, 30, true, 100, true) == FormatMode::None);
	state.scheduleResize(EditorView::Main, 30);
	expect(failures, "new buffer does not inherit old width", state.takeResize(EditorView::Main, 30, true, 80, true) == FormatMode::Fit);
	expect(failures, "timer is consumed once", state.takeResize(EditorView::Main, 30, true, 100, true) == FormatMode::None);
	state.scheduleResize(EditorView::Main, 30);
	expect(failures, "inactive editor timer does not format active view", state.takeResize(EditorView::Main, 30, false, 120, true) == FormatMode::None);
	state.scheduleResize(EditorView::Secondary, 20);
	expect(failures, "resize preserves selected text", state.takeResize(EditorView::Secondary, 20, true, 100, false) == FormatMode::None);
	state.scheduleResize(EditorView::Secondary, 20);
	state.cancelResize(EditorView::Secondary);
	expect(failures, "cancelled native timer has no work", state.takeResize(EditorView::Secondary, 20, true, 100, true) == FormatMode::None);
	state.scheduleResize(EditorView::Main, 30);
	state.forgetBuffer(30);
	expect(failures, "closed buffer invalidates resize", state.takeResize(EditorView::Main, 30, true, 100, true) == FormatMode::None);
}

void testReentrantEdits(int &failures)
{
	AutoFormatCoordinator state;
	{
		AutoFormatOperation operation(state, FormatMode::Fit);
		expect(failures, "fit operation starts", static_cast<bool>(operation));
		AutoFormatOperation nested(state, FormatMode::Align);
		expect(failures, "nested align cannot enter fit", !nested);
		state.beginPluginEdit(10);
		state.beginPluginEdit(10);
		expect(failures, "nested plugin edit blocks other view notifications", state.onContentUpdate(EditorView::Secondary, 20, true, true, true) == FormatMode::None);
		state.endPluginEdit();
		expect(failures, "inner edit exit keeps outer edit blocked", state.onGlobalModified(EditorView::Main, 10, true, true) == FormatMode::None);
		state.endPluginEdit();
		expect(failures, "operation blocks until formatter unwinds", state.onContentUpdate(EditorView::Secondary, 20, true, true, true) == FormatMode::None);
	}
	expect(failures, "plugin edit budget cannot suppress another buffer", state.onContentUpdate(EditorView::Secondary, 20, true, true, true) == FormatMode::Fit);
	expect(failures, "plugin update burst first notification suppressed", state.onContentUpdate(EditorView::Main, 10, true, true, true) == FormatMode::None);
	expect(failures, "cloned view shares buffer edit budget", state.onContentUpdate(EditorView::Secondary, 10, true, true, true) == FormatMode::None);
	expect(failures, "user edits resume after bounded suppression", state.onContentUpdate(EditorView::Main, 10, true, true, true) == FormatMode::Fit);
	expect(failures, "global notification has independent suppression", state.onGlobalModified(EditorView::Main, 10, true, true) == FormatMode::None);
	state.onGlobalModified(EditorView::Main, 10, true, true);
	expect(failures, "global user operation resumes", state.onGlobalModified(EditorView::Main, 10, true, true) == FormatMode::Fit);
	state.scheduleResize(EditorView::Main, 10);
	expect(failures, "plugin resize burst suppressed", state.takeResize(EditorView::Main, 10, true, 80, true) == FormatMode::None);
	state.scheduleResize(EditorView::Main, 10);
	expect(failures, "later user resize resumes", state.takeResize(EditorView::Main, 10, true, 100, true) == FormatMode::Fit);
}

void testModeAndSelectionTransitions(int &failures)
{
	AutoFormatCoordinator state;
	state.setAutoAlignEnabled(false);
	expect(failures, "fit mode retains align invariant", state.autoAlignEnabled());
	state.scheduleResize(EditorView::Main, 10);
	state.setAutoFitEnabled(false);
	expect(failures, "disabling fit invalidates pending timer", state.takeResize(EditorView::Main, 10, true, 80, true) == FormatMode::None);
	expect(failures, "align-only mode requests align", state.onContentUpdate(EditorView::Main, 10, true, true, true) == FormatMode::Align);
	expect(failures, "caret-only updates do not format", state.onContentUpdate(EditorView::Main, 10, true, false, true) == FormatMode::None);
	expect(failures, "inactive updates do not format", state.onContentUpdate(EditorView::Secondary, 20, false, true, true) == FormatMode::None);
	expect(failures, "content update retains selection", state.onContentUpdate(EditorView::Main, 10, true, true, false) == FormatMode::None);
	expect(failures, "global update retains selection", state.onGlobalModified(EditorView::Main, 10, true, false) == FormatMode::None);
	state.setAutoAlignEnabled(false);
	state.queueInitial(20);
	expect(failures, "disabled modes neither queue nor format", !state.initialPending(20) && state.onContentUpdate(EditorView::Main, 10, true, true, true) == FormatMode::None);
	state.setAutoFitEnabled(true);
	expect(failures, "enabling fit restores align mode", state.autoFitEnabled() && state.autoAlignEnabled());
}
}

int runAutoFormatCoordinatorTests()
{
	int failures = 0;
	testBufferLifetime(failures);
	testViewTimers(failures);
	testReentrantEdits(failures);
	testModeAndSelectionTransitions(failures);
	return failures;
}
