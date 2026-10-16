// Copyright Lukasz Baran. All Rights Reserved.

#include "SpeciesEditor/SArcEcotopeBandDetails.h"

#include "Animation/TransformProviderData.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "IDetailPropertyRow.h"
#include "IDetailsView.h"
#include "IPropertyUtilities.h"
#include "InstancedStructDetails.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "Species/ArcEcotopeSpeciesModel.h"
#include "Species/ArcForestStageBand.h"
#include "SpeciesEditor/ArcEcotopeSpeciesEditor.h"
#include "StructUtils/InstancedStruct.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SArcEcotopeBandDetails"

namespace ArcEcotopeBandDetailsInternal
{
	const FName BandsPropertyName(TEXT("Bands"));

	bool IsAssetPropertyVisible(const FPropertyAndParent& PropertyAndParent)
	{
		if (PropertyAndParent.Property.GetFName() == BandsPropertyName)
		{
			return false;
		}

		for (const FProperty* ParentProperty : PropertyAndParent.ParentProperties)
		{
			if (ParentProperty != nullptr && ParentProperty->GetFName() == BandsPropertyName)
			{
				return false;
			}
		}

		return true;
	}

	const UScriptStruct* GetVisualScriptStruct(const TSharedPtr<IPropertyHandle>& VisualHandle)
	{
		TArray<const void*> RawData;
		VisualHandle->AccessRawData(RawData);
		if (RawData.Num() != 1 || RawData[0] == nullptr)
		{
			return nullptr;
		}

		const FInstancedStruct* Instanced = static_cast<const FInstancedStruct*>(RawData[0]);
		return Instanced->GetScriptStruct();
	}

	TSharedPtr<IPropertyHandle> FindChildByName(const TArray<TSharedPtr<IPropertyHandle>>& Handles, const FName Name)
	{
		for (const TSharedPtr<IPropertyHandle>& Handle : Handles)
		{
			if (!Handle.IsValid())
			{
				continue;
			}

			const FProperty* Property = Handle->GetProperty();
			if (Property != nullptr && Property->GetFName() == Name)
			{
				return Handle;
			}
		}

		return TSharedPtr<IPropertyHandle>();
	}

	int32 GetNumArrayElements(const TSharedPtr<IPropertyHandleArray>& ArrayHandle)
	{
		uint32 NumElements = 0;
		if (ArrayHandle->GetNumElements(NumElements) != FPropertyAccess::Success)
		{
			return 0;
		}

		return static_cast<int32>(NumElements);
	}

	TSharedRef<SWidget> MakeObjectPicker(
		const TSharedPtr<IPropertyHandle>& Handle,
		const UClass* AllowedClass,
		const TSharedPtr<FAssetThumbnailPool>& ThumbnailPool)
	{
		return SNew(SObjectPropertyEntryBox)
			.PropertyHandle(Handle)
			.AllowedClass(AllowedClass)
			.ThumbnailPool(ThumbnailPool)
			.DisplayThumbnail(false)
			.AllowClear(true)
			.DisplayUseSelected(true)
			.DisplayBrowse(true);
	}

	TSharedRef<SWidget> MakeValueWidget(const TSharedPtr<IPropertyHandle>& Handle, const float MinWidth)
	{
		return SNew(SBox)
			.MinDesiredWidth(MinWidth)
			[
				Handle->CreatePropertyValueWidget(false)
			];
	}

	void AddStructChildren(IDetailCategoryBuilder& Category, const TSharedPtr<IPropertyHandle>& StructHandle)
	{
		if (!StructHandle.IsValid())
		{
			return;
		}

		uint32 NumChildren = 0;
		if (StructHandle->GetNumChildren(NumChildren) != FPropertyAccess::Success)
		{
			return;
		}

		for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
		{
			const TSharedPtr<IPropertyHandle> ChildHandle = StructHandle->GetChildHandle(ChildIndex);
			if (ChildHandle.IsValid())
			{
				Category.AddProperty(ChildHandle);
			}
		}
	}
}

