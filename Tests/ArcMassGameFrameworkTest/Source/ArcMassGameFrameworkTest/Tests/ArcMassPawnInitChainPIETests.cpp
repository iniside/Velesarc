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

#include "MassActorSubsystem.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/EntityHandle.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcMassPawnInitPIETest, Display, All);

namespace ArcMassPawnInitChainPIETests
{
	UArcMassPawnExtensionComponent* FindLocalExtension(UWorld* World)
	{
		if (World == nullptr)
		{
			return nullptr;
		}
		APlayerController* PC = World->GetFirstPlayerController();
		if (PC == nullptr)
		{
			return nullptr;
		}
		APawn* Pawn = PC->GetPawn();
		if (Pawn == nullptr)
		{
			return nullptr;
		}
		return Pawn->FindComponentByClass<UArcMassPawnExtensionComponent>();
	}
}

TEST_CLASS_WITH_FLAGS(ArcMassPawnInitChain_PIETests,
	"ArcMassGameFramework.PIE.PawnInitChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
	FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
		AArcMassGameMode::StaticClass() };

	const UArcMassGameFrameworkTestSettings* Settings = nullptr;
	UArcMassPawnExtensionComponent* Extension = nullptr;

	BEFORE_EACH()
	{
		// Mass module emits Error-severity logs during PIE startup for processors with
		// empty FMassEntityQuery requirements. Independent of this suite; tracked separately.
		// CQTest captures Error logs as test failures, so we declare them expected here.
		TestRunner->AddExpectedError(
			TEXT("FMassEntityQuery::CacheArchetypes: requirements not valid"),
			EAutomationExpectedErrorFlags::Contains, 0);

		Settings = GetDefault<UArcMassGameFrameworkTestSettings>();
		if (Settings == nullptr || Settings->TestExperience.IsNull() || Settings->TestMap.IsNull())
		{
			TestRunner->AddInfo(TEXT("Skipped: TestExperience or TestMap is null on this checkout."));
			return;
		}
		const FSoftObjectPath MapPath = Settings->TestMap.ToSoftObjectPath();
		UE_LOGF(LogArcMassPawnInitPIETest, Display, "BEFORE_EACH: WithMap path='%ls'", *MapPath.ToString());
		PIE.WithMap(MapPath);
	}

	TEST_METHOD(PawnExtension_ReachesDataInitialized)
	{
		if (Settings == nullptr || Settings->TestExperience.IsNull() || Settings->TestMap.IsNull())
		{
			return;
		}

		PIE.Then(TEXT("Diag: post-PIE world state"), [this]() {
			UWorld* World = PIE.GetState().World;
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			APawn* Pawn = PC ? PC->GetPawn() : nullptr;
			const FString MapName = World ? World->GetMapName() : FString(TEXT("<null>"));
			const FString GMClass = World && World->GetAuthGameMode() ? World->GetAuthGameMode()->GetClass()->GetName() : FString(TEXT("<null>"));
			UE_LOGF(LogArcMassPawnInitPIETest, Display
				, "PostPIE: World=%p MapName='%ls' GameMode='%ls' PC=%p Pawn=%p"
				, World, *MapName, *GMClass, PC, Pawn);
			TestRunner->AddInfo(FString::Printf(TEXT("PostPIE: World=%p MapName='%s' GameMode='%s' PC=%p Pawn=%p")
				, World, *MapName, *GMClass, PC, Pawn));
		})
		.Until(TEXT("Wait for local pawn extension"),
			[this]() {
				Extension = ArcMassPawnInitChainPIETests::FindLocalExtension(PIE.GetState().World);
				static int32 PollCount = 0;
				if ((PollCount++ % 30) == 0)
				{
					UWorld* World = PIE.GetState().World;
					APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
					APawn* Pawn = PC ? PC->GetPawn() : nullptr;
					UE_LOGF(LogArcMassPawnInitPIETest, Display
						, "WaitExt: poll=%d World=%p PC=%p Pawn=%p Extension=%p"
						, PollCount, World, PC, Pawn, Extension);
				}
				return Extension != nullptr;
			},
			FTimespan::FromSeconds(30.0))
		.Until(TEXT("Wait for InitState_DataInitialized"),
			[this]() {
				const FGameplayTag CurrentState = Extension ? Extension->GetInitState() : FGameplayTag();
				const FGameplayTag Target = FArcMassGameFrameworkTags::Get().InitState_DataInitialized;
				static FGameplayTag LastSeen;
				if (CurrentState != LastSeen)
				{
					UE_LOGF(LogArcMassPawnInitPIETest, Display
						, "WaitInit: state changed '%ls' -> '%ls' (target='%ls')"
						, *LastSeen.ToString(), *CurrentState.ToString(), *Target.ToString());
					LastSeen = CurrentState;
				}
				return Extension != nullptr && CurrentState == Target;
			},
			FTimespan::FromSeconds(30.0))
		.Then(TEXT("Verify final init state"), [this]() {
			const FGameplayTag CurrentState = Extension ? Extension->GetInitState() : FGameplayTag();
			UE_LOGF(LogArcMassPawnInitPIETest, Display, "Verify: Extension=%p State='%ls'", Extension, *CurrentState.ToString());
			ASSERT_THAT(IsNotNull(Extension));
			ASSERT_THAT(IsTrue(CurrentState == FArcMassGameFrameworkTags::Get().InitState_DataInitialized));

			// Production wiring contract: pawn carries an Arc agent component, the agent
			// holds a valid entity handle, and the entity's FMassActorFragment points back
			// to the pawn. Also confirm the asset-loaded PawnData survived to this point.
			UWorld* World = PIE.GetState().World;
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			APawn* Pawn = PC ? PC->GetPawn() : nullptr;
			ASSERT_THAT(IsNotNull(Pawn));

			UArcMassFrameworkAgentComponent* AgentComp =
				Pawn->FindComponentByClass<UArcMassFrameworkAgentComponent>();
			UE_LOGF(LogArcMassPawnInitPIETest, Display
				, "VerifyWiring: Pawn=%p AgentComp=%p", Pawn, AgentComp);
			ASSERT_THAT(IsNotNull(AgentComp));

			const FMassEntityHandle EntityHandle = AgentComp->GetEntityHandle();
			UE_LOGF(LogArcMassPawnInitPIETest, Display
				, "VerifyWiring: EntityHandle Index=%d Serial=%d Valid=%d"
				, EntityHandle.Index, EntityHandle.SerialNumber, EntityHandle.IsValid() ? 1 : 0);
			ASSERT_THAT(IsTrue(EntityHandle.IsValid()));

			UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
			ASSERT_THAT(IsNotNull(EntitySubsystem));

			FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
			const FMassEntityView EntityView(EntityManager, EntityHandle);
			const FMassActorFragment* ActorFrag = EntityView.GetFragmentDataPtr<FMassActorFragment>();
			UE_LOGF(LogArcMassPawnInitPIETest, Display
				, "VerifyWiring: ActorFrag=%p ActorPtr=%p ExpectedPawn=%p"
				, ActorFrag, ActorFrag ? ActorFrag->Get() : nullptr, Pawn);
			ASSERT_THAT(IsNotNull(ActorFrag));
			ASSERT_THAT(IsTrue(ActorFrag->Get() == Pawn));

			const UArcMassPawnData* PawnData = Extension->GetPawnData();
			UE_LOGF(LogArcMassPawnInitPIETest, Display
				, "VerifyWiring: PawnData=%p", PawnData);
			ASSERT_THAT(IsNotNull(PawnData));
		});
	}
};

#endif // ENABLE_ARC_STANDALONE_PIE_TEST
