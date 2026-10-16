// Copyright Lukasz Baran. All Rights Reserved.

#include "Tiles/ArcTerrainTileSet.h"
#include "ArcTerrainSubsystem.h"
#include "UObject/Package.h"

namespace ArcTerrainTileSetPrivate
{
	// Creation pattern: LandscapeEditResourcesSubsystem.cpp (ULandscapeScratchRenderTarget::OnRequested).
	UTextureRenderTarget2D* CreateTileRenderTarget(int32 Resolution, ETextureRenderTargetFormat Format)
	{
		const FName RTName = MakeUniqueObjectName(
			GetTransientPackage(), UTextureRenderTarget2D::StaticClass(), TEXT("ArcTerrainTileRT"));
		UTextureRenderTarget2D* RenderTarget =
			NewObject<UTextureRenderTarget2D>(GetTransientPackage(), RTName, RF_Transient);
		RenderTarget->bCanCreateUAV = true;
		RenderTarget->RenderTargetFormat = Format;
		RenderTarget->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
		RenderTarget->InitAutoFormat(Resolution, Resolution);
		RenderTarget->UpdateResourceImmediate(/*bClearRenderTarget = */false);
		return RenderTarget;
	}
}

void FArcTerrainRenderTargetPool::Initialize(UArcTerrainSubsystem& InOwner)
{
	Owner = &InOwner;
}

UTextureRenderTarget2D* FArcTerrainRenderTargetPool::Acquire(int32 Resolution, ETextureRenderTargetFormat Format)
{
	check(Owner != nullptr);
	const FPoolKey Key{Resolution, Format};

	if (TArray<UTextureRenderTarget2D*>* FreeList = FreeLists.Find(Key))
	{
		while (!FreeList->IsEmpty())
		{
			UTextureRenderTarget2D* Pooled = FreeList->Pop(EAllowShrinking::No);
			if (IsValid(Pooled))
			{
				return Pooled;
			}
		}
	}

	UTextureRenderTarget2D* Created = ArcTerrainTileSetPrivate::CreateTileRenderTarget(Resolution, Format);
	Owner->AddOwnedRenderTarget(*Created);
	return Created;
}

void FArcTerrainRenderTargetPool::Release(UTextureRenderTarget2D* RenderTarget)
{
	if (!IsValid(RenderTarget))
	{
		return;
	}
	const FPoolKey Key{RenderTarget->SizeX, RenderTarget->RenderTargetFormat};
	FreeLists.FindOrAdd(Key).Add(RenderTarget);
}

void FArcTerrainRenderTargetPool::Reset()
{
	FreeLists.Empty();
}

bool FArcTerrainTileSet::IsAllocatedFor(int32 NumWeightRTs, int32 WeightResolution) const
{
	if (WeightRTs.Num() != NumWeightRTs)
	{
		return false;
	}
	for (UTextureRenderTarget2D* WeightRT : WeightRTs)
	{
		if (WeightRT == nullptr || WeightRT->SizeX != WeightResolution)
		{
			return false;
		}
	}
	return true;
}

void FArcTerrainTileSet::Acquire(FArcTerrainRenderTargetPool& Pool, int32 NumWeightRTs,
	int32 WeightResolution)
{
	if (IsAllocatedFor(NumWeightRTs, WeightResolution))
	{
		return;
	}

	Release(Pool);

	WeightRTs.Reserve(NumWeightRTs);
	for (int32 Index = 0; Index < NumWeightRTs; ++Index)
	{
		WeightRTs.Add(Pool.Acquire(WeightResolution, RTF_RGBA8));
	}
}

void FArcTerrainTileSet::Release(FArcTerrainRenderTargetPool& Pool)
{
	for (UTextureRenderTarget2D* WeightRT : WeightRTs)
	{
		Pool.Release(WeightRT);
	}
	WeightRTs.Reset();
}
