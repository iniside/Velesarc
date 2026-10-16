// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "JsonAssets/ArcJsonAssetPaths.h"

#include "ArcJsonAssetsCookFiles.generated.h"

UCLASS()
class ARCJSONASSETS_API UArcJsonAssetsCookFiles : public UDataAsset
{
	GENERATED_BODY()

#if WITH_EDITOR
public:
	virtual void OnCookEvent(UE::Cook::ECookEvent CookEvent, UE::Cook::FCookEventContext& Context) override;

private:
	virtual void CookAdditionalFilesOverride(const TCHAR* PackageFilename, const ITargetPlatform* TargetPlatform,
		TFunctionRef<void(const TCHAR* Filename, void* Data, int64 Size)> WriteAdditionalFile) override;

	FArcJsonAssetContentRoots CookRoots;
	TArray<FString> CookExtensions;
	FString CookPackageSuffix;
#endif
};
