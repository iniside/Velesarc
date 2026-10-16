// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "MassEntityTemplate.h"
#include "MassEntityTraitBase.h"
#include "Pawn/ArcMassFrameworkAgentComponent.h"
#include "Pawn/ArcMassPawnDataFragments.h"
#include "Pawn/ArcMassPawnExtensionComponent.h"

#include "ArcMassGameFrameworkTestTypes.generated.h"

class APawn;
class APlayerController;
class APlayerState;
struct FMassEntityManager;

/**
 * Test init fragment: increments a static counter on Initialize and records the
 * pawn pointer for ordering assertions against ::FArcMassTestAddPawnComponent.
 */
USTRUCT(meta = (Hidden))
struct FArcMassTestPawnInitFragment : public FArcMassPawnInitFragment
{
	GENERATED_BODY()

	static int32 InitCount;
	static int32 LastInitOrderIndex;
	static int32 NextOrderCounter;

	static void Reset()
	{
		InitCount = 0;
		LastInitOrderIndex = -1;
		NextOrderCounter = 0;
	}

	virtual void Initialize(APawn* Pawn
							, APlayerState* PlayerState
							, APlayerController* PlayerController
							, FMassEntityManager* EntityManager
							, FMassEntityHandle Entity
							, UArcMassPawnExtensionComponent* OwnerExtension) const override
	{
		InitCount++;
		LastInitOrderIndex = NextOrderCounter++;
	}
};

/**
 * Test add-pawn-component descriptor: increments a counter on GiveComponent and
 * records its order so tests can verify GiveComponent runs before Initialize.
 */
USTRUCT(meta = (Hidden))
struct FArcMassTestAddPawnComponent : public FArcMassAddPawnComponent
{
	GENERATED_BODY()

	static int32 GiveCount;
	static int32 LastGiveOrderIndex;

	static void Reset()
	{
		GiveCount = 0;
		LastGiveOrderIndex = -1;
	}

	virtual void GiveComponent(APawn* Pawn) const override
	{
		GiveCount++;
		LastGiveOrderIndex = FArcMassTestPawnInitFragment::NextOrderCounter++;
	}
};

/**
 * Test seam: UArcMassFrameworkAgentComponent::SetEntityHandle is protected virtual.
 * This stub exposes a public Test_SetEntityHandle() so CanChangeInitState gating tests
 * can stamp a fake handle onto the agent component without driving the full Mass
 * spawn pipeline (UMassAgentSubsystem) which is not available inside FActorTestSpawner.
 */
UCLASS(ClassGroup = "ArcMassTest", NotPlaceable, Hidden)
class UArcMassFrameworkAgentComponentTestStub : public UArcMassFrameworkAgentComponent
{
	GENERATED_BODY()

public:
	void Test_SetEntityHandle(FMassEntityHandle Handle)
	{
		// UArcMassFrameworkAgentComponent::OnRegister intentionally bypasses UMassAgentSubsystem
		// registration, so the engine no longer drives None→EntityPendingCreation for us.
		// SetEntityHandle asserts State==EntityPendingCreation, so drive it manually — the seam's
		// job is to fake "the spawner finished spawning," which includes this transition.
		EntityCreationPending();
		SetEntityHandle(Handle);
	}
};

/**
 * Test seam: UArcMassPawnExtensionComponent::bMassEntityFullyInitialized is protected.
 * This stub exposes Test_IsMassEntityFullyInitialized() so spawner-path tests can verify
 * that NotifyEntityReadyImmediately() ran without needing to complete the full pawn init
 * chain (which requires PawnData + Controller, both unavailable in FActorTestSpawner).
 */
UCLASS(ClassGroup = "ArcMassTest", NotPlaceable, Hidden)
class UArcMassPawnExtensionComponentTestStub : public UArcMassPawnExtensionComponent
{
	GENERATED_BODY()

public:
	bool Test_IsMassEntityFullyInitialized() const
	{
		return bMassEntityFullyInitialized;
	}
};

/**
 * Test-only trait: adds FMassActorFragment + FTransformFragment to the entity composition
 * via BuildContext directly. Reproduces the bare minimum the engine UMassAssortedFragmentsTrait
 * would add, but bypasses that trait's protected `Fragments` UPROPERTY (not settable from C++).
 */
UCLASS(Hidden)
class UArcMassActorBridgeTestTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
