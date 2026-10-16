// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcStandalonePIEComponent.h"

#if ENABLE_ARC_STANDALONE_PIE_TEST

#include "CQTest.h"

#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameMode/ArcMassGameMode.h"

// Mass
#include "MassActorSubsystem.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "MassCommonFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityTraitBase.h"
#include "Mass/EntityHandle.h"
#include "Mass/EntityFragments.h"

// ArcMass spawner
#include "ArcMass/Config/ArcMassEntityConfigAsset.h"

// Items sparse fragments & traits
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Attachments/ArcMassItemAttachmentFragments.h"
#include "Attachments/ArcMassItemAttachmentTrait.h"
#include "QuickBar/ArcMassQuickBarStateFragment.h"
#include "QuickBar/ArcMassQuickBarTrait.h"
#include "Equipment/ArcMassEquipmentFragment.h"
#include "Equipment/ArcMassEquipmentTrait.h"
#include "Equipment/ArcMassEquipmentSlotPreset.h"
#include "Traits/ArcMassItemStoreTrait.h"

// Player path
#include "Pawn/ArcMassFrameworkAgentComponent.h"
#include "Pawn/ArcMassPawnExtensionComponent.h"
#include "Pawn/ArcMassPawnData.h"
#include "Pawn/ArcMassPlayerInputComponent.h"

// Init state tags
#include "Core/ArcMassGameFrameworkTags.h"

// GameplayTags
#include "GameplayTagsManager.h"

// AI path
#include "ArcMassAgentComponent.h"

// Shared test types (stubs)
#include "ArcMassGameFrameworkTestTypes.h"

#include "ArcMassGameFrameworkTestSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcMassAgentSpawnerTest, Display, All);

namespace ArcMassAgentSpawnerTests_Private
{
	/**
	 * Build a minimal UArcMassEntityConfigAsset in-memory that seeds the four item-system
	 * sparse fragments. The outer is set to the provided world so GC keeps it alive for the
	 * duration of the test.
	 *
	 * Composition:
	 *   - UArcMassActorBridgeTestTrait  → adds FMassActorFragment + FTransformFragment to
	 *                                     the archetype so UMassAgentComponent::SetEntityHandle
	 *                                     can stamp the actor pointer.
	 *   - UArcMassItemStoreTrait        → BuildSparseFragments → FArcMassItemStoreFragment
	 *   - UArcMassItemAttachmentTrait   → BuildSparseFragments → FArcMassItemAttachmentStateFragment
	 *                                     (BuildTemplate is skipped because AttachmentConfig == null,
	 *                                      which is fine for structural sparse-fragment verification)
	 *   - UArcMassQuickBarTrait         → BuildSparseFragments → FArcMassQuickBarStateFragment
	 *   - UArcMassEquipmentTrait        → BuildSparseFragments → FArcMassEquipmentFragment
	 */
	static UArcMassEntityConfigAsset* MakeMinimalItemConfig(UWorld& World)
	{
		UArcMassEntityConfigAsset* Cfg = NewObject<UArcMassEntityConfigAsset>(&World);

		// Actor-bridge trait: adds FMassActorFragment so UMassAgentComponent::SetEntityHandle
		// can stamp the actor pointer, and FTransformFragment for the AI transform check.
		// Uses a test-only trait because UMassAssortedFragmentsTrait::Fragments is protected.
		UArcMassActorBridgeTestTrait* BridgeTrait =
			NewObject<UArcMassActorBridgeTestTrait>(Cfg);
		Cfg->GetMutableConfig().AddTrait(*BridgeTrait);

		// Item store: seeds FArcMassItemStoreFragment (sparse).
		UMassEntityTraitBase* StoreTrait =
			NewObject<UArcMassItemStoreTrait>(Cfg);
		Cfg->GetMutableConfig().AddTrait(*StoreTrait);

		// Item attachment: BuildSparseFragments seeds FArcMassItemAttachmentStateFragment.
		// BuildTemplate is skipped (AttachmentConfig == null) — intentional for this test.
		UMassEntityTraitBase* AttachTrait =
			NewObject<UArcMassItemAttachmentTrait>(Cfg);
		Cfg->GetMutableConfig().AddTrait(*AttachTrait);

		// QuickBar: seeds FArcMassQuickBarStateFragment (sparse).
		UMassEntityTraitBase* QuickBarTrait =
			NewObject<UArcMassQuickBarTrait>(Cfg);
		Cfg->GetMutableConfig().AddTrait(*QuickBarTrait);

		// Equipment: seeds FArcMassEquipmentFragment (sparse).
		UMassEntityTraitBase* EquipTrait =
			NewObject<UArcMassEquipmentTrait>(Cfg);
		Cfg->GetMutableConfig().AddTrait(*EquipTrait);

		return Cfg;
	}

