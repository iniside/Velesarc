// Copyright Lukasz Baran. All Rights Reserved.

#include "Details/ArcEcotopeVolumeDetails.h"

#include "ArcEcotopeEditorModule.h"
#include "ArcEcotopeModule.h"
#include "Components/BoxComponent.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IPropertyUtilities.h"
#include "Misc/DataValidation.h"
#include "ScopedTransaction.h"
#include "Simulation/ArcForestBootstrap.h"
#include "Simulation/ArcEcotopeInstanceBuilder.h"
#include "Simulation/ArcForestSimulationController.h"
#include "Simulation/ArcForestSimulationState.h"
#include "Simulation/ArcEcotopeVolume.h"
#include "Simulation/ArcEcotopeSiteBaker.h"
#include "Stamps/ArcTerrainStampComponent.h"
#include "Styling/AppStyle.h"
#include "Terrain/ArcEcotopeTerrainWeightStamp.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ArcEcotopeVolumeDetails"

namespace ArcEcotopeVolumeDetailsPrivate
{
	constexpr uint64 MaximumTransactionalSiteBakeBytes = 64ull * 1024ull * 1024ull;

	uint64 GetWorldSnapshotAllocatedBytes(const FArcForestWorldSnapshot& Snapshot)
	{
		return Snapshot.SpeciesEcologyHashes.GetAllocatedSize() + Snapshot.Individuals.GetAllocatedSize()
			+ Snapshot.Cohorts.GetAllocatedSize() + Snapshot.Patches.GetAllocatedSize()
			+ Snapshot.Deadwood.GetAllocatedSize();
	}

	struct FEditorStatus
	{
		FText LastError;
		FName SelectedCategory;
	};

	TMap<FGuid, FEditorStatus> StatusByVolumeId;

	FEditorStatus* FindStatus(const AArcEcotopeVolume* Volume)
	{
		return Volume != nullptr && Volume->VolumeId.IsValid()
			? StatusByVolumeId.Find(Volume->VolumeId)
			: nullptr;
	}

	FEditorStatus* FindOrAddStatus(const AArcEcotopeVolume& Volume)
	{
		return Volume.VolumeId.IsValid()
			? &StatusByVolumeId.FindOrAdd(Volume.VolumeId)
			: nullptr;
	}

	const TCHAR* ToString(const EArcForestSimulationSessionStatus Status)
	{
		switch (Status)
		{
		case EArcForestSimulationSessionStatus::Running: return TEXT("Running");
		case EArcForestSimulationSessionStatus::Cancelling: return TEXT("Cancelling");
		case EArcForestSimulationSessionStatus::Succeeded: return TEXT("Succeeded");
		case EArcForestSimulationSessionStatus::Cancelled: return TEXT("Cancelled");
		case EArcForestSimulationSessionStatus::Failed: return TEXT("Failed");
		case EArcForestSimulationSessionStatus::Stale: return TEXT("Stale");
		default: return TEXT("Unknown");
		}
	}

	const TCHAR* ToString(const EArcForestGrowthPhase Phase)
	{
		switch (Phase)
		{
		case EArcForestGrowthPhase::Idle: return TEXT("Idle");
		case EArcForestGrowthPhase::Validation: return TEXT("Validation");
		case EArcForestGrowthPhase::CanopyAndHabitat: return TEXT("Canopy & habitat");
		case EArcForestGrowthPhase::GrowthAndCompetition: return TEXT("Growth & competition");
		case EArcForestGrowthPhase::Mortality: return TEXT("Mortality");
		case EArcForestGrowthPhase::BranchShed: return TEXT("Branch shed");
		case EArcForestGrowthPhase::DeadwoodAndDecay: return TEXT("Deadwood & decay");
		case EArcForestGrowthPhase::Reproduction: return TEXT("Reproduction");
		case EArcForestGrowthPhase::Recruitment: return TEXT("Recruitment");
		case EArcForestGrowthPhase::CohortPromotion: return TEXT("Cohort promotion");
		case EArcForestGrowthPhase::Canonicalization: return TEXT("Canonicalization");
		case EArcForestGrowthPhase::Complete: return TEXT("Complete");
		case EArcForestGrowthPhase::Cancelled: return TEXT("Cancelled");
		case EArcForestGrowthPhase::Failed: return TEXT("Failed");
		default: return TEXT("Unknown");
		}
	}

	FString DescribeCanStartOperationRefusal(const AArcEcotopeVolume* Volume)
	{
		if (Volume == nullptr)
		{
			return TEXT("Volume is null");
		}
		if (!FArcEcotopeEditorModule::IsAvailable())
		{
			return TEXT("FArcEcotopeEditorModule is not available");
		}
		if (FArcEcotopeEditorModule::Get().GetSimulationController().HasActiveSession())
		{
			return TEXT("a simulation session is already active");
		}
		return TEXT("unknown");
	}

	static const FName ArcEcotopeScriptPackageName(TEXT("/Script/ArcEcotope"));
	static const FName ActionsCategoryName(TEXT("ArcEcotope Actions"));
	static const FName StatusCategoryName(TEXT("ArcEcotope Status"));
	static const FName TabsCategoryName(TEXT("ArcEcotope Categories"));
	static const FName TransformCommonCategoryName(TEXT("TransformCommon"));
	static const FName TransformCategoryName(TEXT("Transform"));
	static const FName TerrainCategoryName(TEXT("Terrain"));

