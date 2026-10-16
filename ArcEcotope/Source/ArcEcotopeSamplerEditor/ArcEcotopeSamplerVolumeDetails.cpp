// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEcotopeSamplerVolumeDetails.h"

#include "ArcEcotopeModule.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Engine/World.h"
#include "Foliage/ArcEcotopeFoliageFields.h"
#include "IDetailGroup.h"
#include "Misc/ScopedSlowTask.h"
#include "PropertyHandle.h"
#include "Sampler/ArcForestSampler.h"
#include "Sampler/ArcForestSamplerInput.h"
#include "ScopedTransaction.h"
#include "Simulation/ArcEcotopeInstanceBuilder.h"
#include "Simulation/ArcEcotopeSamplerVolume.h"
#include "Simulation/ArcEcotopeSiteBaker.h"
#include "Simulation/ArcForestGrowthEngine.h"
#include "Simulation/ArcForestSimulationState.h"
#include "Simulation/ArcForestWorldSnapshot.h"
#include "UObject/Package.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ArcEcotopeSamplerVolumeDetails"

namespace ArcEcotopeSamplerVolumeDetailsPrivate
{
	constexpr uint64 MaximumTransactionalSiteBakeBytes = 64ull * 1024ull * 1024ull;

	constexpr int32 PinnedSortOrderBase = static_cast<int32>(ECategoryPriority::Important) * 1000;

	static const FName ActionsCategoryName(TEXT("ArcEcotope Actions"));
	static const FName SamplerCategoryName(TEXT("Sampler"));
	static const FName SamplerGroupName(TEXT("ArcEcotopeForestSampler"));

	TMap<FGuid, FText> LastErrorByVolumeId;

	uint64 GetWorldSnapshotAllocatedBytes(const FArcForestWorldSnapshot& Snapshot)
	{
		return Snapshot.SpeciesEcologyHashes.GetAllocatedSize() + Snapshot.Individuals.GetAllocatedSize()
			+ Snapshot.Cohorts.GetAllocatedSize() + Snapshot.Patches.GetAllocatedSize()
			+ Snapshot.Deadwood.GetAllocatedSize();
	}

	TSharedRef<SWidget> MakeButton(const FText& Label, const FText& ToolTip, const FOnClicked& OnClicked)
	{
		return SNew(SBox)
			.MinDesiredWidth(150.0f)
			[
				SNew(SButton)
				.Text(Label)
				.ToolTipText(ToolTip)
				.OnClicked(OnClicked)
			];
	}
}

TSharedRef<IDetailCustomization> FArcEcotopeSamplerVolumeDetails::MakeInstance()
{
	return MakeShared<FArcEcotopeSamplerVolumeDetails>();
}

void FArcEcotopeSamplerVolumeDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	using namespace ArcEcotopeSamplerVolumeDetailsPrivate;

	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() != 1)
	{
		return;
	}

	SelectedVolume = Cast<AArcEcotopeSamplerVolume>(Objects[0].Get());
	if (!SelectedVolume.IsValid())
	{
		return;
	}

	IDetailCategoryBuilder& ActionsCategory = DetailBuilder.EditCategory(
		ActionsCategoryName,
		LOCTEXT("ActionsCategory", "ArcEcotope Actions"),
		ECategoryPriority::Important);
	ActionsCategory.SetSortOrder(PinnedSortOrderBase);

	IDetailGroup& SamplerGroup = ActionsCategory.AddGroup(
		SamplerGroupName,
		LOCTEXT("SamplerGroup", "Forest Sampler"),
		/*bForAdvanced=*/false,
		/*bStartExpanded=*/true);
	SamplerGroup.SetDisplayMode(EDetailGroupDisplayMode::Category);

	SamplerGroup.AddWidgetRow()
	.FilterString(LOCTEXT("SamplerBakeSiteFilter", "Sampler Bake Site"))
	.WholeRowContent()[MakeButton(
		LOCTEXT("SamplerBakeSite", "Bake Site (Sampler)"),
		LOCTEXT("SamplerBakeSiteTip", "Trace the standard box volume and replace the baked site; this clears any existing forest state."),
		FOnClicked::CreateSP(this, &FArcEcotopeSamplerVolumeDetails::BakeSite))];
	SamplerGroup.AddWidgetRow()
	.FilterString(LOCTEXT("SampleForestFilter", "Sample Forest"))
	.WholeRowContent()[MakeButton(
		LOCTEXT("SampleForest", "Sample Forest"),
		LOCTEXT("SampleForestTip", "Draw a whole stand in one probabilistic pass and emit it, replacing the saved snapshot."),
		FOnClicked::CreateSP(this, &FArcEcotopeSamplerVolumeDetails::SampleForest))];
	SamplerGroup.AddWidgetRow()
	.FilterString(LOCTEXT("SamplerRebuildFilter", "Sampler Rebuild Instances"))
	.WholeRowContent()[MakeButton(
		LOCTEXT("SamplerRebuildInstances", "Rebuild Instances (Sampler)"),
		LOCTEXT("SamplerRebuildTip", "Recreate standard ISM/ISKM components from the current authoritative snapshot."),
		FOnClicked::CreateSP(this, &FArcEcotopeSamplerVolumeDetails::RebuildInstances))];
	SamplerGroup.AddWidgetRow()
	.FilterString(LOCTEXT("SamplerClearStateFilter", "Sampler Clear State"))
	.WholeRowContent()[MakeButton(
		LOCTEXT("SamplerClearState", "Clear State (Sampler)"),
		LOCTEXT("SamplerClearStateTip", "Remove the saved forest snapshot and its generated native instances."),
		FOnClicked::CreateSP(this, &FArcEcotopeSamplerVolumeDetails::ClearState))];

	SamplerGroup.AddWidgetRow()
	.FilterString(LOCTEXT("SamplerLastErrorFilter", "Sampler Last Error"))
	.NameContent()[SNew(STextBlock).Text(LOCTEXT("SamplerLastErrorLabel", "Sampler last error"))]
	.ValueContent()[SNew(STextBlock).AutoWrapText(true).Text(this, &FArcEcotopeSamplerVolumeDetails::GetLastErrorText)];

	// The base volume layout also runs here and hides every category it did not gather from
	// AArcEcotopeVolume, so the sampler knobs only survive inside a category it keeps pinned.
	DetailBuilder.HideCategory(SamplerCategoryName);
	const TSharedRef<IPropertyHandle> SamplerSettingsHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(AArcEcotopeSamplerVolume, SamplerSettings),
		AArcEcotopeSamplerVolume::StaticClass());
	SamplerGroup.AddPropertyRow(SamplerSettingsHandle);
}

AArcEcotopeSamplerVolume* FArcEcotopeSamplerVolumeDetails::GetVolume() const
{
	return SelectedVolume.Get();
}

