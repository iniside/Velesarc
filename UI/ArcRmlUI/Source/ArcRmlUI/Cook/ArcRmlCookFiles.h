// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Interfaces/ArcRmlPaths.h"

#include "ArcRmlCookFiles.generated.h"

UCLASS()
class ARCRMLUI_API UArcRmlCookFiles : public UDataAsset
{
	GENERATED_BODY()

public:
	virtual bool NeedsLoadForServer() const override;

#if WITH_EDITOR
	virtual void OnCookEvent(UE::Cook::ECookEvent CookEvent, UE::Cook::FCookEventContext& Context) override;

private:
	virtual void CookAdditionalFilesOverride(const TCHAR* PackageFilename, const ITargetPlatform* TargetPlatform,
		TFunctionRef<void(const TCHAR* Filename, void* Data, int64 Size)> WriteAdditionalFile) override;

	FArcRmlContentRoots CookRoots;
	FString CookPackageSuffix;
#endif
};
