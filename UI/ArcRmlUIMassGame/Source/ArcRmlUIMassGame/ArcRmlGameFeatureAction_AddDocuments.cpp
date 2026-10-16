// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRmlGameFeatureAction_AddDocuments.h"

#include "Components/GameFrameworkComponentManager.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/DataValidation.h"
#include "Pawn/ArcMassPawnExtensionComponent.h"

#if ARC_WITH_RMLUI
#include "Host/ArcRmlViewportSubsystem.h"
#endif

#define LOCTEXT_NAMESPACE "ArcRmlUIMassGame"

void UArcRmlGameFeatureAction_AddDocuments::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	FPerContextData* ActiveData = ContextData.Find(Context);
	if (ActiveData == nullptr)
	{
		return;
	}

	CloseDocuments(*ActiveData);
	TArray<TSharedPtr<FComponentRequestHandle>> ComponentRequests = MoveTemp(ActiveData->ComponentRequests);
	ContextData.Remove(Context);
	ComponentRequests.Empty();
}

#if WITH_EDITOR
EDataValidationResult UArcRmlGameFeatureAction_AddDocuments::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context)
		, EDataValidationResult::Valid);

	for (int32 EntryIndex = 0; EntryIndex < Documents.Num(); ++EntryIndex)
	{
		if (!Documents[EntryIndex].Document.IsSet())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("DocumentNotSet"
					, "Document is not set at index {0} in Documents")
				, FText::AsNumber(EntryIndex)));
		}
	}

	return Result;
}
#endif

void UArcRmlGameFeatureAction_AddDocuments::AddToWorld(const FWorldContext& WorldContext
													   , const FGameFeatureStateChangeContext& ChangeContext)
{
#if ARC_WITH_RMLUI
	UWorld* World = WorldContext.World();
	UGameInstance* GameInstance = WorldContext.OwningGameInstance;
	if ((GameInstance == nullptr) || (World == nullptr) || !World->IsGameWorld())
	{
		return;
	}

	UGameFrameworkComponentManager* ComponentManager = UGameInstance::GetSubsystem<UGameFrameworkComponentManager>(GameInstance);
	if (ComponentManager == nullptr)
	{
		return;
	}

	FPerContextData& ActiveData = ContextData.FindOrAdd(ChangeContext);
	ActiveData.ComponentRequests.Add(ComponentManager->AddExtensionHandler(APawn::StaticClass()
		, UGameFrameworkComponentManager::FExtensionHandlerDelegate::CreateUObject(this
			, &ThisClass::HandlePawnExtension
			, ChangeContext)));
#endif
}

void UArcRmlGameFeatureAction_AddDocuments::CloseDocuments(FPerContextData& ActiveData) const
{
#if ARC_WITH_RMLUI
	if (ULocalPlayer* LocalPlayer = ActiveData.LocalPlayer.Get())
	{
		if (UArcRmlViewportSubsystem* Subsystem = LocalPlayer->GetSubsystem<UArcRmlViewportSubsystem>())
		{
			for (const FArcRmlDocumentEntry& Entry : Documents)
			{
				Subsystem->CloseDocument(Entry.Document);
			}
		}
	}
#endif
	ActiveData.Pawn.Reset();
	ActiveData.LocalPlayer.Reset();
}

void UArcRmlGameFeatureAction_AddDocuments::HandlePawnExtension(AActor* Actor
																, FName EventName
																, FGameFeatureStateChangeContext ChangeContext)
{
	if ((EventName == UGameFrameworkComponentManager::NAME_ExtensionRemoved)
		|| (EventName == UGameFrameworkComponentManager::NAME_ReceiverRemoved))
	{
		FPerContextData* ActiveData = ContextData.Find(ChangeContext);
		// A respawned local pawn can be recorded before the old pawn's removal arrives; other pawns never close the HUD.
		if ((ActiveData != nullptr) && (Actor != nullptr)
			&& ActiveData->Pawn.HasSameIndexAndSerialNumber(TWeakObjectPtr<AActor>(Actor)))
		{
			CloseDocuments(*ActiveData);
		}
	}
	else if (EventName == UGameFrameworkComponentManager::NAME_ExtensionAdded)
	{
		if (UArcMassPawnExtensionComponent* PawnExt = UArcMassPawnExtensionComponent::FindPawnExtensionComponent(Actor))
		{
			PawnExt->OnPawnReadyToInitialize_RegisterAndCall(
				UArcMassPawnExtensionComponent::FArcMassOnPawnReadyToInitialize::FDelegate::CreateUObject(this
					, &ThisClass::HandlePawnReady
					, ChangeContext));
		}
	}
	else if (EventName == UGameFrameworkComponentManager::NAME_GameActorReady)
	{
		if (UArcMassPawnExtensionComponent* PawnExt = UArcMassPawnExtensionComponent::FindPawnExtensionComponent(Actor))
		{
			HandlePawnReady(PawnExt
				, ChangeContext);
		}
	}
}

void UArcRmlGameFeatureAction_AddDocuments::HandlePawnReady(UArcMassPawnExtensionComponent* PawnExt
															, FGameFeatureStateChangeContext ChangeContext)
{
#if ARC_WITH_RMLUI
	FPerContextData* ActiveData = ContextData.Find(ChangeContext);
	if (ActiveData == nullptr)
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(PawnExt->GetOwner());
	if ((Pawn == nullptr) || !Pawn->IsLocallyControlled())
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(Pawn->GetController());
	if (PlayerController == nullptr)
	{
		return;
	}

	ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	if (LocalPlayer == nullptr)
	{
		return;
	}

	UArcRmlViewportSubsystem* Subsystem = LocalPlayer->GetSubsystem<UArcRmlViewportSubsystem>();
	if (Subsystem == nullptr)
	{
		return;
	}

	ActiveData->Pawn = Pawn;
	ActiveData->LocalPlayer = LocalPlayer;

	for (const FArcRmlDocumentEntry& Entry : Documents)
	{
		Subsystem->OpenDocument(Entry.Document);
	}
#endif
}

#undef LOCTEXT_NAMESPACE