void SArcEcotopeBandDetails::Construct(const FArguments& InArgs)
{
	Species = InArgs._Species;
	Selection = InArgs._Selection;

	check(Selection.IsValid());
	Selection->OnChanged.AddSP(this, &SArcEcotopeBandDetails::HandleSelectionChanged);

	FPropertyEditorModule& PropertyEditorModule =
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	FDetailsViewArgs BandViewArgs;
	BandViewArgs.bAllowSearch = false;
	BandViewArgs.bShowOptions = false;
	BandViewArgs.bShowObjectLabel = false;
	BandViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	BandDetailsView = PropertyEditorModule.CreateDetailView(BandViewArgs);
	BandDetailsView->RegisterInstancedCustomPropertyLayout(
		UArcEcotopeSpeciesModel::StaticClass(),
		FOnGetDetailCustomizationInstance::CreateLambda(
			[SelectionRail = Selection]() -> TSharedRef<IDetailCustomization>
			{
				return StaticCastSharedRef<IDetailCustomization>(MakeShared<FArcEcotopeBandDetails>(SelectionRail));
			}));
	BandDetailsView->SetObject(Species.Get());

	FDetailsViewArgs AssetViewArgs;
	AssetViewArgs.bAllowSearch = false;
	AssetViewArgs.bShowOptions = false;
	AssetViewArgs.bShowObjectLabel = false;
	AssetViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	AssetDetailsView = PropertyEditorModule.CreateDetailView(AssetViewArgs);
	AssetDetailsView->SetIsPropertyVisibleDelegate(
		FIsPropertyVisible::CreateStatic(&ArcEcotopeBandDetailsInternal::IsAssetPropertyVisible));
	AssetDetailsView->SetObject(Species.Get());

	ChildSlot
	[
		SNew(SSplitter)
		.Orientation(Orient_Vertical)
		+ SSplitter::Slot()
		.Value(0.72f)
		[
			BandDetailsView.ToSharedRef()
		]
		+ SSplitter::Slot()
		.Value(0.28f)
		[
			AssetDetailsView.ToSharedRef()
		]
	];
}

void SArcEcotopeBandDetails::HandleSelectionChanged()
{
	if (BandDetailsView.IsValid())
	{
		BandDetailsView->ForceRefresh();
	}
}

FArcEcotopeBandDetails::FArcEcotopeBandDetails(const TSharedPtr<FArcEcotopeBandSelection>& InSelection)
	: Selection(InSelection)
{
}

void FArcEcotopeBandDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	PropertyUtilities = DetailBuilder.GetPropertyUtilities();
	ThumbnailPool = DetailBuilder.GetThumbnailPool();

	TArray<FName> CategoryNames;
	DetailBuilder.GetCategoryNames(CategoryNames);
	for (const FName& CategoryName : CategoryNames)
	{
		DetailBuilder.HideCategory(CategoryName);
	}

	IDetailCategoryBuilder& TerrainCategory =
		DetailBuilder.EditCategory(TEXT("SpeciesTerrain"), LOCTEXT("SpeciesTerrainCategory", "Terrain"));
	const TSharedRef<IPropertyHandle> TerrainCoverChannelHandle =
		DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UArcEcotopeSpeciesModel, TerrainCoverChannel));
	if (TerrainCoverChannelHandle->IsValidHandle())
	{
		TerrainCategory.AddProperty(TerrainCoverChannelHandle);
	}

	if (!Selection.IsValid())
	{
		return;
	}

	const int32 BandIndex = Selection->GetBandIndex();
	if (BandIndex == INDEX_NONE)
	{
		return;
	}

	const TSharedRef<IPropertyHandle> BandsHandle =
		DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UArcEcotopeSpeciesModel, Bands));
	const TSharedPtr<IPropertyHandleArray> BandsArray = BandsHandle->AsArray();
	if (!BandsArray.IsValid())
	{
		return;
	}

	if (BandIndex < 0 || BandIndex >= ArcEcotopeBandDetailsInternal::GetNumArrayElements(BandsArray))
	{
		return;
	}

	const TSharedRef<IPropertyHandle> BandHandle = BandsArray->GetElement(BandIndex);

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(TEXT("Band"), LOCTEXT("BandCategory", "Band"));

	const FName ScalarNames[] =
	{
		GET_MEMBER_NAME_CHECKED(FArcForestStageBand, Kind),
		GET_MEMBER_NAME_CHECKED(FArcForestStageBand, MinHeightCm),
		GET_MEMBER_NAME_CHECKED(FArcForestStageBand, MaxHeightCm),
		GET_MEMBER_NAME_CHECKED(FArcForestStageBand, OffsetAlongUpCm)
	};

	for (const FName& ScalarName : ScalarNames)
	{
		const TSharedPtr<IPropertyHandle> ScalarHandle = BandHandle->GetChildHandle(ScalarName);
		if (ScalarHandle.IsValid())
		{
			Category.AddProperty(ScalarHandle);
		}
	}

	IDetailCategoryBuilder& RotationCategory =
		DetailBuilder.EditCategory(TEXT("Rotation"), LOCTEXT("RotationCategory", "Rotation"));
	ArcEcotopeBandDetailsInternal::AddStructChildren(
		RotationCategory, BandHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcForestStageBand, Rotation)));

	IDetailCategoryBuilder& ScaleCategory =
		DetailBuilder.EditCategory(TEXT("Scale"), LOCTEXT("ScaleCategory", "Scale"));
	ArcEcotopeBandDetailsInternal::AddStructChildren(
		ScaleCategory, BandHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcForestStageBand, Scale)));

	const TSharedPtr<IPropertyHandle> VisualHandle =
		BandHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcForestStageBand, Visual));
	if (!VisualHandle.IsValid())
	{
		return;
	}

	VisualHandle->SetOnPropertyValueChanged(
		FSimpleDelegate::CreateSP(this, &FArcEcotopeBandDetails::HandleVisualTypeChanged));

	IDetailPropertyRow& VisualRow = Category.AddProperty(VisualHandle);
	TSharedPtr<SWidget> VisualNameWidget;
	TSharedPtr<SWidget> VisualValueWidget;
	FDetailWidgetRow VisualDefaultRow;
	VisualRow.GetDefaultWidgets(VisualNameWidget, VisualValueWidget, VisualDefaultRow);
	VisualRow.CustomWidget(false)
		.NameContent()
		[
			VisualNameWidget.ToSharedRef()
		]
		.ValueContent()
		.MinDesiredWidth(220.0f)
		[
			VisualValueWidget.ToSharedRef()
		];

	const UScriptStruct* VisualStruct = ArcEcotopeBandDetailsInternal::GetVisualScriptStruct(VisualHandle);
	if (VisualStruct == nullptr)
	{
		return;
	}

	// Stale nodes from the previous visual type survive a type change otherwise.
	VisualHandle->RemoveChildren();

	// A TInstancedStruct reflects only its script struct and bytes, so its inner properties have no
	// handles until AddChildStructure builds an FStructurePropertyNode over the live memory.
	const TArray<TSharedPtr<IPropertyHandle>> VisualChildren =
		VisualHandle->AddChildStructure(MakeShared<FInstancedStructProvider>(VisualHandle));

	if (VisualStruct->IsChildOf(FArcForestBandVisual_StaticMesh::StaticStruct()))
	{
		const TSharedPtr<IPropertyHandle> VariantsHandle = ArcEcotopeBandDetailsInternal::FindChildByName(
			VisualChildren, GET_MEMBER_NAME_CHECKED(FArcForestBandVisual_StaticMesh, Variants));
		BuildAddVariantRow(Category, VariantsHandle, LOCTEXT("AddStaticMesh", "Add Mesh"));
		BuildStaticMeshRows(Category, VariantsHandle);
	}
	else if (VisualStruct->IsChildOf(FArcForestBandVisual_SkinnedMesh::StaticStruct()))
	{
		const TSharedPtr<IPropertyHandle> VariantsHandle = ArcEcotopeBandDetailsInternal::FindChildByName(
			VisualChildren, GET_MEMBER_NAME_CHECKED(FArcForestBandVisual_SkinnedMesh, Variants));
		BuildAddVariantRow(Category, VariantsHandle, LOCTEXT("AddSkinnedMesh", "Add Skinned Mesh"));
		BuildSkinnedMeshRows(Category, VariantsHandle);
	}
	else if (VisualStruct->IsChildOf(FArcForestBandVisual_Entity::StaticStruct()))
	{
		const TSharedPtr<IPropertyHandle> EntityConfigHandle = ArcEcotopeBandDetailsInternal::FindChildByName(
			VisualChildren, GET_MEMBER_NAME_CHECKED(FArcForestBandVisual_Entity, EntityConfig));
		if (EntityConfigHandle.IsValid())
		{
			Category.AddProperty(EntityConfigHandle);
		}
	}
}

