// Copyright Lukasz Baran. All Rights Reserved.

#include "Interfaces/ArcRmlSystemInterface.h"

#include "ArcRmlUILog.h"
#include "ArcRmlUIPerf.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/ArcRmlPaths.h"
#include "Interfaces/ArcRmlStrings.h"
#include "Thread/ArcRmlUIQueues.h"

FArcRmlSystemInterface::FArcRmlSystemInterface()
	: StartSeconds(FPlatformTime::Seconds())
{
}

void FArcRmlSystemInterface::SetActiveView(FString* InCursorTarget, FArcRmlViewChannel* InChannel)
{
	CursorTarget = InCursorTarget;
	Channel = InChannel;
}

double FArcRmlSystemInterface::GetElapsedTime()
{
#if !UE_BUILD_SHIPPING
	double FrozenSeconds = 0.0;
	if (ArcRmlUIPerf::GetClockOverride(FrozenSeconds))
	{
		return FrozenSeconds;
	}
#endif
	return FPlatformTime::Seconds() - StartSeconds;
}

bool FArcRmlSystemInterface::LogMessage(Rml::Log::Type Type, const Rml::String& Message)
{
	const FString Text = ArcRmlStrings::ToFString(Message);
	switch (Type)
	{
	case Rml::Log::LT_ERROR:
	case Rml::Log::LT_ASSERT:
		UE_LOGF(LogArcRmlUI, Error, "RmlUi: %ls", *Text);
		break;
	case Rml::Log::LT_WARNING:
		UE_LOGF(LogArcRmlUI, Warning, "RmlUi: %ls", *Text);
		break;
	case Rml::Log::LT_INFO:
		UE_LOGF(LogArcRmlUI, Log, "RmlUi: %ls", *Text);
		break;
	default:
		UE_LOGF(LogArcRmlUI, Verbose, "RmlUi: %ls", *Text);
		break;
	}
	return true;
}

void FArcRmlSystemInterface::SetMouseCursor(const Rml::String& CursorName)
{
	if (CursorTarget)
	{
		*CursorTarget = ArcRmlStrings::ToFString(CursorName);
	}
}

void FArcRmlSystemInterface::SetClipboardText(const Rml::String& Text)
{
	if (!Channel)
	{
		return;
	}

	FArcRmlOutboundEvent Event;
	Event.Type = EArcRmlOutboundType::ClipboardCopy;
	Event.Text = ArcRmlStrings::ToFString(Text);
	Channel->PushOutbound(MoveTemp(Event));
}

void FArcRmlSystemInterface::GetClipboardText(Rml::String& Text)
{
	FString Pasted;
	FPlatformApplicationMisc::ClipboardPaste(Pasted);
	Text = ArcRmlStrings::ToRmlString(Pasted);
}

void FArcRmlSystemInterface::JoinPath(Rml::String& TranslatedPath, const Rml::String& DocumentPath, const Rml::String& Path)
{
	TranslatedPath = ArcRmlStrings::ToRmlString(ArcRmlPaths::JoinPackagePath(ArcRmlStrings::ToFString(DocumentPath), ArcRmlStrings::ToFString(Path)));
}
