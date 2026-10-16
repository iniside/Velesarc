// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "GameFeatures/ArcMassGameFeatureAction_AddWidget.h"
#include "GameFeatures/ArcMassGameFeatureAction_WorldActionBase.h"

#include "ArcMassGameFeatureAction_PushWidgetToLayer.generated.h"

class AActor;
class APawn;
class UArcMassPawnExtensionComponent;
struct FComponentRequestHandle;
struct FStreamableHandle;
struct FWorldContext;

/**
 * GameFeatureAction that pushes CommonActivatableWidgets to UI layers once the
 * pawn reaches the DataInitialized init state (via ArcMassPawnExtensionComponent).
 *
 * Handles only the Layout half of UArcMassGameFeatureAction_AddWidget; rebinds the
 * trigger from AHUD/NAME_GameActorReady to pawn-ready via
 * UArcMassPawnExtensionComponent::OnPawnReadyToInitialize_RegisterAndCall.
 */
UCLASS(MinimalAPI, meta = (DisplayName = "Push Widget To Layer (Mass)"))
class UArcMassGameFeatureAction_PushWidgetToLayer final : public UArcMassGameFeatureAction_WorldActionBase
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
	// Layout widgets to push to their respective UI layers when the pawn is ready.
	UPROPERTY(EditAnywhere, Category = UI, meta = (TitleProperty = "{LayerID} -> {LayoutClass}"))
	TArray<FArcMassHUDLayoutRequest> Layout;

private:
	struct FPerContextData
	{
		TArray<TSharedPtr<FComponentRequestHandle>> ComponentRequests;
		TArray<TSharedPtr<FStreamableHandle>> StreamingHandles;
		TArray<TWeakObjectPtr<UCommonActivatableWidget>> LayoutsAdded;
		// Bumped by Reset() so in-flight async-load callbacks can detect a teardown and skip a stale push.
		uint32 Generation = 0;
	};

	TMap<FGameFeatureStateChangeContext, FPerContextData> ContextData;

	//~ Begin UArcMassGameFeatureAction_WorldActionBase interface
	virtual void AddToWorld(const FWorldContext& WorldContext
							, const FGameFeatureStateChangeContext& ChangeContext) override;
	//~ End UArcMassGameFeatureAction_WorldActionBase interface

	void Reset(FPerContextData& ActiveData);

	void HandlePawnExtension(AActor* Actor
							 , FName EventName
							 , FGameFeatureStateChangeContext ChangeContext);

	void OnPawnReadyToInitialize(UArcMassPawnExtensionComponent* PawnExt
								 , FGameFeatureStateChangeContext ChangeContext);
};
