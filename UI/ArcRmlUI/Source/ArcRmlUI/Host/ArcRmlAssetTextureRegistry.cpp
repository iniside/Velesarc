// Copyright Lukasz Baran. All Rights Reserved.

#include "Host/ArcRmlAssetTextureRegistry.h"

#include "ArcRmlUILog.h"
#include "Engine/Texture.h"
#include "Thread/ArcRmlUIThread.h"

FArcRmlAssetTextureRegistry::FArcRmlAssetTextureRegistry(FArcRmlUIThread& InThread)
	: Thread(InThread)
{
	check(IsInGameThread());
}

FArcRmlAssetTextureRegistry::~FArcRmlAssetTextureRegistry()
{
#if WITH_EDITOR
	if (CompilationTicker.IsValid())
	{
		FTSTicker::RemoveTicker(CompilationTicker);
	}
#endif
}

void FArcRmlAssetTextureRegistry::Request(uint32 ViewId, const FString& AssetPath, const Rml::AssetTextureRequest& TextureRequest)
{
	check(IsInGameThread());
	if (AcknowledgedViews.Contains(ViewId))
	{
		return;
	}

	const FSoftObjectPath Path(AssetPath);
	if (!Path.IsValid())
	{
		UE_LOGF(LogArcRmlUI, Warning, "ArcRmlUI: asset texture '%ls' is not an object path", *AssetPath);
		Thread.InstallAssetTexture(ViewId, TextureRequest, FIntPoint::ZeroValue, 0);
		return;
	}

	if (FPendingLoad* Pending = Loads.Find(Path))
	{
		Pending->Waiters.Add(FWaiter{ViewId, TextureRequest});
		return;
	}

	Loads.Add(Path).Waiters.Add(FWaiter{ViewId, TextureRequest});
	TSharedPtr<FStreamableHandle> Handle = StreamableManager.RequestAsyncLoad(Path,
		FStreamableDelegateWithHandle::CreateSP(this, &FArcRmlAssetTextureRegistry::HandleLoaded, Path));
	if (FPendingLoad* Started = Loads.Find(Path))
	{
		Started->Handle = MoveTemp(Handle);
	}
}

void FArcRmlAssetTextureRegistry::AcknowledgeRelease(uint32 ViewId)
{
	check(IsInGameThread());
	AcknowledgedViews.Add(ViewId);
	ViewTextures.Remove(ViewId);
}

void FArcRmlAssetTextureRegistry::ReleaseAll()
{
	check(IsInGameThread());
	TMap<FSoftObjectPath, FPendingLoad> Canceled = MoveTemp(Loads);
	Loads.Reset();
	for (TPair<FSoftObjectPath, FPendingLoad>& Pair : Canceled)
	{
		if (Pair.Value.Handle.IsValid())
		{
			Pair.Value.Handle->CancelHandle();
		}
	}
	ViewTextures.Reset();
}

void FArcRmlAssetTextureRegistry::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (TPair<uint32, TMap<FSoftObjectPath, TObjectPtr<UTexture>>>& View : ViewTextures)
	{
		for (TPair<FSoftObjectPath, TObjectPtr<UTexture>>& Texture : View.Value)
		{
			Collector.AddReferencedObject(Texture.Value);
		}
	}
}

FString FArcRmlAssetTextureRegistry::GetReferencerName() const
{
	return TEXT("FArcRmlAssetTextureRegistry");
}

void FArcRmlAssetTextureRegistry::HandleLoaded(TSharedPtr<FStreamableHandle> Handle, FSoftObjectPath Path)
{
	FPendingLoad* Pending = Loads.Find(Path);
	if (!Pending)
	{
		return;
	}

#if WITH_EDITOR
	const UTexture* Texture = Cast<UTexture>(Path.ResolveObject());
	if (Texture && Texture->IsDefaultTexture())
	{
		Pending->bAwaitingCompilation = true;
		if (!CompilationTicker.IsValid())
		{
			CompilationTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &FArcRmlAssetTextureRegistry::TickCompilation));
		}
		return;
	}
#endif

	FinishLoad(Path);
}

#if WITH_EDITOR
bool FArcRmlAssetTextureRegistry::TickCompilation(float DeltaTime)
{
	TArray<FSoftObjectPath> Compiled;
	bool bStillCompiling = false;
	for (const TPair<FSoftObjectPath, FPendingLoad>& Pair : Loads)
	{
		if (!Pair.Value.bAwaitingCompilation)
		{
			continue;
		}

		const UTexture* Texture = Cast<UTexture>(Pair.Key.ResolveObject());
		if (Texture && Texture->IsDefaultTexture())
		{
			bStillCompiling = true;
			continue;
		}
		Compiled.Add(Pair.Key);
	}

	for (const FSoftObjectPath& Path : Compiled)
	{
		FinishLoad(Path);
	}

	if (!bStillCompiling)
	{
		CompilationTicker.Reset();
	}
	return bStillCompiling;
}
#endif

void FArcRmlAssetTextureRegistry::FinishLoad(const FSoftObjectPath& Path)
{
	FPendingLoad Load;
	if (!Loads.RemoveAndCopyValue(Path, Load))
	{
		return;
	}

	UObject* Object = Path.ResolveObject();
	UTexture* Texture = Cast<UTexture>(Object);
	FIntPoint Size = FIntPoint::ZeroValue;
	if (!Object)
	{
		UE_LOGF(LogArcRmlUI, Warning, "ArcRmlUI: asset texture '%ls' could not be loaded", *Path.ToString());
	}
	else if (!Texture || Texture->GetTextureClass() != ETextureClass::TwoD || Texture->IsCurrentlyVirtualTextured() || !Texture->GetResource())
	{
		UE_LOGF(LogArcRmlUI, Warning, "ArcRmlUI: asset texture '%ls' is a %ls, not a 2D texture with a GPU resource and no virtual texturing",
			*Path.ToString(), *Object->GetClass()->GetName());
	}
	else
	{
		Size = FIntPoint(FMath::RoundToInt32(Texture->GetSurfaceWidth()), FMath::RoundToInt32(Texture->GetSurfaceHeight()));
		if (Size.X <= 0 || Size.Y <= 0)
		{
			UE_LOGF(LogArcRmlUI, Warning, "ArcRmlUI: asset texture '%ls' has no size", *Path.ToString());
			Size = FIntPoint::ZeroValue;
		}
	}

	const bool bLoaded = Size.X > 0;
	const Rml::AssetTextureToken Token = bLoaded ? reinterpret_cast<Rml::AssetTextureToken>(&Texture->TextureReference) : 0;
	for (const FWaiter& Waiter : Load.Waiters)
	{
		if (bLoaded && !AcknowledgedViews.Contains(Waiter.ViewId))
		{
			ViewTextures.FindOrAdd(Waiter.ViewId).Add(Path, Texture);
		}
		Thread.InstallAssetTexture(Waiter.ViewId, Waiter.TextureRequest, Size, Token);
	}

	if (Load.Handle.IsValid())
	{
		Load.Handle->ReleaseHandle();
	}
}
