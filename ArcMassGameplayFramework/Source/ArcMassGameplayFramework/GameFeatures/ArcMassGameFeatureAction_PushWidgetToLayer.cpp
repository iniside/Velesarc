// Copyright Lukasz Baran. All Rights Reserved.

#include "GameFeatures/ArcMassGameFeatureAction_PushWidgetToLayer.h"

#include "CommonUIExtensions.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFeaturesSubsystemSettings.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/DataValidation.h"
#include "Pawn/ArcMassPawnExtensionComponent.h"

#define LOCTEXT_NAMESPACE "ArcMassGameFeatures"

//////////////////////////////////////////////////////////////////////
// UArcMassGameFeatureAction_PushWidgetToLayer

void UArcMassGameFeatureAction_PushWidgetToLayer::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	FPerContextData* ActiveData = ContextData.Find(Context);
	if (ensure(ActiveData))
	{
		Reset(*ActiveData);
	}
}

#if WITH_EDITORONLY_DATA
void UArcMassGameFeatureAction_PushWidgetToLayer::AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData)
{
	for (const FArcMassHUDLayoutRequest& Entry : Layout)
	{
		AssetBundleData.AddBundleAsset(UGameFeaturesSubsystemSettings::LoadStateClient
			, Entry.LayoutClass.ToSoftObjectPath().GetAssetPath());
	}
}
#endif

#if WITH_EDITOR
EDataValidationResult UArcMassGameFeatureAction_PushWidgetToLayer::IsDataValid(class FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context)
		, EDataValidationResult::Valid);

	int32 EntryIndex = 0;
	for (const FArcMassHUDLayoutRequest& Entry : Layout)
	{
		if (Entry.LayoutClass.IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("LayoutHasNullClass"
					, "Null LayoutClass at index {0} in Layout")
				, FText::AsNumber(EntryIndex)));
		}

		if (!Entry.LayerID.IsValid())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("LayoutHasNoTag"
					, "LayerID is not set at index {0} in Layout")
				, FText::AsNumber(EntryIndex)));
		}

		++EntryIndex;
	}

	return Result;
}
#endif

void UArcMassGameFeatureAction_PushWidgetToLayer::AddToWorld(const FWorldContext& WorldContext
															  , const FGameFeatureStateChangeContext& ChangeContext)
{
	UWorld* World = WorldContext.World();
	UGameInstance* GameInstance = WorldContext.OwningGameInstance;
	FPerContextData& ActiveData = ContextData.FindOrAdd(ChangeContext);

	if ((GameInstance != nullptr) && (World != nullptr) && World->IsGameWorld())
	{
		if (UGameFrameworkComponentManager* ComponentManager = UGameInstance::GetSubsystem<UGameFrameworkComponentManager>(GameInstance))
		{
			TSharedPtr<FComponentRequestHandle> ExtensionRequestHandle = ComponentManager->AddExtensionHandler(
				APawn::StaticClass()
				, UGameFrameworkComponentManager::FExtensionHandlerDelegate::CreateUObject(this
					, &ThisClass::HandlePawnExtension
					, ChangeContext));
			ActiveData.ComponentRequests.Add(ExtensionRequestHandle);
		}
	}
}

void UArcMassGameFeatureAction_PushWidgetToLayer::Reset(FPerContextData& ActiveData)
{
	ActiveData.ComponentRequests.Empty();

	for (const TSharedPtr<FStreamableHandle>& Handle : ActiveData.StreamingHandles)
	{
		if (Handle.IsValid())
		{
			Handle->CancelHandle();
		}
	}
	ActiveData.StreamingHandles.Empty();

	// Invalidate any in-flight async-load callbacks captured before this teardown.
	++ActiveData.Generation;

	for (TWeakObjectPtr<UCommonActivatableWidget>& AddedLayout : ActiveData.LayoutsAdded)
	{
		if (AddedLayout.IsValid())
		{
			AddedLayout->DeactivateWidget();
		}
	}
	ActiveData.LayoutsAdded.Reset();
}

void UArcMassGameFeatureAction_PushWidgetToLayer::HandlePawnExtension(AActor* Actor
																	   , FName EventName
																	   , FGameFeatureStateChangeContext ChangeContext)
{
	if ((EventName == UGameFrameworkComponentManager::NAME_ExtensionRemoved) || (
			EventName == UGameFrameworkComponentManager::NAME_ReceiverRemoved))
	{
		FPerContextData* ActiveData = ContextData.Find(ChangeContext);
		if (ActiveData)
		{
			Reset(*ActiveData);
		}
	}
	else if ((EventName == UGameFrameworkComponentManager::NAME_ExtensionAdded) || (
				 EventName == UGameFrameworkComponentManager::NAME_GameActorReady))
	{
		UArcMassPawnExtensionComponent* PawnExt = UArcMassPawnExtensionComponent::FindPawnExtensionComponent(Actor);
		if (PawnExt)
		{
			PawnExt->OnPawnReadyToInitialize_RegisterAndCall(
				UArcMassPawnExtensionComponent::FArcMassOnPawnReadyToInitialize::FDelegate::CreateUObject(
					this
					, &ThisClass::OnPawnReadyToInitialize
					, ChangeContext));
		}
	}
}

void UArcMassGameFeatureAction_PushWidgetToLayer::OnPawnReadyToInitialize(UArcMassPawnExtensionComponent* PawnExt
																		   , FGameFeatureStateChangeContext ChangeContext)
{
	FPerContextData& ActiveData = ContextData.FindOrAdd(ChangeContext);

	APawn* Pawn = CastChecked<APawn>(PawnExt->GetOwner());
	APlayerController* PlayerController = Cast<APlayerController>(Pawn->GetController());
	if (PlayerController == nullptr)
	{
		return;
	}

	ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(PlayerController->Player);
	if (LocalPlayer == nullptr)
	{
		return;
	}

	const uint32 RequestGeneration = ActiveData.Generation;
	const TWeakObjectPtr<ULocalPlayer> WeakLocalPlayer(LocalPlayer);

	for (const FArcMassHUDLayoutRequest& Entry : Layout)
	{
		if (Entry.LayoutClass.IsNull())
		{
			continue;
		}

		const TSoftClassPtr<UCommonActivatableWidget> SoftClass = Entry.LayoutClass;
		const FGameplayTag LayerID = Entry.LayerID;

		TSharedPtr<FStreamableHandle> StreamingHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			SoftClass.ToSoftObjectPath()
			, FStreamableDelegate::CreateWeakLambda(this
				, [this, ChangeContext, SoftClass, LayerID, WeakLocalPlayer, RequestGeneration]()
				{
					FPerContextData* CtxData = ContextData.Find(ChangeContext);
					if (CtxData == nullptr)
					{
						return;
					}

					// Late-callback guard: if Reset() ran (teardown / pawn re-ready) the generation
					// no longer matches and we must not push a widget into a torn-down context.
					if (CtxData->Generation != RequestGeneration)
					{
						return;
					}

					ULocalPlayer* LocalPlayerPtr = WeakLocalPlayer.Get();
					if (LocalPlayerPtr == nullptr)
					{
						return;
					}

					TSubclassOf<UCommonActivatableWidget> ConcreteClass = SoftClass.Get();
					if (ConcreteClass == nullptr)
					{
						return;
					}

					CtxData->LayoutsAdded.Add(UCommonUIExtensions::PushContentToLayer_ForPlayer(LocalPlayerPtr
						, LayerID
						, ConcreteClass));
				}));
		ActiveData.StreamingHandles.Add(StreamingHandle);
	}
}

#undef LOCTEXT_NAMESPACE