	/**
	 * Build a UArcMassEntityConfigAsset for the PlayerReachesDataInitialized test.
	 * Same as MakeMinimalItemConfig but the UArcMassEquipmentTrait carries a
	 * UArcMassEquipmentSlotDefaultItems asset with 2 entries so InitializeEntity
	 * seeds the store with 2 items.
	 */
	static UArcMassEntityConfigAsset* MakeEquipmentItemConfig(UWorld& World)
	{
		UArcMassEntityConfigAsset* Cfg = NewObject<UArcMassEntityConfigAsset>(&World);

		UArcMassActorBridgeTestTrait* BridgeTrait =
			NewObject<UArcMassActorBridgeTestTrait>(Cfg);
		Cfg->GetMutableConfig().AddTrait(*BridgeTrait);

		UMassEntityTraitBase* StoreTrait =
			NewObject<UArcMassItemStoreTrait>(Cfg);
		Cfg->GetMutableConfig().AddTrait(*StoreTrait);

		UMassEntityTraitBase* AttachTrait =
			NewObject<UArcMassItemAttachmentTrait>(Cfg);
		Cfg->GetMutableConfig().AddTrait(*AttachTrait);

		UMassEntityTraitBase* QuickBarTrait =
			NewObject<UArcMassQuickBarTrait>(Cfg);
		Cfg->GetMutableConfig().AddTrait(*QuickBarTrait);

		// Build a DefaultItems asset with 2 slot entries so InitializeEntity seeds
		// the store with 2 items. SlotId tags must be registered in the project's
		// DefaultGameplayTags.ini — SlotId.Character.Chest and SlotId.Character.Head
		// are both registered there.
		UArcMassEquipmentSlotDefaultItems* DefaultItems =
			NewObject<UArcMassEquipmentSlotDefaultItems>(Cfg);

		UGameplayTagsManager& TagsManager = UGameplayTagsManager::Get();

		FArcMassEquipmentSlotDefaultItem SlotEntry1;
		SlotEntry1.SlotId = TagsManager.RequestGameplayTag(FName("SlotId.Character.Chest"));
		SlotEntry1.ItemId = FPrimaryAssetId(FPrimaryAssetType("ArcItemDefinition"), FName("TestEquipItem1"));
		DefaultItems->EquipmentSlots.Add(SlotEntry1);

		FArcMassEquipmentSlotDefaultItem SlotEntry2;
		SlotEntry2.SlotId = TagsManager.RequestGameplayTag(FName("SlotId.Character.Head"));
		SlotEntry2.ItemId = FPrimaryAssetId(FPrimaryAssetType("ArcItemDefinition"), FName("TestEquipItem2"));
		DefaultItems->EquipmentSlots.Add(SlotEntry2);

		UArcMassEquipmentTrait* EquipTrait = NewObject<UArcMassEquipmentTrait>(Cfg);
		EquipTrait->DefaultItems = DefaultItems;
		Cfg->GetMutableConfig().AddTrait(*EquipTrait);

		return Cfg;
	}
} // namespace ArcMassAgentSpawnerTests_Private

