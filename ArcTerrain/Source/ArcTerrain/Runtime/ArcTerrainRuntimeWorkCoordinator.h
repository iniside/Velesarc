// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTerrainTileAccess.h"
#include "Tasks/Task.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"
#include <atomic>

enum class EArcTerrainRuntimeLane : uint8
{
	Atlas = 0,
	Lighting = 1,
	Far = 2,
	Count
};

enum class EArcTerrainRuntimeWorkStatus : uint8
{
	Succeeded = 0,
	BudgetExceeded = 1,
	Cancelled = 2
};

struct FArcTerrainRuntimeOwnerId
{
	uint32 Value = 0;

	bool operator==(const FArcTerrainRuntimeOwnerId& Other) const
	{
		return Value == Other.Value;
	}

	bool operator!=(const FArcTerrainRuntimeOwnerId& Other) const
	{
		return Value != Other.Value;
	}
};

inline uint32 GetTypeHash(const FArcTerrainRuntimeOwnerId& Id)
{
	return GetTypeHash(Id.Value);
}

struct FArcTerrainRetainedPayload
{
	uint32 PayloadId = 0;
	uint64 Bytes = 0;
	FArcTerrainTilePayloadPtr Payload;
};

struct FArcTerrainRuntimeReservationRequest
{
	FArcTerrainRuntimeOwnerId OwnerId;
	uint64 TerrainId = 0;
	EArcTerrainRuntimeLane Lane = EArcTerrainRuntimeLane::Atlas;
	uint64 ExactOwnedInputBytes = 0;
	uint64 PreReservedResultBytes = 0;
	TArray<FArcTerrainRetainedPayload> Payloads;
};

class FArcTerrainRuntimeWorkCoordinator;

class ARCTERRAIN_API FArcTerrainRuntimeReservationToken
{
public:
	FArcTerrainRuntimeReservationToken() = default;
	FArcTerrainRuntimeReservationToken(FArcTerrainRuntimeReservationToken&& Other) noexcept;
	FArcTerrainRuntimeReservationToken& operator=(FArcTerrainRuntimeReservationToken&& Other) noexcept;
	~FArcTerrainRuntimeReservationToken();

	FArcTerrainRuntimeReservationToken(const FArcTerrainRuntimeReservationToken&) = delete;
	FArcTerrainRuntimeReservationToken& operator=(const FArcTerrainRuntimeReservationToken&) = delete;

	bool IsValid() const { return ReservationId != 0; }
	uint64 GetReservationId() const { return ReservationId; }
	void Release();
	uint64 Detach();

private:
	friend class FArcTerrainRuntimeWorkCoordinator;

	TWeakPtr<FArcTerrainRuntimeWorkCoordinator, ESPMode::ThreadSafe> Coordinator;
	uint64 ReservationId = 0;
};

class ARCTERRAIN_API FArcTerrainRuntimeWorkContext
{
public:
	bool TryGrow(uint64 AdditionalBytes);
	bool IsAbandoned() const;

private:
	friend class FArcTerrainRuntimeWorkCoordinator;

	FArcTerrainRuntimeWorkCoordinator* Coordinator = nullptr;
	uint64 ReservationId = 0;
	uint64 RemainingPreReserve = 0;
};

struct FArcTerrainRuntimeResult
{
	EArcTerrainRuntimeWorkStatus Status = EArcTerrainRuntimeWorkStatus::Succeeded;
	TSharedPtr<void, ESPMode::ThreadSafe> Payload;
};

struct FArcTerrainRuntimeCompletion
{
	FArcTerrainRuntimeOwnerId OwnerId;
	uint64 TerrainId = 0;
	EArcTerrainRuntimeLane Lane = EArcTerrainRuntimeLane::Atlas;
	EArcTerrainRuntimeWorkStatus Status = EArcTerrainRuntimeWorkStatus::Succeeded;
	TSharedPtr<void, ESPMode::ThreadSafe> Payload;
	FArcTerrainRuntimeReservationToken Reservation;
};

struct FArcTerrainRuntimeOwnerTerrainLane
{
	FArcTerrainRuntimeOwnerId OwnerId;
	uint64 TerrainId = 0;
	EArcTerrainRuntimeLane Lane = EArcTerrainRuntimeLane::Atlas;

	bool operator==(const FArcTerrainRuntimeOwnerTerrainLane& Other) const
	{
		return OwnerId == Other.OwnerId && TerrainId == Other.TerrainId && Lane == Other.Lane;
	}
};

