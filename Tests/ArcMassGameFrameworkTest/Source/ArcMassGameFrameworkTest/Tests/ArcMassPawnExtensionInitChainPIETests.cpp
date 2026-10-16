// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcStandalonePIEComponent.h"

#if ENABLE_ARC_STANDALONE_PIE_TEST

#include "CQTest.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "ArcMassGameFrameworkTestSettings.h"
#include "Core/ArcMassGameFrameworkTags.h"
#include "GameMode/ArcMassGameMode.h"
#include "Pawn/ArcMassFrameworkAgentComponent.h"
#include "Pawn/ArcMassPawnData.h"
#include "Pawn/ArcMassPawnExtensionComponent.h"
#include "Player/ArcMassPlayerController.h"

#include "MassActorSubsystem.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/EntityHandle.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcMassPawnExtInitPIETest, Display, All);

/**
 * PIE-tier positive path for the PawnExtension init chain.
 *
 * Extracted from ArcMassPawnExtensionInitChainTests.cpp where
 * CanChangeInitState_PassesWithAssetDrivenSetup bailed at line ~306 with
 * "Skipped: UMassEntitySubsystem unavailable in FActorTestSpawner world."
 *
 * Under FArcStandalonePIEComponent the full UMassAgentSubsystem-driven entity
 * creation pipeline is available. The production BeginPlay -> SpawnForAgentComponent
 * path calls UArcMassFrameworkAgentComponent::SetEntityHandle internally, so no
 * public test seam is required — the assertion is read-only via GetEntityHandle().
 */
