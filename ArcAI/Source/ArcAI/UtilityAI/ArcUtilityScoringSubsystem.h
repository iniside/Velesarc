// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UtilityAI/ArcUtilityTypes.h"
#include "UtilityAI/ArcUtilityScoringInstance.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcUtilityScoringSubsystem.generated.h"

DECLARE_DELEGATE_OneParam(FArcUtilityScoringFinished, FArcUtilityScoringInstance& /*CompletedInstance*/);

#if !UE_BUILD_SHIPPING
struct FArcUtilityDebugData
{
	TArray<FArcUtilityScoringInstance::FScoredPair> AllPairs;
	FArcUtilityResult Result;
	FVector QuerierLocation = FVector::ZeroVector;
	double ExecutionTimeMs = 0.0;
	EArcUtilityScoringStatus Status = EArcUtilityScoringStatus::Pending;
	EArcUtilitySelectionMode SelectionMode = EArcUtilitySelectionMode::HighestScore;
	int32 NumEntries = 0;
	int32 NumTargets = 0;
	double Timestamp = 0.0;
};
#endif

UCLASS()
class ARCAI_API UArcUtilityScoringSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UArcUtilityScoringSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/**
	 * Submit a scoring request.
	 * @return request ID, or INDEX_NONE on failure
	 */
	int32 SubmitRequest(
		TArray<FArcUtilityEntry>&& InEntries,
		TArray<FArcUtilityTarget>&& InTargets,
		const FArcUtilityContext& InContext,
		EArcUtilitySelectionMode InSelectionMode,
		float InMinScore,
		float InTopPercent,
		FArcUtilityScoringFinished OnFinished,
		int32 Priority = 0);

	bool AbortRequest(int32 RequestId);
	bool IsRequestRunning(int32 RequestId) const;

#if !UE_BUILD_SHIPPING
	const FArcUtilityDebugData* GetDebugData(FMassEntityHandle Entity) const;
	const TMap<FMassEntityHandle, FArcUtilityDebugData>& GetAllDebugData() const { return DebugData; }
#endif

	UPROPERTY(EditAnywhere, Category = "ArcUtility")
	float MaxAllowedTestingTime = 0.003f; // 3ms budget

	static constexpr double DebugDataExpiryTime = 10.0;

private:
	TArray<TSharedPtr<FArcUtilityScoringInstance>> RunningRequests;
	int32 NextRequestId = 1;

#if !UE_BUILD_SHIPPING
	void StoreDebugData(const FArcUtilityScoringInstance& Instance);
	void CleanupDebugData();
	TMap<FMassEntityHandle, FArcUtilityDebugData> DebugData;
#endif
};
