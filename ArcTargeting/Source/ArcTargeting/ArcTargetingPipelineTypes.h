#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcTargetingPipelineTypes.generated.h"

/**
 * Opaque request handle. Stable for the lifetime of one targeting request.
 * Constructed by the subsystem; callers compare/store/reset only.
 */
USTRUCT(BlueprintType)
struct ARCTARGETING_API FArcTargetingHandle
{
	GENERATED_BODY()

	FArcTargetingHandle() = default;
	explicit FArcTargetingHandle(uint64 InId) : Id(InId) {}

	bool IsValid() const { return Id != 0; }
	void Reset() { Id = 0; }

	uint64 GetId() const { return Id; }

	bool operator==(const FArcTargetingHandle& Other) const { return Id == Other.Id; }
	bool operator!=(const FArcTargetingHandle& Other) const { return Id != Other.Id; }

	friend uint32 GetTypeHash(const FArcTargetingHandle& H) { return ::GetTypeHash(H.Id); }

private:
	UPROPERTY()
	uint64 Id = 0;
};

/**
 * Pipeline lifecycle state of a single request.
 *
 * Pending  — request created, not yet running.
 * Running  — currently inside ExecuteRequestAtomic.
 * Done     — pipeline finished, completion delegate dispatched (or about to be).
 * Cancelled— request was cancelled before/during execution; no delegate fires.
 */
UENUM()
enum class EArcTargetingState : uint8
{
	Pending,
	Running,
	Done,
	Cancelled
};

/**
 * One result row produced by Acquisition, narrowed by Filter, scored by Sort.
 *
 * `HitResult` carries the trace fidelity (impact point, normal, bone, physmat)
 * that downstream consumers need. `Entity` is resolved from the hit by
 * acquisition nodes via `UArcMassEntityLibrary::ResolveHitToEntity` (physics
 * link); invalid when the hit doesn't correspond to a Mass entity.
 * `CandidateData` is a per-candidate payload an acquisition op can use to
 * pass extra info to filter/sort ops.
 */
USTRUCT(BlueprintType)
struct ARCTARGETING_API FArcTargetCandidate
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FHitResult HitResult;

	UPROPERTY(BlueprintReadOnly)
	float Score = 0.f;

	UPROPERTY(BlueprintReadOnly)
	int32 Priority = 0;

	FMassEntityHandle Entity;

	UPROPERTY()
	FInstancedStruct CandidateData;
};