// ---------------------------------------------------------------------------
// Test 1: player path
// ---------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(ArcMassAgentSpawner,
	"ArcMassGameFramework.PawnSpawner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
	FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
		AArcMassGameMode::StaticClass() };

	const UArcMassGameFrameworkTestSettings* Settings = nullptr;
	UWorld* World = nullptr;
	UArcMassEntityConfigAsset* Cfg = nullptr;
	ADefaultPawn* Pawn = nullptr;
	UArcMassFrameworkAgentComponent* AgentComp = nullptr;
	UArcMassPawnExtensionComponentTestStub* PawnExt = nullptr;
	FMassEntityHandle SpawnedHandle;

	BEFORE_EACH()
	{
		// Mass emits Error-level logs for processors/observers with empty
		// FMassEntityQuery requirements when a new entity is created.
		// This is independent of the tests; declare it expected so CQTest
		// does not turn it into a test failure.
		TestRunner->AddExpectedError(
			TEXT("FMassEntityQuery::CacheArchetypes"),
			EAutomationExpectedErrorFlags::Contains, -1);

		// AArcMassGameMode requires an experience asset to fully initialize.
		// These tests don't load one — they only need the game world to run
		// the spawner pipeline. Declare the expected error so CQTest does
		// not treat it as a test failure.
		TestRunner->AddExpectedError(
			TEXT("Failed to identify experience"),
			EAutomationExpectedErrorFlags::Contains, -1);

		Settings = GetDefault<UArcMassGameFrameworkTestSettings>();
		if (Settings != nullptr && !Settings->TestMap.IsNull())
		{
			PIE.WithMap(Settings->TestMap.ToSoftObjectPath());
		}

		PIE.Then(TEXT("ResolveWorld"), [this]()
		{
			FAutomationTestBase::bSuppressLogErrors = true;
			FAutomationTestBase::bSuppressLogWarnings = true;

			World = PIE.GetState().World;
			ASSERT_THAT(IsNotNull(World));
			Cfg = ArcMassAgentSpawnerTests_Private::MakeMinimalItemConfig(*World);
			ASSERT_THAT(IsNotNull(Cfg));
		});
	}

	TEST_METHOD(PlayerSeedsFragmentsOnBeginPlay)
	{
		PIE
			.Then(TEXT("SpawnPawn"), [this]()
			{
				ASSERT_THAT(IsNotNull(World));

				// Spawn a DefaultPawn as the actor host. Use a deferred spawn so we can
				// attach components and set the EntityConfig before BeginPlay fires.
				Pawn = World->SpawnActorDeferred<ADefaultPawn>(
					ADefaultPawn::StaticClass(),
					FTransform::Identity,
					/*Owner*/ nullptr,
					/*Instigator*/ nullptr,
					ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

				ASSERT_THAT(IsNotNull(Pawn));

				// PawnExtension must be added first so that
				// UArcMassFrameworkAgentComponent::SetEntityHandle can find it via
				// FindComponentByClass during BeginPlay.
				PawnExt = NewObject<UArcMassPawnExtensionComponentTestStub>(Pawn);
				PawnExt->RegisterComponent();

				// Build the EntityConfig with the parent asset, then register the agent
				// component. The config must be set BEFORE RegisterComponent so OnRegister
				// picks up the correct template.
				AgentComp = NewObject<UArcMassFrameworkAgentComponent>(Pawn);

				FMassEntityConfig AgentEntityCfg;
				AgentEntityCfg.SetParentAsset(*Cfg);
				AgentComp->SetEntityConfig(AgentEntityCfg);
				AgentComp->RegisterComponent();

				// FinishSpawning drives PostInitializeComponents then BeginPlay.
				// UArcMassFrameworkAgentComponent::BeginPlay calls SpawnForAgentComponent
				// which calls EntityCreationPending, spawns the entity synchronously via
				// FEntityBuilder, and finally calls SetEntityHandle.
				Pawn->FinishSpawning(FTransform::Identity);
			})
			.Then(TEXT("AssertSeeded"), [this]()
			{
				ASSERT_THAT(IsNotNull(AgentComp));

				SpawnedHandle = AgentComp->GetEntityHandle();
				UE_LOGF(LogArcMassAgentSpawnerTest, Display,"PlayerSeedsFragmentsOnBeginPlay: Handle Index=%d Serial=%d",
					SpawnedHandle.Index, SpawnedHandle.SerialNumber);

				ASSERT_THAT(IsTrue(SpawnedHandle.IsValid()));

				UMassEntitySubsystem* MES = World->GetSubsystem<UMassEntitySubsystem>();
				ASSERT_THAT(IsNotNull(MES));
				FMassEntityManager& EntityManager = MES->GetMutableEntityManager();

				ASSERT_THAT(IsTrue(EntityManager.IsEntityBuilt(SpawnedHandle)));

				const FMassEntityView EntityView(EntityManager, SpawnedHandle);

				// --- Sparse fragments seeded by traits via BuildSparseFragments ---

				const FArcMassItemStoreFragment* ItemStore =
					EntityView.GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
				ASSERT_THAT(IsNotNull(ItemStore));

				const FArcMassItemAttachmentStateFragment* AttachState =
					EntityView.GetSparseFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
				ASSERT_THAT(IsNotNull(AttachState));

				const FArcMassQuickBarStateFragment* QuickBarState =
					EntityView.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
				ASSERT_THAT(IsNotNull(QuickBarState));

				const FArcMassEquipmentFragment* Equipment =
					EntityView.GetSparseFragmentDataPtr<FArcMassEquipmentFragment>();
				ASSERT_THAT(IsNotNull(Equipment));

				// --- Regular fragment: actor bridge ---

				const FMassActorFragment* ActorFrag =
					EntityView.GetFragmentDataPtr<FMassActorFragment>();
				ASSERT_THAT(IsNotNull(ActorFrag));
				ASSERT_THAT(IsTrue(ActorFrag->Get() == Pawn));

				// --- UMassActorSubsystem reverse-lookup ---

				UMassActorSubsystem* ActorSubsystem =
					World->GetSubsystem<UMassActorSubsystem>();
				if (ActorSubsystem != nullptr)
				{
					AActor* LookedUpActor = ActorSubsystem->GetActorFromHandle(SpawnedHandle);
					ASSERT_THAT(IsTrue(LookedUpActor == Pawn));
				}

				// --- NotifyEntityReadyImmediately ran: bMassEntityFullyInitialized was set ---
				ASSERT_THAT(IsTrue(PawnExt->Test_IsMassEntityFullyInitialized()));

				// Teardown: destroying the actor defers entity destruction.
				Pawn->Destroy();
			});
	}
};