	constexpr int32 PinnedSortOrderBase = static_cast<int32>(ECategoryPriority::Important) * 1000;

	bool IsPinnedCategory(const FName CategoryName)
	{
		return CategoryName == ActionsCategoryName
			|| CategoryName == StatusCategoryName
			|| CategoryName == TabsCategoryName
			|| CategoryName == TransformCommonCategoryName
			|| CategoryName == TransformCategoryName;
	}

	FName GetTopLevelCategory(const FName CategoryName)
	{
		FString CategoryString = CategoryName.ToString();
		int32 DelimiterIndex = INDEX_NONE;
		if (!CategoryString.FindChar(TEXT('|'), DelimiterIndex))
		{
			return CategoryName;
		}
		CategoryString.LeftInline(DelimiterIndex);
		CategoryString.TrimEndInline();
		return FName(*CategoryString);
	}

	struct FLaidOutProperty
	{
		const FProperty* Property = nullptr;
		FName Category;
		FName InstanceName;
	};

	bool IsArcEcotopeOwned(const FProperty& Property)
	{
		const UStruct* OwnerStruct = Property.GetOwnerStruct();
		const UPackage* OwnerPackage = OwnerStruct != nullptr ? OwnerStruct->GetOutermost() : nullptr;
		return OwnerPackage != nullptr && OwnerPackage->GetFName() == ArcEcotopeScriptPackageName;
	}

	void GatherLaidOutProperties(const UStruct& Owner, const FName InstanceName, TArray<FLaidOutProperty>& OutProperties)
	{
		static const FName CategoryKey(TEXT("Category"));
		static const FName ShowOnlyInnerPropertiesKey(TEXT("ShowOnlyInnerProperties"));

		for (TFieldIterator<FProperty> PropertyIt(&Owner, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			const FProperty* Property = *PropertyIt;
			const FString& CategoryString = Property->GetMetaData(CategoryKey);
			OutProperties.Add(FLaidOutProperty{
				Property,
				CategoryString.IsEmpty() ? NAME_None : FName(*CategoryString),
				InstanceName});

			const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
			if (StructProperty != nullptr && StructProperty->Struct != nullptr && Property->HasMetaData(ShowOnlyInnerPropertiesKey))
			{
				GatherLaidOutProperties(*StructProperty->Struct, Property->GetFName(), OutProperties);
			}
		}
	}

	TSharedRef<SWidget> MakeButton(
		const FText& Label,
		const FText& ToolTip,
		const FOnClicked& OnClicked,
		const TAttribute<bool>& IsEnabled = true)
	{
		return SNew(SBox)
			.MinDesiredWidth(150.0f)
			[
				SNew(SButton)
				.Text(Label)
				.ToolTipText(ToolTip)
				.IsEnabled(IsEnabled)
				.OnClicked(OnClicked)
			];
	}

	void AddTerrainStampRows(IDetailLayoutBuilder& DetailBuilder, UArcEcotopeTerrainWeightStamp& Stamp)
	{
		IDetailCategoryBuilder& TerrainCategory = DetailBuilder.EditCategory(
			TerrainCategoryName,
			LOCTEXT("TerrainCategory", "Terrain"));

		TArray<UObject*> StampObjects;
		StampObjects.Add(&Stamp);

		const FName StampPropertyNames[] = {
			GET_MEMBER_NAME_CHECKED(UArcTerrainStampComponent, AffectedTerrain),
			GET_MEMBER_NAME_CHECKED(UArcTerrainStampComponent, Priority),
			GET_MEMBER_NAME_CHECKED(UArcEcotopeTerrainWeightStamp, CanopyChannel),
			GET_MEMBER_NAME_CHECKED(UArcEcotopeTerrainWeightStamp, LitterChannel),
			GET_MEMBER_NAME_CHECKED(UArcEcotopeTerrainWeightStamp, MaskExpandCm),
			GET_MEMBER_NAME_CHECKED(UArcEcotopeTerrainWeightStamp, MaskFalloffCm),
			GET_MEMBER_NAME_CHECKED(UArcEcotopeTerrainWeightStamp, MaskFalloffExponent)};

		for (const FName PropertyName : StampPropertyNames)
		{
			const IDetailPropertyRow* Row = TerrainCategory.AddExternalObjectProperty(StampObjects, PropertyName);
			UE_CLOGF(
				Row == nullptr,
				LogArcEcotope,
				Warning,"ArcEcotope volume details: terrain stamp property '%ls' could not be added.",
				*PropertyName.ToString());
		}
	}
}

TSharedRef<IDetailCustomization> FArcEcotopeVolumeDetails::MakeInstance()
{
	return MakeShared<FArcEcotopeVolumeDetails>();
}

void FArcEcotopeVolumeDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() != 1)
	{
		return;
	}

	SelectedVolume = Cast<AArcEcotopeVolume>(Objects[0].Get());
	if (!SelectedVolume.IsValid())
	{
		return;
	}

	using namespace ArcEcotopeVolumeDetailsPrivate;

	PropertyUtilities = DetailBuilder.GetPropertyUtilities();

	BuildActionsCategory(DetailBuilder);
	BuildStatusCategory(DetailBuilder);

	IDetailCategoryBuilder& TransformCategory = DetailBuilder.EditCategory(
		TransformCommonCategoryName,
		LOCTEXT("TransformCategory", "Transform"),
		ECategoryPriority::Important);
	TransformCategory.SetSortOrder(PinnedSortOrderBase + 2);

	TransformCategory.AddCustomRow(LOCTEXT("BoxExtentFilter", "Box Extent"))
	.IsEnabled(TAttribute<bool>(this, &FArcEcotopeVolumeDetails::CanStartOperation))
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("BoxExtentLabel", "Box Extent"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(125.0f * 3.0f)
	.MaxDesiredWidth(125.0f * 3.0f)
	[
		SNew(SNumericVectorInputBox<FVector::FReal>)
		.X(this, &FArcEcotopeVolumeDetails::GetBoxExtentX)
		.Y(this, &FArcEcotopeVolumeDetails::GetBoxExtentY)
		.Z(this, &FArcEcotopeVolumeDetails::GetBoxExtentZ)
		.AllowSpin(false)
		.OnXCommitted(this, &FArcEcotopeVolumeDetails::OnBoxExtentXCommitted)
		.OnYCommitted(this, &FArcEcotopeVolumeDetails::OnBoxExtentYCommitted)
		.OnZCommitted(this, &FArcEcotopeVolumeDetails::OnBoxExtentZCommitted)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	];

	TArray<FLaidOutProperty> LaidOutProperties;
	GatherLaidOutProperties(*AArcEcotopeVolume::StaticClass(), NAME_None, LaidOutProperties);

	TArray<FName> AllowedTabs;
	for (const FLaidOutProperty& Entry : LaidOutProperties)
	{
		if (Entry.Category.IsNone() || !IsArcEcotopeOwned(*Entry.Property))
		{
			continue;
		}
		const FName TopLevelCategory = GetTopLevelCategory(Entry.Category);
		if (!IsPinnedCategory(TopLevelCategory))
		{
			AllowedTabs.AddUnique(TopLevelCategory);
		}
	}

	UArcEcotopeTerrainWeightStamp* TerrainWeightStamp = SelectedVolume->TerrainWeightStamp;
	if (TerrainWeightStamp != nullptr)
	{
		AllowedTabs.AddUnique(TerrainCategoryName);
	}

	FName SelectedTab = GetSelectedTab();
	if (!SelectedTab.IsNone() && !AllowedTabs.Contains(SelectedTab))
	{
		SelectedTab = NAME_None;
		if (FEditorStatus* Status = FindOrAddStatus(*SelectedVolume.Get()))
		{
			Status->SelectedCategory = NAME_None;
		}
	}

	BuildCategoryTabs(DetailBuilder, AllowedTabs);

	// The stamp rows exist before the hide loop so the category takes part in tab hiding like every other tab.
	if (TerrainWeightStamp != nullptr)
	{
		AddTerrainStampRows(DetailBuilder, *TerrainWeightStamp);
	}

	TArray<FName> CategoryNames;
	DetailBuilder.GetCategoryNames(CategoryNames);
	for (const FName CategoryName : CategoryNames)
	{
		if (IsPinnedCategory(CategoryName))
		{
			continue;
		}
		const FName TopLevelCategory = GetTopLevelCategory(CategoryName);
		if (!AllowedTabs.Contains(TopLevelCategory)
			|| (!SelectedTab.IsNone() && TopLevelCategory != SelectedTab))
		{
			DetailBuilder.HideCategory(CategoryName);
		}
	}

	for (const FLaidOutProperty& Entry : LaidOutProperties)
	{
		if (Entry.Category.IsNone() || IsArcEcotopeOwned(*Entry.Property))
		{
			continue;
		}
		if (AllowedTabs.Contains(GetTopLevelCategory(Entry.Category)))
		{
			DetailBuilder.HideProperty(Entry.Property->GetFName(), Entry.Property->GetOwnerStruct(), Entry.InstanceName);
		}
	}
}

