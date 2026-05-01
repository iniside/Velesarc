// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Schema/ArcAbilityStateTreeSchema.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "Abilities/Conditions/ArcAbilityConditionBase.h"
#include "Abilities/Schema/ArcAbilityContext.h"
#include "Abilities/Schema/ArcAbilitySchemaNames.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "StructUtils/InstancedStruct.h"
#include "Subsystems/WorldSubsystem.h"

UArcAbilityStateTreeSchema::UArcAbilityStateTreeSchema()
	: ContextDataDescs({
		{ Arcx::MassAbilities::OwnedTags, FArcOwnedTagsFragment::StaticStruct(),
			FGuid(0xA1B2C3D4, 0xE5F60718, 0x293A4B5C, 0x6D7E8F90) },
		{ Arcx::MassAbilities::EffectStack, FArcEffectStackFragment::StaticStruct(),
			FGuid(0x1A2B3C4D, 0x5E6F7081, 0x92A3B4C5, 0xD6E7F809) },
		{ Arcx::MassAbilities::AbilityContext, FArcAbilityContext::StaticStruct(),
			FGuid(0xB1C2D3E4, 0xF5061728, 0x394A5B6C, 0x7D8E9FA0) },
		{ Arcx::MassAbilities::AbilityInput, FArcAbilityInputFragment::StaticStruct(),
			FGuid(0xC1D2E3F4, 0x05162738, 0x495A6B7C, 0x8D9EAFB0) },
		{ Arcx::MassAbilities::SourceData, FInstancedStruct::StaticStruct(),
			FGuid(0xD1E2F3A4, 0x15263748, 0x596A7B8C, 0x9DAEBFC0) },
	})
{
	ContextDataDescs[1].Requirement = EStateTreeExternalDataRequirement::Optional;
	ContextDataDescs[4].Requirement = EStateTreeExternalDataRequirement::Optional;
}

bool UArcAbilityStateTreeSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	return InScriptStruct->IsChildOf(FArcAbilityTaskBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FArcAbilityConditionBase::StaticStruct());
}

bool UArcAbilityStateTreeSchema::IsClassAllowed(const UClass* InClass) const
{
	return IsChildOfBlueprintBase(InClass);
}

bool UArcAbilityStateTreeSchema::IsExternalItemAllowed(const UStruct& InStruct) const
{
	return InStruct.IsChildOf(UWorldSubsystem::StaticClass());
}
