// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRPGMechanicsTestHarness.h"

#include "ArcRPGMechanicsTestTypes.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "ArcMassDamageSystem/ArcAttributeSet_Health.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Attributes/ArcAttributeSet_RPGCoreStats.h"
#include "Attributes/ArcAttributeSet_RPGEnergy.h"
#include "Attributes/ArcAttributeSet_RPGEntropy.h"
#include "Attributes/ArcAttributeSet_RPGHealthExtension.h"
#include "Attributes/ArcAttributeSet_RPGStamina.h"
#include "Components/ActorTestSpawner.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Fragments/ArcAbilityCooldownFragment.h"
#include "Fragments/ArcAbilityCustomData.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcAttributeHandlerSharedFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Fragments/ArcRPGItemFragment_CascadingCost.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemId.h"
#include "Items/ArcItemsHelpers.h"
#include "Mass/EntityBuilder.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "Processors/ArcAbilityInputProcessor.h"
#include "Processors/ArcAbilityStateTreeTickProcessor.h"
#include "Processors/ArcCooldownProcessor.h"
#include "Processors/ArcPendingAttributeOpsProcessor.h"
#include "Signals/ArcAbilitySignals.h"
#include "Sources/ArcAbilitySource_Item.h"

const FArcItemDefinitionData* ArcRPGMechanicsTestHarness::LoadItem(const TCHAR* Name)
{
	struct FItemRecord
	{
		const TCHAR* Name;
		const TCHAR* Guid;
	};
	static const FItemRecord Records[] =
	{
		{ TEXT("ID_Armor_Shield40"), TEXT("19DCB309496113C8A043C4BB6B710289") },
		{ TEXT("ID_Armor_Torso60"), TEXT("E73D54124F76E54489BC60845CEF79CE") },
		{ TEXT("ID_Armor_Torso60_Slash"), TEXT("AAB4DB8D4A5C64ABC7215EB93E41BCE4") },
		{ TEXT("ID_Spell_Cascade0_5"), TEXT("B6306D764B0309DA9A71CF9A15DF3B68") },
		{ TEXT("ID_Spell_Cascade2"), TEXT("434DE89B43D6F2D85BD1F190BE177A7C") },
		{ TEXT("ID_Spell_Cascade3"), TEXT("7EC855BC4201321443DFC68F52493DB3") },
		{ TEXT("ID_Spell_EnergyHealth"), TEXT("497CE5104DA2679819AF7DA1EB187E2B") },
		{ TEXT("ID_Spell_Entropy"), TEXT("23BCAEFD41BA7DA5621300A3E9F33576") },
		{ TEXT("ID_Spell_EntropyB"), TEXT("0FA888C64C28F661911EA1B6A5D6F229") },
		{ TEXT("ID_Spell_Stamina"), TEXT("D1FA4EB348293570DB82EDBA709D9504") },
		{ TEXT("ID_Weapon_Test"), TEXT("163744194C14642C861A9BBF1A322BBC") },
	};

	for (const FItemRecord& Record : Records)
	{
		if (FCString::Strcmp(Record.Name, Name) == 0)
		{
			const FPrimaryAssetId Id(FArcItemDefinitionData::JsonPrimaryAssetType(), FName(Record.Guid));
			const FArcItemDefinitionData* Definition = FArcItemDefinitionData::Find(Id);
			if (Definition == nullptr)
			{
				ArcMechTest::ReportMissingTestAsset(Id.ToString(), nullptr);
			}
			return Definition;
		}
	}
	return nullptr;
}

UArcAbilityDefinition* ArcRPGMechanicsTestHarness::LoadCostAbility(const TCHAR* Name)
{
	return ArcRPGMechTest::LoadTestAsset<UArcAbilityDefinition>(*FString::Printf(TEXT("Costs/%s"), Name));
}

void ArcRPGMechanicsTestHarness::MakeItem(const FArcItemDefinitionData* Definition, FArcItemData& OutItem)
{
	OutItem.Spec.SetItemDefinition(Definition->GetPrimaryAssetId());
	OutItem.ItemDefinition = Definition;
	OutItem.ItemId = FArcItemId::Generate();
	OutItem.InitializeMass(nullptr);
}