FReply FArcEcotopeSamplerVolumeDetails::BakeSite()
{
	using namespace ArcEcotopeSamplerVolumeDetailsPrivate;

	AArcEcotopeSamplerVolume* Volume = GetVolume();
	UE_LOGF(
		LogArcEcotope,
		Log,"ArcEcotope sampler action: BakeSite invoked for '%ls'.",
		Volume != nullptr ? *Volume->GetName() : TEXT("<none>"));
	if (Volume == nullptr)
	{
		UE_LOGF(LogArcEcotope, Error,"ArcEcotope sampler action: BakeSite refused: Volume is null.");
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
		? GetWorldSnapshotAllocatedBytes(Volume->SimulationState->Snapshot)
		: 0;
	const uint64 EstimatedUndoBytes = NewSiteBytes > MAX_uint64 - ExistingStateBytes
		? MAX_uint64
		: NewSiteBytes + ExistingStateBytes;
	TUniquePtr<FScopedTransaction> Transaction;
	if (EstimatedUndoBytes <= MaximumTransactionalSiteBakeBytes)
	{
		Transaction = MakeUnique<FScopedTransaction>(LOCTEXT("SamplerBakeSiteTransaction", "Bake Ecotope Site"));
	}
	else
	{
		UE_LOGF(
			LogArcEcotope,
			Warning,"ArcEcotope Bake Site is non-undoable: estimated derived data %.2f MiB exceeds the %.2f MiB transaction limit.",
			static_cast<double>(EstimatedUndoBytes) / (1024.0 * 1024.0),
			static_cast<double>(MaximumTransactionalSiteBakeBytes) / (1024.0 * 1024.0));
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

FReply FArcEcotopeSamplerVolumeDetails::SampleForest()
{
	AArcEcotopeSamplerVolume* Volume = GetVolume();
	UE_LOGF(
		LogArcEcotope,
		Log,"ArcEcotope sampler action: SampleForest invoked for '%ls'.",
		Volume != nullptr ? *Volume->GetName() : TEXT("<none>"));
	if (Volume == nullptr)
	{
		UE_LOGF(LogArcEcotope, Error,"ArcEcotope sampler action: SampleForest refused: Volume is null.");
		return FReply::Handled();
	}

	UArcForestSimulationState* State = Volume->SimulationState;
	if (State == nullptr)
	{
		SetLastError(LOCTEXT("SamplerMissingState", "The forest sampler requires a simulation state."));
		return FReply::Handled();
	}
	const UWorld* World = Volume->GetWorld();
	if (World == nullptr)
	{
		SetLastError(LOCTEXT("SamplerMissingWorld", "The forest sampler requires the volume to be in a world."));
		return FReply::Handled();
	}
	const FName WorldPackageName = World->GetPackage()->GetFName();

	FScopedSlowTask SlowTask(3.0f, LOCTEXT("SamplerSlowTask", "Sampling forest"));
	SlowTask.MakeDialog();

	FText Error;
	FArcForestSamplerInput Input;
	if (!Volume->FreezeSamplerInput(Input, Error))
	{
		SetLastError(Error);
		return FReply::Handled();
	}

	SlowTask.EnterProgressFrame(1.0f, LOCTEXT("SamplerSlowTaskSample", "Placing stems"));
	FArcForestCancellationToken Cancellation;
	FArcForestProgressState Progress;
	FArcForestSamplerResult Result;
	if (!FArcForestSampler::Sample(Input, Cancellation, Progress, Result)
		|| Result.Status != EArcForestRunStatus::Succeeded)
	{
		SetLastError(FText::Format(
			LOCTEXT("SamplerFailed", "The forest sampler failed: {0}"),
			Result.Error.IsEmpty()
				? LOCTEXT("SamplerFailedUnknown", "no reason reported")
				: FText::FromString(Result.Error)));
		return FReply::Handled();
	}

	SlowTask.EnterProgressFrame(1.0f, LOCTEXT("SamplerSlowTaskBuild", "Preparing instances"));
	FArcEcotopePreparedInstances Prepared;
	if (!FArcEcotopeInstanceBuilder::Build(*Volume, Result.Snapshot, Prepared, Error))
	{
		FArcEcotopeInstanceBuilder::CommitFailedBakeRecords(*Volume, Prepared);
		SetLastError(Error);
		return FReply::Handled();
	}

	SlowTask.EnterProgressFrame(1.0f, LOCTEXT("SamplerSlowTaskCommit", "Committing"));
	const FScopedTransaction Transaction(LOCTEXT("SampleForestTransaction", "Sample Ecotope Forest"));
	Volume->Modify();
	State->Modify();
	FArcEcotopeInstanceBuilder::CommitEmittedActorChanges(*Volume, Prepared);
	if (!FArcEcotopeInstanceBuilder::AdoptPreparedInstances(
			*Volume, Result.Snapshot, Prepared, Error))
	{
		SetLastError(Error);
		return FReply::Handled();
	}
	State->ApplyCommittedSnapshot(MoveTemp(Result.Snapshot), WorldPackageName);
	FText FieldError;
	if (!ArcEcotopeFoliageFields::DeriveFieldPayload(*Volume, Volume->FoliageFieldPayload, FieldError))
	{
		Volume->FoliageFieldPayload.Reset();
		UE_LOGF(
			LogArcEcotope,
			Warning,"[FoliageFields] volume='%ls' has no forest field after this sample, so its ground foliage will not draw: %ls",
			*Volume->GetName(),
			*FieldError.ToString());
	}
	ClearLastError();
	return FReply::Handled();
}

FReply FArcEcotopeSamplerVolumeDetails::RebuildInstances()
{
	AArcEcotopeSamplerVolume* Volume = GetVolume();
	UE_LOGF(
		LogArcEcotope,
		Log,"ArcEcotope sampler action: RebuildInstances invoked for '%ls'.",
		Volume != nullptr ? *Volume->GetName() : TEXT("<none>"));
	if (Volume == nullptr)
	{
		UE_LOGF(LogArcEcotope, Error,"ArcEcotope sampler action: RebuildInstances refused: Volume is null.");
		return FReply::Handled();
	}
	if (Volume->SimulationState == nullptr)
	{
		SetLastError(LOCTEXT("SamplerRebuildMissingState", "Ecotope instance rebuild requires a simulation state."));
		return FReply::Handled();
	}

	FText Error;
	FArcEcotopePreparedInstances Prepared;
	if (!FArcEcotopeInstanceBuilder::Build(*Volume, Volume->SimulationState->Snapshot, Prepared, Error))
	{
		FArcEcotopeInstanceBuilder::CommitFailedBakeRecords(*Volume, Prepared);
		SetLastError(Error);
		return FReply::Handled();
	}

	const FScopedTransaction Transaction(LOCTEXT("SamplerRebuildInstancesTransaction", "Rebuild Ecotope Instances"));
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

FReply FArcEcotopeSamplerVolumeDetails::ClearState()
{
	AArcEcotopeSamplerVolume* Volume = GetVolume();
	UE_LOGF(
		LogArcEcotope,
		Log,"ArcEcotope sampler action: ClearState invoked for '%ls'.",
		Volume != nullptr ? *Volume->GetName() : TEXT("<none>"));
	if (Volume == nullptr)
	{
		UE_LOGF(LogArcEcotope, Error,"ArcEcotope sampler action: ClearState refused: Volume is null.");
		return FReply::Handled();
	}

	const FScopedTransaction Transaction(LOCTEXT("SamplerClearStateTransaction", "Clear Ecotope State"));
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

FText FArcEcotopeSamplerVolumeDetails::GetLastErrorText() const
{
	const AArcEcotopeSamplerVolume* Volume = GetVolume();
	if (Volume == nullptr || !Volume->VolumeId.IsValid())
	{
		return FText::GetEmpty();
	}
	const FText* Found = ArcEcotopeSamplerVolumeDetailsPrivate::LastErrorByVolumeId.Find(Volume->VolumeId);
	return Found != nullptr ? *Found : FText::GetEmpty();
}

void FArcEcotopeSamplerVolumeDetails::SetLastError(const FText& Error) const
{
	const AArcEcotopeSamplerVolume* Volume = GetVolume();
	if (Volume == nullptr)
	{
		UE_LOGF(
			LogArcEcotope,
			Error,"ArcEcotope sampler action: failed with no selected volume: %ls",
			*Error.ToString());
		return;
	}
	if (Volume->VolumeId.IsValid())
	{
		ArcEcotopeSamplerVolumeDetailsPrivate::LastErrorByVolumeId.Add(Volume->VolumeId, Error);
	}
	UE_LOGF(
		LogArcEcotope,
		Error,"ArcEcotope sampler action: '%ls' failed: %ls",
		*Volume->GetName(),
		*Error.ToString());
}

void FArcEcotopeSamplerVolumeDetails::ClearLastError() const
{
	const AArcEcotopeSamplerVolume* Volume = GetVolume();
	if (Volume == nullptr)
	{
		return;
	}
	if (Volume->VolumeId.IsValid())
	{
		ArcEcotopeSamplerVolumeDetailsPrivate::LastErrorByVolumeId.Remove(Volume->VolumeId);
	}
	UE_LOGF(LogArcEcotope, Verbose,"ArcEcotope sampler action: '%ls' succeeded.", *Volume->GetName());
}

#undef LOCTEXT_NAMESPACE
