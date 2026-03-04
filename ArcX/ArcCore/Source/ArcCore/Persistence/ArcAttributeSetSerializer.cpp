// Copyright Lukasz Baran. All Rights Reserved.

#include "Persistence/ArcAttributeSetSerializer.h"

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "Serialization/ArcSaveArchive.h"
#include "Serialization/ArcLoadArchive.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcAttributeSetSerializer, Log, All);

UE_ARC_IMPLEMENT_PERSISTENCE_SERIALIZER(FArcAttributeSetSerializer, "ArcAttributeSet");

namespace
{
	template<typename Func>
	void ForEachSaveGameAttribute(const UAttributeSet& Set, Func&& Callback)
	{
		for (TFieldIterator<FStructProperty> It(Set.GetClass()); It; ++It)
		{
			FStructProperty* StructProp = *It;
			if (!StructProp || !StructProp->Struct)
			{
				continue;
			}
			if (!StructProp->Struct->IsChildOf(FGameplayAttributeData::StaticStruct()))
			{
				continue;
			}
			if (!StructProp->HasAnyPropertyFlags(CPF_SaveGame))
			{
				continue;
			}

			const FGameplayAttributeData* AttrData =
				StructProp->ContainerPtrToValuePtr<FGameplayAttributeData>(&Set);
			if (AttrData)
			{
				Callback(StructProp, *AttrData);
			}
		}
	}
}

void FArcAttributeSetSerializer::Save(const UAttributeSet& Source, FArcSaveArchive& Ar)
{
	int32 Count = 0;
	ForEachSaveGameAttribute(Source, [&](FStructProperty*, const FGameplayAttributeData&) { ++Count; });

	Ar.BeginArray(FName(TEXT("Attributes")), Count);

	int32 Index = 0;
	ForEachSaveGameAttribute(Source, [&](FStructProperty* Prop, const FGameplayAttributeData& Data)
	{
		Ar.BeginArrayElement(Index);
		Ar.WriteProperty(FName(TEXT("Name")), FString(Prop->GetName()));
		Ar.WriteProperty(FName(TEXT("BaseValue")), Data.GetBaseValue());
		Ar.EndArrayElement();
		++Index;
	});

	Ar.EndArray();

	UE_LOG(LogArcAttributeSetSerializer, Log,
		TEXT("Saved %d attributes from '%s'"), Count, *Source.GetClass()->GetName());
}

void FArcAttributeSetSerializer::Load(UAttributeSet& Target, FArcLoadArchive& Ar)
{
	int32 Count = 0;
	if (!Ar.BeginArray(FName(TEXT("Attributes")), Count))
	{
		return;
	}

	UAbilitySystemComponent* ASC = Target.GetOwningAbilitySystemComponent();

	int32 Applied = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		if (!Ar.BeginArrayElement(i))
		{
			continue;
		}

		FString AttrName;
		float BaseValue = 0.0f;

		Ar.ReadProperty(FName(TEXT("Name")), AttrName);
		Ar.ReadProperty(FName(TEXT("BaseValue")), BaseValue);
		Ar.EndArrayElement();

		FStructProperty* FoundProp = nullptr;
		for (TFieldIterator<FStructProperty> It(Target.GetClass()); It; ++It)
		{
			FStructProperty* StructProp = *It;
			if (StructProp && StructProp->Struct &&
				StructProp->Struct->IsChildOf(FGameplayAttributeData::StaticStruct()) &&
				StructProp->GetName() == AttrName)
			{
				FoundProp = StructProp;
				break;
			}
		}

		if (!FoundProp)
		{
			UE_LOG(LogArcAttributeSetSerializer, Warning,
				TEXT("Attribute '%s' not found on '%s', skipping."),
				*AttrName, *Target.GetClass()->GetName());
			continue;
		}

		if (ASC)
		{
			FGameplayAttribute Attribute(FoundProp);
			ASC->SetNumericAttributeBase(Attribute, BaseValue);
			++Applied;
		}
		else
		{
			FGameplayAttributeData* AttrData =
				FoundProp->ContainerPtrToValuePtr<FGameplayAttributeData>(&Target);
			if (AttrData)
			{
				AttrData->SetBaseValue(BaseValue);
				AttrData->SetCurrentValue(BaseValue);
				++Applied;
			}
		}
	}

	Ar.EndArray();

	UE_LOG(LogArcAttributeSetSerializer, Log,
		TEXT("Loaded %d attributes into '%s'"), Applied, *Target.GetClass()->GetName());
}