void FArcEcotopeVolumeDetails::BuildActionsCategory(IDetailLayoutBuilder& DetailBuilder)
{
	using namespace ArcEcotopeVolumeDetailsPrivate;

	IDetailCategoryBuilder& ActionsCategory = DetailBuilder.EditCategory(
		ActionsCategoryName,
		LOCTEXT("ActionsCategory", "ArcEcotope Actions"),
		ECategoryPriority::Important);
	ActionsCategory.SetSortOrder(PinnedSortOrderBase);
	ActionsCategory.AddCustomRow(LOCTEXT("BakeSiteFilter", "Bake Site"))
	.WholeRowContent()[MakeButton(
		LOCTEXT("BakeSite", "Bake Site"),
		LOCTEXT("BakeSiteTip", "Trace the standard box volume and replace the baked site; this clears any existing forest state."),
		FOnClicked::CreateSP(this, &FArcEcotopeVolumeDetails::BakeSite),
		TAttribute<bool>::CreateSP(this, &FArcEcotopeVolumeDetails::CanMutateState))];
	ActionsCategory.AddCustomRow(LOCTEXT("ValidateFilter", "Validate"))
	.WholeRowContent()[MakeButton(
		LOCTEXT("Validate", "Validate"),
		LOCTEXT("ValidateTip", "Validate this volume, its site, state, species settings, and saved native instances."),
		FOnClicked::CreateSP(this, &FArcEcotopeVolumeDetails::Validate))];
	ActionsCategory.AddCustomRow(LOCTEXT("BootstrapFilter", "Bootstrap Cohorts & Spin Up"))
	.WholeRowContent()[MakeButton(
		LOCTEXT("Bootstrap", "Bootstrap Cohorts & Spin Up"),
		LOCTEXT("BootstrapTip", "Initialize configured 30 cm cohorts, then simulate SpinUpYears."),
		FOnClicked::CreateSP(this, &FArcEcotopeVolumeDetails::StartBootstrapAndSpinUp),
		TAttribute<bool>::CreateSP(this, &FArcEcotopeVolumeDetails::CanStartOperation))];
	ActionsCategory.AddCustomRow(LOCTEXT("BareGroundFilter", "Bare Ground & Spin Up"))
	.WholeRowContent()[MakeButton(
		LOCTEXT("BareGround", "Bare Ground & Spin Up"),
		LOCTEXT("BareGroundTip", "Initialize an empty site, then simulate SpinUpYears."),
		FOnClicked::CreateSP(this, &FArcEcotopeVolumeDetails::StartBareGroundAndSpinUp),
		TAttribute<bool>::CreateSP(this, &FArcEcotopeVolumeDetails::CanStartOperation))];
	ActionsCategory.AddCustomRow(LOCTEXT("SimulateFilter", "Simulate Configured Years"))
	.WholeRowContent()[MakeButton(
		LOCTEXT("Simulate", "Simulate YearsPerRun"),
		LOCTEXT("SimulateTip", "Continue the compatible saved snapshot for YearsPerRun."),
		FOnClicked::CreateSP(this, &FArcEcotopeVolumeDetails::SimulateConfiguredYears),
		TAttribute<bool>::CreateSP(this, &FArcEcotopeVolumeDetails::CanStartOperation))];
	ActionsCategory.AddCustomRow(LOCTEXT("CancelFilter", "Cancel"))
	.WholeRowContent()[MakeButton(
		LOCTEXT("Cancel", "Cancel"),
		LOCTEXT("CancelTip", "Request cancellation of the active simulation. The existing saved state remains unchanged."),
		FOnClicked::CreateSP(this, &FArcEcotopeVolumeDetails::Cancel),
		TAttribute<bool>::CreateLambda([this]()
		{
			const AArcEcotopeVolume* Volume = GetVolume();
			return Volume != nullptr && FArcEcotopeEditorModule::IsAvailable()
				&& FArcEcotopeEditorModule::Get().GetSimulationController().HasSessionFor(*Volume);
		}))];
	ActionsCategory.AddCustomRow(LOCTEXT("ClearStateFilter", "Clear State"))
	.WholeRowContent()[MakeButton(
		LOCTEXT("ClearState", "Clear State"),
		LOCTEXT("ClearStateTip", "Remove the saved forest snapshot and its generated native instances."),
		FOnClicked::CreateSP(this, &FArcEcotopeVolumeDetails::ClearState),
		TAttribute<bool>::CreateSP(this, &FArcEcotopeVolumeDetails::CanMutateState))];
	ActionsCategory.AddCustomRow(LOCTEXT("RebuildFilter", "Rebuild Instances"))
	.WholeRowContent()[MakeButton(
		LOCTEXT("RebuildInstances", "Rebuild Instances"),
		LOCTEXT("RebuildTip", "Recreate standard ISM/ISKM components from the current authoritative snapshot."),
		FOnClicked::CreateSP(this, &FArcEcotopeVolumeDetails::RebuildInstances),
		TAttribute<bool>::CreateSP(this, &FArcEcotopeVolumeDetails::CanMutateState))];
}

