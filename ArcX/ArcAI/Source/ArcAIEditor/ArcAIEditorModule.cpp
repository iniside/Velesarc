/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "ArcAIEditorModule.h"

#include "ArcEditorToolsModule.h"
#include "Debugger/ArcTQSRewindDebuggerTrack.h"
#include "Debugger/ArcTQSRewindDebuggerExtensions.h"
#include "Debugger/ArcTQSTraceModule.h"
#include "Features/IModularFeatures.h"
#include "TraceServices/ModuleService.h"

void FArcAIEditorModule::StartupModule()
{
	// Register TQS trace module with TraceServices
	TQSTraceModule = MakeUnique<FArcTQSTraceModule>();
	IModularFeatures::Get().RegisterModularFeature(TraceServices::ModuleFeatureName, TQSTraceModule.Get());

	// Register Rewind Debugger track creator
	//TQSTrackCreator = MakeUnique<FArcTQSRewindDebuggerTrackCreator>();
	//IModularFeatures::Get().RegisterModularFeature(
	//	RewindDebugger::IRewindDebuggerTrackCreator::ModularFeatureName,
	//	TQSTrackCreator.Get());
	
	// Register Rewind Debugger playback extension
	TQSPlaybackExtension = MakeUnique<FArcTQSRewindDebuggerPlaybackExtension>();
	IModularFeatures::Get().RegisterModularFeature(
		IRewindDebuggerExtension::ModularFeatureName,
		TQSPlaybackExtension.Get());
	
	TQSTrackCreator = MakePimpl<FArcTQSRewindDebuggerTrackCreator>();
	IModularFeatures::Get().RegisterModularFeature(
	RewindDebugger::IRewindDebuggerTrackCreator::ModularFeatureName,
		TQSTrackCreator.Get());
	
	// Register Rewind Debugger recording extension (channel toggle)
	TQSRecordingExtension = MakeUnique<FArcTQSRewindDebuggerRecordingExtension>();
	IModularFeatures::Get().RegisterModularFeature(
		IRewindDebuggerRuntimeExtension::ModularFeatureName,
		TQSRecordingExtension.Get());
}

void FArcAIEditorModule::ShutdownModule()
{
	if (TQSRecordingExtension)
	{
		IModularFeatures::Get().UnregisterModularFeature(
			IRewindDebuggerRuntimeExtension::ModularFeatureName,
			TQSRecordingExtension.Get());
	}

	if (TQSPlaybackExtension)
	{
		IModularFeatures::Get().UnregisterModularFeature(
			IRewindDebuggerExtension::ModularFeatureName,
			TQSPlaybackExtension.Get());
	}

	if (TQSTrackCreator)
	{
		IModularFeatures::Get().UnregisterModularFeature(
			RewindDebugger::IRewindDebuggerTrackCreator::ModularFeatureName,
			TQSTrackCreator.Get());
	}

	if (TQSTraceModule)
	{
		IModularFeatures::Get().UnregisterModularFeature(
			TraceServices::ModuleFeatureName,
			TQSTraceModule.Get());
	}
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FArcAIEditorModule, ArcAIEditor)