void FArcEcotopeBandDetails::BuildAddVariantRow(
	IDetailCategoryBuilder& Category,
	const TSharedPtr<IPropertyHandle>& VariantsHandle,
	const FText& Label)
{
	if (!VariantsHandle.IsValid())
	{
		return;
	}

	Category.AddCustomRow(Label)
		.WholeRowContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				PropertyCustomizationHelpers::MakeAddButton(
					FSimpleDelegate::CreateSP(this, &FArcEcotopeBandDetails::HandleAddVariant, VariantsHandle),
					Label)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(6.0f, 0.0f, 0.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(Label)
			]
		];
}

void FArcEcotopeBandDetails::BuildStaticMeshRows(
	IDetailCategoryBuilder& Category,
	const TSharedPtr<IPropertyHandle>& VariantsHandle)
{
	if (!VariantsHandle.IsValid())
	{
		return;
	}

	const TSharedPtr<IPropertyHandleArray> VariantsArray = VariantsHandle->AsArray();
	if (!VariantsArray.IsValid())
	{
		return;
	}

	const int32 NumVariants = ArcEcotopeBandDetailsInternal::GetNumArrayElements(VariantsArray);
	for (int32 ElementIndex = 0; ElementIndex < NumVariants; ++ElementIndex)
	{
		const TSharedRef<IPropertyHandle> ElementHandle = VariantsArray->GetElement(ElementIndex);
		const TSharedPtr<IPropertyHandle> MeshHandle =
			ElementHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcForestStaticMeshVariant, StaticMesh));
		const TSharedPtr<IPropertyHandle> WeightHandle =
			ElementHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcForestStaticMeshVariant, SelectionWeight));
		const TSharedPtr<IPropertyHandle> LengthAxisHandle =
			ElementHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcForestStaticMeshVariant, LengthAxis));
		if (!MeshHandle.IsValid() || !WeightHandle.IsValid() || !LengthAxisHandle.IsValid())
		{
			continue;
		}

		Category.AddCustomRow(LOCTEXT("StaticMeshVariantFilter", "Mesh Variant"))
			.WholeRowContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					ArcEcotopeBandDetailsInternal::MakeObjectPicker(
						MeshHandle, UStaticMesh::StaticClass(), ThumbnailPool)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					ArcEcotopeBandDetailsInternal::MakeValueWidget(WeightHandle, 60.0f)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					LengthAxisHandle->CreatePropertyValueWidget(false)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					PropertyCustomizationHelpers::MakeDeleteButton(
						FSimpleDelegate::CreateSP(
							this, &FArcEcotopeBandDetails::HandleDeleteVariant, VariantsHandle, ElementIndex),
						LOCTEXT("DeleteVariant", "Remove this variant"))
				]
			];
	}
}

