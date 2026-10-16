// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemId.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/InstancedStruct.h"

class UArcEffectDefinition;
struct FArcItemDefinitionData;
class UEditorValidatorBase;
struct FArcDamageDealtPayload;
struct FArcItemData;
struct FMassEntityManager;

namespace ArcCombatTest
{
	class FCombatHarness;
}

namespace ArcRPGCombatTest
{
	void SuppressTestLogs();

	FGameplayTagContainer Tags(std::initializer_list<FGameplayTag> InTags);

	UArcEffectDefinition* LoadCombat(const TCHAR* Name);

	UArcEffectDefinition* LoadTrigger(const TCHAR* Name);

	UArcEffectDefinition* LoadStatus(const TCHAR* Name);

	// Applies the strike and returns the payload of the one damage event it published, or null when it published none.
	const FArcDamageDealtPayload* Strike(ArcCombatTest::FCombatHarness& Harness, FMassEntityHandle Defender, FMassEntityHandle Attacker,
		UArcEffectDefinition* Effect, const FInstancedStruct& SourceData = FInstancedStruct());

	// Final damage of the strike's damage event; 0 when it published none.
	float StrikeFinal(ArcCombatTest::FCombatHarness& Harness, FMassEntityHandle Defender, FMassEntityHandle Attacker,
		UArcEffectDefinition* Effect, const FInstancedStruct& SourceData = FInstancedStruct());

	// Adds an active item in Slot to the entity's item store; the zone sources read slotted, active items.
	FArcItemId Equip(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FArcItemDefinitionData* Definition, const FGameplayTag& Slot);

	// Builds OutItem from Items/ID_Weapon_Test, the item carrying the Skill Scaling fragment and Weapon Damage; false when it does not load.
	bool MakeSkillWeapon(FArcItemData& OutItem);

	// Effect source data naming Item; OutItem must outlive the spec build, the source item is read through its data handle.
	FInstancedStruct MakeItemSource(const FArcItemData& Item);

	UEditorValidatorBase* MakeValidator(const TCHAR* ClassPath);

	// True when validating Asset is Invalid and one issue message contains Fragment.
	bool FailsWith(UEditorValidatorBase& Validator, UObject* Asset, const TCHAR* Fragment);

	bool Passes(UEditorValidatorBase& Validator, UObject* Asset);
}