void FArcEcotopeVolumeDetails::BuildStatusCategory(IDetailLayoutBuilder& DetailBuilder)
{
	using namespace ArcEcotopeVolumeDetailsPrivate;

	IDetailCategoryBuilder& StatusCategory = DetailBuilder.EditCategory(
		StatusCategoryName,
		LOCTEXT("StatusCategory", "ArcEcotope Status"),
		ECategoryPriority::Important);
	StatusCategory.SetSortOrder(PinnedSortOrderBase + 1);
	StatusCategory.AddCustomRow(LOCTEXT("CurrentYearFilter", "Current Year"))
	.NameContent()[SNew(STextBlock).Text(LOCTEXT("CurrentYearLabel", "Current year"))]
	.ValueContent()[SNew(STextBlock).Text(this, &FArcEcotopeVolumeDetails::GetCurrentYearText)];
	StatusCategory.AddCustomRow(LOCTEXT("ProgressFilter", "Progress"))
	.NameContent()[SNew(STextBlock).Text(LOCTEXT("ProgressLabel", "Progress"))]
	.ValueContent()[SNew(STextBlock).Text(this, &FArcEcotopeVolumeDetails::GetProgressText)];
	StatusCategory.AddCustomRow(LOCTEXT("LastErrorFilter", "Last Error"))
	.NameContent()[SNew(STextBlock).Text(LOCTEXT("LastErrorLabel", "Last error"))]
	.ValueContent()[SNew(STextBlock).AutoWrapText(true).Text(this, &FArcEcotopeVolumeDetails::GetLastErrorText)];
}

