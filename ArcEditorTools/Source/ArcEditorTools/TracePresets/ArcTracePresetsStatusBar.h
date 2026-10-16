// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Adds a "Trace Presets" combo button to the level editor status bar, next to the Insights menu. The presets come
 * from FTraceAuxiliary: the engine's fixed presets plus the [Trace.ChannelPresets] section of the engine ini, so the
 * same entries also work with -trace= and the Trace.Enable console command.
 */
namespace ArcTracePresetsStatusBar
{
	/** Queue the status bar extension (call from FArcEditorToolsModule::StartupModule). */
	void Register();

	/** Drop the status bar extension (call from FArcEditorToolsModule::ShutdownModule). */
	void Unregister();
}
