/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "Pawn/ArcMassPawnDataFragmentsImpl.h"

#include "ArcMassGameplayFramework.h"
#include "Animation/AnimInstance.h"
#include "Pawn/ArcMassPawnExtensionComponent.h"
#include "Components/ActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"

#include "Abilities/ArcMassAbilitySet.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Items/ArcItemSpec.h"
#include "Operations/ArcMassItemOperations.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Bags/ArcBagOperations.h"
#include "Bags/ArcBagStoreFragment.h"
#include "Mass/EntityManager.h"

void FArcMassAddPawnComponent_Single::GiveComponent(APawn* Pawn) const
{
	if (Pawn == nullptr)
	{
		return;
	}

	if (bServerOnly && Pawn->HasAuthority() == false)
	{
		return;
	}

	if (bClientOnly && Pawn->HasAuthority() == true)
	{
		return;
	}

	UClass* LoadedClass = ComponentClass.LoadSynchronous();
	if (LoadedClass == nullptr)
	{
		return;
	}

	UActorComponent* Comp = NewObject<UActorComponent>(Pawn
		, LoadedClass
		, LoadedClass->GetFName());
	if (Comp == nullptr)
	{
		return;
	}

	Comp->RegisterComponentWithWorld(Pawn->GetWorld());
	Comp->SetIsReplicated(bReplicate);
}

void FArcMassPawnInitFragment_SetSkeletalMesh::Initialize(APawn* Pawn
														  , APlayerState* PlayerState
														  , APlayerController* PlayerController
														  , FMassEntityManager* EntityManager
														  , FMassEntityHandle Entity
														  , UArcMassPawnExtensionComponent* OwnerExtension) const
{
	if (Pawn == nullptr)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(Pawn);
	if (Character == nullptr)
	{
		return;
	}

	USkeletalMeshComponent* Mesh = Character->GetMesh();
	if (Mesh == nullptr)
	{
		return;
	}

	USkeletalMesh* LoadedMesh = SkeletalMesh.LoadSynchronous();
	Mesh->SetSkeletalMesh(LoadedMesh);
	Mesh->SetActive(true);
}

namespace ArcMassPawnDataFragmentsImpl_SetAnimBlueprint
{
	static void ApplyAnimClassAndLayers(USkeletalMeshComponent* Mesh
										, const TSoftClassPtr<UAnimInstance>& AnimBlueprint
										, const TArray<TSoftClassPtr<UAnimInstance>>& AnimLayers)
	{
		UClass* AnimClass = AnimBlueprint.Get();
		if (AnimClass)
		{
			Mesh->SetAnimInstanceClass(AnimClass);
		}

		for (const TSoftClassPtr<UAnimInstance>& Layer : AnimLayers)
		{
			UClass* LayerClass = Layer.Get();
			if (LayerClass)
			{
				Mesh->LinkAnimClassLayers(LayerClass);
			}
		}
	}

	static USkeletalMeshComponent* ResolveSkeletalMeshComponent(APawn* Pawn, FName MeshComponentTag)
	{
		if (Pawn == nullptr)
		{
			return nullptr;
		}

		if (MeshComponentTag.IsNone() == false)
		{
			AActor* PawnAsActor = Pawn;
			TArray<UActorComponent*> Found = PawnAsActor->GetComponentsByTag(USkeletalMeshComponent::StaticClass(), MeshComponentTag);
			if (Found.Num() > 0)
			{
				return Cast<USkeletalMeshComponent>(Found[0]);
			}
			UE_LOGF(LogArcMassGameFramework, Warning,"FArcMassPawnInitFragment_SetAnimBlueprint: no USkeletalMeshComponent with tag '%ls' found on pawn '%ls'. Anim blueprint will not be applied.",
				*MeshComponentTag.ToString(), *GetNameSafe(Pawn));
			return nullptr;
		}

		if (ACharacter* C = Cast<ACharacter>(Pawn))
		{
			return C->GetMesh();
		}
		AActor* PawnAsActor = Pawn;
		return Cast<USkeletalMeshComponent>(PawnAsActor->FindComponentByClass(USkeletalMeshComponent::StaticClass()));
	}
} // namespace ArcMassPawnDataFragmentsImpl_SetAnimBlueprint

