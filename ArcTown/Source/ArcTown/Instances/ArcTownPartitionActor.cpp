// Copyright Lukasz Baran. All Rights Reserved.

#include "Instances/ArcTownPartitionActor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Town/ArcTownVolume.h"
#include "UObject/UObjectGlobals.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHandle.h"

#if WITH_EDITOR
namespace ArcTownPartitionActorAdoption
{
	/** Generated instance components are derived data: renaming or destroying one never dirties the transaction buffer. */
	constexpr ERenameFlags DerivedRenameFlags = REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional;

	struct FOldName
	{
		TObjectPtr<UActorComponent> Component;
		FName Name;
	};

	bool IsGeneratedComponentName(const UActorComponent& Component)
	{
		const FString Name = Component.GetName();
		return Name.StartsWith(ArcTownGeneratedISMPrefix)
			|| Name.StartsWith(ArcTownRoadSegmentPrefix)
			|| Name.StartsWith(ArcTownRoadNodePrefix)
			|| Name.StartsWith(TEXT("ArcTownCandidateISM"))
			|| Name.StartsWith(TEXT("ArcTownCandidateRoad"))
			|| Name.StartsWith(TEXT("ArcTownRetired"))
			|| Name.StartsWith(TEXT("ArcTownDiscarded"));
	}

	bool HasValidGeneratedComponentContract(const USceneComponent& Component, const USceneComponent* RootComponent)
	{
		return Component.CreationMethod == EComponentCreationMethod::Instance
			&& RootComponent != nullptr
			&& Component.GetAttachParent() == RootComponent;
	}

	void RenameForDestruction(UActorComponent& Component, const TCHAR* BaseName)
	{
		const FName UniqueName = MakeUniqueObjectName(
			Component.GetOuter(), Component.GetClass(), FName(BaseName));
		Component.Rename(*UniqueName.ToString(), nullptr, DerivedRenameFlags);
	}

	template <typename ComponentType>
	void DestroyComponents(
		AActor& Owner,
		TArray<TObjectPtr<ComponentType>>& Components,
		const TCHAR* DiscardBaseName)
	{
		for (ComponentType* Component : Components)
		{
			if (IsValid(Component))
			{
				Owner.RemoveInstanceComponent(Component);
				RenameForDestruction(*Component, DiscardBaseName);
				Component->DestroyComponent();
			}
		}
	}

	void RestoreOldNames(AActor& Owner, const TArray<FOldName>& OldNames)
	{
		for (const FOldName& Old : OldNames)
		{
			if (IsValid(Old.Component))
			{
				const bool bRestored = Old.Component->Rename(*Old.Name.ToString(), &Owner, DerivedRenameFlags);
				ensureMsgf(bRestored, TEXT("ArcTown failed to restore an old component name during adoption rollback."));
			}
		}
	}

	bool Preflight(
		AActor& Owner,
		UActorComponent* Component,
		const FName FinalName,
		const TCHAR* ExpectedPrefix,
		TSet<FName>& FinalNames,
		TSet<const UActorComponent*>& CandidateComponents,
		const TSet<const UActorComponent*>& OldComponents,
		FText& OutError)
	{
		if (!IsValid(Component) || Component->GetOuter() != &Owner || Component->IsRegistered()
			|| Component->HasAnyFlags(RF_Transactional)
			|| !FinalName.ToString().StartsWith(ExpectedPrefix)
			|| FinalNames.Contains(FinalName)
			|| CandidateComponents.Contains(Component))
		{
			OutError = FText::FromString(TEXT("ArcTown prepared component failed ownership, transaction, registration, or naming preflight."));
			return false;
		}
		if (const UObject* Existing = StaticFindObjectFast(UObject::StaticClass(), &Owner, FinalName))
		{
			if (!OldComponents.Contains(Cast<UActorComponent>(Existing)))
			{
				OutError = FText::FromString(
					FString::Printf(TEXT("ArcTown final component name '%s' is already occupied."), *FinalName.ToString()));
				return false;
			}
		}
		FinalNames.Add(FinalName);
		CandidateComponents.Add(Component);
		return true;
	}

