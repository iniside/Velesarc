// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTerrainInvokers.h"
#include "ArcTerrainModule.h"
#include "Algo/Unique.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/Engine.h"
#include "LevelEditorViewport.h"
#endif

namespace ArcTerrainInvokersPrivate
{
	// 10 000 km, four orders past any authored map: past this the key math's FloorToInt32 saturates.
	constexpr double GMaxInvokerCoord = 1.0e9;

	bool IsUsableViewPosition(const FVector& Position)
	{
		return !Position.ContainsNaN() && Position.GetAbsMax() <= GMaxInvokerCoord;
	}

	bool PositionLess(const FVector& A, const FVector& B)
	{
		if (A.X != B.X)
		{
			return A.X < B.X;
		}
		if (A.Y != B.Y)
		{
			return A.Y < B.Y;
		}
		return A.Z < B.Z;
	}

	void PruneStaleViewers(FArcTerrainInvokerCache& Cache)
	{
		for (TMap<TWeakObjectPtr<const APlayerController>, FArcTerrainInvokerViewState>::TIterator It =
			Cache.LastGoodByController.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}

	// 1 m quantization: keeps the ring's resident set stable under small camera jitter.
	FVector Quantize(const FVector& Position)
	{
		return FVector(
			FMath::GridSnap(Position.X, 100.0),
			FMath::GridSnap(Position.Y, 100.0),
			FMath::GridSnap(Position.Z, 100.0));
	}

#if WITH_EDITOR
	bool IsPlayInEditorSessionActive()
	{
		if (GEditor != nullptr && GEditor->PlayWorld != nullptr)
		{
			return true;
		}

		if (GEngine == nullptr)
		{
			return false;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World() != nullptr)
			{
				return true;
			}
		}

		return false;
	}

	// Port of FVoxelUtilities::GetCameraView's editor branch (VoxelGameUtilities.cpp): current
	// level-editing client when it belongs to the world; otherwise the active viewport's client,
	// upgraded to a perspective sibling when the active one is ortho; otherwise the last
	// world-matching client.
	const FEditorViewportClient* FindBestViewportClient(const UWorld* World)
	{
		if (GEditor == nullptr)
		{
			return nullptr;
		}

		if (GCurrentLevelEditingViewportClient != nullptr
			&& GCurrentLevelEditingViewportClient->GetWorld() == World)
		{
			return GCurrentLevelEditingViewportClient;
		}

		TArray<const FEditorViewportClient*, TInlineAllocator<8>> ValidClients;
		for (const FEditorViewportClient* ViewportClient : GEditor->GetAllViewportClients())
		{
			if (ViewportClient != nullptr && ViewportClient->GetWorld() == World)
			{
				ValidClients.Add(ViewportClient);
			}
		}

		const FViewport* ActiveViewport = GEditor->GetActiveViewport();
		const FEditorViewportClient* ActiveViewportClient = nullptr;
		for (const FEditorViewportClient* ViewportClient : ValidClients)
		{
			if (ViewportClient->Viewport == ActiveViewport)
			{
				ActiveViewportClient = ViewportClient;
			}
		}

		if (ActiveViewportClient != nullptr)
		{
			if (!ActiveViewportClient->IsPerspective())
			{
				for (const FEditorViewportClient* ViewportClient : ValidClients)
				{
					if (ViewportClient->IsPerspective())
					{
						return ViewportClient;
					}
				}
			}
			return ActiveViewportClient;
		}

		return (ValidClients.Num() > 0) ? ValidClients.Last() : nullptr;
	}
#endif // WITH_EDITOR
}

