// Copyright Lukasz Baran. All Rights Reserved.

#include "Instances/ArcEcotopeInstancePartitionActor.h"

#include "Components/InstancedSkinnedMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Instances/ArcEcotopeInstanceComponents.h"
#include "Simulation/ArcEcotopeVolume.h"

AArcEcotopeInstancePartitionActor::AArcEcotopeInstancePartitionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	// The instance components attach to this root, and it is what carries the cell-centre location the
	// partition subsystem spawns the actor at; without it every instance transform would be off by that centre.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Static);
}

void AArcEcotopeInstancePartitionActor::PostLoad()
{
	Super::PostLoad();

	for (UInstancedStaticMeshComponent* Component : StaticMeshInstances)
	{
		if (Component != nullptr)
		{
			Component->ClearFlags(RF_Transactional);
		}
	}
	for (UInstancedSkinnedMeshComponent* Component : SkinnedMeshInstances)
	{
		if (Component != nullptr)
		{
			Component->ClearFlags(RF_Transactional);
		}
	}
}

#if WITH_EDITOR
uint32 AArcEcotopeInstancePartitionActor::GetDefaultGridSize(UWorld* InWorld) const
{
	// Only the CDO fallback: every actor ArcEcotope creates is given the grid size its renderer authors.
	return 12800;
}

AActor* AArcEcotopeInstancePartitionActor::GetSceneOutlinerParent() const
{
	if (AArcEcotopeVolume* Volume = OwningVolume.Get())
	{
		return Volume;
	}
	return Super::GetSceneOutlinerParent();
}

