// Copyright Lukasz Baran. All Rights Reserved.

#include "Schema/ArcEffectStateTreeSchema.h"

#include "Effects/ArcEffectStateTreeContext.h"
#include "Mass/EntityHandle.h"
#include "Mass/EntityElementTypes.h"
#include "MassEntityTypes.h"
#include "Effects/Tasks/ArcEffectTaskBase.h"
#include "Effects/Conditions/ArcEffectConditionBase.h"
#include "Subsystems/WorldSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcEffectStateTreeSchema)

namespace Arcx::MassAbilities::Effect
{
	const FName OwnerEntityHandle = TEXT("OwnerEntityHandle");
	const FName SourceEntityHandle = TEXT("SourceEntityHandle");
	const FName EffectTreeContext = TEXT("EffectTreeContext");
}

UArcEffectStateTreeSchema::UArcEffectStateTreeSchema()
	: ContextDataDescs({
		{ Arcx::MassAbilities::Effect::OwnerEntityHandle, FMassEntityHandle::StaticStruct(),
			FGuid(0xD1E2F3A4, 0xB5C60718, 0x293A4B5C, 0x6D7E8F01) },
		{ Arcx::MassAbilities::Effect::SourceEntityHandle, FMassEntityHandle::StaticStruct(),
			FGuid(0xE1F2A3B4, 0xC5D60728, 0x394A5B6C, 0x7D8E9F02) },
		{ Arcx::MassAbilities::Effect::EffectTreeContext, FArcEffectStateTreeContext::StaticStruct(),
			FGuid(0xF1A2B3C4, 0xD5E60738, 0x495A6B7C, 0x8D9EAF03) },
	})
{
}

bool UArcEffectStateTreeSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	return InScriptStruct->IsChildOf(FArcEffectTaskBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FArcEffectConditionBase::StaticStruct());
}

bool UArcEffectStateTreeSchema::IsExternalItemAllowed(const UStruct& InStruct) const
{
	return Super::IsExternalItemAllowed(InStruct)
		|| InStruct.IsChildOf(UWorldSubsystem::StaticClass())
		|| UE::Mass::IsA<FMassFragment>(&InStruct)
		|| UE::Mass::IsA<FMassSharedFragment>(&InStruct)
		|| UE::Mass::IsA<FMassConstSharedFragment>(&InStruct);
}