void FArcEcotopeVolumeDetails::BuildCategoryTabs(IDetailLayoutBuilder& DetailBuilder, const TArray<FName>& TabNames)
{
	using namespace ArcEcotopeVolumeDetailsPrivate;

	IDetailCategoryBuilder& TabsCategory = DetailBuilder.EditCategory(
		TabsCategoryName,
		LOCTEXT("TabsCategory", "ArcEcotope Categories"),
		ECategoryPriority::Important);
	TabsCategory.SetSortOrder(PinnedSortOrderBase + 3);

	TSharedRef<SWrapBox> TabStrip = SNew(SWrapBox)
		.UseAllottedSize(true)
		.InnerSlotPadding(FVector2D(2.0f, 2.0f));

	TabStrip->AddSlot()[MakeCategoryTab(NAME_None, LOCTEXT("AllTab", "All"))];
	for (const FName TabName : TabNames)
	{
		TabStrip->AddSlot()[MakeCategoryTab(TabName, FText::FromName(TabName))];
	}

	TabsCategory.AddCustomRow(LOCTEXT("TabStripFilter", "Categories"))
	.WholeRowContent()[TabStrip];
}

TSharedRef<SWidget> FArcEcotopeVolumeDetails::MakeCategoryTab(const FName TabName, const FText& Label)
{
	return SNew(SCheckBox)
		.Style(FAppStyle::Get(), "DetailsView.SectionButton")
		.IsChecked(this, &FArcEcotopeVolumeDetails::IsTabSelected, TabName)
		.OnCheckStateChanged(this, &FArcEcotopeVolumeDetails::OnTabSelected, TabName)
		[
			SNew(STextBlock)
			.TextStyle(FAppStyle::Get(), "SmallText")
			.Text(Label)
		];
}

FName FArcEcotopeVolumeDetails::GetSelectedTab() const
{
	const ArcEcotopeVolumeDetailsPrivate::FEditorStatus* Status =
		ArcEcotopeVolumeDetailsPrivate::FindStatus(GetVolume());
	return Status != nullptr ? Status->SelectedCategory : NAME_None;
}

