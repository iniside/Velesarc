// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/ArcAbilityCooldown.h"
#include "ArcCooldownExtensibilityTestTypes.generated.h"

/**
 * Non-blocking cooldown subclass for extensibility tests.
 * IsBlocking() returns false so CheckCooldown never gates activation for entries stamped by this type.
 * OnExpired() increments a test-global counter so tests can assert it was called.
 */
USTRUCT(meta = (Hidden))
struct FArcTestNonBlockingCooldown : public FArcAbilityCooldown_Duration
{
	GENERATED_BODY()

	// Test-observable counter — reset before each use.
	static int32 OnExpiredCallCount;

	virtual bool CheckCooldown(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcAbilityHandle Handle, FArcCooldownCheckResult* OutResult = nullptr) const override
	{
		// Delegate to Duration logic: non-blocking entries don't block activation.
		// For this test type we never place blocking entries, so always pass.
		return true;
	}

	virtual bool IsBlocking() const override { return false; }

	virtual void OnExpired(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcAbilityHandle Handle) const override
	{
		++OnExpiredCallCount;
	}
};
