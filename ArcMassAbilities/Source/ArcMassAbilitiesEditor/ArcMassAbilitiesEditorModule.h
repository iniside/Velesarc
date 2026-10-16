// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "Templates/PimplPtr.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "Styling/SlateStyle.h"

class FArcMassAbilityTraceModule;
class FArcMassAbilityRewindDebuggerTrackCreator;
class FArcMassAbilityRewindDebuggerPlaybackExtension;
class FArcMassAbilityRewindDebuggerRecordingExtension;

class FArcMassAbilitiesEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TUniquePtr<FArcMassAbilityTraceModule>                            ArcMABTraceModule;
	TPimplPtr<FArcMassAbilityRewindDebuggerTrackCreator>              ArcMABTrackCreator;
	TUniquePtr<FArcMassAbilityRewindDebuggerPlaybackExtension>        ArcMABPlaybackExtension;
	TUniquePtr<FArcMassAbilityRewindDebuggerRecordingExtension>       ArcMABRecordingExtension;

	TSharedPtr<FSlateStyleSet> StyleSet;

	FDelegateHandle PresetToolbarStartupHandle;
};