TEST_CLASS_WITH_FLAGS(ArcMassPawnExtension_InitChain_PIE,
	"ArcMassGameFramework.PIE.PawnExtension",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
	FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
		AArcMassGameMode::StaticClass() };

	const UArcMassGameFrameworkTestSettings* Settings = nullptr;
	UArcMassPawnExtensionComponent* Extension = nullptr;
	UArcMassFrameworkAgentComponent* AgentComp = nullptr;
	FGameplayTag LastSeenInitState;

	BEFORE_EACH()
	{
		// Mass emits Error-level logs for processors with empty FMassEntityQuery
		// requirements during PIE startup. Declare them expected so CQTest does
		// not treat them as failures.
		TestRunner->AddExpectedError(
			TEXT("FMassEntityQuery::CacheArchetypes: requirements not valid"),
			EAutomationExpectedErrorFlags::Contains, 0);

		Settings = GetDefault<UArcMassGameFrameworkTestSettings>();
		if (Settings == nullptr || Settings->TestMap.IsNull()
			|| Settings->TestPawnClass.IsNull() || Settings->TestPawnData.IsNull())
		{
			TestRunner->AddInfo(TEXT("Skipped: TestMap/TestPawnClass/TestPawnData is null on this checkout."));
			return;
		}

		PIE.WithMap(Settings->TestMap.ToSoftObjectPath());
	}

	/**
	 * Positive path: spawn the asset-configured TestPawnClass, possess with
	 * AArcMassPlayerController, set PawnData on the PawnExtensionComponent, then
	 * poll until InitState_DataInitialized.  Asserts:
	 *   - Extension reaches DataInitialized
	 *   - AgentComp has a valid FMassEntityHandle (wired by production BeginPlay)
	 *   - FMassActorFragment points back to the pawn
	 *   - PawnData survived to final state
	 */
	TEST_METHOD(CanChangeInitState_PassesWithAssetDrivenSetup_PIE)
	{
		if (Settings == nullptr || Settings->TestMap.IsNull()
			|| Settings->TestPawnClass.IsNull() || Settings->TestPawnData.IsNull())
		{
			return;
		}

		PIE.Then(TEXT("SpawnPawnAndPossess"), [this]()
		{
			UWorld* World = PIE.GetState().World;
			ASSERT_THAT(IsNotNull(World));

			UClass* PawnClass = Settings->TestPawnClass.LoadSynchronous();
			ASSERT_THAT(IsNotNull(PawnClass));

			APawn* Pawn = World->SpawnActor<APawn>(
				PawnClass, FTransform::Identity, FActorSpawnParameters{});
			ASSERT_THAT(IsNotNull(Pawn));

			Extension = Pawn->FindComponentByClass<UArcMassPawnExtensionComponent>();
			ASSERT_THAT(IsNotNull(Extension));

			AgentComp = Pawn->FindComponentByClass<UArcMassFrameworkAgentComponent>();
			ASSERT_THAT(IsNotNull(AgentComp));

			UArcMassPawnData* PawnData = Settings->TestPawnData.LoadSynchronous();
			ASSERT_THAT(IsNotNull(PawnData));

			Extension->SetPawnData(PawnData);

			AArcMassPlayerController* PC = World->SpawnActor<AArcMassPlayerController>(
				AArcMassPlayerController::StaticClass(), FTransform::Identity, FActorSpawnParameters{});
			ASSERT_THAT(IsNotNull(PC));

			PC->bAutoManageActiveCameraTarget = false;
			PC->Possess(Pawn);

			UE_LOGF(LogArcMassPawnExtInitPIETest, Display,
				"SpawnPawnAndPossess: Pawn=%p Extension=%p AgentComp=%p PC=%p",
				Pawn, Extension, AgentComp, PC);
		})
		.Until(TEXT("Wait for InitState_DataInitialized"),
			[this]()
			{
				const FGameplayTag CurrentState = Extension ? Extension->GetInitState() : FGameplayTag();
				const FGameplayTag Target = FArcMassGameFrameworkTags::Get().InitState_DataInitialized;
				if (CurrentState != LastSeenInitState)
				{
					UE_LOGF(LogArcMassPawnExtInitPIETest, Display,
						"WaitInit: state changed '%ls' -> '%ls' (target='%ls')",
						*LastSeenInitState.ToString(), *CurrentState.ToString(), *Target.ToString());
					LastSeenInitState = CurrentState;
				}
				return Extension != nullptr && CurrentState == Target;
			},
			FTimespan::FromSeconds(30.0))
		.Then(TEXT("VerifyWiring"), [this]()
		{
			ASSERT_THAT(IsNotNull(Extension));
			ASSERT_THAT(IsNotNull(AgentComp));

			const FGameplayTag CurrentState = Extension->GetInitState();
			UE_LOGF(LogArcMassPawnExtInitPIETest, Display,
				"Verify: Extension=%p State='%ls'", Extension, *CurrentState.ToString());
			ASSERT_THAT(IsTrue(CurrentState == FArcMassGameFrameworkTags::Get().InitState_DataInitialized));

			const FMassEntityHandle EntityHandle = AgentComp->GetEntityHandle();
			UE_LOGF(LogArcMassPawnExtInitPIETest, Display,
				"VerifyWiring: EntityHandle Index=%d Serial=%d Valid=%d",
				EntityHandle.Index, EntityHandle.SerialNumber, EntityHandle.IsValid() ? 1 : 0);
			ASSERT_THAT(IsTrue(EntityHandle.IsValid()));

			UWorld* World = PIE.GetState().World;
			ASSERT_THAT(IsNotNull(World));

			UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
			ASSERT_THAT(IsNotNull(EntitySubsystem));

			FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
			const FMassEntityView EntityView(EntityManager, EntityHandle);

			const FMassActorFragment* ActorFrag = EntityView.GetFragmentDataPtr<FMassActorFragment>();
			UE_LOGF(LogArcMassPawnExtInitPIETest, Display,
				"VerifyWiring: ActorFrag=%p ActorPtr=%p",
				ActorFrag, ActorFrag ? ActorFrag->Get() : nullptr);
			ASSERT_THAT(IsNotNull(ActorFrag));

			APlayerController* PC = World->GetFirstPlayerController();
			APawn* Pawn = PC ? PC->GetPawn() : nullptr;
			ASSERT_THAT(IsNotNull(Pawn));
			ASSERT_THAT(IsTrue(ActorFrag->Get() == Pawn));

			const UArcMassPawnData* PawnData = Extension->GetPawnData();
			UE_LOGF(LogArcMassPawnExtInitPIETest, Display,
				"VerifyWiring: PawnData=%p", PawnData);
			ASSERT_THAT(IsNotNull(PawnData));
		});
	}
};

#endif // ENABLE_ARC_STANDALONE_PIE_TEST