void FArcMassPawnInitFragment_SetAnimBlueprint::Initialize(APawn* Pawn
														   , APlayerState* PlayerState
														   , APlayerController* PlayerController
														   , FMassEntityManager* EntityManager
														   , FMassEntityHandle Entity
														   , UArcMassPawnExtensionComponent* OwnerExtension) const
{
	if (Pawn == nullptr)
	{
		return;
	}

	USkeletalMeshComponent* Mesh = ArcMassPawnDataFragmentsImpl_SetAnimBlueprint::ResolveSkeletalMeshComponent(Pawn, MeshComponentTag);
	if (Mesh == nullptr)
	{
		return;
	}

	// Fast path: anim blueprint already resident in memory -- no I/O, apply synchronously.
	if (UClass* AnimClass = AnimBlueprint.Get())
	{
		Mesh->SetAnimInstanceClass(AnimClass);

		for (const TSoftClassPtr<UAnimInstance>& Layer : AnimLayers)
		{
			UClass* LayerClass = Layer.Get();
			if (LayerClass)
			{
				Mesh->LinkAnimClassLayers(LayerClass);
			}
		}
		return;
	}

	// Async fallback: blueprint not yet resident.  Build the full path list and issue a
	// non-blocking RequestAsyncLoad.  The handle is tracked on OwnerExtension so it stays
	// alive until completion (or EndPlay cancels it).
	//
	// Guard: if no OwnerExtension is available (non-standard caller), fall back to the
	// old synchronous load so the caller still gets a valid anim instance.
	if (OwnerExtension == nullptr)
	{
		UClass* LoadedAnimClass = AnimBlueprint.LoadSynchronous();
		Mesh->SetAnimInstanceClass(LoadedAnimClass);

		for (const TSoftClassPtr<UAnimInstance>& AnimLayer : AnimLayers)
		{
			TSubclassOf<UAnimInstance> LoadedLayer = AnimLayer.LoadSynchronous();
			if (LoadedLayer)
			{
				Mesh->LinkAnimClassLayers(LoadedLayer);
			}
		}
		return;
	}

	TArray<FSoftObjectPath> Paths;
	Paths.Add(AnimBlueprint.ToSoftObjectPath());
	for (const TSoftClassPtr<UAnimInstance>& Layer : AnimLayers)
	{
		if (!Layer.IsNull())
		{
			Paths.Add(Layer.ToSoftObjectPath());
		}
	}

	// Capture only named, lifetime-safe values.
	// WeakMesh:             guards the skeletal mesh component across the async boundary.
	// CapturedAnimBlueprint / CapturedAnimLayers: value copies -- the fragment lives in a
	//                       movable FInstancedStruct; never capture `this` or `[=]`.
	TWeakObjectPtr<USkeletalMeshComponent> WeakMesh(Mesh);
	TSoftClassPtr<UAnimInstance> CapturedAnimBlueprint = AnimBlueprint;
	TArray<TSoftClassPtr<UAnimInstance>> CapturedAnimLayers = AnimLayers;

	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	TSharedPtr<FStreamableHandle> Handle = StreamableManager.RequestAsyncLoad(
		Paths,
		FStreamableDelegate::CreateWeakLambda(
			OwnerExtension,
			[WeakMesh, CapturedAnimBlueprint, CapturedAnimLayers]() mutable
			{
				USkeletalMeshComponent* M = WeakMesh.Get();
				if (!M)
				{
					return;
				}

				ArcMassPawnDataFragmentsImpl_SetAnimBlueprint::ApplyAnimClassAndLayers(
					M, CapturedAnimBlueprint, CapturedAnimLayers);

				// Step 3b fallback: FArcMassItemAttachmentStateFragment::ActiveAttachments
				// cannot be safely accessed here -- EntityManager is a raw struct pointer
				// with no weak-ref mechanism, making cross-async-boundary access unsafe.
				// This is acceptable: FArcMassAttachmentHandler_AnimLayer::HandleItemAttach
				// already guards on GetAnimInstance() == nullptr and no-ops gracefully when
				// the anim instance is not yet set.  On the fast path (Steps 5-6 pre-stream
				// the anim BP under the loading screen) this async branch never executes,
				// so the attachment window does not open in the shipping configuration.
			}));

	OwnerExtension->TrackInitLoadHandle(Handle);
}

void FArcMassPawnInitFragment_GrantAbilitySet::Initialize(APawn* Pawn
														   , APlayerState* PlayerState
														   , APlayerController* PlayerController
														   , FMassEntityManager* EntityManager
														   , FMassEntityHandle Entity
														   , UArcMassPawnExtensionComponent* OwnerExtension) const
{
	if (Pawn == nullptr || Pawn->HasAuthority() == false)
	{
		return;
	}

	if (EntityManager == nullptr || Entity.IsSet() == false)
	{
		return;
	}

	if (AbilitySet == nullptr)
	{
		return;
	}

	ArcAbilities::GrantAbilitySet(*EntityManager, Entity, AbilitySet);
}

