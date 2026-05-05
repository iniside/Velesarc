// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/InstancedStruct.h"
#include "Equipment/ArcSocketArray.h"
#include "ArcMassItemAttachmentFragments.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "ArcMassAttachmentHandler.generated.h"

struct FMassEntityManager;
struct FArcItemData;
struct FArcItemAttachmentSlot;
class UArcItemDefinition;
class USceneComponent;
class AActor;

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassAttachmentHandler
{
	GENERATED_BODY()

public:
	virtual ~FArcMassAttachmentHandler() = default;

	// Idempotent one-time hook. Must NOT stash per-trait state on the
	// handler — instances live on the shared UArcMassItemAttachmentConfig
	// data asset and are visited by every trait that references it.
	virtual void Initialize(const UScriptStruct* InStoreType) {}

	// Add side — item is live in the store at this point.
	virtual bool HandleItemAddedToSlot(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FArcMassItemAttachmentStateFragment& State,
		const FArcItemData* Item,
		const FArcItemData* OwnerItem) const
	{
		return false;
	}

	virtual void HandleItemAttach(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FArcMassItemAttachmentStateFragment& State,
		const FArcItemId ItemId,
		const FArcItemId OwnerItem) const
	{
	}

	// Detach side — item may already be gone from the store. Snapshot
	// carries the data handlers need (ItemDefinition, SocketName, etc.).
	virtual void HandleItemRemovedFromSlot(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FArcMassItemAttachmentStateFragment& State,
		const FArcMassItemAttachment& Snapshot) const
	{
	}

	virtual void HandleItemDetach(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FArcMassItemAttachmentStateFragment& State,
		const FArcMassItemAttachment& Snapshot) const
	{
	}

	virtual void HandleItemAttachmentChanged(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FArcMassItemAttachmentStateFragment& State,
		const FArcMassItemAttachment& ItemAttachment) const
	{
	}

	virtual UScriptStruct* SupportedItemFragment() const
	{
		return nullptr;
	}
};

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassAttachmentHandlerCommon : public FArcMassAttachmentHandler
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Base")
	TArray<FArcSocketArray> AttachmentData;

	UPROPERTY(EditAnywhere, Category = "Base")
	FName ComponentTag;

	UPROPERTY(EditAnywhere, Category = "Base", meta = (BaseStruct = "/Script/ArcCore.ArcAttachmentTransformFinder"))
	FInstancedStruct TransformFinder;

	virtual void Initialize(const UScriptStruct* InStoreType) override
	{
		checkf(InStoreType && InStoreType->IsChildOf(FArcMassItemStoreFragment::StaticStruct()),
			TEXT("StoreType must derive from FArcMassItemStoreFragment"));
	}

	AActor* FindActor(FMassEntityManager& EntityManager, FMassEntityHandle Entity) const;

	USceneComponent* FindAttachmentComponent(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const FArcMassItemAttachment* ItemAttachment) const;

	// Reads State.TakenSockets to skip already-taken socket names.
	FName FindSocketName(
		const FArcItemData* Item,
		const FArcItemData* OwnerItem,
		const FArcMassItemAttachmentStateFragment& State) const;

	FTransform FindRelativeTransform(const FArcItemData* Item, const FArcItemData* OwnerItem) const;

	// Now consults FindAttachmentHandlerOnOwnerItem for nested-attachment
	// socket override and uses GetVisualItem for visual resolution.
	FArcMassItemAttachment MakeItemAttachment(
		const FArcItemData* Item,
		const FArcItemData* OwnerItem,
		const FArcMassItemAttachmentStateFragment& State) const;

	FName FindFinalAttachSocket(const FArcMassItemAttachment* ItemAttachment) const;

	// Visual-item resolution: reads FArcItemFragment_ItemVisualAttachment +
	// FArcItemInstance_ItemVisualAttachment + DefaultVisualItem fallback.
	virtual UArcItemDefinition* GetVisualItem(const FArcItemData* Item) const;

	const FArcMassAttachmentHandlerCommon* FindAttachmentHandlerOnOwnerItem(
		const FArcItemData* Item,
		const FArcItemData* OwnerItem,
		UScriptStruct* Type) const;

	template<typename T>
	const T* FindAttachmentFragment(const FArcMassItemAttachment& ItemAttachment) const
	{
		if (ItemAttachment.VisualItemDefinition && ItemAttachment.VisualItemDefinition == ItemAttachment.OldVisualItemDefinition)
		{
			return nullptr;
		}

		if (ItemAttachment.VisualItemDefinition)
		{
			return ItemAttachment.VisualItemDefinition->FindFragment<T>();
		}

		if (ItemAttachment.ItemDefinition)
		{
			return ItemAttachment.ItemDefinition->FindFragment<T>();
		}

		return nullptr;
	}

	template<typename T = USceneComponent>
	T* SpawnComponent(
		AActor* OwnerActor,
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const FArcMassItemAttachment* ItemAttachment,
		TSubclassOf<UActorComponent> ComponentClass = nullptr,
		const FName& Name = NAME_None) const
	{
		if (!OwnerActor)
		{
			return nullptr;
		}
		USceneComponent* ParentComponent = FindAttachmentComponent(EntityManager, Entity, ItemAttachment);
		if (ParentComponent == nullptr)
		{
			return nullptr;
		}

		UClass* ComponentClassToUse = ComponentClass ? ComponentClass.Get() : T::StaticClass();
		const FName SocketName = FindFinalAttachSocket(ItemAttachment);

		T* Comp = NewObject<T>(OwnerActor, ComponentClassToUse, Name);
		Comp->SetCanEverAffectNavigation(false);
		Comp->SetupAttachment(ParentComponent, SocketName);
		Comp->CreationMethod = EComponentCreationMethod::Instance;
		Comp->RegisterComponent();
		return Comp;
	}
};
