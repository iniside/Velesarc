// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include <atomic>

#include "GameplayTagContainer.h"
#include "Mass/EntityElementTypes.h"
#include "Mass/ExternalSubsystemTraits.h"
#include "ArcAbilityComboStepFragment.generated.h"

class UAnimMontage;
class UAnimInstance;

USTRUCT()
struct FArcAbilityComboStepFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	uint32 Serial = 0;
	float MontageDuration = 0.f;
	float ElapsedTime = 0.f;
	float ComboWindowStart = 0.f;
	float ComboWindowEnd = 0.f;
	FGameplayTag InputTag;
	bool bInputReceived = false;
	TWeakObjectPtr<UAnimMontage> Montage;
	TWeakObjectPtr<UAnimInstance> AnimInstance;
};

namespace ArcAbilities
{
	inline uint32 NextComboStepSerial()
	{
		static std::atomic<uint32> Counter{0};
		return ++Counter;
	}
}

template <>
struct TMassFragmentTraits<FArcAbilityComboStepFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT()
struct FArcAbilityComboStepTag : public FMassSparseTag
{
	GENERATED_BODY()
};