// ---------------------------------------------------------------------------
// Test 2: player DataInitialized regression test
//
// Catches the regression class that Bug B (Pawn->InputComponent null gate stall)
// and Bug A (UArcMassEquipmentTrait::DefaultItems null seeding) belong to.
//
// Uses the real UArcMassPawnExtensionComponent (not the stub) and drives the
// full init-state chain via PC->Possess + SetPawnData, then waits for the
// extension to reach InitState_DataInitialized.
// ---------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(ArcMassAgentSpawnerPlayerInitChain,
	"ArcMassGameFramework.PawnSpawner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
	FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
		AArcMassGameMode::StaticClass() };

	const UArcMassGameFrameworkTestSettings* Settings = nullptr;
	UWorld* World = nullptr;
	UArcMassEntityConfigAsset* Cfg = nullptr;
	ADefaultPawn* Pawn = nullptr;
	UArcMassPawnExtensionComponent* PawnExt = nullptr;
	UArcMassFrameworkAgentComponent* AgentComp = nullptr;
	FMassEntityHandle SpawnedHandle;

	BEFORE_EACH()
	{
		TestRunner->AddExpectedError(
			TEXT("FMassEntityQuery::CacheArchetypes"),
			EAutomationExpectedErrorFlags::Contains, -1);

		TestRunner->AddExpectedError(
			TEXT("Failed to identify experience"),
			EAutomationExpectedErrorFlags::Contains, -1);

		// UArcCoreAssetManager will warn when it can't resolve the synthetic
		// FPrimaryAssetIds used for test equipment items. This is expected.
		TestRunner->AddExpectedError(
			TEXT("TestEquipItem"),
			EAutomationExpectedErrorFlags::Contains, -1);

		Settings = GetDefault<UArcMassGameFrameworkTestSettings>();
		if (Settings != nullptr && !Settings->TestMap.IsNull())
		{
			PIE.WithMap(Settings->TestMap.ToSoftObjectPath());
		}

		PIE.Then(TEXT("ResolveWorld"), [this]()
		{
			FAutomationTestBase::bSuppressLogErrors = true;
			FAutomationTestBase::bSuppressLogWarnings = true;

			World = PIE.GetState().World;
			ASSERT_THAT(IsNotNull(World));
			Cfg = ArcMassAgentSpawnerTests_Private::MakeEquipmentItemConfig(*World);
			ASSERT_THAT(IsNotNull(Cfg));
		});
	}

	TEST_METHOD(PlayerReachesDataInitialized)
	{
		PIE
			.Then(TEXT("SpawnAndPossess"), [this]()
			{
				ASSERT_THAT(IsNotNull(World));

				// Spawn ADefaultPawn deferred so we can attach components before BeginPlay.
				Pawn = World->SpawnActorDeferred<ADefaultPawn>(
					ADefaultPawn::StaticClass(),
					FTransform::Identity,
					/*Owner*/ nullptr,
					/*Instigator*/ nullptr,
					ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				ASSERT_THAT(IsNotNull(Pawn));

				// Add the real UArcMassPawnExtensionComponent so the init-state machine
				// runs its full production path (Bug B and Bug A regression coverage).
				PawnExt = NewObject<UArcMassPawnExtensionComponent>(Pawn);
				PawnExt->RegisterComponent();

				// Add the agent component with the equipment entity config.
				AgentComp = NewObject<UArcMassFrameworkAgentComponent>(Pawn);
				FMassEntityConfig AgentEntityCfg;
				AgentEntityCfg.SetParentAsset(*Cfg);
				AgentComp->SetEntityConfig(AgentEntityCfg);
				AgentComp->RegisterComponent();

				// FinishSpawning fires PostInitializeComponents then BeginPlay.
				// UArcMassFrameworkAgentComponent::BeginPlay spawns the entity via
				// FEntityBuilder and calls SetEntityHandle which triggers
				// NotifyEntityReadyImmediately on PawnExt.
				Pawn->FinishSpawning(FTransform::Identity);

				// Assign PawnData (requires authority; we're the server in a standalone PIE).
				// This is gate 1 in CanChangeInitState[Spawned->DataAvailable].
				UArcMassPawnData* PawnData = NewObject<UArcMassPawnData>(World);
				PawnExt->SetPawnData(PawnData);

				// Possess drives PossessedBy, which wires InputComponent on the pawn (gate 3
				// fix: Bug B) and pairs the PlayerState with the controller (gate 2b).
				APlayerController* PC = World->GetFirstPlayerController();
				ASSERT_THAT(IsNotNull(PC));
				PC->Possess(Pawn);
			})
			.Until(TEXT("WaitForDataInitialized"), [this]()
			{
				if (PawnExt == nullptr)
				{
					return false;
				}
				const FGameplayTag CurrentState = PawnExt->GetInitState();
				return CurrentState == FArcMassGameFrameworkTags::Get().InitState_DataInitialized;
			},
			FTimespan::FromSeconds(15.0))
			.Then(TEXT("AssertDataInitialized"), [this]()
			{
				ASSERT_THAT(IsNotNull(PawnExt));

				const FGameplayTag CurrentState = PawnExt->GetInitState();
				ASSERT_THAT(IsTrue(CurrentState == FArcMassGameFrameworkTags::Get().InitState_DataInitialized));

				// Entity must be valid and built.
				ASSERT_THAT(IsNotNull(AgentComp));
				SpawnedHandle = AgentComp->GetEntityHandle();
				UE_LOGF(LogArcMassAgentSpawnerTest, Display,"PlayerReachesDataInitialized: Handle Index=%d Serial=%d",
					SpawnedHandle.Index, SpawnedHandle.SerialNumber);
				ASSERT_THAT(IsTrue(SpawnedHandle.IsValid()));

				UMassEntitySubsystem* MES = World->GetSubsystem<UMassEntitySubsystem>();
				ASSERT_THAT(IsNotNull(MES));
				FMassEntityManager& EntityManager = MES->GetMutableEntityManager();
				ASSERT_THAT(IsTrue(EntityManager.IsEntityBuilt(SpawnedHandle)));

				const FMassEntityView EntityView(EntityManager, SpawnedHandle);

				// Bug A regression: DefaultItems != null means InitializeEntity ran and
				// seeded 2 items into the store. If the null-gate bug were present, the
				// equipment trait would skip seeding and Items.Num() would be 0.
				const FArcMassItemStoreFragment* ItemStore =
					EntityView.GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
				ASSERT_THAT(IsNotNull(ItemStore));
				ASSERT_THAT(AreEqual(2, ItemStore->ReplicatedItems.Items.Num()));

				// Bug B regression: UArcMassPlayerInputComponent is auto-spawned by
				// UArcMassPawnExtensionComponent::HandleChangeInitState when InputComponent
				// is non-null on a locally-controlled non-bot pawn. If Bug B were present
				// the gate would have stalled at DataAvailable and this component would be
				// absent.
				const UArcMassPlayerInputComponent* InputComp =
					Pawn->FindComponentByClass<UArcMassPlayerInputComponent>();
				ASSERT_THAT(IsNotNull(InputComp));

				Pawn->Destroy();
			});
	}
};

