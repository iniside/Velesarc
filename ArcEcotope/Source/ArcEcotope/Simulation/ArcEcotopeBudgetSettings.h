// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ArcEcotopeBudgetSettings.generated.h"

USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcEcotopeBudgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Quality", meta = (ClampMin = "1"))
	int32 MaxLiveStems = 1000000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Quality", meta = (ClampMin = "1"))
	int32 MaxLiveRecords = 150000;

	/** Candidate-pool pressure before admission: 1 uses live-stem headroom alone; higher values trade work for selection choice. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Quality", meta = (ClampMin = "1"))
	int32 RecruitmentOversamplingFactor = 4;

	/**
	 * Budget for ground-cover patch records alone, additive on top of MaxLiveRecords so spreading
	 * cover cannot crowd individuals and cohorts out of theirs.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Quality", meta = (ClampMin = "0"))
	int32 MaxCoverRecords = 60000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Quality", meta = (ClampMin = "0"))
	int32 MaxDeadwoodRecords = 100000;

	/**
	 * Budget for Branch records alone, additive on top of MaxDeadwoodRecords so a shedding stand
	 * cannot crowd stumps, snags and logs out of theirs.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Quality", meta = (ClampMin = "0"))
	int32 MaxBranchRecords = 40000;
};

namespace UE::ArcEcotope
{
	template <typename BudgetCarrierType>
	FArcEcotopeBudgetSettings MakeBudgetSettings(const BudgetCarrierType& Carrier)
	{
		FArcEcotopeBudgetSettings Settings;
		Settings.MaxLiveStems = Carrier.MaxLiveStems;
		Settings.MaxLiveRecords = Carrier.MaxLiveRecords;
		Settings.RecruitmentOversamplingFactor = Carrier.RecruitmentOversamplingFactor;
		Settings.MaxCoverRecords = Carrier.MaxCoverRecords;
		Settings.MaxDeadwoodRecords = Carrier.MaxDeadwoodRecords;
		Settings.MaxBranchRecords = Carrier.MaxBranchRecords;
		return Settings;
	}
}
