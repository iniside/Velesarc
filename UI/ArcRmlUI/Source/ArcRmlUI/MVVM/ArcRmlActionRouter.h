// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "RmlUi/Core/EventListener.h"

class FArcRmlViewChannel;

namespace ArcRmlEmbedAttributes
{
	inline const Rml::String Container = "arc-embed";
	inline const Rml::String Wrapper = "arc-embedded";
}

class FArcRmlActionRouter final : public Rml::EventListener
{
public:
	explicit FArcRmlActionRouter(const TSharedRef<FArcRmlViewChannel, ESPMode::ThreadSafe>& InChannel);

	virtual void ProcessEvent(Rml::Event& Event) override;

private:
	TSharedRef<FArcRmlViewChannel, ESPMode::ThreadSafe> Channel;
};
