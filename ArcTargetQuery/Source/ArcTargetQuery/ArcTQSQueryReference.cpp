// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSQueryReference.h"

#include "ArcTQSQueryDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTQSQueryReference)

void FArcTQSQueryReference::SetDefinition(UArcTQSQueryDefinition* NewDefinition)
{
	Definition = NewDefinition;
	SyncParameters();
}

void FArcTQSQueryReference::SetPropertyOverridden(const FGuid PropertyID, const bool bIsOverridden)
{
	if (bIsOverridden)
	{
		PropertyOverrides.AddUnique(PropertyID);
		return;
	}
	PropertyOverrides.Remove(PropertyID);
	SyncParameters();
}

void FArcTQSQueryReference::SyncParameters()
{
	if (!Definition)
	{
		Parameters.Reset();
		PropertyOverrides.Reset();
		return;
	}
	Parameters.MigrateToNewBagInstanceWithOverrides(Definition->Parameters, PropertyOverrides);
	const UPropertyBag* Bag = Parameters.GetPropertyBagStruct();
	PropertyOverrides.RemoveAll([Bag](const FGuid& PropertyID)
	{
		return !Bag || !Bag->FindPropertyDescByID(PropertyID);
	});
}

bool FArcTQSQueryReference::HasMatchingLayout() const
{
	return Definition && Parameters.GetPropertyBagStruct() == Definition->Parameters.GetPropertyBagStruct();
}
