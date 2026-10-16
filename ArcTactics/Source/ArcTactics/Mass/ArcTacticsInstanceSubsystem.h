// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Actions/ArcTacticsAction.h"
#include "Debugger/ArcTacticsTrace.h"
#include "Decision/ArcTacticsInstance.h"
#include "Mass/ArcTacticsInstanceHandle.h"
#include "Mass/ExternalSubsystemTraits.h"
#include "MassSubsystemBase.h"
#include "MassSystem.h"
#include "UObject/ObjectKey.h"

#include "ArcTacticsInstanceSubsystem.generated.h"

class UArcGameEventSubsystem;
class UArcTacticsConfig;
class UMassSignalSubsystem;
struct FArcGameEvent;
struct FArcTQSQueryInstance;
struct FMassEntityManager;

namespace ArcPathFollow
{
	struct FContext;
}

struct ARCTACTICS_API FArcTacticsPackage
{
	TObjectPtr<const UArcTacticsConfig> Config = nullptr;
	uint32 SeenRevision = 0;
	TSharedPtr<const FArcTacticsCompiledData> Definitions;
	FArcTacticsInstanceData Instance;
	int32 StaticCount = 0;
	int32 RuntimeCount = 0;
	uint32 Serial = 0;
	bool bActive = false;
	bool bExcluded = false;
};

struct FArcTacticsPendingPackageChange
{
	TObjectPtr<const UArcTacticsConfig> Config = nullptr;
	bool bAdd = false;
};

using FArcTacticsCooldownKey = TPair<TObjectKey<UArcTacticsConfig>, int32>;

struct FArcTacticsEndedAction
{
	bool bSet = false;
	FName ConfigName;
	FName Name;
	EArcTacticsActionEndStatus Status = EArcTacticsActionEndStatus::Cancelled;
	EArcTacticsActionEndReason Reason = EArcTacticsActionEndReason::Completed;
	FName FailureReason;
	double Time = 0.0;
};

struct ARCTACTICS_API FArcTacticsInstanceSlot
{
	FArcTacticsInstanceSlot() = default;
	FArcTacticsInstanceSlot(const FArcTacticsInstanceSlot&) = delete;
	FArcTacticsInstanceSlot& operator=(const FArcTacticsInstanceSlot&) = delete;
	FArcTacticsInstanceSlot(FArcTacticsInstanceSlot&&) = default;
	FArcTacticsInstanceSlot& operator=(FArcTacticsInstanceSlot&&) = default;

	bool Initialize(FMassEntityHandle InEntity, const FArcTacticsDecisionSettings& InSettings, int32 RandomSeed);
	FArcTacticsPackage& AddPackage(const UArcTacticsConfig* Config, TSharedPtr<const FArcTacticsCompiledData> InDefinitions);
	int32 FindPackageIndex(const UArcTacticsConfig& Config) const;
	FArcTacticsPackage* GetActionPackage(const FArcTacticsOption& Option);
	bool HasActivePackage() const;
	bool ResetForDecision(const FArcTQSFunctionContext& FunctionContext);
	bool HasSelectedAction() const;
	void ActivateSelectedAction();
	void ClearActiveAction();
	void AddReferencedObjects(FReferenceCollector& Collector, const UObject* Referencer);

	FMassEntityHandle Entity;
	FArcTacticsContextData Context;
	FArcTacticsDecisionSettings Settings;
	TArray<TUniquePtr<FArcTacticsPackage>> Packages;
	uint32 NextPackageSerial = 0;
	uint32 DecisionGeneration = 0;
	EArcTacticsDecisionStatus DecisionStatus = EArcTacticsDecisionStatus::Idle;
	int32 PackageCursor = INDEX_NONE;
	TArray<FArcTacticsCandidate> Candidates;
	FArcTacticsOption SelectedAction;
	FArcTacticsOption ActiveAction;
	FArcTacticsEndedAction LastEndedAction;
	TMap<FArcTacticsCooldownKey, double> CooldownEndTimes;
	FRandomStream RandomStream;
	uint32 NextActionSerial = 0;
	FName Diagnostic;
	FArcTacticsMovementHoldState MovementHold;
	FDelegateHandle AbilityActivatedHandle;
	FDelegateHandle AbilityEndedHandle;
	FDelegateHandle AbilityCooldownExpiredHandle;
	double NextDecisionTime = 0.0;
	double ScheduledDecisionTime = 0.0;
	double ActionWakeTime = 0.0;
	uint32 Generation = 1;
	uint32 ActionWakeSerial = 0;
	int32 ExecutionDepth = 0;
	TArray<FArcTacticsPendingPackageChange> PendingPackageChanges;
	int32 PendingApplyIndex = 0;
	bool bAllocated = false;
	bool bRetired = false;
	bool bDecisionRequested = false;
	bool bActionEntered = false;
};

UCLASS()
class ARCTACTICS_API UArcTacticsInstanceSubsystem : public UMassSubsystemBase
{
	GENERATED_BODY()

public:
	FArcTacticsInstanceHandle AllocateInstance(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		TConstArrayView<TObjectPtr<const UArcTacticsConfig>> Configs,
		const FArcTacticsDecisionSettings& Settings);
	void CancelAndFreeInstance(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FArcTacticsInstanceHandle Handle,
		const ArcPathFollow::FContext* PathContext);
	FArcTacticsInstanceHandle ReinitializeInstance(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FArcTacticsInstanceHandle Handle,
		TConstArrayView<TObjectPtr<const UArcTacticsConfig>> Configs,
		const FArcTacticsDecisionSettings& Settings,
		const ArcPathFollow::FContext* PathContext);

