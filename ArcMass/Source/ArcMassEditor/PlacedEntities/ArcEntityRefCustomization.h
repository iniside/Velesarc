// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "EdMode.h"

class IPropertyHandle;
class AArcPlacedEntityPartitionActor;
struct FArcPlacedEntityEntry;

DECLARE_DELEGATE_TwoParams(FOnEntityPicked, const FGuid& /*PickedGuid*/, AArcPlacedEntityPartitionActor* /*SourcePartitionActor*/);

/** Minimal editor mode for one-shot ISM instance picking. */
class FArcEntityPickerEdMode : public FEdMode
{
public:
	static const FEditorModeID ModeId;

	FOnEntityPicked OnPicked;

	virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;
	virtual bool IsCompatibleWith(FEditorModeID OtherModeID) const override { return true; }
	virtual bool UsesToolkits() const override { return false; }
};

/** Property type customization for FArcEntityRef — eyedropper ISM instance picker. */
class FArcEntityRefCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
	~FArcEntityRefCustomization();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	FReply OnPickClicked();
	FReply OnClearClicked();
	FText GetGuidDisplayText() const;
	FReply OnSelectClicked();
	bool IsSelectEnabled() const;

	void OnEntityPicked(const FGuid& PickedGuid, AArcPlacedEntityPartitionActor* SourceActor);

	TSharedPtr<IPropertyHandle> StructPropertyHandle;
	TSharedPtr<IPropertyHandle> TargetGuidHandle;
	TSharedPtr<IPropertyHandle> SourcePartitionActorHandle;
};
