// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PanelSlot.h"
#include "UBoxShadowSlot.generated.h"

class SOverlay;

/**
 * Minimal content slot for UBoxShadow.
 *
 * UBoxShadow wraps its single child in an SOverlay whose top layer holds the
 * content; this slot only inserts the child widget into that layer.  Layout
 * (offset / spread / tint) is owned by the UBoxShadow widget itself, so the
 * slot exposes no layout properties of its own.
 */
UCLASS(MinimalAPI)
class UBoxShadowSlot : public UPanelSlot
{
	GENERATED_BODY()

public:
	/** Inserts the slot's content as the top (content) layer of the owning overlay. */
	ARCMARKUPUI_API void BuildSlot(TSharedRef<SOverlay> InOverlay);

	//~ Begin UPanelSlot interface
	ARCMARKUPUI_API virtual void SynchronizeProperties() override;
	//~ End UPanelSlot interface

	ARCMARKUPUI_API virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	/** The overlay the content was inserted into, kept so the child can be cleared. */
	TWeakPtr<SOverlay> Overlay;
};