	bool RetireOld(AActor& Owner, UActorComponent* Component, TArray<FOldName>& OldNames, FText& OutError)
	{
		if (!IsValid(Component))
		{
			OutError = FText::FromString(TEXT("ArcTown saved component set contains an invalid component."));
			return false;
		}
		FOldName& Old = OldNames.AddDefaulted_GetRef();
		Old.Component = Component;
		Old.Name = Component->GetFName();
		const FName RetiredName = MakeUniqueObjectName(&Owner, Component->GetClass(), FName(TEXT("ArcTownRetired")));
		if (!Component->Rename(*RetiredName.ToString(), &Owner, DerivedRenameFlags))
		{
			OutError = FText::FromString(TEXT("ArcTown could not reserve final names by retiring its old component set."));
			OldNames.Pop(EAllowShrinking::No);
			return false;
		}
		return true;
	}

	void DestroyOwnedBuildingActors(UWorld& World, TArray<FArcTownEmittedActor>& InOutRecords)
	{
		if (World.bIsTearingDown)
		{
			InOutRecords.Reset();
			return;
		}

		UWorldPartition* const WorldPartition = World.GetWorldPartition();
		TArray<FWorldPartitionReference> Pins;
		TArray<AActor*> Resolved;
		for (const FArcTownEmittedActor& Record : InOutRecords)
		{
			AActor* Actor = Record.Actor.Get();
			if (!IsValid(Actor) && WorldPartition != nullptr && Record.ActorGuid.IsValid())
			{
				FWorldPartitionReference Reference(WorldPartition, Record.ActorGuid);
				if (Reference.IsValid())
				{
					Actor = Reference.GetActor();
					if (Actor != nullptr)
					{
						Pins.Add(Reference);
					}
				}
			}
			if (IsValid(Actor))
			{
				Resolved.Add(Actor);
			}
		}

		for (AActor* const Actor : Resolved)
		{
			// EditorDestroyActor, not DestroyActor: only the editor wrapper unregisters the actor from
			// the navigation system, and it is what keeps an external-package building off the level.
			World.EditorDestroyActor(Actor, !Actor->IsPackageExternal());
		}
		Pins.Reset();
		InOutRecords.Reset();
	}

	bool RegisterCandidate(AActor& Owner, USceneComponent* Component, const FName FinalName, FText& OutError)
	{
		if (!Component->Rename(*FinalName.ToString(), &Owner, DerivedRenameFlags))
		{
			OutError = FText::FromString(
				FString::Printf(TEXT("ArcTown could not assign final component name '%s'."), *FinalName.ToString()));
			return false;
		}
		Component->AttachToComponent(Owner.GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		if (Component->GetAttachParent() != Owner.GetRootComponent())
		{
			OutError = FText::FromString(
				FString::Printf(TEXT("ArcTown component '%s' failed to attach to its partition actor root."), *FinalName.ToString()));
			return false;
		}
		Owner.AddInstanceComponent(Component);
		Component->ClearFlags(RF_Transactional);
		Component->RegisterComponent();
		if (!Component->IsRegistered()
			|| !Owner.GetInstanceComponents().Contains(Component)
			|| !HasValidGeneratedComponentContract(*Component, Owner.GetRootComponent()))
		{
			OutError = FText::FromString(
				FString::Printf(TEXT("ArcTown component '%s' failed registration or its native-component contract."), *FinalName.ToString()));
			return false;
		}
		return true;
	}
}
#endif

AArcTownPartitionActor::AArcTownPartitionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	// The instance components attach to this root, and it carries the cell-centre location the partition
	// subsystem spawns the actor at; without it every instance transform would be off by that centre.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Static);
}

void AArcTownPartitionActor::PostLoad()
{
	Super::PostLoad();

	for (UInstancedStaticMeshComponent* Component : StaticMeshInstances)
	{
		if (Component != nullptr)
		{
			Component->ClearFlags(RF_Transactional);
		}
	}

	for (USplineMeshComponent* Component : RoadSegments)
	{
		if (Component != nullptr)
		{
			Component->ClearFlags(RF_Transactional);
		}
	}

	for (UStaticMeshComponent* Component : RoadNodes)
	{
		if (Component != nullptr)
		{
			Component->ClearFlags(RF_Transactional);
		}
	}
}

