// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/ArcEffectDefinition.h"

void UArcEffectDefinition::PostInitProperties()
{
	Super::PostInitProperties();
	if (!EffectId.IsValid() && !HasAnyFlags(RF_ClassDefaultObject))
	{
		EffectId = FGuid::NewGuid();
	}
}

void UArcEffectDefinition::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);
	if (DuplicateMode == EDuplicateMode::Normal)
	{
		EffectId = FGuid::NewGuid();
	}
}

void UArcEffectDefinition::RegenerateEffectId()
{
	EffectId = FGuid::NewGuid();
	MarkPackageDirty();
}

FPrimaryAssetId UArcEffectDefinition::GetPrimaryAssetId() const
{
	if (EffectType.IsValid())
	{
		return FPrimaryAssetId(EffectType, GetFName());
	}
	return Super::GetPrimaryAssetId();
}

void UArcEffectDefinition::PostLoad()
{
	Super::PostLoad();

	uint8 RawDuration = static_cast<uint8>(StackingPolicy.DurationType);
	if (RawDuration == 3)
	{
		StackingPolicy.DurationType = EArcEffectDuration::Duration;
		StackingPolicy.Periodicity = EArcEffectPeriodicity::Periodic;
		MarkPackageDirty();
	}
}
