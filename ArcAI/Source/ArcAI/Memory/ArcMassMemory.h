// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMass/Config/ArcMassEntityTraitBase.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityElementTypes.h"
#include "Mass/EntityHandle.h"
#include "NativeGameplayTags.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcMassMemory.generated.h"

struct FMassEntityManager;

/** The entity's spawn location, written once by UArcMassMemoryTrait::InitializeEntity. */
ARCAI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AI_Memory_Anchor);

/** The patrol point Generate Patrol Points holds until the entity arrives at it. */
ARCAI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AI_Memory_PatrolPoint);

USTRUCT(BlueprintType, meta = (ToolTip = "A float remembered in NPC memory."))
struct ARCAI_API FArcMemoryFloat
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Memory", meta = (ToolTip = "Remembered value."))
	float Value = 0.0f;
};

USTRUCT(BlueprintType, meta = (ToolTip = "A bool remembered in NPC memory."))
struct ARCAI_API FArcMemoryBool
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Memory", meta = (ToolTip = "Remembered value."))
	bool bValue = false;
};

/** One NPC's private memory: typed values, each under a key. Values live until Forget or until the entity dies. */
USTRUCT()
struct ARCAI_API FArcMassMemoryFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FGameplayTag, FInstancedStruct> Values;
};

template<>
struct TMassFragmentTraits<FArcMassMemoryFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

template<typename T>
struct TArcMemoryValueTraits;

template<>
struct TArcMemoryValueTraits<FVector>
{
	using FStored = FVector;
	static const FVector& ToStored(const FVector& Value) { return Value; }
	static const FVector& FromStored(const FVector& Stored) { return Stored; }
};

template<>
struct TArcMemoryValueTraits<FMassEntityHandle>
{
	using FStored = FMassEntityHandle;
	static const FMassEntityHandle& ToStored(const FMassEntityHandle& Value) { return Value; }
	static const FMassEntityHandle& FromStored(const FMassEntityHandle& Stored) { return Stored; }
};

template<>
struct TArcMemoryValueTraits<FGameplayTag>
{
	using FStored = FGameplayTag;
	static const FGameplayTag& ToStored(const FGameplayTag& Value) { return Value; }
	static const FGameplayTag& FromStored(const FGameplayTag& Stored) { return Stored; }
};

template<>
struct TArcMemoryValueTraits<float>
{
	using FStored = FArcMemoryFloat;
	static FArcMemoryFloat ToStored(const float Value) { FArcMemoryFloat Stored; Stored.Value = Value; return Stored; }
	static float FromStored(const FArcMemoryFloat& Stored) { return Stored.Value; }
};

template<>
struct TArcMemoryValueTraits<bool>
{
	using FStored = FArcMemoryBool;
	static FArcMemoryBool ToStored(const bool bValue) { FArcMemoryBool Stored; Stored.bValue = bValue; return Stored; }
	static bool FromStored(const FArcMemoryBool& Stored) { return Stored.bValue; }
};

namespace ArcMassMemory
{
	ARCAI_API FArcMassMemoryFragment* FindMutableFragment(FMassEntityManager& EntityManager, FMassEntityHandle Entity);

	ARCAI_API const FArcMassMemoryFragment* FindFragment(const FMassEntityManager& EntityManager, FMassEntityHandle Entity);

	/** Logs once per key, stored type and requested type; game thread only. */
	ARCAI_API void WarnTypeMismatch(FGameplayTag Key, const UScriptStruct* StoredType, const UScriptStruct* RequestedType);

#if WITH_DEV_AUTOMATION_TESTS
	ARCAI_API void ResetReportedMismatchesForTesting();
#endif

	ARCAI_API void Forget(FArcMassMemoryFragment& Fragment, FGameplayTag Key);

	/** False when the entity carries no memory fragment. */
	ARCAI_API bool Forget(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FGameplayTag Key);

	template<typename T>
	void Write(FArcMassMemoryFragment& Fragment, const FGameplayTag Key, const T& Value)
	{
		if (!Key.IsValid())
		{
			return;
		}
		using FStored = typename TArcMemoryValueTraits<T>::FStored;
		Fragment.Values.FindOrAdd(Key).InitializeAs<FStored>(TArcMemoryValueTraits<T>::ToStored(Value));
	}

	/** False when Key is absent or holds another type; Out is untouched then. */
	template<typename T>
	bool Read(const FArcMassMemoryFragment& Fragment, const FGameplayTag Key, T& Out)
	{
		const FInstancedStruct* Stored = Fragment.Values.Find(Key);
		if (!Stored || !Stored->IsValid())
		{
			return false;
		}
		using FStored = typename TArcMemoryValueTraits<T>::FStored;
		const UScriptStruct* RequestedType = TBaseStructure<FStored>::Get();
		if (Stored->GetScriptStruct() != RequestedType)
		{
			WarnTypeMismatch(Key, Stored->GetScriptStruct(), RequestedType);
			return false;
		}
		Out = TArcMemoryValueTraits<T>::FromStored(*reinterpret_cast<const FStored*>(Stored->GetMemory()));
		return true;
	}

	/** False when the entity carries no memory fragment. */
	template<typename T>
	bool Write(FMassEntityManager& EntityManager, const FMassEntityHandle Entity, const FGameplayTag Key, const T& Value)
	{
		FArcMassMemoryFragment* Fragment = FindMutableFragment(EntityManager, Entity);
		if (!Fragment)
		{
			return false;
		}
		Write(*Fragment, Key, Value);
		return true;
	}

	/** False when the entity carries no memory fragment, exactly as for an absent key. */
	template<typename T>
	bool Read(const FMassEntityManager& EntityManager, const FMassEntityHandle Entity, const FGameplayTag Key, T& Out)
	{
		const FArcMassMemoryFragment* Fragment = FindFragment(EntityManager, Entity);
		return Fragment && Read(*Fragment, Key, Out);
	}
}

/** Gives an entity the memory fragment and seeds AI.Memory.Anchor with its spawn location. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Mass Memory", Category = "AI"))
class ARCAI_API UArcMassMemoryTrait : public UArcMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const override;

	virtual void InitializeEntity(FMassEntityManager& Mgr, FMassEntityHandle Entity, const UWorld& World) const override;
};