void FArcEcotopeBandDetails::BuildSkinnedMeshRows(
	IDetailCategoryBuilder& Category,
	const TSharedPtr<IPropertyHandle>& VariantsHandle)
{
	if (!VariantsHandle.IsValid())
	{
		return;
	}

	const TSharedPtr<IPropertyHandleArray> VariantsArray = VariantsHandle->AsArray();
	if (!VariantsArray.IsValid())
	{
		return;
	}

	const int32 NumVariants = ArcEcotopeBandDetailsInternal::GetNumArrayElements(VariantsArray);
	for (int32 ElementIndex = 0; ElementIndex < NumVariants; ++ElementIndex)
	{
		const TSharedRef<IPropertyHandle> ElementHandle = VariantsArray->GetElement(ElementIndex);
		const TSharedPtr<IPropertyHandle> MeshHandle =
			ElementHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcForestSkeletalMeshVariant, SkeletalMesh));
		const TSharedPtr<IPropertyHandle> ProviderHandle =
			ElementHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcForestSkeletalMeshVariant, TransformProvider));
		const TSharedPtr<IPropertyHandle> AnimationIndexHandle =
			ElementHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcForestSkeletalMeshVariant, AnimationIndex));
		const TSharedPtr<IPropertyHandle> WeightHandle =
			ElementHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcForestSkeletalMeshVariant, SelectionWeight));
		if (!MeshHandle.IsValid() || !ProviderHandle.IsValid()
			|| !AnimationIndexHandle.IsValid() || !WeightHandle.IsValid())
		{
			continue;
		}

		Category.AddCustomRow(LOCTEXT("SkinnedMeshVariantFilter", "Skinned Mesh Variant"))
			.WholeRowContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					ArcEcotopeBandDetailsInternal::MakeObjectPicker(
						MeshHandle, USkeletalMesh::StaticClass(), ThumbnailPool)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					ArcEcotopeBandDetailsInternal::MakeObjectPicker(
						ProviderHandle, UTransformProviderData::StaticClass(), ThumbnailPool)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					ArcEcotopeBandDetailsInternal::MakeValueWidget(AnimationIndexHandle, 50.0f)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					ArcEcotopeBandDetailsInternal::MakeValueWidget(WeightHandle, 60.0f)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					PropertyCustomizationHelpers::MakeDeleteButton(
						FSimpleDelegate::CreateSP(
							this, &FArcEcotopeBandDetails::HandleDeleteVariant, VariantsHandle, ElementIndex),
						LOCTEXT("DeleteSkinnedVariant", "Remove this variant"))
				]
			];
	}
}

void FArcEcotopeBandDetails::HandleAddVariant(TSharedPtr<IPropertyHandle> VariantsHandle)
{
	if (!VariantsHandle.IsValid())
	{
		return;
	}

	const TSharedPtr<IPropertyHandleArray> VariantsArray = VariantsHandle->AsArray();
	if (!VariantsArray.IsValid())
	{
		return;
	}

	VariantsArray->AddItem();

	if (PropertyUtilities.IsValid())
	{
		PropertyUtilities->RequestForceRefresh();
	}
}

void FArcEcotopeBandDetails::HandleDeleteVariant(TSharedPtr<IPropertyHandle> VariantsHandle, int32 ElementIndex)
{
	if (!VariantsHandle.IsValid())
	{
		return;
	}

	const TSharedPtr<IPropertyHandleArray> VariantsArray = VariantsHandle->AsArray();
	if (!VariantsArray.IsValid())
	{
		return;
	}

	VariantsArray->DeleteItem(ElementIndex);

	if (PropertyUtilities.IsValid())
	{
		PropertyUtilities->RequestForceRefresh();
	}
}

void FArcEcotopeBandDetails::HandleVisualTypeChanged()
{
	if (PropertyUtilities.IsValid())
	{
		PropertyUtilities->RequestForceRefresh();
	}
}

#undef LOCTEXT_NAMESPACE