bool AArcEcotopeInstancePartitionActor::AdoptPreparedInstanceComponents(
	TArray<TObjectPtr<UInstancedStaticMeshComponent>>&& InStaticComponents,
	TArray<FName>&& InStaticFinalNames,
	TArray<TObjectPtr<UInstancedSkinnedMeshComponent>>&& InSkinnedComponents,
	TArray<FName>&& InSkinnedFinalNames,
	FText& OutError)
{
	using namespace UE::ArcEcotope;
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcEcotope_AdoptPreparedInstanceComponents);

	auto DestroyCandidates = [&]()
	{
		for (UInstancedStaticMeshComponent* Component : InStaticComponents)
		{
			if (IsValid(Component))
			{
				RemoveInstanceComponent(Component);
				RenameForDestruction(*Component, TEXT("ArcEcotopeDiscardedISM"));
				Component->DestroyComponent();
			}
		}
		for (UInstancedSkinnedMeshComponent* Component : InSkinnedComponents)
		{
			if (IsValid(Component))
			{
				RemoveInstanceComponent(Component);
				RenameForDestruction(*Component, TEXT("ArcEcotopeDiscardedISKM"));
				Component->DestroyComponent();
			}
		}
	};

	if (GetRootComponent() == nullptr)
	{
		OutError = FText::FromString(TEXT("ArcEcotope prepared instance adoption requires a root component."));
		DestroyCandidates();
		return false;
	}

	if (InStaticComponents.Num() != InStaticFinalNames.Num()
		|| InSkinnedComponents.Num() != InSkinnedFinalNames.Num())
	{
		OutError = FText::FromString(TEXT("ArcEcotope prepared instance metadata is incomplete."));
		DestroyCandidates();
		return false;
	}

	TSet<FName> FinalNames;
	TSet<const UActorComponent*> CandidateComponents;
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
			OutError = FText::FromString(TEXT("ArcEcotope saved static component set is invalid or contains duplicates."));
			DestroyCandidates();
			return false;
		}
		OldComponents.Add(Component);
	}
	for (UInstancedSkinnedMeshComponent* Component : SkinnedMeshInstances)
	{
		if (Component != nullptr)
		{
			Component->ClearFlags(RF_Transactional);
		}
		if (!IsValid(Component) || Component->GetOuter() != this || !Component->IsRegistered()
			|| !IsGeneratedComponentName(*Component) || OldComponents.Contains(Component))
		{
			OutError = FText::FromString(TEXT("ArcEcotope saved skinned component set is invalid or contains duplicates."));
			DestroyCandidates();
			return false;
		}
		OldComponents.Add(Component);
	}

	auto Preflight = [&](UActorComponent* Component, const FName FinalName, const TCHAR* ExpectedPrefix) -> bool
	{
		if (!IsValid(Component) || Component->GetOuter() != this || Component->IsRegistered()
			|| Component->HasAnyFlags(RF_Transactional)
			|| !FinalName.ToString().StartsWith(ExpectedPrefix)
			|| FinalNames.Contains(FinalName)
			|| CandidateComponents.Contains(Component))
		{
			OutError = FText::FromString(TEXT("ArcEcotope prepared component failed ownership, transaction, registration, or naming preflight."));
			return false;
		}
		if (const UObject* Existing = StaticFindObjectFast(UObject::StaticClass(), this, FinalName))
		{
			if (!OldComponents.Contains(Cast<UActorComponent>(Existing)))
			{
				OutError = FText::FromString(
					FString::Printf(TEXT("ArcEcotope final component name '%s' is already occupied."), *FinalName.ToString()));
				return false;
			}
		}
		FinalNames.Add(FinalName);
		CandidateComponents.Add(Component);
		return true;
	};

	for (int32 Index = 0; Index < InStaticComponents.Num(); ++Index)
	{
		if (!Preflight(InStaticComponents[Index], InStaticFinalNames[Index], TEXT("ArcEcotopeISM_")))
		{
			DestroyCandidates();
			return false;
		}
	}
	for (int32 Index = 0; Index < InSkinnedComponents.Num(); ++Index)
	{
		if (!Preflight(InSkinnedComponents[Index], InSkinnedFinalNames[Index], TEXT("ArcEcotopeISKM_")))
		{
			DestroyCandidates();
			return false;
		}
	}

	struct FOldName
	{
		TObjectPtr<UActorComponent> Component;
		FName Name;
	};
	TArray<FOldName> OldNames;
	OldNames.Reserve(OldComponents.Num());

	auto RestoreOldNames = [&]()
	{
		for (const FOldName& Old : OldNames)
		{
			if (IsValid(Old.Component))
			{
				const bool bRestored = Old.Component->Rename(
					*Old.Name.ToString(), this, DerivedRenameFlags);
				ensureMsgf(bRestored, TEXT("ArcEcotope failed to restore an old component name during adoption rollback."));
			}
		}
	};

	auto RetireOld = [&](UActorComponent* Component) -> bool
	{
		if (!IsValid(Component))
		{
			OutError = FText::FromString(TEXT("ArcEcotope saved component set contains an invalid component."));
			return false;
		}
		FOldName& Old = OldNames.AddDefaulted_GetRef();
		Old.Component = Component;
		Old.Name = Component->GetFName();
		const FName RetiredName = MakeUniqueObjectName(
			this, Component->GetClass(), FName(TEXT("ArcEcotopeRetired")));
		if (!Component->Rename(*RetiredName.ToString(), this, DerivedRenameFlags))
		{
			OutError = FText::FromString(TEXT("ArcEcotope could not reserve final names by retiring its old component set."));
			OldNames.Pop(EAllowShrinking::No);
			return false;
		}
		return true;
	};

	for (UInstancedStaticMeshComponent* Component : StaticMeshInstances)
	{
		if (!RetireOld(Component))
		{
			RestoreOldNames();
			DestroyCandidates();
			return false;
		}
	}
	for (UInstancedSkinnedMeshComponent* Component : SkinnedMeshInstances)
	{
		if (!RetireOld(Component))
		{
			RestoreOldNames();
			DestroyCandidates();
			return false;
		}
	}

	auto RegisterCandidate = [&](USceneComponent* Component, const FName FinalName) -> bool
	{
		if (!Component->Rename(*FinalName.ToString(), this, DerivedRenameFlags))
		{
			OutError = FText::FromString(
				FString::Printf(TEXT("ArcEcotope could not assign final component name '%s'."), *FinalName.ToString()));
			return false;
		}
		Component->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		if (Component->GetAttachParent() != GetRootComponent())
		{
			OutError = FText::FromString(
				FString::Printf(TEXT("ArcEcotope component '%s' failed to attach to its partition actor root."), *FinalName.ToString()));
			return false;
		}
		AddInstanceComponent(Component);
		Component->ClearFlags(RF_Transactional);
		Component->RegisterComponent();
		if (!Component->IsRegistered()
			|| !GetInstanceComponents().Contains(Component)
			|| !HasValidGeneratedComponentContract(*Component, GetRootComponent()))
		{
			OutError = FText::FromString(
				FString::Printf(TEXT("ArcEcotope component '%s' failed registration or its native-component contract."), *FinalName.ToString()));
			return false;
		}
		return true;
	};

	for (int32 Index = 0; Index < InStaticComponents.Num(); ++Index)
	{
		if (!RegisterCandidate(InStaticComponents[Index], InStaticFinalNames[Index]))
		{
			DestroyCandidates();
			RestoreOldNames();
			return false;
		}
	}
	for (int32 Index = 0; Index < InSkinnedComponents.Num(); ++Index)
	{
		if (!RegisterCandidate(InSkinnedComponents[Index], InSkinnedFinalNames[Index]))
		{
			DestroyCandidates();
			RestoreOldNames();
			return false;
		}
	}

	TArray<TObjectPtr<UInstancedStaticMeshComponent>> OldStaticComponents = MoveTemp(StaticMeshInstances);
	TArray<TObjectPtr<UInstancedSkinnedMeshComponent>> OldSkinnedComponents = MoveTemp(SkinnedMeshInstances);
	StaticMeshInstances = MoveTemp(InStaticComponents);
	SkinnedMeshInstances = MoveTemp(InSkinnedComponents);
	RemoveInstanceComponents(*this, OldStaticComponents, OldSkinnedComponents);
	OutError = FText::GetEmpty();
	return true;
}
#endif