void AArcTownPartitionActor::Destroyed()
{
#if WITH_EDITOR
	UWorld* const World = GetWorld();
	const ULevel* const Level = GetLevel();
	if (World != nullptr
		&& World->WorldType == EWorldType::Editor
		&& !IsEngineExitRequested()
		&& Level != nullptr
		&& !Level->bIsBeingRemoved)
	{
		ArcTownPartitionActorAdoption::DestroyOwnedBuildingActors(*World, OwnedBuildingActors);
	}
#endif

	Super::Destroyed();
}

#if WITH_EDITOR
AActor* AArcTownPartitionActor::GetSceneOutlinerParent() const
{
	if (AArcTownVolume* Town = OwningTown.Get())
	{
		return Town;
	}
	return Super::GetSceneOutlinerParent();
}

bool AArcTownPartitionActor::AdoptPreparedInstanceComponents(
	TArray<TObjectPtr<UInstancedStaticMeshComponent>>&& InComponents,
	TArray<FName>&& InFinalNames,
	FText& OutError)
{
	using namespace ArcTownPartitionActorAdoption;
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcTown_AdoptPreparedInstanceComponents);

	if (GetRootComponent() == nullptr)
	{
		OutError = FText::FromString(TEXT("ArcTown prepared instance adoption requires a root component."));
		DestroyComponents(*this, InComponents, TEXT("ArcTownDiscardedISM"));
		return false;
	}

	if (InComponents.Num() != InFinalNames.Num())
	{
		OutError = FText::FromString(TEXT("ArcTown prepared instance metadata is incomplete."));
		DestroyComponents(*this, InComponents, TEXT("ArcTownDiscardedISM"));
		return false;
	}

	TSet<const UActorComponent*> OldComponents;
	for (UInstancedStaticMeshComponent* Component : StaticMeshInstances)
	{
		if (Component != nullptr)
		{
			Component->ClearFlags(RF_Transactional);
		}
		if (!IsValid(Component) || Component->GetOuter() != this || !Component->IsRegistered()
			|| !IsGeneratedComponentName(*Component) || OldComponents.Contains(Component))
		{
			OutError = FText::FromString(TEXT("ArcTown saved component set is invalid or contains duplicates."));
			DestroyComponents(*this, InComponents, TEXT("ArcTownDiscardedISM"));
			return false;
		}
		OldComponents.Add(Component);
	}

	TSet<FName> FinalNames;
	TSet<const UActorComponent*> CandidateComponents;
	for (int32 Index = 0; Index < InComponents.Num(); ++Index)
	{
		if (!Preflight(*this, InComponents[Index], InFinalNames[Index], ArcTownGeneratedISMPrefix,
			FinalNames, CandidateComponents, OldComponents, OutError))
		{
			DestroyComponents(*this, InComponents, TEXT("ArcTownDiscardedISM"));
			return false;
		}
	}

	TArray<FOldName> OldNames;
	OldNames.Reserve(OldComponents.Num());
	for (UInstancedStaticMeshComponent* Component : StaticMeshInstances)
	{
		if (!RetireOld(*this, Component, OldNames, OutError))
		{
			RestoreOldNames(*this, OldNames);
			DestroyComponents(*this, InComponents, TEXT("ArcTownDiscardedISM"));
			return false;
		}
	}

	for (int32 Index = 0; Index < InComponents.Num(); ++Index)
	{
		if (!RegisterCandidate(*this, InComponents[Index], InFinalNames[Index], OutError))
		{
			DestroyComponents(*this, InComponents, TEXT("ArcTownDiscardedISM"));
			RestoreOldNames(*this, OldNames);
			return false;
		}
	}

	TArray<TObjectPtr<UInstancedStaticMeshComponent>> OldInstanceComponents = MoveTemp(StaticMeshInstances);
	StaticMeshInstances = MoveTemp(InComponents);
	DestroyComponents(*this, OldInstanceComponents, TEXT("ArcTownDiscardedISM"));
	OutError = FText::GetEmpty();
	return true;
}

