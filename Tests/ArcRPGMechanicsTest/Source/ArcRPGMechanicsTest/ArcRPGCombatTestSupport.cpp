// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRPGCombatTestSupport.h"

#include "ArcCombatTestHarness.h"
#include "ArcRPGMechanicsTestHarness.h"
#include "ArcRPGMechTestSupport.h"
#include "ArcMassMechanics/Combat/ArcHitTypes.h"
#include "AssetRegistry/AssetData.h"
#include "EditorValidatorBase.h"
#include "Effects/ArcEffectDefinition.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemId.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Sources/ArcSourceItemResolver.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "UObject/Package.h"

void ArcRPGCombatTest::SuppressTestLogs()
{
	FAutomationTestBase::bSuppressLogWarnings = true;
	FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
	FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
	FAutomationTestBase::SuppressedLogCategories.AddUnique("LogStateTree");
	FAutomationTestBase::SuppressedLogCategories.AddUnique("LogMass");
}

FGameplayTagContainer ArcRPGCombatTest::Tags(std::initializer_list<FGameplayTag> InTags)
{
	FGameplayTagContainer Container;
	for (const FGameplayTag& Tag : InTags)
	{
		Container.AddTag(Tag);
	}
	return Container;
}

UArcEffectDefinition* ArcRPGCombatTest::LoadCombat(const TCHAR* Name)
{
	return ArcRPGMechTest::LoadTestAsset<UArcEffectDefinition>(*FString::Printf(TEXT("Combat/%s"), Name));
}

UArcEffectDefinition* ArcRPGCombatTest::LoadTrigger(const TCHAR* Name)
{
	return ArcRPGMechTest::LoadTestAsset<UArcEffectDefinition>(*FString::Printf(TEXT("Triggers/%s"), Name));
}

UArcEffectDefinition* ArcRPGCombatTest::LoadStatus(const TCHAR* Name)
{
	return ArcRPGMechTest::LoadTestAsset<UArcEffectDefinition>(*FString::Printf(TEXT("Statuses/%s"), Name));
}

const FArcDamageDealtPayload* ArcRPGCombatTest::Strike(ArcCombatTest::FCombatHarness& Harness, FMassEntityHandle Defender,
	FMassEntityHandle Attacker, UArcEffectDefinition* Effect, const FInstancedStruct& SourceData)
{
	if (!Effect)
	{
		return nullptr;
	}
	const int32 EventsBefore = Harness.CountDamageEvents();
	Harness.Apply(Effect, Defender, Attacker, SourceData);
	return Harness.CountDamageEvents() == EventsBefore + 1 ? Harness.FindLastDamage() : nullptr;
}

float ArcRPGCombatTest::StrikeFinal(ArcCombatTest::FCombatHarness& Harness, FMassEntityHandle Defender, FMassEntityHandle Attacker,
	UArcEffectDefinition* Effect, const FInstancedStruct& SourceData)
{
	const FArcDamageDealtPayload* Payload = Strike(Harness, Defender, Attacker, Effect, SourceData);
	return Payload ? Payload->Final : 0.f;
}

FArcItemId ArcRPGCombatTest::Equip(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FArcItemDefinitionData* Definition,
	const FGameplayTag& Slot)
{
	FMassEntityView View(EntityManager, Entity);
	FArcMassItemStoreFragment* Store = View.GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
	check(Store);

	FArcMassReplicatedItem Replicated;
	Replicated.ItemData.InitializeAs<FArcItemData>();
	FArcItemData* Item = Replicated.ToItem();
	Item->ItemDefinition = Definition;
	Item->ItemId = FArcItemId::Generate();
	Item->Slot = Slot;
	Item->bIsActive = true;
	Item->InitializeMass(nullptr);
	const FArcItemId ItemId = Item->ItemId;

	Store->ReplicatedItems.AddItem(MoveTemp(Replicated));
	return ItemId;
}

bool ArcRPGCombatTest::MakeSkillWeapon(FArcItemData& OutItem)
{
	const FArcItemDefinitionData* Definition = ArcRPGMechanicsTestHarness::LoadItem(TEXT("ID_Weapon_Test"));
	if (!Definition)
	{
		return false;
	}
	ArcRPGMechanicsTestHarness::MakeItem(Definition, OutItem);
	return true;
}

FInstancedStruct ArcRPGCombatTest::MakeItemSource(const FArcItemData& Item)
{
	return FInstancedStruct::Make(ArcAbilityKit::MakeItemSource(&Item, FGameplayTag()));
}

UEditorValidatorBase* ArcRPGCombatTest::MakeValidator(const TCHAR* ClassPath)
{
	UClass* ValidatorClass = FindObject<UClass>(nullptr, ClassPath);
	return ValidatorClass ? NewObject<UEditorValidatorBase>(GetTransientPackage(), ValidatorClass) : nullptr;
}

bool ArcRPGCombatTest::FailsWith(UEditorValidatorBase& Validator, UObject* Asset, const TCHAR* Fragment)
{
	FDataValidationContext Context;
	if (Validator.ValidateLoadedAsset(FAssetData(), Asset, Context) != EDataValidationResult::Invalid)
	{
		return false;
	}
	for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
	{
		if (Issue.Message.ToString().Contains(Fragment))
		{
			return true;
		}
	}
	return false;
}

bool ArcRPGCombatTest::Passes(UEditorValidatorBase& Validator, UObject* Asset)
{
	FDataValidationContext Context;
	return Validator.ValidateLoadedAsset(FAssetData(), Asset, Context) == EDataValidationResult::Valid;
}
