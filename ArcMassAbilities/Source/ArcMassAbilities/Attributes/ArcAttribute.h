// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "ArcAttribute.generated.h"

struct FArcAttributeHandlerContext;
struct FArcAttribute;
struct FArcAttributeRef;

namespace ArcAttributes
{
	ARCMASSABILITIES_API void SetAttributeDirectAndNotify(
		const FArcAttributeHandlerContext&, const FArcAttributeRef&, FArcAttribute&, float);
}

USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcAttribute
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	float BaseValue = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	float CurrentValue = 0.f;

	// Base-only; CurrentValue refreshes on the next recalc (or never, for attributes without
	// an aggregator) — use InitValue to seed both.
	void SetBaseValue(float NewValue)
	{
		BaseValue = NewValue;
	}

	void InitValue(float Value)
	{
		BaseValue = Value;
		CurrentValue = Value;
	}

	// Writes BaseValue AND CurrentValue together. Use for resources that intentionally carry
	// NO aggregator (Energy/Stamina/Health/Entropy): no AttributeRecalculate pass exists to
	// reconcile CurrentValue from BaseValue, so a committed change must update both.
	void SetValueDirect(float NewValue)
	{
		BaseValue = NewValue;
		CurrentValue = NewValue;
	}

	float GetCurrentValue() const { return CurrentValue; }
	float GetBaseValue() const { return BaseValue; }
};

USTRUCT()
struct ARCMASSABILITIES_API FArcMassAttributesBase : public FMassSparseFragment
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcAttributeRef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TObjectPtr<UScriptStruct> FragmentType = nullptr;

	UPROPERTY(EditAnywhere)
	FName PropertyName = NAME_None;

	FProperty* GetCachedProperty() const;

	bool IsValid() const;

	bool operator==(const FArcAttributeRef& Other) const;

	friend ARCMASSABILITIES_API uint32 GetTypeHash(const FArcAttributeRef& Ref);

private:
	mutable FProperty* CachedProperty = nullptr;
};

#define ARC_MASS_ATTRIBUTE_ACCESSORS(OwnerType, AttrName) \
	static FArcAttributeRef Get##AttrName##Attribute() \
	{ \
		static FArcAttributeRef Ref = []() \
		{ \
			FArcAttributeRef R; \
			R.FragmentType = OwnerType::StaticStruct(); \
			R.PropertyName = GET_MEMBER_NAME_CHECKED(OwnerType, AttrName); \
			return R; \
		}(); \
		return Ref; \
	} \
	void Set##AttrName(float NewValue) { AttrName.SetBaseValue(NewValue); } \
	float Get##AttrName() const { return AttrName.GetCurrentValue(); } \
	void Init##AttrName(float Value) { AttrName.InitValue(Value); } \
	void Set##AttrName##Direct(float NewValue) { AttrName.SetValueDirect(NewValue); } \
	void Set##AttrName##DirectAndNotify(const FArcAttributeHandlerContext& Ctx, float NewValue) \
	{ ArcAttributes::SetAttributeDirectAndNotify(Ctx, Get##AttrName##Attribute(), AttrName, NewValue); }
