// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumClassFlags.h"
#include "Conditions/ArcMassActorGameplayAttributeCondition.h"
#include "UtilityAI/ArcUtilityTypes.h"
#include "Math/ArcResponseCurve.h"
#include "ArcUtilityConsideration.generated.h"

/**
 * Outcome of a single consideration. A rejection reason vetoes the whole entry x candidate
 * pair regardless of its score or of MinScore; without one the pair keeps scoring.
 */
struct FArcUtilityConsiderationResult
{
	float Score = 0.0f;

	FName RejectionReason = NAME_None;

	static FArcUtilityConsiderationResult Accept(float InScore)
	{
		FArcUtilityConsiderationResult Result;
		Result.Score = InScore;
		return Result;
	}

	static FArcUtilityConsiderationResult Reject(FName Reason)
	{
		FArcUtilityConsiderationResult Result;
		Result.RejectionReason = Reason;
		return Result;
	}

	bool IsAccepted() const
	{
		return RejectionReason.IsNone();
	}
};

UENUM()
enum class EArcUtilitySubject : uint8
{
	Querier,
	Target
};

#if WITH_EDITOR
enum class EArcUtilityConsiderationRequirements : uint8
{
	None = 0,
	Ability = 1 << 0,
	TargetEntity = 1 << 1,
	TargetLocation = 1 << 2,
	ExecutionPosition = 1 << 3
};
ENUM_CLASS_FLAGS(EArcUtilityConsiderationRequirements);
#endif

/**
 * Base consideration for utility AI evaluation.
 * Derive to create custom considerations. Return raw [0,1] from Score().
 * The pipeline applies ResponseCurve and clamps the result to [0,1].
 */
USTRUCT(BlueprintType, meta = (ExcludeBaseStruct))
struct ARCAI_API FArcUtilityConsideration
{
	GENERATED_BODY()

	virtual ~FArcUtilityConsideration() = default;

	virtual FArcUtilityConsiderationResult Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
	{
		return FArcUtilityConsiderationResult::Accept(1.0f);
	}

#if WITH_EDITOR
	/** Which axes of FArcUtilityTarget / FArcUtilityContext this consideration reads. */
	virtual EArcUtilityConsiderationRequirements GetRequirements() const
	{
		return EArcUtilityConsiderationRequirements::None;
	}

	virtual bool Validate(FString& OutError) const
	{
		return true;
	}
#endif

	UPROPERTY(EditAnywhere, Category = "Consideration", meta = (ArcTacticsParameter))
	FArcResponseCurve ResponseCurve;
};

USTRUCT(BlueprintType, DisplayName = "Distance Consideration")
struct ARCAI_API FArcUtilityConsideration_Distance : public FArcUtilityConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Consideration", meta = (ClampMin = 1.0))
	float MaxDistance = 5000.0f;

	virtual FArcUtilityConsiderationResult Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;

#if WITH_EDITOR
	virtual EArcUtilityConsiderationRequirements GetRequirements() const override
	{
		return EArcUtilityConsiderationRequirements::TargetLocation;
	}
#endif
};

USTRUCT(BlueprintType, DisplayName = "Constant Consideration")
struct ARCAI_API FArcUtilityConsideration_Constant : public FArcUtilityConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Consideration", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float Value = 1.0f;

	virtual FArcUtilityConsiderationResult Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;

#if WITH_EDITOR
	virtual EArcUtilityConsiderationRequirements GetRequirements() const override
	{
		return EArcUtilityConsiderationRequirements::None;
	}
#endif
};
