// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/EntityElementTypes.h"
#include "Mass/ExternalSubsystemTraits.h"
#include "ArcAbilityMontageFragment.generated.h"

class UAnimMontage;
class UAnimInstance;

USTRUCT()
struct FArcAbilityMontageFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	float Duration = 0.f;
	float ElapsedTime = 0.f;
	bool bLoop = false;
	TWeakObjectPtr<UAnimMontage> Montage;
	TWeakObjectPtr<UAnimInstance> AnimInstance;
};

template <>
struct TMassFragmentTraits<FArcAbilityMontageFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT()
struct FArcAbilityMontageTag : public FMassSparseTag
{
	GENERATED_BODY()
};
