// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "UtilityAI/ArcUtilityEntry.h"
#include "UtilityAI/ArcUtilityTypes.h"

#include "ArcBodyUtilityFragments.generated.h"

USTRUCT(BlueprintType)
struct ARCWORLDSIM_API FArcBodyUtilityOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Utility")
	FGameplayTag GoalTag;

	UPROPERTY(EditAnywhere, Category = "Utility")
	FArcUtilityEntry Entry;
};

USTRUCT(BlueprintType)
struct ARCWORLDSIM_API FArcBodyUtilityConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	FArcBodyUtilityConfigFragment();

	UPROPERTY(EditAnywhere, Category = "Utility")
	TArray<FArcBodyUtilityOption> Options;

	UPROPERTY(EditAnywhere, Category = "Utility")
	EArcUtilitySelectionMode SelectionMode = EArcUtilitySelectionMode::HighestScore;

	UPROPERTY(EditAnywhere, Category = "Utility", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinScore = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Utility", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SwitchMargin = 0.10f;

	UPROPERTY(EditAnywhere, Category = "Utility", meta = (ClampMin = "0.0"))
	float NormalInterval = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Utility", meta = (ClampMin = "0.0"))
	float RetryInterval = 1.0f;
};

template<>
struct TMassFragmentTraits<FArcBodyUtilityConfigFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT()
struct ARCWORLDSIM_API FArcBodyUtilityIntentFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	FGameplayTag SelectedGoal;
	FGameplayTag ActiveGoal;
	uint32 ActiveCommandSerial = 0;
	int32 ScoringRequestId = INDEX_NONE;
	uint32 SubmittedBodyCommandSerial = 0;
	double NextEvaluationTime = 0.0;
	bool bEvaluationScheduled = false;
	bool bEvaluationDeferred = false;
	bool bIntentReady = false;
	float WinningScore = 0.0f;
	float ContinueScore = 0.0f;
};