namespace ArcTerrain
{
	void GatherInvokers(const UWorld* World, FArcTerrainInvokerCache& Cache,
		TArray<FVector>& OutPositions, int32& OutNumSubstituted)
	{
		using namespace ArcTerrainInvokersPrivate;

		OutPositions.Reset();
		OutNumSubstituted = 0;
		if (World == nullptr)
		{
			return;
		}

		PruneStaleViewers(Cache);

		if (World->IsGameWorld())
		{
			TArray<FVector> Substituted;
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				const APlayerController* PlayerController = It->Get();
				if (PlayerController == nullptr)
				{
					continue;
				}

				FVector ViewLocation = FVector::ZeroVector;
				FRotator ViewRotation = FRotator::ZeroRotator;
				PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

				FArcTerrainInvokerViewState& ViewState =
					Cache.LastGoodByController.FindOrAdd(PlayerController);
				if (IsUsableViewPosition(ViewLocation))
				{
					ViewState.LastGoodPosition = Quantize(ViewLocation);
					ViewState.bHasLastGoodPosition = true;
					ViewState.bRejecting = false;
					OutPositions.Add(ViewState.LastGoodPosition);
					continue;
				}

				UE_CLOGF(!ViewState.bRejecting, LogArcTerrain, Warning,"Rejected the view point of '%ls': (%f,%f,%f) is not finite or exceeds %f cm. %ls",
					*PlayerController->GetName(), ViewLocation.X, ViewLocation.Y, ViewLocation.Z,
					GMaxInvokerCoord,
					ViewState.bHasLastGoodPosition
						? TEXT("Holding its last accepted position.")
						: TEXT("It has no accepted position yet and contributes no invoker."));
				ViewState.bRejecting = true;
				if (ViewState.bHasLastGoodPosition)
				{
					Substituted.Add(ViewState.LastGoodPosition);
				}
			}

			OutPositions.Sort(&PositionLess);
			OutPositions.SetNum(Algo::Unique(OutPositions));

			Substituted.Sort(&PositionLess);
			Substituted.SetNum(Algo::Unique(Substituted));
			for (const FVector& Position : Substituted)
			{
				if (!OutPositions.Contains(Position))
				{
					OutPositions.Add(Position);
					++OutNumSubstituted;
				}
			}
		}
		else
		{
#if WITH_EDITOR
			if (IsPlayInEditorSessionActive())
			{
				return;
			}
			if (const FEditorViewportClient* ViewportClient = FindBestViewportClient(World))
			{
				const FVector ViewLocation = ViewportClient->GetViewLocation();
				if (IsUsableViewPosition(ViewLocation))
				{
					Cache.EditorView.LastGoodPosition = Quantize(ViewLocation);
					Cache.EditorView.bHasLastGoodPosition = true;
					Cache.EditorView.bRejecting = false;
					OutPositions.Add(Cache.EditorView.LastGoodPosition);
					return;
				}

				UE_CLOGF(!Cache.EditorView.bRejecting, LogArcTerrain, Warning,
					"Rejected the editor viewport view point: (%f,%f,%f) is not finite or exceeds "
					"%f cm. %ls",
					ViewLocation.X, ViewLocation.Y, ViewLocation.Z, GMaxInvokerCoord,
					Cache.EditorView.bHasLastGoodPosition
						? TEXT("Holding the last accepted position.")
						: TEXT("There is no accepted position yet and no invoker."));
				Cache.EditorView.bRejecting = true;
				if (Cache.EditorView.bHasLastGoodPosition)
				{
					OutPositions.Add(Cache.EditorView.LastGoodPosition);
					++OutNumSubstituted;
				}
			}
#endif
		}
	}

	bool GetViewPosition(const UWorld* World, FVector& OutPosition)
	{
		using namespace ArcTerrainInvokersPrivate;

		if (World == nullptr)
		{
			return false;
		}

		if (World->IsGameWorld())
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				const APlayerController* PlayerController = It->Get();
				if (PlayerController == nullptr || !PlayerController->IsLocalController())
				{
					continue;
				}

				if (PlayerController->PlayerCameraManager != nullptr)
				{
					const FVector CameraLocation =
						PlayerController->PlayerCameraManager->GetCameraLocation();
					if (!IsUsableViewPosition(CameraLocation))
					{
						return false;
					}
					OutPosition = CameraLocation;
					return true;
				}
				if (const APawn* Pawn = PlayerController->GetPawn())
				{
					const FVector PawnLocation = Pawn->GetActorLocation();
					if (!IsUsableViewPosition(PawnLocation))
					{
						return false;
					}
					OutPosition = PawnLocation;
					return true;
				}
			}
			return false;
		}

#if WITH_EDITOR
		if (const FEditorViewportClient* ViewportClient = FindBestViewportClient(World))
		{
			const FVector ViewLocation = ViewportClient->GetViewLocation();
			if (!IsUsableViewPosition(ViewLocation))
			{
				return false;
			}
			OutPosition = ViewLocation;
			return true;
		}
#endif
		return false;
	}
}
