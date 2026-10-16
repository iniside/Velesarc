// Copyright Lukasz Baran. All Rights Reserved.

#include "GameFeatures/ArcMassGameFeatureAction_AddWidget.h"

#include "CommonUIExtensions.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFeaturesSubsystemSettings.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "ArcMassGameFeatures"

//////////////////////////////////////////////////////////////////////
// UArcMassGameFeatureAction_AddWidget

void UArcMassGameFeatureAction_AddWidget::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	FPerContextData* ActiveData = ContextData.Find(Context);
	if (ensure(ActiveData))
	{
		Reset(*ActiveData);
	}
}

#if WITH_EDITORONLY_DATA
void UArcMassGameFeatureAction_AddWidget::AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData)
{
	for (const FArcMassHUDElementEntry& Entry : Widgets)
	{
		AssetBundleData.AddBundleAsset(UGameFeaturesSubsystemSettings::LoadStateClient
			, Entry.WidgetClass.ToSoftObjectPath().GetAssetPath());
	}
}
#endif

#if WITH_EDITOR
EDataValidationResult UArcMassGameFeatureAction_AddWidget::IsDataValid(class FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context)
		, EDataValidationResult::Valid);

	{
		int32 EntryIndex = 0;
		for (const FArcMassHUDLayoutRequest& Entry : Layout)
		{
			if (Entry.LayoutClass.IsNull())
			{
				Result = EDataValidationResult::Invalid;
				Context.AddError(FText::Format(LOCTEXT("LayoutHasNullClass"
						, "Null WidgetClass at index {0} in Layout")
					, FText::AsNumber(EntryIndex)));
			}

			if (!Entry.LayerID.IsValid())
			{
				Result = EDataValidationResult::Invalid;
				Context.AddError(FText::Format(LOCTEXT("LayoutHasNoTag"
						, "LayerID is not set at index {0} in Widgets")
					, FText::AsNumber(EntryIndex)));
			}

			++EntryIndex;
		}
	}

	{
		int32 EntryIndex = 0;
		for (const FArcMassHUDElementEntry& Entry : Widgets)
		{
			if (Entry.WidgetClass.IsNull())
			{
				Result = EDataValidationResult::Invalid;
				Context.AddError(FText::Format(LOCTEXT("EntryHasNullClass"
						, "Null WidgetClass at index {0} in Widgets")
					, FText::AsNumber(EntryIndex)));
			}

			if (!Entry.SlotID.IsValid())
			{
				Result = EDataValidationResult::Invalid;
				Context.AddError(FText::Format(LOCTEXT("EntryHasNoTag"
						, "SlotID is not set at index {0} in Widgets")
					, FText::AsNumber(EntryIndex)));
			}
			++EntryIndex;
		}
	}

	return Result;
}
#endif

void UArcMassGameFeatureAction_AddWidget::AddToWorld(const FWorldContext& WorldContext
													  , const FGameFeatureStateChangeContext& ChangeContext)
{
	UWorld* World = WorldContext.World();
	UGameInstance* GameInstance = WorldContext.OwningGameInstance;
	FPerContextData& ActiveData = ContextData.FindOrAdd(ChangeContext);

	if ((GameInstance != nullptr) && (World != nullptr) && World->IsGameWorld())
	{
		if (UGameFrameworkComponentManager* ComponentManager = UGameInstance::GetSubsystem<UGameFrameworkComponentManager>(GameInstance))
		{
			TSoftClassPtr<AActor> HUDActorClass = AHUD::StaticClass();

			TSharedPtr<FComponentRequestHandle> ExtensionRequestHandle = ComponentManager->AddExtensionHandler(
				HUDActorClass
				, UGameFrameworkComponentManager::FExtensionHandlerDelegate::CreateUObject(this
					, &ThisClass::HandleActorExtension
					, ChangeContext));
			ActiveData.ComponentRequests.Add(ExtensionRequestHandle);
		}
	}
}

void UArcMassGameFeatureAction_AddWidget::Reset(FPerContextData& ActiveData)
{
	ActiveData.ComponentRequests.Empty();
	ActiveData.LayoutsAdded.Empty();

	for (FUIExtensionHandle& Handle : ActiveData.ExtensionHandles)
	{
		Handle.Unregister();
	}
	ActiveData.ExtensionHandles.Reset();
}

void UArcMassGameFeatureAction_AddWidget::HandleActorExtension(AActor* Actor
																, FName EventName
																, FGameFeatureStateChangeContext ChangeContext)
{
	FPerContextData& ActiveData = ContextData.FindOrAdd(ChangeContext);
	if ((EventName == UGameFrameworkComponentManager::NAME_ExtensionRemoved) || (
			EventName == UGameFrameworkComponentManager::NAME_ReceiverRemoved))
	{
		RemoveWidgets(Actor
			, ActiveData);
	}
	else if ((EventName == UGameFrameworkComponentManager::NAME_ExtensionAdded) || (
				 EventName == UGameFrameworkComponentManager::NAME_GameActorReady))
	{
		AddWidgets(Actor
			, ActiveData);
	}
}

void UArcMassGameFeatureAction_AddWidget::AddWidgets(AActor* Actor
													  , FPerContextData& ActiveData)
{
	AHUD* HUD = CastChecked<AHUD>(Actor);

	if (APlayerController* OwningPC = HUD->GetOwningPlayerController())
	{
		if (ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(OwningPC->Player))
		{
			for (const FArcMassHUDLayoutRequest& Entry : Layout)
			{
				if (TSubclassOf<UCommonActivatableWidget> ConcreteWidgetClass = Entry.LayoutClass.Get())
				{
					ActiveData.LayoutsAdded.Add(UCommonUIExtensions::PushContentToLayer_ForPlayer(LocalPlayer
						, Entry.LayerID
						, ConcreteWidgetClass));
				}
			}

			UUIExtensionSubsystem* ExtensionSubsystem = HUD->GetWorld()->GetSubsystem<UUIExtensionSubsystem>();
			for (const FArcMassHUDElementEntry& Entry : Widgets)
			{
				ActiveData.ExtensionHandles.Add(ExtensionSubsystem->RegisterExtensionAsWidgetForContext(Entry.SlotID
					, LocalPlayer
					, Entry.WidgetClass.Get()
					, -1));
			}
		}
	}
}

void UArcMassGameFeatureAction_AddWidget::RemoveWidgets(AActor* Actor
														 , FPerContextData& ActiveData)
{
	// Actor parameter intentionally unused; layouts/extensions track lifetime themselves.
	(void)Actor;

	for (TWeakObjectPtr<UCommonActivatableWidget>& AddedLayout : ActiveData.LayoutsAdded)
	{
		if (AddedLayout.IsValid())
		{
			AddedLayout->DeactivateWidget();
		}
	}
	ActiveData.LayoutsAdded.Reset();

	for (FUIExtensionHandle& Handle : ActiveData.ExtensionHandles)
	{
		Handle.Unregister();
	}
	ActiveData.ExtensionHandles.Reset();
}

#undef LOCTEXT_NAMESPACE
