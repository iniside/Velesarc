// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MassEntityTypes.h"
#include "SmartObjectTypes.h"
#include "ArcAreaSlotDefinition.h"
#include "MassEntityHandle.h"
#include "ArcAreaTypes.generated.h"

/** Lightweight handle to an area registered in UArcAreaSubsystem. */
USTRUCT(BlueprintType)
struct ARCAREA_API FArcAreaHandle
{
	GENERATED_BODY()

	FArcAreaHandle() = default;
	explicit FArcAreaHandle(uint32 InValue) : Value(InValue) {}

	bool IsValid() const { return Value != 0; }
	uint32 GetValue() const { return Value; }

	bool operator==(const FArcAreaHandle& Other) const { return Value == Other.Value; }
	bool operator!=(const FArcAreaHandle& Other) const { return Value != Other.Value; }

	friend uint32 GetTypeHash(const FArcAreaHandle& Handle) { return ::GetTypeHash(Handle.Value); }

	static FArcAreaHandle Make()
	{
		static uint32 Counter = 0;
		return FArcAreaHandle(++Counter);
	}

private:
	UPROPERTY()
	uint32 Value = 0;
};

/** Identifies a specific slot within an area. */
USTRUCT(BlueprintType)
struct ARCAREA_API FArcAreaSlotHandle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	FArcAreaHandle AreaHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	int32 SlotIndex = INDEX_NONE;

	bool IsValid() const { return AreaHandle.IsValid() && SlotIndex != INDEX_NONE; }

	bool operator==(const FArcAreaSlotHandle& Other) const
	{
		return AreaHandle == Other.AreaHandle && SlotIndex == Other.SlotIndex;
	}

	bool operator!=(const FArcAreaSlotHandle& Other) const { return !(*this == Other); }

	friend uint32 GetTypeHash(const FArcAreaSlotHandle& Handle)
	{
		return HashCombine(GetTypeHash(Handle.AreaHandle), ::GetTypeHash(Handle.SlotIndex));
	}
};

/** Current state of an area slot. */
UENUM(BlueprintType)
enum class EArcAreaSlotState : uint8
{
	/** No NPC assigned to this slot. */
	Vacant,
	/** An NPC is assigned but not actively at the SmartObject (away eating, sleeping, etc.). */
	Assigned,
	/** An NPC is assigned AND currently has a SmartObject claim (actively working). */
	Active,
	/** Slot is manually disabled — no assignment or vacancy posting. */
	Disabled
};

/** Runtime per-slot data, owned by the subsystem. */
USTRUCT()
struct ARCAREA_API FArcAreaSlotRuntime
{
	GENERATED_BODY()

	UPROPERTY()
	EArcAreaSlotState State = EArcAreaSlotState::Vacant;

	UPROPERTY()
	FMassEntityHandle AssignedEntity;
};

/** Runtime per-area data, owned by the subsystem. Single source of truth for area state. */
USTRUCT(BlueprintType)
struct ARCAREA_API FArcAreaData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Area")
	FArcAreaHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "Area")
	FPrimaryAssetId DefinitionId;

	UPROPERTY(BlueprintReadOnly, Category = "Area")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Area")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Area")
	FGameplayTagContainer AreaTags;

	/** Linked SmartObject handle. */
	UPROPERTY()
	FSmartObjectHandle SmartObjectHandle;

	/** Mass entity that registered this area (the SO entity). */
	UPROPERTY()
	FMassEntityHandle OwnerEntity;

	/** Runtime state per slot — parallel to SlotDefinitions. */
	UPROPERTY()
	TArray<FArcAreaSlotRuntime> Slots;

	/** Copied from definition for runtime access without loading the asset. */
	UPROPERTY()
	TArray<FArcAreaSlotDefinition> SlotDefinitions;
};