	bool AddConfig(FMassEntityHandle Entity, const UArcTacticsConfig& Config);
	bool RemoveConfig(FMassEntityHandle Entity, const UArcTacticsConfig& Config);

	void RequestDecision(FMassEntityHandle Entity);
	void RequestDecision(FArcTacticsInstanceHandle Handle);

	FArcTacticsInstanceSlot* Resolve(FArcTacticsInstanceHandle Handle);
	const FArcTacticsInstanceSlot* Resolve(FArcTacticsInstanceHandle Handle) const;
	bool IsCurrentRevision(FArcTacticsInstanceHandle Handle) const;

	void CompleteTaskQuery(const FArcTacticsAsyncRequestToken& Token, FArcTQSQueryInstance& CompletedQuery);
	void CompleteActionQuery(const FArcTacticsAsyncRequestToken& Token, FArcTQSQueryInstance& CompletedQuery);

	int32 GetSlotCount() const { return Slots.Num(); }
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void PreDeinitialize() override;
	virtual void Deinitialize() override;

private:
	friend struct FArcTacticsExecutionScope;

	void EndExecution(FArcTacticsInstanceHandle Handle);
	TSharedPtr<const FArcTacticsCompiledData> ResolveDefinitions(const UArcTacticsConfig& Config);
	FArcTacticsPackage& BuildPackage(
		FMassEntityManager& EntityManager,
		FArcTacticsInstanceSlot& Slot,
		const UArcTacticsConfig& Config);
	FArcTacticsInstanceHandle FindHandle(FMassEntityHandle Entity) const;
	void ApplyAddConfig(FArcTacticsInstanceHandle Handle, const UArcTacticsConfig& Config);
	void ApplyRemoveConfig(FArcTacticsInstanceHandle Handle, const UArcTacticsConfig& Config);
	void CancelSlot(
		FMassEntityManager& EntityManager,
		FArcTacticsInstanceSlot& Slot,
		FArcTacticsInstanceHandle Handle,
		const ArcPathFollow::FContext* PathContext,
		EArcTacticsActionCancelMode CancelMode);
	void FreeSlot(FArcTacticsInstanceHandle Handle);
	void SignalEntity(FName Signal, FMassEntityHandle Entity) const;
	void HandleAbilityActivated(const FArcGameEvent& Event);
	void HandleAbilityEnded(const FArcGameEvent& Event);
	void HandleAbilityCooldownExpired(const FArcGameEvent& Event);

	TArray<TUniquePtr<FArcTacticsInstanceSlot>> Slots;
	TArray<int32> Freelist;
	TMap<TWeakObjectPtr<const UArcTacticsConfig>, TWeakPtr<const FArcTacticsCompiledData>> CurrentDefinitions;
	TWeakObjectPtr<UMassSignalSubsystem> SignalSubsystem;
	TWeakObjectPtr<UArcGameEventSubsystem> GameEventSubsystem;
};

USTRUCT()
struct FMassSystem_ArcTacticsInstanceSubsystem : public FMassSystem
{
	GENERATED_BODY()
};

UE_DECLARE_MASS_SUBSYSTEM_WRAPPER(UArcTacticsInstanceSubsystem, FMassSystem_ArcTacticsInstanceSubsystem, /*GameThreadOnly=*/true, /*ThreadSafeWrite=*/false);

struct ARCTACTICS_API FArcTacticsExecutionScope
{
	FArcTacticsExecutionScope(UArcTacticsInstanceSubsystem& InSubsystem, FArcTacticsInstanceHandle InHandle);
	~FArcTacticsExecutionScope();
	FArcTacticsExecutionScope(const FArcTacticsExecutionScope&) = delete;
	FArcTacticsExecutionScope& operator=(const FArcTacticsExecutionScope&) = delete;

private:
	TWeakObjectPtr<UArcTacticsInstanceSubsystem> Subsystem;
	FArcTacticsInstanceHandle Handle;
	bool bEntered = false;
};

namespace ArcTacticsInstances
{
	ARCTACTICS_API FArcTacticsPackage* ResolveActivePackage(FArcTacticsInstanceSlot& Slot);

	ARCTACTICS_API const FArcTacticsActionNode* ResolveActiveAction(
		FArcTacticsInstanceSlot& Slot,
		FArcTacticsActionView& OutAction);

	ARCTACTICS_API FArcTacticsActionContext MakeActionContext(
		UArcTacticsInstanceSubsystem& Subsystem,
		FMassEntityManager& EntityManager,
		FArcTacticsInstanceSlot& Slot,
		FArcTacticsInstanceHandle Handle,
		const ArcPathFollow::FContext* PathContext);

	ARCTACTICS_API void StampCooldown(FArcTacticsInstanceSlot& Slot, const FArcTacticsPackage& Package, UWorld& World);

	ARCTACTICS_API void RecordLastEndedAction(
		FArcTacticsInstanceSlot& Slot,
		const UWorld* World,
		EArcTacticsActionEndStatus EndStatus,
		EArcTacticsActionEndReason EndReason,
		FName FailureReason);

	ARCTACTICS_API void RecordEndedAction(
		FArcTacticsInstanceSlot& Slot,
		const FArcTacticsPackage& Package,
		UWorld& World,
		const FMassEntityManager& EntityManager,
		EArcTacticsActionEndStatus EndStatus,
		EArcTacticsActionEndReason EndReason,
		FName FailureReason);
}
