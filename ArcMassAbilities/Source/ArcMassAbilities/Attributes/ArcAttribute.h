// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"
#include "MassEntityTypes.h"
#include "ArcAttribute.generated.h"

USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcAttribute
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float BaseValue = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float CurrentValue = 0.f;

	void SetBaseValue(float NewValue)
	{
		BaseValue = NewValue;
		CurrentValue = NewValue;
	}

	void InitValue(float Value)
	{
		BaseValue = Value;
		CurrentValue = Value;
	}

	float GetCurrentValue() const { return CurrentValue; }
	float GetBaseValue() const { return BaseValue; }
};

USTRUCT()
struct ARCMASSABILITIES_API FArcMassAttributesBase : public FMassFragment
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
	void Init##AttrName(float Value) { AttrName.InitValue(Value); }
