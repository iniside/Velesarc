// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "Fragments/ArcAbilityCustomDataBase.h"
#include "ArcAbilityCustomData.generated.h"

/**
 * Typed custom data container attached to an active ability.
 *
 * Wraps TArray<FInstancedStruct> of FArcAbilityCustomDataBase subclasses.
 * Enforces at-most-one-of-each-type semantics via AddOrReplace<T>.
 * Linear scan is intentional — typical N is 1-3 entries per ability.
 *
 * The UPROPERTY BaseStruct filter ensures the editor dropdown shows only
 * valid FArcAbilityCustomDataBase subclasses.
 */
USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcAbilityCustomData
{
	GENERATED_BODY()

	FArcAbilityCustomData();
	~FArcAbilityCustomData();
	FArcAbilityCustomData(const FArcAbilityCustomData&);
	FArcAbilityCustomData(FArcAbilityCustomData&&) noexcept;
	FArcAbilityCustomData& operator=(const FArcAbilityCustomData&);
	FArcAbilityCustomData& operator=(FArcAbilityCustomData&&) noexcept;

	UPROPERTY(EditAnywhere, meta = (BaseStruct = "/Script/ArcMassAbilities.ArcAbilityCustomDataBase"))
	TArray<FInstancedStruct> Sources;

	/** Find the first source of type T. Returns nullptr if not found. */
	template <typename T>
	const T* Find() const
	{
		for (const FInstancedStruct& S : Sources)
		{
			if (const T* P = S.GetPtr<T>())
			{
				return P;
			}
		}
		return nullptr;
	}

	/** Find the first source of type T, mutable. Returns nullptr if not found. */
	template <typename T>
	T* FindMutable()
	{
		for (FInstancedStruct& S : Sources)
		{
			if (T* P = S.GetMutablePtr<T>())
			{
				return P;
			}
		}
		return nullptr;
	}

	/**
	 * Add or replace the source of type T. Enforces at-most-one-of-T semantics.
	 * Returns a reference to the stored instance.
	 */
	template <typename T>
	T& AddOrReplace(T&& Value)
	{
		for (FInstancedStruct& S : Sources)
		{
			if (S.GetPtr<T>() != nullptr)
			{
				S = FInstancedStruct::Make<T>(MoveTemp(Value));
				return *S.GetMutablePtr<T>();
			}
		}
		FInstancedStruct& New = Sources.Emplace_GetRef(FInstancedStruct::Make<T>(MoveTemp(Value)));
		return *New.GetMutablePtr<T>();
	}

	/** Append a raw FInstancedStruct. Caller is responsible for correct BaseStruct. */
	void Add(FInstancedStruct&& Source);

	bool IsEmpty() const;

	/** Find the first entry whose script struct is, or derives from, Type. nullptr if not found / Type is null. */
	const FInstancedStruct* FindByScriptStruct(const UScriptStruct* Type) const;

	/**
	 * Primary source accessor for legacy code paths (effect pipeline / StateTree schema slot)
	 * that still expect a single FInstancedStruct. Returns the first source, or a static empty
	 * instance if the list is empty. The mutable variant returns a stable reference suitable
	 * for binding into a StateTree external-data slot for the duration of one ability tick.
	 */
	const FInstancedStruct& GetPrimary() const;
	FInstancedStruct& GetPrimaryMutable();
};