FArcAbilityCustomData ArcRPGMechanicsTestHarness::MakeItemCustomData(FArcItemData& Item)
{
	FArcAbilitySource_Item Source;
	Source.ItemDataHandle = FArcItemDataHandle(&Item);
	FArcAbilityCustomData CustomData;
	CustomData.AddOrReplace<FArcAbilitySource_Item>(MoveTemp(Source));
	return CustomData;
}

int32 ArcRPGMechanicsTestHarness::GetCascadeStacks(const FArcItemData& Item)
{
	const FArcRPGItemInstance_CascadingCost* Cascade = ArcItemsHelper::FindInstance<FArcRPGItemInstance_CascadingCost>(&Item);
	return Cascade ? Cascade->GetStacks() : -1;
}

void ArcRPGMechanicsTestHarness::AddCascadeStacks(const FArcItemData& Item, int32 Count)
{
	FArcRPGItemInstance_CascadingCost* Cascade = ArcItemsHelper::FindMutableInstance<FArcRPGItemInstance_CascadingCost>(&Item);
	check(Cascade);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Cascade->AddStack();
	}
}

bool ArcRPGMechanicsTestHarness::FHarness::Initialize(FActorTestSpawner& Spawner)
{
	LogScope.Emplace();
	ArcRPGMechanicsTest::ResetCounts();

	Spawner.GetWorld();
	Spawner.InitializeGameSubsystems();
	World = &Spawner.GetWorld();

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
	if (!EntitySubsystem || !SignalSubsystem)
	{
		return false;
	}
	EntityManager = &EntitySubsystem->GetMutableEntityManager();
	HandlerConfig = ArcRPGMechTest::LoadTestAsset<UArcAttributeHandlerConfig>(TEXT("Handlers/AHC_RPG_Test"));
	if (!HandlerConfig)
	{
		return false;
	}

	PendingProcessor = NewObject<UArcPendingAttributeOpsProcessor>();
	PendingProcessor->CallInitialize(EntityManager->GetOwner(), EntityManager->AsShared());

	CooldownProcessor = NewObject<UArcCooldownProcessor>();
	CooldownProcessor->CallInitialize(World, EntityManager->AsShared());
	return true;
}

FMassEntityHandle ArcRPGMechanicsTestHarness::FHarness::CreateCaster(const FCasterOptions& Options)
{
	FArcAttributeSet_RPGEnergy Energy;
	Energy.InitEnergy(Options.Energy);
	Energy.InitMaxEnergy(10000.f);
	Energy.InitAddEnergy(0.f);
	Energy.InitRemoveEnergy(0.f);

	FArcAttributeSet_RPGStamina Stamina;
	Stamina.InitStamina(Options.Stamina);
	Stamina.InitMaxStamina(10000.f);
	Stamina.InitAddStamina(0.f);
	Stamina.InitRemoveStamina(0.f);

	FArcAttributeSet_Health Health;
	Health.InitHealth(Options.Health);
	Health.InitMaxHealth(10000.f);

	FArcAttributeSet_RPGHealthExtension HealthExtension;
	HealthExtension.InitDegenDamage(0.f);

	FArcAttributeSet_RPGEntropy Entropy;
	Entropy.InitEntropy(Options.Entropy);
	Entropy.InitMaxEntropy(10000.f);
	Entropy.InitThresholdEntropy(Options.ThresholdEntropy);
	Entropy.InitAddEntropy(0.f);
	Entropy.InitRemoveEntropy(0.f);

	// Neutral core stats (modifier 0) keep the magic.yaml cost factors at 1.
	FArcAttributeSet_RPGCoreStats CoreStats;
	CoreStats.InitIntelligence(10.f);
	CoreStats.InitWillpower(10.f);
	CoreStats.InitWisdom(10.f);

	UE::Mass::FEntityBuilder Builder(*EntityManager);
	Builder.Add<FArcAttributeSet_RPGEnergy>(Energy)
		.Add<FArcAttributeSet_RPGCoreStats>(CoreStats)
		.Add<FArcAttributeSet_RPGStamina>(Stamina)
		.Add<FArcAttributeSet_Health>(Health)
		.Add<FArcAttributeSet_RPGHealthExtension>(HealthExtension)
		.Add<FArcAttributeSet_RPGEntropy>(Entropy)
		.Add<FArcAbilityCollectionFragment>()
		.Add<FArcAbilityInputFragment>()
		.Add<FArcOwnedTagsFragment>()
		.Add<FArcAbilityCooldownFragment>()
		.Add<FArcEffectStackFragment>()
		.Add<FArcAggregatorFragment>()
		.Add<FArcPendingAttributeOpsFragment>();

	if (Options.bHandlerConfig)
	{
		FArcAttributeHandlerSharedFragment HandlerShared;
		HandlerShared.Config = HandlerConfig;
		Builder.Add<FArcAttributeHandlerSharedFragment>(HandlerShared);
	}

	return Builder.Commit();
}