ECheckBoxState FArcEcotopeVolumeDetails::IsTabSelected(const FName TabName) const
{
	return GetSelectedTab() == TabName ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FArcEcotopeVolumeDetails::OnTabSelected(const ECheckBoxState NewState, const FName TabName)
{
	const AArcEcotopeVolume* Volume = GetVolume();
	if (NewState != ECheckBoxState::Checked || Volume == nullptr || !Volume->VolumeId.IsValid())
	{
		return;
	}

	if (ArcEcotopeVolumeDetailsPrivate::FEditorStatus* Status =
		ArcEcotopeVolumeDetailsPrivate::FindOrAddStatus(*Volume))
	{
		Status->SelectedCategory = TabName;
	}

	const TSharedPtr<IPropertyUtilities> Utilities = PropertyUtilities;
	if (Utilities.IsValid())
	{
		Utilities->ForceRefresh();
	}
}

AArcEcotopeVolume* FArcEcotopeVolumeDetails::GetVolume() const
{
	return SelectedVolume.Get();
}

bool FArcEcotopeVolumeDetails::CanStartOperation() const
{
	const AArcEcotopeVolume* Volume = GetVolume();
	return Volume != nullptr && FArcEcotopeEditorModule::IsAvailable()
		&& !FArcEcotopeEditorModule::Get().GetSimulationController().HasActiveSession();
}

bool FArcEcotopeVolumeDetails::CanMutateState() const
{
	return CanStartOperation();
}

FReply FArcEcotopeVolumeDetails::BakeSite()
{
	AArcEcotopeVolume* Volume = GetVolume();
	UE_LOGF(
		LogArcEcotope,
		Log,"ArcEcotope editor action: BakeSite invoked for '%ls'.",
		Volume != nullptr ? *Volume->GetName() : TEXT("<none>"));
	if (Volume == nullptr || !CanMutateState())
	{
		UE_LOGF(
			LogArcEcotope,
			Error,"ArcEcotope editor action: BakeSite refused: %ls.",
			*ArcEcotopeVolumeDetailsPrivate::DescribeCanStartOperationRefusal(Volume));
		return FReply::Handled();
	}

	FArcEcotopeSiteBakeResult BakedSite;
	FText Error;
	if (!FArcEcotopeSiteBaker::Bake(*Volume, BakedSite, Error))
	{
		SetLastError(Error);
		return FReply::Handled();
	}

	uint64 NewSiteBytes = 0;
	for (const FSharedBuffer& Payload : BakedSite.ChunkPayloads)
	{
		NewSiteBytes = Payload.GetSize() > MAX_uint64 - NewSiteBytes
			? MAX_uint64
			: NewSiteBytes + Payload.GetSize();
	}
	const uint64 ExistingStateBytes = Volume->SimulationState != nullptr
		? ArcEcotopeVolumeDetailsPrivate::GetWorldSnapshotAllocatedBytes(Volume->SimulationState->Snapshot)
		: 0;
	const uint64 EstimatedUndoBytes = NewSiteBytes > MAX_uint64 - ExistingStateBytes
		? MAX_uint64
		: NewSiteBytes + ExistingStateBytes;
	TUniquePtr<FScopedTransaction> Transaction;
	if (EstimatedUndoBytes <= ArcEcotopeVolumeDetailsPrivate::MaximumTransactionalSiteBakeBytes)
	{
		Transaction = MakeUnique<FScopedTransaction>(LOCTEXT("BakeSiteTransaction", "Bake Ecotope Site"));
	}
	else
	{
		UE_LOGF(
			LogArcEcotope,
			Warning,"ArcEcotope Bake Site is non-undoable: estimated derived data %.2f MiB exceeds the %.2f MiB transaction limit.",
			static_cast<double>(EstimatedUndoBytes) / (1024.0 * 1024.0),
			static_cast<double>(ArcEcotopeVolumeDetailsPrivate::MaximumTransactionalSiteBakeBytes)
				/ (1024.0 * 1024.0));
	}
	Volume->Modify();
	if (!Volume->ReplaceSiteWithValidatedChunks(
			MoveTemp(BakedSite.Metadata),
			MoveTemp(BakedSite.ChunkManifest),
			MoveTemp(BakedSite.ChunkPayloads),
			Error))
	{
		SetLastError(Error);
		return FReply::Handled();
	}
	if (Volume->SimulationState != nullptr)
	{
		Volume->SimulationState->Modify();
		Volume->SimulationState->ResetToEmpty();
	}
	if (!FArcEcotopeInstanceBuilder::ClearEmittedActors(*Volume, /*bRequiresRebuild=*/false, Error))
	{
		SetLastError(Error);
		return FReply::Handled();
	}
	ClearLastError();
	return FReply::Handled();
}

FReply FArcEcotopeVolumeDetails::Validate()
{
	AArcEcotopeVolume* Volume = GetVolume();
	UE_LOGF(
		LogArcEcotope,
		Log,"ArcEcotope editor action: Validate invoked for '%ls'.",
		Volume != nullptr ? *Volume->GetName() : TEXT("<none>"));
	if (Volume == nullptr)
	{
		UE_LOGF(LogArcEcotope, Error,"ArcEcotope editor action: Validate refused: Volume is null.");
		return FReply::Handled();
	}

	FDataValidationContext Context;
	const EDataValidationResult Result = Volume->IsDataValid(Context);
	if (Result == EDataValidationResult::Invalid)
	{
		FString Errors;
		for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
		{
			if (Issue.Severity == EMessageSeverity::Error)
			{
				if (!Errors.IsEmpty()) Errors += TEXT("\n");
				Errors += Issue.Message.ToString();
			}
		}
		SetLastError(FText::FromString(Errors.IsEmpty() ? TEXT("ArcEcotope validation failed.") : Errors));
	}
	else
	{
		ClearLastError();
	}
	return FReply::Handled();
}

FReply FArcEcotopeVolumeDetails::StartBootstrapAndSpinUp()
{
	AArcEcotopeVolume* Volume = GetVolume();
	UE_LOGF(
		LogArcEcotope,
		Log,"ArcEcotope editor action: StartBootstrapAndSpinUp invoked for '%ls' (SpinUpYears=%d).",
		Volume != nullptr ? *Volume->GetName() : TEXT("<none>"),
		Volume != nullptr ? Volume->SpinUpYears : -1);
	if (Volume == nullptr || !CanStartOperation())
	{
		UE_LOGF(
			LogArcEcotope,
			Error,"ArcEcotope editor action: StartBootstrapAndSpinUp refused: %ls.",
			*ArcEcotopeVolumeDetailsPrivate::DescribeCanStartOperationRefusal(Volume));
		return FReply::Handled();
	}

	FArcForestStartRequest Request;
	FText Error;
	if (!FArcForestStartRequest::MakeBootstrapAndSpinUp(
			EArcForestStartMode::BootstrapCohorts,
			Volume->SpinUpYears,
			Request,
			Error)
		|| !FArcEcotopeEditorModule::Get().GetSimulationController().Start(*Volume, Request, Error))
	{
		SetLastError(Error);
		return FReply::Handled();
	}
	ClearLastError();
	return FReply::Handled();
}

FReply FArcEcotopeVolumeDetails::StartBareGroundAndSpinUp()
{
	AArcEcotopeVolume* Volume = GetVolume();
	UE_LOGF(
		LogArcEcotope,
		Log,"ArcEcotope editor action: StartBareGroundAndSpinUp invoked for '%ls' (SpinUpYears=%d).",
		Volume != nullptr ? *Volume->GetName() : TEXT("<none>"),
		Volume != nullptr ? Volume->SpinUpYears : -1);
	if (Volume == nullptr || !CanStartOperation())
	{
		UE_LOGF(
			LogArcEcotope,
			Error,"ArcEcotope editor action: StartBareGroundAndSpinUp refused: %ls.",
			*ArcEcotopeVolumeDetailsPrivate::DescribeCanStartOperationRefusal(Volume));
		return FReply::Handled();
	}

	FArcForestStartRequest Request;
	FText Error;
	if (!FArcForestStartRequest::MakeBootstrapAndSpinUp(
			EArcForestStartMode::BareGround,
			Volume->SpinUpYears,
			Request,
			Error)
		|| !FArcEcotopeEditorModule::Get().GetSimulationController().Start(*Volume, Request, Error))
	{
		SetLastError(Error);
		return FReply::Handled();
	}
	ClearLastError();
	return FReply::Handled();
}

FReply FArcEcotopeVolumeDetails::SimulateConfiguredYears()
{
	AArcEcotopeVolume* Volume = GetVolume();
	UE_LOGF(
		LogArcEcotope,
		Log,"ArcEcotope editor action: SimulateConfiguredYears invoked for '%ls' (YearsPerRun=%d).",
		Volume != nullptr ? *Volume->GetName() : TEXT("<none>"),
		Volume != nullptr ? Volume->YearsPerRun : -1);
	if (Volume == nullptr || !CanStartOperation())
	{
		UE_LOGF(
			LogArcEcotope,
			Error,"ArcEcotope editor action: SimulateConfiguredYears refused: %ls.",
			*ArcEcotopeVolumeDetailsPrivate::DescribeCanStartOperationRefusal(Volume));
		return FReply::Handled();
	}

	FArcForestStartRequest Request;
	FText Error;
	if (!FArcForestStartRequest::MakeContinue(Volume->YearsPerRun, Request, Error)
		|| !FArcEcotopeEditorModule::Get().GetSimulationController().Start(*Volume, Request, Error))
	{
		SetLastError(Error);
		return FReply::Handled();
	}
	ClearLastError();
	return FReply::Handled();
}

FReply FArcEcotopeVolumeDetails::Cancel()
{
	AArcEcotopeVolume* Volume = GetVolume();
	UE_LOGF(
		LogArcEcotope,
		Log,"ArcEcotope editor action: Cancel invoked for '%ls'.",
		Volume != nullptr ? *Volume->GetName() : TEXT("<none>"));
	if (Volume != nullptr && FArcEcotopeEditorModule::IsAvailable())
	{
		FArcEcotopeEditorModule::Get().GetSimulationController().Cancel(*Volume);
	}
	else
	{
		UE_LOGF(
			LogArcEcotope,
			Error,"ArcEcotope editor action: Cancel refused: %ls.",
			Volume == nullptr ? TEXT("Volume is null") : TEXT("FArcEcotopeEditorModule is not available"));
	}
	return FReply::Handled();
}

FReply FArcEcotopeVolumeDetails::ClearState()
{
	AArcEcotopeVolume* Volume = GetVolume();
	UE_LOGF(
		LogArcEcotope,
		Log,"ArcEcotope editor action: ClearState invoked for '%ls'.",
		Volume != nullptr ? *Volume->GetName() : TEXT("<none>"));
	if (Volume == nullptr || !CanMutateState())
	{
		UE_LOGF(
			LogArcEcotope,
			Error,"ArcEcotope editor action: ClearState refused: %ls.",
			*ArcEcotopeVolumeDetailsPrivate::DescribeCanStartOperationRefusal(Volume));
		return FReply::Handled();
	}

	const FScopedTransaction Transaction(LOCTEXT("ClearStateTransaction", "Clear Ecotope State"));
	Volume->Modify();
	if (Volume->SimulationState != nullptr)
	{
		Volume->SimulationState->Modify();
		Volume->SimulationState->ResetToEmpty();
	}
	FText Error;
	if (!FArcEcotopeInstanceBuilder::ClearEmittedActors(*Volume, /*bRequiresRebuild=*/false, Error))
	{
		SetLastError(Error);
		return FReply::Handled();
	}
	ClearLastError();
	return FReply::Handled();
}

FReply FArcEcotopeVolumeDetails::RebuildInstances()
{
	AArcEcotopeVolume* Volume = GetVolume();
	UE_LOGF(
		LogArcEcotope,
		Log,"ArcEcotope editor action: RebuildInstances invoked for '%ls'.",
		Volume != nullptr ? *Volume->GetName() : TEXT("<none>"));
	if (Volume == nullptr || !CanMutateState())
	{
		UE_LOGF(
			LogArcEcotope,
			Error,"ArcEcotope editor action: RebuildInstances refused: %ls.",
			*ArcEcotopeVolumeDetailsPrivate::DescribeCanStartOperationRefusal(Volume));
		return FReply::Handled();
	}

	FText Error;
	if (Volume->SimulationState == nullptr)
	{
		SetLastError(LOCTEXT("RebuildMissingState", "Ecotope instance rebuild requires a simulation state."));
		return FReply::Handled();
	}

	FArcEcotopePreparedInstances Prepared;
	if (!FArcEcotopeInstanceBuilder::Build(*Volume, Volume->SimulationState->Snapshot, Prepared, Error))
	{
		FArcEcotopeInstanceBuilder::CommitFailedBakeRecords(*Volume, Prepared);
		SetLastError(Error);
		return FReply::Handled();
	}

	const FScopedTransaction Transaction(LOCTEXT("RebuildInstancesTransaction", "Rebuild Ecotope Instances"));
	Volume->Modify();
	FArcEcotopeInstanceBuilder::CommitEmittedActorChanges(*Volume, Prepared);
	if (!FArcEcotopeInstanceBuilder::AdoptPreparedInstances(
			*Volume, Volume->SimulationState->Snapshot, Prepared, Error))
	{
		SetLastError(Error);
		return FReply::Handled();
	}
	ClearLastError();
	return FReply::Handled();
}

FText FArcEcotopeVolumeDetails::GetCurrentYearText() const
{
	const AArcEcotopeVolume* Volume = GetVolume();
	return Volume != nullptr && Volume->SimulationState != nullptr
		? FText::AsNumber(Volume->SimulationState->Snapshot.CurrentYear)
		: LOCTEXT("NotAvailable", "N/A");
}

FText FArcEcotopeVolumeDetails::GetProgressText() const
{
	const AArcEcotopeVolume* Volume = GetVolume();
	if (Volume == nullptr || !FArcEcotopeEditorModule::IsAvailable())
	{
		return LOCTEXT("IdleProgress", "Idle");
	}
	const TOptional<FArcForestSimulationSessionSnapshot> Session =
		FArcEcotopeEditorModule::Get().GetSimulationController().GetSessionSnapshot(*Volume);
	if (!Session.IsSet())
	{
		return LOCTEXT("IdleProgress", "Idle");
	}
	return FText::FromString(FString::Printf(
		TEXT("%s — %s (%d/%d years)"),
		ArcEcotopeVolumeDetailsPrivate::ToString(Session->Status),
		ArcEcotopeVolumeDetailsPrivate::ToString(Session->Phase),
		Session->CompletedYears,
		Session->TotalYears));
}

FText FArcEcotopeVolumeDetails::GetLastErrorText() const
{
	const AArcEcotopeVolume* Volume = GetVolume();
	if (Volume == nullptr)
	{
		return FText::GetEmpty();
	}
	if (FArcEcotopeEditorModule::IsAvailable())
	{
		const TOptional<FArcForestSimulationSessionSnapshot> Session =
			FArcEcotopeEditorModule::Get().GetSimulationController().GetSessionSnapshot(*Volume);
		if (Session.IsSet() && !Session->Error.IsEmpty())
		{
			return FText::FromString(Session->Error);
		}
	}
	if (const ArcEcotopeVolumeDetailsPrivate::FEditorStatus* Status =
		ArcEcotopeVolumeDetailsPrivate::FindStatus(Volume))
	{
		return Status->LastError;
	}
	return FText::GetEmpty();
}

void FArcEcotopeVolumeDetails::SetLastError(const FText& Error) const
{
	if (const AArcEcotopeVolume* Volume = GetVolume())
	{
		if (ArcEcotopeVolumeDetailsPrivate::FEditorStatus* Status =
			ArcEcotopeVolumeDetailsPrivate::FindOrAddStatus(*Volume))
		{
			Status->LastError = Error;
		}
		UE_LOGF(
			LogArcEcotope,
			Error,"ArcEcotope editor action: '%ls' failed: %ls",
			*Volume->GetName(),
			*Error.ToString());
	}
	else
	{
		UE_LOGF(
			LogArcEcotope,
			Error,"ArcEcotope editor action: failed with no selected volume: %ls",
			*Error.ToString());
	}
}

TOptional<FVector::FReal> FArcEcotopeVolumeDetails::GetBoxExtentAxis(const int32 AxisIndex) const
{
	const AArcEcotopeVolume* Volume = GetVolume();
	if (Volume == nullptr || Volume->VolumeBounds == nullptr)
	{
		return TOptional<FVector::FReal>();
	}
	return Volume->VolumeBounds->GetUnscaledBoxExtent()[AxisIndex];
}

TOptional<FVector::FReal> FArcEcotopeVolumeDetails::GetBoxExtentX() const
{
	return GetBoxExtentAxis(0);
}

TOptional<FVector::FReal> FArcEcotopeVolumeDetails::GetBoxExtentY() const
{
	return GetBoxExtentAxis(1);
}

TOptional<FVector::FReal> FArcEcotopeVolumeDetails::GetBoxExtentZ() const
{
	return GetBoxExtentAxis(2);
}

void FArcEcotopeVolumeDetails::CommitBoxExtentAxis(const int32 AxisIndex, FVector::FReal NewValue)
{
	if (!SelectedVolume.IsValid() || SelectedVolume->VolumeBounds == nullptr || !FMath::IsFinite(NewValue)
		|| SelectedVolume->IsTemplate() || !CanStartOperation())
	{
		return;
	}
	// IsDataValid rejects a non-positive extent, so the floor is above zero rather than at it.
	NewValue = FMath::Max(NewValue, static_cast<FVector::FReal>(UE_KINDA_SMALL_NUMBER));

	UBoxComponent* BoxComponent = SelectedVolume->VolumeBounds;
	FVector NewExtent = BoxComponent->GetUnscaledBoxExtent();
	NewExtent[AxisIndex] = NewValue;

	const FScopedTransaction Transaction(LOCTEXT("SetBoxExtentTransaction", "Set Ecotope Volume Box Extent"));
	BoxComponent->Modify();
	BoxComponent->SetBoxExtent(NewExtent, /*bUpdateOverlaps=*/false);
	SelectedVolume->MarkPackageDirty();
}

void FArcEcotopeVolumeDetails::OnBoxExtentXCommitted(const FVector::FReal NewValue, ETextCommit::Type CommitType)
{
	CommitBoxExtentAxis(0, NewValue);
}

void FArcEcotopeVolumeDetails::OnBoxExtentYCommitted(const FVector::FReal NewValue, ETextCommit::Type CommitType)
{
	CommitBoxExtentAxis(1, NewValue);
}

void FArcEcotopeVolumeDetails::OnBoxExtentZCommitted(const FVector::FReal NewValue, ETextCommit::Type CommitType)
{
	CommitBoxExtentAxis(2, NewValue);
}

void FArcEcotopeVolumeDetails::ClearLastError() const
{
	if (const AArcEcotopeVolume* Volume = GetVolume())
	{
		if (ArcEcotopeVolumeDetailsPrivate::FEditorStatus* Status =
			ArcEcotopeVolumeDetailsPrivate::FindOrAddStatus(*Volume))
		{
			Status->LastError = FText::GetEmpty();
		}
		UE_LOGF(LogArcEcotope, Verbose,"ArcEcotope editor action: '%ls' succeeded.", *Volume->GetName());
	}
}

#undef LOCTEXT_NAMESPACE