inline uint32 GetTypeHash(const FArcTerrainRuntimeOwnerTerrainLane& Key)
{
	return HashCombine(
		HashCombine(GetTypeHash(Key.OwnerId), GetTypeHash(Key.TerrainId)),
		static_cast<uint32>(Key.Lane));
}

class ARCTERRAIN_API FArcTerrainRuntimeWorkCoordinator
	: public TSharedFromThis<FArcTerrainRuntimeWorkCoordinator, ESPMode::ThreadSafe>
{
public:
	static constexpr int32 DefaultWorkerTasks = 16;
	static constexpr int32 DefaultOwnedMiB = 256;
	static constexpr int32 DefaultCompletionGrantsPerFrame = 16;
	static constexpr int32 DefaultAtlasLaunchesPerFrame = 4;
	static constexpr int32 DefaultLightingLaunchesPerFrame = 8;
	static constexpr int32 DefaultFarLaunchesPerFrame = 4;

	FArcTerrainRuntimeWorkCoordinator();
	~FArcTerrainRuntimeWorkCoordinator();

	static FArcTerrainRuntimeWorkCoordinator* TryGet();

	bool TryReserveSubmission(
		const FArcTerrainRuntimeReservationRequest& Request,
		FArcTerrainRuntimeReservationToken& OutToken);

	void SetTerrainLaneLaunchLimit(
		FArcTerrainRuntimeOwnerId OwnerId,
		uint64 TerrainId,
		EArcTerrainRuntimeLane Lane,
		int32 MaxLaunchesPerFrame);

	void Submit(
		FArcTerrainRuntimeReservationToken&& Token,
		UE::Tasks::ETaskPriority TaskPriority,
		TUniqueFunction<FArcTerrainRuntimeResult(FArcTerrainRuntimeWorkContext&)>&& Work);

	void BeginFrameAndPump(uint64 FrameCounter);

	void DrainGrantedCompletions(
		FArcTerrainRuntimeOwnerId OwnerId,
		TFunctionRef<void(FArcTerrainRuntimeCompletion&&)> Consumer);

	void AbandonTerrain(FArcTerrainRuntimeOwnerId OwnerId, uint64 TerrainId);
	void AbandonOwner(FArcTerrainRuntimeOwnerId OwnerId);
	void ShutdownAndWait();

	void ApplyReservationRelease(uint64 ReservationId);
	bool TryGrowReservation(uint64 ReservationId, uint64 AdditionalBytes);
	bool IsReservationAbandoned(uint64 ReservationId) const;

private:
	struct FPayloadRetain
	{
		FArcTerrainTilePayloadPtr Payload;
		uint64 Bytes = 0;
		int32 RefCount = 0;
	};

	struct FReservationRecord
	{
		FArcTerrainRuntimeOwnerId OwnerId;
		uint64 TerrainId = 0;
		EArcTerrainRuntimeLane Lane = EArcTerrainRuntimeLane::Atlas;
		uint64 JobOwnedBytes = 0;
		uint64 PreReservedResultBytes = 0;
		TArray<uint32> PayloadIds;
		TArray<FArcTerrainTilePayloadPtr> Payloads;
		bool bAbandoned = false;
	};

	struct FQueuedJob
	{
		uint64 ReservationId = 0;
		FArcTerrainRuntimeOwnerId OwnerId;
		uint64 TerrainId = 0;
		EArcTerrainRuntimeLane Lane = EArcTerrainRuntimeLane::Atlas;
		uint64 PreReservedResultBytes = 0;
		UE::Tasks::ETaskPriority TaskPriority = UE::Tasks::ETaskPriority::BackgroundNormal;
		TUniqueFunction<FArcTerrainRuntimeResult(FArcTerrainRuntimeWorkContext&)> Work;
		std::atomic<bool> bAbandoned { false };
	};

	struct FInFlightJob
	{
		TSharedPtr<FQueuedJob, ESPMode::ThreadSafe> Job;
		UE::Tasks::FTask Task;
	};

	struct FLaneQueue
	{
		TArray<uint64> JobIds;
		int32 Head = 0;
	};

	struct FTerrainQueue
	{
		uint64 TerrainId = 0;
		FLaneQueue Lanes[static_cast<int32>(EArcTerrainRuntimeLane::Count)];
		int32 LaneCursor = 0;
	};

	struct FOwnerQueue
	{
		FArcTerrainRuntimeOwnerId OwnerId;
		TArray<FTerrainQueue> Terrains;
		int32 TerrainCursor = 0;
	};

	struct FOwnerCompletions
	{
		TArray<FArcTerrainRuntimeCompletion> Lanes[static_cast<int32>(EArcTerrainRuntimeLane::Count)];
		int32 DrainCursor = 0;

		int32 PendingCount() const
		{
			int32 Count = 0;
			for (int32 Lane = 0; Lane < static_cast<int32>(EArcTerrainRuntimeLane::Count); ++Lane)
			{
				Count += Lanes[Lane].Num();
			}
			return Count;
		}
	};

	void ReleaseReservation_NoLock(uint64 ReservationId);
	void RetainPayloads_NoLock(const TArray<FArcTerrainRetainedPayload>& Payloads);
	void ReleasePayloadIds_NoLock(TConstArrayView<uint32> PayloadIds);
	FOwnerQueue& FindOrAddOwner_NoLock(FArcTerrainRuntimeOwnerId OwnerId);
	FTerrainQueue& FindOrAddTerrain_NoLock(FOwnerQueue& OwnerQueue, uint64 TerrainId);
	void EnqueueReady_NoLock(uint64 ReservationId, FArcTerrainRuntimeOwnerId OwnerId, uint64 TerrainId,
		EArcTerrainRuntimeLane Lane);
	void RemoveReadyJobs_NoLock(FArcTerrainRuntimeOwnerId OwnerId, uint64 TerrainIdFilter, bool bFilterTerrain);
	void DiscardCompletions_NoLock(FArcTerrainRuntimeOwnerId OwnerId, uint64 TerrainIdFilter, bool bFilterTerrain);
	void MarkInFlightAbandoned_NoLock(FArcTerrainRuntimeOwnerId OwnerId, uint64 TerrainIdFilter,
		bool bFilterTerrain);
	void CompleteJob(uint64 ReservationId, FArcTerrainRuntimeResult&& Result);
	void LaunchReadyJobs_NoLock();
	void GrantCompletions_NoLock();
	void CompactLaneQueue_NoLock(FLaneQueue& Queue);
	bool IsTerrainQueueEmpty_NoLock(const FTerrainQueue& TerrainQueue) const;
	void DropEmptyTerrainAt_NoLock(FOwnerQueue& OwnerQueue, int32 TerrainIndex);
	int32 GetLaneLimit_NoLock(FArcTerrainRuntimeOwnerId OwnerId, uint64 TerrainId,
		EArcTerrainRuntimeLane Lane) const;
	void ReadBudgetsFromSettings();
	void RemoveEmptyOwner_NoLock(FArcTerrainRuntimeOwnerId OwnerId);
	FArcTerrainRuntimeReservationToken MakeToken(uint64 ReservationId);

	mutable FCriticalSection Mutex;
	TMap<uint64, FReservationRecord> Reservations;
	TMap<uint32, FPayloadRetain> PayloadRetains;
	TMap<uint64, TSharedPtr<FQueuedJob, ESPMode::ThreadSafe>> ReadyJobs;
	TMap<uint64, FInFlightJob> InFlightJobs;
	TArray<FOwnerQueue> OwnerQueues;
	int32 OwnerCursor = 0;
	TMap<FArcTerrainRuntimeOwnerId, FOwnerCompletions> Completions;
	TMap<FArcTerrainRuntimeOwnerId, int32> GrantedCredits;
	TArray<FArcTerrainRuntimeOwnerId> GrantOwnerOrder;
	int32 GrantCursor = 0;
	TMap<FArcTerrainRuntimeOwnerTerrainLane, int32> LaneLimits;
	TMap<FArcTerrainRuntimeOwnerTerrainLane, int32> LaunchedThisFrame;
	uint64 NextReservationId = 1;
	uint64 OwnedBytes = 0;
	uint64 MaxOwnedBytes = static_cast<uint64>(DefaultOwnedMiB) << 20;
	int32 LiveTasks = 0;
	int32 MaxTasks = DefaultWorkerTasks;
	int32 CompletionGrantsPerFrame = DefaultCompletionGrantsPerFrame;
	int32 DefaultLaneLimits[static_cast<int32>(EArcTerrainRuntimeLane::Count)] = {
		DefaultAtlasLaunchesPerFrame,
		DefaultLightingLaunchesPerFrame,
		DefaultFarLaunchesPerFrame
	};
	uint64 LastPumpedFrame = MAX_uint64;
	bool bShutdown = false;
};
