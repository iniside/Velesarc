// Copyright Lukasz Baran. All Rights Reserved.

#include "Attributes/ArcAttribute.h"

FProperty* FArcAttributeRef::GetCachedProperty() const
{
	if (!CachedProperty && FragmentType && PropertyName != NAME_None)
	{
		CachedProperty = FragmentType->FindPropertyByName(PropertyName);
	}
	return CachedProperty;
}

bool FArcAttributeRef::IsValid() const
{
	return FragmentType != nullptr && PropertyName != NAME_None;
}

bool FArcAttributeRef::operator==(const FArcAttributeRef& Other) const
{
	return FragmentType == Other.FragmentType && PropertyName == Other.PropertyName;
}

uint32 GetTypeHash(const FArcAttributeRef& Ref)
{
	return HashCombine(GetTypeHash(Ref.FragmentType), GetTypeHash(Ref.PropertyName));
}
