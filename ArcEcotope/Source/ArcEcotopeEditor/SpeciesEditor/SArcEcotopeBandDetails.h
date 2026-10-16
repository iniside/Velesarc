// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FAssetThumbnailPool;
class IDetailCategoryBuilder;
class IDetailLayoutBuilder;
class IDetailsView;
class IPropertyHandle;
class IPropertyUtilities;
class UArcEcotopeSpeciesModel;
struct FArcEcotopeBandSelection;

class SArcEcotopeBandDetails : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcEcotopeBandDetails) {}
		SLATE_ARGUMENT_DEFAULT(UArcEcotopeSpeciesModel*, Species) { nullptr };
		SLATE_ARGUMENT(TSharedPtr<FArcEcotopeBandSelection>, Selection)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void HandleSelectionChanged();

	TWeakObjectPtr<UArcEcotopeSpeciesModel> Species;
	TSharedPtr<FArcEcotopeBandSelection> Selection;
	TSharedPtr<IDetailsView> BandDetailsView;
	TSharedPtr<IDetailsView> AssetDetailsView;
};

class FArcEcotopeBandDetails : public IDetailCustomization
{
public:
	explicit FArcEcotopeBandDetails(const TSharedPtr<FArcEcotopeBandSelection>& InSelection);

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	void BuildStaticMeshRows(IDetailCategoryBuilder& Category, const TSharedPtr<IPropertyHandle>& VariantsHandle);
	void BuildSkinnedMeshRows(IDetailCategoryBuilder& Category, const TSharedPtr<IPropertyHandle>& VariantsHandle);
	void BuildAddVariantRow(IDetailCategoryBuilder& Category, const TSharedPtr<IPropertyHandle>& VariantsHandle, const FText& Label);

	void HandleAddVariant(TSharedPtr<IPropertyHandle> VariantsHandle);
	void HandleDeleteVariant(TSharedPtr<IPropertyHandle> VariantsHandle, int32 ElementIndex);
	void HandleVisualTypeChanged();

	TSharedPtr<FArcEcotopeBandSelection> Selection;
	TSharedPtr<IPropertyUtilities> PropertyUtilities;
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
};
