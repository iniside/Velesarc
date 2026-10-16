// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcRmlDocumentRef.h"
#include "CommonActivatableWidget.h"
#include "MVVM/ArcRmlViewModelDeclarations.h"

#include "ArcRmlActivatableWidget.generated.h"

class SArcRmlViewport;
class UArcRmlViewportSubsystem;
class UArcRmlViewportWidget;

UCLASS(MinimalAPI, NotBlueprintable)
class UArcRmlActivatableWidget final : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	void InitDocument(UArcRmlViewportSubsystem& InOwner, const FArcRmlDocumentRef& InDocument, const FArcRmlDocumentMeta& InMeta,
		const TSharedRef<SArcRmlViewport>& InViewport);

	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
	virtual void NativeOnInitialized() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UArcRmlViewportWidget> ViewportWidget;

	TWeakObjectPtr<UArcRmlViewportSubsystem> Owner;
	TWeakPtr<SArcRmlViewport> Viewport;
	FArcRmlDocumentRef Document;
	FArcRmlDocumentMeta Meta;
};
