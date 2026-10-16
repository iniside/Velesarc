// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ContentBrowserFileDataSource.h"
#include "ArcJsonAssetsFileDataSource.generated.h"

UCLASS()
class UArcJsonAssetsFileDataSource : public UContentBrowserFileDataSource
{
	GENERATED_BODY()

public:
	virtual bool CanCopyItem(const FContentBrowserItemData& InItem, const FName InDestPath, FText* OutErrorMsg) override;
	virtual bool CopyItem(const FContentBrowserItemData& InItem, const FName InDestPath) override;
	virtual bool BulkCopyItems(TArrayView<const FContentBrowserItemData> InItems, const FName InDestPath) override;
};
