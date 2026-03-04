// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPlayerPersistenceObserver.h"

#include "ArcMassPlayerPersistence.h"
#include "ArcPlayerPersistenceSubsystem.h"
#include "MassActorSubsystem.h"
#include "MassExecutionContext.h"
#include "ArcMass/Persistence/ArcMassFragmentSerializer.h"
#include "ArcMass/Persistence/ArcMassPersistence.h"
#include "Serialization/ArcJsonLoadArchive.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcMassPlayerPersistenceObserver, Log, All);

UArcMassPlayerPersistenceObserver::UArcMassPlayerPersistenceObserver()
	: EntityQuery{*this}
{
	ObservedType = FArcMassPlayerPersistenceTag::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Add;
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UArcMassPlayerPersistenceObserver::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FArcMassPlayerPersistenceTag>(
		EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FMassActorFragment>(
		EMassFragmentAccess::ReadOnly);
}

void UArcMassPlayerPersistenceObserver::Execute(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return;
	}

	UArcPlayerPersistenceSubsystem* PlayerPersistence =
		GI->GetSubsystem<UArcPlayerPersistenceSubsystem>();
	if (!PlayerPersistence)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, World, PlayerPersistence](FMassExecutionContext& Ctx)
		{
			const auto ActorFragments =
				Ctx.GetFragmentView<FMassActorFragment>();

			for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
			{
				const FMassEntityHandle Entity = Ctx.GetEntity(i);

				const AActor* Actor = ActorFragments[i].Get();
				if (!Actor)
				{
					continue;
				}

				const APawn* Pawn = Cast<APawn>(Actor);
				if (!Pawn)
				{
					continue;
				}

				APlayerController* PC =
					Cast<APlayerController>(Pawn->GetController());
				if (!PC)
				{
					continue;
				}

				APlayerState* PS = PC->GetPlayerState<APlayerState>();
				if (!PS)
				{
					continue;
				}

				const FUniqueNetIdRepl& NetId = PS->GetUniqueId();
				if (!NetId.IsValid())
				{
					continue;
				}

				// TODO: Extract shared utility for PlayerId hashing
				// (duplicated from UArcPlayerPersistenceSubsystem::OnPlayerPostLogin)
				const FString NetIdStr = NetId.ToString();
				const FGuid PlayerId = FGuid(
					FCrc::StrCrc32(*NetIdStr),
					FCrc::StrCrc32(*NetIdStr) ^ 0x50455253,
					FCrc::StrCrc32(*FString::Printf(
						TEXT("%s_player"), *NetIdStr)),
					0x504C4159
				);

				if (!PlayerPersistence->IsPlayerDataLoaded(PlayerId))
				{
					continue;
				}

				// Load cached bytes directly and apply to the entity using
				// the chunk entity handle. This avoids round-tripping through
				// MassAgentComponent::GetEntityHandle() which may not be set
				// yet when the observer fires during entity creation.
				const FString Domain = TEXT("Pawn/MassFragments");
				const TArray<uint8>* CachedData =
					PlayerPersistence->GetCachedData(PlayerId, Domain);
				if (!CachedData)
				{
					continue;
				}

				FArcJsonLoadArchive LoadAr;
				if (!LoadAr.InitializeFromData(*CachedData))
				{
					continue;
				}

				FArcMassFragmentSerializer::LoadEntityFragments(
					EntityManager, Entity, LoadAr, World);

				UE_LOG(LogArcMassPlayerPersistenceObserver, Log,
					TEXT("Deferred load: Applied fragment data for player %s"),
					*PlayerId.ToString());
			}
		});
}