void FArcMassPawnInitFragment_AddItems::Initialize(APawn* Pawn
												   , APlayerState* PlayerState
												   , APlayerController* PlayerController
												   , FMassEntityManager* EntityManager
												   , FMassEntityHandle Entity
												   , UArcMassPawnExtensionComponent* OwnerExtension) const
{
	if (Pawn == nullptr || Pawn->HasAuthority() == false)
	{
		return;
	}

	if (EntityManager == nullptr || Entity.IsSet() == false)
	{
		return;
	}

	const UScriptStruct* Store = StoreType ? StoreType.Get() : FArcMassItemStoreFragment::StaticStruct();

	for (const FArcMassPawnAddItemEntry& Entry : Items)
	{
		if (!Entry.Item.IsValid())
		{
			continue;
		}

		FArcItemSpec Spec = FArcItemSpec::NewItem(Entry.Item, Entry.Level, Entry.Amount);
		ArcMassItems::AddItem(*EntityManager, Entity, Store, Spec);
	}
}

void FArcMassPawnInitFragment_AddItemsToSlots::Initialize(APawn* Pawn
														   , APlayerState* PlayerState
														   , APlayerController* PlayerController
														   , FMassEntityManager* EntityManager
														   , FMassEntityHandle Entity
														   , UArcMassPawnExtensionComponent* OwnerExtension) const
{
	if (Pawn == nullptr || Pawn->HasAuthority() == false)
	{
		return;
	}

	if (EntityManager == nullptr || Entity.IsSet() == false)
	{
		return;
	}

	const UScriptStruct* Store = StoreType ? StoreType.Get() : FArcMassItemStoreFragment::StaticStruct();

	for (const FArcMassPawnAddItemSlotEntry& Entry : Items)
	{
		if (!Entry.Item.IsValid())
		{
			continue;
		}

		FArcItemSpec Spec = FArcItemSpec::NewItem(Entry.Item, Entry.Level, Entry.Amount);
		const FArcItemId NewId = ArcMassItems::AddItem(*EntityManager, Entity, Store, Spec);
		if (NewId.IsValid() && Entry.Slot.IsValid())
		{
			ArcMassItems::AddItemToSlot(*EntityManager, Entity, Store, NewId, Entry.Slot);
		}
	}
}

void FArcMassPawnInitFragment_AddItemsToBag::Initialize(APawn* Pawn
														 , APlayerState* PlayerState
														 , APlayerController* PlayerController
														 , FMassEntityManager* EntityManager
														 , FMassEntityHandle Entity
														 , UArcMassPawnExtensionComponent* OwnerExtension) const
{
	if (Pawn == nullptr || Pawn->HasAuthority() == false)
	{
		return;
	}

	if (EntityManager == nullptr || Entity.IsSet() == false)
	{
		return;
	}

	FArcMassBagStoreFragment* Frag = EntityManager->GetMutableSparseElementDataForEntity(FArcMassBagStoreFragment::StaticStruct(), Entity).GetPtr<FArcMassBagStoreFragment>();
	if (Frag == nullptr)
	{
		UE_LOGF(LogArcMassGameFramework, Warning,"FArcMassPawnInitFragment_AddItemsToBag: entity has no bag overlay fragment; items not added.");
		return;
	}

	// TargetStoreType is stable across the AddItem calls below; read it once. The bag
	// fragment pointer is NOT — AddItem opens a transaction that may add a sparse
	// fragment and invalidate it, so never cache Frag across an AddItem call.
	const UScriptStruct* Store = Frag->TargetStoreType ? Frag->TargetStoreType.Get() : FArcMassItemStoreFragment::StaticStruct();

	for (const FArcMassPawnAddItemEntry& Entry : Items)
	{
		if (!Entry.Item.IsValid())
		{
			continue;
		}

		FArcItemSpec Spec = FArcItemSpec::NewItem(Entry.Item, Entry.Level, Entry.Amount);
		const FArcItemId NewId = ArcMassItems::AddItem(*EntityManager, Entity, Store, Spec);
		if (!NewId.IsValid())
		{
			continue;
		}

		FArcBagId OutBag;
		int32 OutSlot = INDEX_NONE;
		if (ArcMassItems::Bags::FindFirstFreeSlotForItem(*EntityManager, Entity, NewId, OutBag, OutSlot))
		{
			ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, OutBag, OutSlot, NewId);
		}
		else
		{
			UE_LOGF(LogArcMassGameFramework, Warning,"FArcMassPawnInitFragment_AddItemsToBag: no free bag slot for item; left in store.");
		}
	}
}