bool AArcTownPartitionActor::AdoptPreparedRoadComponents(
	TArray<TObjectPtr<USplineMeshComponent>>&& InComponents,
	TArray<FName>&& InFinalNames,
	FText& OutError)
{
	using namespace ArcTownPartitionActorAdoption;
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcTown_AdoptPreparedRoadComponents);

	if (GetRootComponent() == nullptr)
	{
		OutError = FText::FromString(TEXT("ArcTown prepared road adoption requires a root component."));
		DestroyComponents(*this, InComponents, TEXT("ArcTownDiscardedRoadSeg"));
		return false;
	}

	if (InComponents.Num() != InFinalNames.Num())
	{
		OutError = FText::FromString(TEXT("ArcTown prepared road segment metadata is incomplete."));
		DestroyComponents(*this, InComponents, TEXT("ArcTownDiscardedRoadSeg"));
		return false;
	}

	TSet<const UActorComponent*> OldComponents;
	for (USplineMeshComponent* Component : RoadSegments)
	{
		if (Component != nullptr)
		{
			Component->ClearFlags(RF_Transactional);
		}
		if (!IsValid(Component) || Component->GetOuter() != this || !Component->IsRegistered()
			|| !IsGeneratedComponentName(*Component) || OldComponents.Contains(Component))
		{
			OutError = FText::FromString(TEXT("ArcTown saved road segment set is invalid or contains duplicates."));
			DestroyComponents(*this, InComponents, TEXT("ArcTownDiscardedRoadSeg"));
			return false;
		}
		OldComponents.Add(Component);
	}

	TSet<FName> FinalNames;
	TSet<const UActorComponent*> CandidateComponents;
	for (int32 Index = 0; Index < InComponents.Num(); ++Index)
	{
		if (!Preflight(*this, InComponents[Index], InFinalNames[Index], ArcTownRoadSegmentPrefix,
			FinalNames, CandidateComponents, OldComponents, OutError))
		{
			DestroyComponents(*this, InComponents, TEXT("ArcTownDiscardedRoadSeg"));
			return false;
		}
	}

	TArray<FOldName> OldNames;
	OldNames.Reserve(OldComponents.Num());
	for (USplineMeshComponent* Component : RoadSegments)
	{
		if (!RetireOld(*this, Component, OldNames, OutError))
		{
			RestoreOldNames(*this, OldNames);
			DestroyComponents(*this, InComponents, TEXT("ArcTownDiscardedRoadSeg"));
			return false;
		}
	}

	for (int32 Index = 0; Index < InComponents.Num(); ++Index)
	{
		if (!RegisterCandidate(*this, InComponents[Index], InFinalNames[Index], OutError))
		{
			DestroyComponents(*this, InComponents, TEXT("ArcTownDiscardedRoadSeg"));
			RestoreOldNames(*this, OldNames);
			return false;
		}
	}

	TArray<TObjectPtr<USplineMeshComponent>> OldRoadSegments = MoveTemp(RoadSegments);
	RoadSegments = MoveTemp(InComponents);
	DestroyComponents(*this, OldRoadSegments, TEXT("ArcTownDiscardedRoadSeg"));
	OutError = FText::GetEmpty();
	return true;
}

int32 AArcTownPartitionActor::DestroyGeneratedRoadSegments()
{
	using namespace ArcTownPartitionActorAdoption;
	check(IsInGameThread());

	TArray<USplineMeshComponent*> Existing;
	GetComponents<USplineMeshComponent>(Existing);

	int32 DestroyedCount = 0;
	for (USplineMeshComponent* Component : Existing)
	{
		if (!IsValid(Component)
			|| !Component->GetName().StartsWith(ArcTownRoadSegmentPrefix, ESearchCase::CaseSensitive))
		{
			continue;
		}
		RemoveInstanceComponent(Component);
		RenameForDestruction(*Component, TEXT("ArcTownDiscardedRoadSeg"));
		Component->DestroyComponent();
		++DestroyedCount;
	}
	RoadSegments.Reset();
	return DestroyedCount;
}

