// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"
#include "GameFeatures/ArcMassGameFeatureAction_WorldActionBase.h"
#include "GameplayTagContainer.h"
#include "UIExtensionSystem.h"

#include "ArcMassGameFeatureAction_AddWidget.generated.h"

struct FComponentRequestHandle;
struct FWorldContext;

USTRUCT()
struct FArcMassHUDLayoutRequest
{
	GENERATED_BODY()

	// The layout widget to spawn
	UPROPERTY(EditAnywhere, Category = UI, meta = (AssetBundles = "Client"))
	TSoftClassPtr<UCommonActivatableWidget> LayoutClass;

	// The layer to insert the widget in
	UPROPERTY(EditAnywhere, Category = UI, meta = (Categories = "UI.Layer"))
	FGameplayTag LayerID;
};

USTRUCT()
struct FArcMassHUDElementEntry
{
	GENERATED_BODY()

	// The widget to spawn
	UPROPERTY(EditAnywhere, Category = UI, meta = (AssetBundles = "Client"))
	TSoftClassPtr<UUserWidget> WidgetClass;

	// The slot ID where we should place this widget
	UPROPERTY(EditAnywhere, Category = UI, meta = (Categories = "UI"))
	FGameplayTag SlotID;
};

//////////////////////////////////////////////////////////////////////
// UArcMassGameFeatureAction_AddWidget

/**
 * GameFeatureAction responsible for adding HUD layouts and widgets.
 *
 * Mirror of ArcCore's UGameFeatureAction_AddWidgets but decoupled from ArcCore;
 * targets the engine AHUD class so any HUD subclass can be used.
 */
UCLASS(MinimalAPI, meta = (DisplayName = "Add Widgets (Mass)"))
class UArcMassGameFeatureAction_AddWidget final : public UArcMassGameFeatureAction_WorldActionBase
{
	GENERATED_BODY()

public:
	//~ Begin UGameFeatureAction interface
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
#if WITH_EDITORONLY_DATA
	virtual void AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData) override;
#endif
	//~ End UGameFeatureAction interface

	//~ Begin UObject interface
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	//~ End UObject interface

private:
	// Layout to add to the HUD
	UPROPERTY(EditAnywhere, Category = UI, meta = (TitleProperty = "{LayerID} -> {LayoutClass}"))
	TArray<FArcMassHUDLayoutRequest> Layout;

	// Widgets to add to the HUD
	UPROPERTY(EditAnywhere, Category = UI, meta = (TitleProperty = "{SlotID} -> {WidgetClass}"))
	TArray<FArcMassHUDElementEntry> Widgets;

private:
	struct FPerContextData
	{
		TArray<TSharedPtr<FComponentRequestHandle>> ComponentRequests;
		TArray<TWeakObjectPtr<UCommonActivatableWidget>> LayoutsAdded;
		TArray<FUIExtensionHandle> ExtensionHandles;
	};

	TMap<FGameFeatureStateChangeContext, FPerContextData> ContextData;

	//~ Begin UArcMassGameFeatureAction_WorldActionBase interface
	virtual void AddToWorld(const FWorldContext& WorldContext
							, const FGameFeatureStateChangeContext& ChangeContext) override;
	//~ End UArcMassGameFeatureAction_WorldActionBase interface

	void Reset(FPerContextData& ActiveData);

	void HandleActorExtension(AActor* Actor
							  , FName EventName
							  , FGameFeatureStateChangeContext ChangeContext);

	void AddWidgets(AActor* Actor
					, FPerContextData& ActiveData);

	void RemoveWidgets(AActor* Actor
					   , FPerContextData& ActiveData);
};
