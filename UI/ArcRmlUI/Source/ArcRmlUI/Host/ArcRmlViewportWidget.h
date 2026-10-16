// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"

#include "ArcRmlViewportWidget.generated.h"

class SArcRmlViewport;
class SArcRmlViewportHost;

UCLASS(MinimalAPI, NotBlueprintable)
class UArcRmlViewportWidget final : public UWidget
{
	GENERATED_BODY()

public:
	void SetViewport(const TSharedPtr<SArcRmlViewport>& InViewport);
	bool HasHostedViewport() const;

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedPtr<SArcRmlViewport> Viewport;
	TSharedPtr<SArcRmlViewportHost> Host;
};