// ---------------------------------------------------------------------------
// Test 3: AI path
// ---------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(ArcMassAgentSpawnerAI,
	"ArcMassGameFramework.PawnSpawner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
	FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
		AArcMassGameMode::StaticClass() };

	const UArcMassGameFrameworkTestSettings* Settings = nullptr;
	UWorld* World = nullptr;
	UArcMassEntityConfigAsset* Cfg = nullptr;
	ADefaultPawn* Pawn = nullptr;
	UArcMassAgentComponent* AgentComp = nullptr;
	FMassEntityHandle SpawnedHandle;

	BEFORE_EACH()
	{
		TestRunner->AddExpectedError(
			TEXT("FMassEntityQuery::CacheArchetypes"),
			EAutomationExpectedErrorFlags::Contains, -1);

		TestRunner->AddExpectedError(
			TEXT("Failed to identify experience"),
			EAutomationExpectedErrorFlags::Contains, -1);

		Settings = GetDefault<UArcMassGameFrameworkTestSettings>();
		if (Settings != nullptr && !Settings->TestMap.IsNull())
		{
			PIE.WithMap(Settings->TestMap.ToSoftObjectPath());
		}

		PIE.Then(TEXT("ResolveWorld"), [this]()
		{
			FAutomationTestBase::bSuppressLogErrors = true;
			FAutomationTestBase::bSuppressLogWarnings = true;

			World = PIE.GetState().World;
			ASSERT_THAT(IsNotNull(World));
			Cfg = ArcMassAgentSpawnerTests_Private::MakeMinimalItemConfig(*World);
			ASSERT_THAT(IsNotNull(Cfg));
		});
	}

	TEST_METHOD(AISeedsFragmentsOnBeginPlay)
	{
		PIE
			.Then(TEXT("SpawnPawn"), [this]()
			{
				ASSERT_THAT(IsNotNull(World));

				Pawn = World->SpawnActorDeferred<ADefaultPawn>(
					ADefaultPawn::StaticClass(),
					FTransform::Identity,
					/*Owner*/ nullptr,
					/*Instigator*/ nullptr,
					ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

				ASSERT_THAT(IsNotNull(Pawn));

				// UArcMassAgentComponent is the AI subclass. No PawnExtension needed.
				AgentComp = NewObject<UArcMassAgentComponent>(Pawn);

				FMassEntityConfig AgentEntityCfg;
				AgentEntityCfg.SetParentAsset(*Cfg);
				AgentComp->SetEntityConfig(AgentEntityCfg);
				AgentComp->RegisterComponent();

				// Drive BeginPlay via FinishSpawning.
				const FTransform SpawnTransform(FQuat::Identity, FVector(100.f, 200.f, 300.f));
				Pawn->FinishSpawning(SpawnTransform);
			})
			.Then(TEXT("AssertSeeded"), [this]()
			{
				ASSERT_THAT(IsNotNull(AgentComp));

				SpawnedHandle = AgentComp->GetEntityHandle();
				UE_LOGF(LogArcMassAgentSpawnerTest, Display,"AISeedsFragmentsOnBeginPlay: Handle Index=%d Serial=%d",
					SpawnedHandle.Index, SpawnedHandle.SerialNumber);

				ASSERT_THAT(IsTrue(SpawnedHandle.IsValid()));

				UMassEntitySubsystem* MES = World->GetSubsystem<UMassEntitySubsystem>();
				ASSERT_THAT(IsNotNull(MES));
				FMassEntityManager& EntityManager = MES->GetMutableEntityManager();

				ASSERT_THAT(IsTrue(EntityManager.IsEntityBuilt(SpawnedHandle)));

				const FMassEntityView EntityView(EntityManager, SpawnedHandle);

				// --- Actor bridge ---

				const FMassActorFragment* ActorFrag =
					EntityView.GetFragmentDataPtr<FMassActorFragment>();
				ASSERT_THAT(IsNotNull(ActorFrag));
				ASSERT_THAT(IsTrue(ActorFrag->Get() == Pawn));

				// --- UArcMassAgentComponent::SetEntityHandle syncs FTransformFragment ---

				const FTransformFragment* TransformFrag =
					EntityView.GetFragmentDataPtr<FTransformFragment>();
				if (TransformFrag != nullptr)
				{
					const FVector EntityLocation = TransformFrag->GetTransform().GetLocation();
					const FVector ActorLocation  = Pawn->GetActorLocation();
					const float Delta = FVector::Dist(EntityLocation, ActorLocation);
					ASSERT_THAT(IsTrue(Delta < 1.f));
				}

				// Teardown
				Pawn->Destroy();
			});
	}
};

#endif // ENABLE_ARC_STANDALONE_PIE_TEST
