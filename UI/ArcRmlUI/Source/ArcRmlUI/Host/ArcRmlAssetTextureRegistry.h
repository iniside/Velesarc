// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#if WITH_EDITOR
#include "Containers/Ticker.h"
#endif
#include "Engine/StreamableManager.h"
#include "UObject/GCObject.h"
#include "UObject/SoftObjectPath.h"

#include "RmlUi/Core/AssetTexture.h"

class FArcRmlUIThread;
class UTexture;

class FArcRmlAssetTextureRegistry final : public FGCObject, public TSharedFromThis<FArcRmlAssetTextureRegistry, ESPMode::ThreadSafe>
{
public:
	explicit FArcRmlAssetTextureRegistry(FArcRmlUIThread& InThread);
	virtual ~FArcRmlAssetTextureRegistry() override;

	void Request(uint32 ViewId, const FString& AssetPath, const Rml::AssetTextureRequest& TextureRequest);
	void AcknowledgeRelease(uint32 ViewId);
	void ReleaseAll();

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

private:
	struct FWaiter
	{
		uint32 ViewId = 0;
		Rml::AssetTextureRequest TextureRequest;
	};

	struct FPendingLoad
	{
		TSharedPtr<FStreamableHandle> Handle;
		TArray<FWaiter> Waiters;
#if WITH_EDITOR
		bool bAwaitingCompilation = false;
#endif
	};

	void HandleLoaded(TSharedPtr<FStreamableHandle> Handle, FSoftObjectPath Path);
#if WITH_EDITOR
	bool TickCompilation(float DeltaTime);
#endif
	void FinishLoad(const FSoftObjectPath& Path);

	FArcRmlUIThread& Thread;
	FStreamableManager StreamableManager;
	TMap<FSoftObjectPath, FPendingLoad> Loads;
	TMap<uint32, TMap<FSoftObjectPath, TObjectPtr<UTexture>>> ViewTextures;
	TSet<uint32> AcknowledgedViews;
#if WITH_EDITOR
	FTSTicker::FDelegateHandle CompilationTicker;
#endif
};