bool AArcTownPartitionActor::AdoptPreparedRoadNodeComponents(
	TArray<TObjectPtr<UStaticMeshComponent>>&& InComponents,
	TArray<FName>&& InFinalNames,
	FText& OutError)
{
	using namespace ArcTownPartitionActorAdoption;
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcTown_AdoptPreparedRoadNodeComponents);

	if (GetRootComponent() == nullptr)
	{
		OutError = FText::FromString(TEXT("ArcTown prepared road node adoption requires a root component."));
		DestroyComponents(*this, InComponents, TEXT("ArcTownDiscardedRoadNode"));
		return false;
	}

	if (InComponents.Num() != InFinalNames.Num())
	{
		OutError = FText::FromString(TEXT("ArcTown prepared road node metadata is incomplete."));
		DestroyComponents(*this, InComponents, TEXT("ArcTownDiscardedRoadNode"));
		return false;
	}

	TSet<const UActorComponent*> OldComponents;
	for (UStaticMeshComponent* Component : RoadNodes)
	{
		if (Component != nullptr)
		{
			Component->ClearFlags(RF_Transactional);
		}
		if (!IsValid(Component) || Component->GetOuter() != this || !Component->IsRegistered()
			|| !IsGeneratedComponentName(*Component) || OldComponents.Contains(Component))
		{
			OutError = FText::FromString(TEXT("ArcTown saved road node set is invalid or contains duplicates."));
			DestroyComponents(*this, InComponents, TEXT("ArcTownDiscardedRoadNode"));
			return false;
		}
		OldComponents.Add(Component);
	}

	TSet<FName> FinalNames;
	TSet<const UActorComponent*> CandidateComponents;
	for (int32 Index = 0; Index < InComponents.Num(); ++Index)
	{
		if (!Preflight(*this, InComponents[Index], InFinalNames[Index], ArcTownRoadNodePrefix,
			FinalNames, CandidateComponents, OldComponents, OutError))
		{
			DestroyComponents(*this, InComponents, TEXT("ArcTownDiscardedRoadNode"));
			return false;
		}
	}

	TArray<FOldName> OldNames;
	OldNames.Reserve(OldComponents.Num());
	for (UStaticMeshComponent* Component : RoadNodes)
	{
		if (!RetireOld(*this, Component, OldNames, OutError))
		{
			RestoreOldNames(*this, OldNames);
			DestroyComponents(*this, InComponents, TEXT("ArcTownDiscardedRoadNode"));
			return false;
		}
	}

	for (int32 Index = 0; Index < InComponents.Num(); ++Index)
	{
		if (!RegisterCandidate(*this, InComponents[Index], InFinalNames[Index], OutError))
		{
			DestroyComponents(*this, InComponents, TEXT("ArcTownDiscardedRoadNode"));
			RestoreOldNames(*this, OldNames);
			return false;
		}
	}

	TArray<TObjectPtr<UStaticMeshComponent>> OldRoadNodes = MoveTemp(RoadNodes);
	RoadNodes = MoveTemp(InComponents);
	DestroyComponents(*this, OldRoadNodes, TEXT("ArcTownDiscardedRoadNode"));
	OutError = FText::GetEmpty();
	return true;
}

int32 AArcTownPartitionActor::DestroyGeneratedRoadNodes()
{
	using namespace ArcTownPartitionActorAdoption;
	check(IsInGameThread());

	TArray<UStaticMeshComponent*> Existing;
	GetComponents<UStaticMeshComponent>(Existing);

	int32 DestroyedCount = 0;
	for (UStaticMeshComponent* Component : Existing)
	{
		if (!IsValid(Component)
			|| !Component->GetName().StartsWith(ArcTownRoadNodePrefix, ESearchCase::CaseSensitive))
		{
			continue;
		}
		RemoveInstanceComponent(Component);
		RenameForDestruction(*Component, TEXT("ArcTownDiscardedRoadNode"));
		Component->DestroyComponent();
		++DestroyedCount;
	}
	RoadNodes.Reset();
	return DestroyedCount;
}

bool AArcTownPartitionActor::AdoptPreparedBuildingActors(TArray<FArcTownEmittedActor>&& InActors, FText& OutError)
{
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcTown_AdoptPreparedBuildingActors);

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		OutError = FText::FromString(TEXT("ArcTown building-actor adoption requires a world."));
		return false;
	}

	ArcTownPartitionActorAdoption::DestroyOwnedBuildingActors(*World, OwnedBuildingActors);
	OwnedBuildingActors = MoveTemp(InActors);
	OutError = FText::GetEmpty();
	return true;
}
#endif
