// Copyright Lukasz Baran. All Rights Reserved.

#include "Browser/ArcJsonAssetsFileDataSource.h"

#include "ContentBrowserFileDataCore.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcJsonAssetsFileDataSource)

#define LOCTEXT_NAMESPACE "ArcJsonAssetsEditor"

bool UArcJsonAssetsFileDataSource::CanCopyItem(const FContentBrowserItemData& InItem, const FName InDestPath, FText* OutErrorMsg)
{
	ContentBrowserFileData::SetOptionalErrorMessage(OutErrorMsg,
		LOCTEXT("CannotCopy", "JSON assets cannot be copied: the copy would repeat the record id"));
	return false;
}

bool UArcJsonAssetsFileDataSource::CopyItem(const FContentBrowserItemData& InItem, const FName InDestPath)
{
	return false;
}

bool UArcJsonAssetsFileDataSource::BulkCopyItems(TArrayView<const FContentBrowserItemData> InItems, const FName InDestPath)
{
	return false;
}

#undef LOCTEXT_NAMESPACE