void ArcRPGMechanicsTestHarness::FHarness::Activate(FMassEntityHandle Entity, FGameplayTag InputTag)
{
	ArcAbilities::PushInput(*EntityManager, Entity, InputTag);

	UArcAbilityInputProcessor* InputProcessor = NewObject<UArcAbilityInputProcessor>();
	InputProcessor->CallInitialize(World, EntityManager->AsShared());
	SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityInputReceived, Entity);
	UE::Mass::FProcessingContext InputContext(*EntityManager, 0.016f);
	InputProcessor->CallExecute(*EntityManager, InputContext.GetExecutionContext());

	UArcAbilityStateTreeTickProcessor* TickProcessor = NewObject<UArcAbilityStateTreeTickProcessor>();
	TickProcessor->CallInitialize(World, EntityManager->AsShared());
	SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, Entity);
	UE::Mass::FProcessingContext TickContext(*EntityManager, 0.016f);
	TickProcessor->CallExecute(*EntityManager, TickContext.GetExecutionContext());
}

void ArcRPGMechanicsTestHarness::FHarness::DrainPendingOps(FMassEntityHandle Entity)
{
	SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::PendingAttributeOps, Entity);
	FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
	UE::Mass::Executor::Run(*PendingProcessor, ProcessingContext);
}

void ArcRPGMechanicsTestHarness::FHarness::TickCooldowns(float DeltaSeconds)
{
	FMassProcessingContext ProcessingContext(*EntityManager, DeltaSeconds);
	UE::Mass::Executor::Run(*CooldownProcessor, ProcessingContext);
}

FArcAttributeSet_RPGEnergy* ArcRPGMechanicsTestHarness::FHarness::GetEnergy(FMassEntityHandle Entity) const
{
	FMassEntityView EntityView(*EntityManager, Entity);
	return EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_RPGEnergy>();
}

FArcAttributeSet_RPGStamina* ArcRPGMechanicsTestHarness::FHarness::GetStamina(FMassEntityHandle Entity) const
{
	FMassEntityView EntityView(*EntityManager, Entity);
	return EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_RPGStamina>();
}

FArcAttributeSet_RPGEntropy* ArcRPGMechanicsTestHarness::FHarness::GetEntropy(FMassEntityHandle Entity) const
{
	FMassEntityView EntityView(*EntityManager, Entity);
	return EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_RPGEntropy>();
}

FArcAttributeSet_Health* ArcRPGMechanicsTestHarness::FHarness::GetHealth(FMassEntityHandle Entity) const
{
	FMassEntityView EntityView(*EntityManager, Entity);
	return EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_Health>();
}

FArcAttributeSet_RPGHealthExtension* ArcRPGMechanicsTestHarness::FHarness::GetHealthExtension(FMassEntityHandle Entity) const
{
	FMassEntityView EntityView(*EntityManager, Entity);
	return EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_RPGHealthExtension>();
}

int32 ArcRPGMechanicsTestHarness::FHarness::CountCooldownEntries(FMassEntityHandle Entity) const
{
	FMassEntityView EntityView(*EntityManager, Entity);
	const FArcAbilityCooldownFragment* Cooldowns = EntityView.GetFragmentDataPtr<FArcAbilityCooldownFragment>();
	return Cooldowns ? Cooldowns->ActiveCooldowns.Num() : -1;
}
