// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "RmlUi/Core/SystemInterface.h"

class FArcRmlViewChannel;

class FArcRmlSystemInterface final : public Rml::SystemInterface
{
public:
	FArcRmlSystemInterface();

	void SetActiveView(FString* InCursorTarget, FArcRmlViewChannel* InChannel);

	virtual double GetElapsedTime() override;
	virtual bool LogMessage(Rml::Log::Type Type, const Rml::String& Message) override;
	virtual void SetMouseCursor(const Rml::String& CursorName) override;
	virtual void SetClipboardText(const Rml::String& Text) override;
	virtual void GetClipboardText(Rml::String& Text) override;
	virtual void JoinPath(Rml::String& TranslatedPath, const Rml::String& DocumentPath, const Rml::String& Path) override;

private:
	double StartSeconds = 0.0;
	FString* CursorTarget = nullptr;
	FArcRmlViewChannel* Channel = nullptr;
};
