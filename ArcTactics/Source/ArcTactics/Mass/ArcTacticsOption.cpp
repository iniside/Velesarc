// Copyright Lukasz Baran. All Rights Reserved.

#include "Mass/ArcTacticsOption.h"

#include "UObject/UObjectGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTacticsOption)

bool FArcTacticsCandidateIdentity::operator==(const FArcTacticsCandidateIdentity& Other) const
{
	if (Type != Other.Type)
	{
		return false;
	}
	if (Type == EArcTacticsValueIdentity::None)
	{
		return true;
	}
	if (Type == EArcTacticsValueIdentity::Entity)
	{
		return Entity == Other.Entity;
	}
	return Type == EArcTacticsValueIdentity::Vector && Vector == Other.Vector;
}

bool FArcTacticsOption::IsSet() const
{
	return Action.IsValid() && ActionData.IsValid();
}

bool FArcTacticsOption::HasSameIdentity(const FArcTacticsOption& Other) const
{
	return PackageSerial == Other.PackageSerial && Action == Other.Action && Identity == Other.Identity;
}

void FArcTacticsOption::Reset()
{
	*this = FArcTacticsOption();
}

void FArcTacticsOption::AddReferencedObjects(FReferenceCollector& Collector)
{
	ActionData.AddStructReferencedObjects(Collector);
	StepData.AddStructReferencedObjects(Collector);
	ActiveStepData.AddStructReferencedObjects(Collector);
	ParameterBag.AddStructReferencedObjects(Collector);
	GlobalParameters.AddStructReferencedObjects(Collector);
	CapturedNodeData.AddStructReferencedObjects(Collector);
}

void FArcTacticsCandidate::AddReferencedObjects(FReferenceCollector& Collector)
{
	Option.AddReferencedObjects(Collector);
}
