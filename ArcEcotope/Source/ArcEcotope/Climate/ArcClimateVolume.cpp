// Copyright Lukasz Baran. All Rights Reserved.

#include "Climate/ArcClimateVolume.h"

#include "ArcEcotopeModule.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Serialization/ArcEcotopeCustomVersion.h"
#include "UObject/Package.h"

namespace ArcClimateVolumePrivate
{
	bool AccumulatePayloadSize(const int64 PayloadSize, uint64& InOutTotalBytes)
	{
		if (PayloadSize <= 0
			|| static_cast<uint64>(PayloadSize) > FArcClimateProductStorage::MaxPayloadBytes
			|| static_cast<uint64>(PayloadSize) > MAX_uint64 - InOutTotalBytes)
		{
			return false;
		}
		InOutTotalBytes += static_cast<uint64>(PayloadSize);
		return InOutTotalBytes <= FArcClimateProductStorage::MaxTotalPayloadBytes;
	}

#if WITH_EDITOR
	bool CanRepairAuthoredClimate(const AActor& Actor)
	{
		if (!GIsEditor)
		{
			return false;
		}
		const UPackage* Package = Actor.GetPackage();
		return Package == nullptr || !Package->HasAnyPackageFlags(PKG_PlayInEditor);
	}
#endif
}

AArcClimateVolume::AArcClimateVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	VolumeBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("ClimateBounds"));
	SetRootComponent(VolumeBounds);
	VolumeBounds->SetBoxExtent(FVector(5000.0, 5000.0, 1000.0));
	VolumeBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VolumeBounds->SetCanEverAffectNavigation(false);
}

void AArcClimateVolume::ApplyClimateBakeQualityPreset()
{
	TGuardValue<bool> DeriveGuard(bDerivingBakeQuality, true);
	switch (BakeQuality)
	{
	case EArcClimateBakeQuality::Low:
		FieldSettings.SunLoadLevel = 2;
		FieldSettings.HorizonLevel = 5;
		FieldSettings.HorizonRadiusM = 200.0f;
		FieldSettings.HorizonAzimuths = 8;
		FieldSettings.bAutoMinRelief = true;
		FieldSettings.TemperatureLevel = 0;
		FieldSettings.HumidityLevel = 0;
		break;
	case EArcClimateBakeQuality::Medium:
		FieldSettings.SunLoadLevel = 0;
		FieldSettings.HorizonLevel = 3;
		FieldSettings.HorizonRadiusM = 500.0f;
		FieldSettings.HorizonAzimuths = 16;
		FieldSettings.bAutoMinRelief = true;
		FieldSettings.TemperatureLevel = 0;
		FieldSettings.HumidityLevel = 0;
		break;
	case EArcClimateBakeQuality::High:
		FieldSettings.SunLoadLevel = 0;
		FieldSettings.HorizonLevel = 1;
		FieldSettings.HorizonRadiusM = 1500.0f;
		FieldSettings.HorizonAzimuths = 32;
		FieldSettings.bAutoMinRelief = true;
		FieldSettings.TemperatureLevel = 0;
		FieldSettings.HumidityLevel = 0;
		break;
	default:
		break;
	}
}

void AArcClimateVolume::PostInitProperties()
{
	Super::PostInitProperties();
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad) && !ClimateVolumeId.IsValid())
	{
		ClimateVolumeId = FGuid::NewGuid();
	}
}

void AArcClimateVolume::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FArcEcotopeCustomVersion::GUID);
	Super::Serialize(Ar);

#if WITH_EDITORONLY_DATA
	const TFunction<void()> DiscardProduct = [this]() -> void
	{
		ClimateProductMetadata = FArcClimateProductMetadata();
		ClimateChunkManifest.Reset();
		ClimateChunkPayloads.Reset();
	};
	const bool bSkipBulk = !Ar.IsPersistent() || (!Ar.IsLoading() && !Ar.IsSaving())
		|| Ar.IsTransacting() || Ar.IsCooking() || Ar.IsFilterEditorOnly()
		|| Ar.IsObjectReferenceCollector() || Ar.ShouldSkipBulkData()
		|| Ar.HasAnyPortFlags(PPF_Duplicate | PPF_DuplicateForPIE);
	if (bSkipBulk)
	{
		return;
	}
	const int32 ExpectedPayloadCount = ClimateChunkManifest.Chunks.Num();
	if (Ar.IsSaving())
	{
		const int32 PayloadCount = ClimateChunkPayloads.Num();
		if (PayloadCount < 0 || PayloadCount > FArcClimateProductStorage::MaxClimateChunkCount
			|| ExpectedPayloadCount > FArcClimateProductStorage::MaxClimateChunkCount
			|| PayloadCount != ExpectedPayloadCount)
		{
			UE_LOGF(LogArcEcotope, Error,"%ls cannot save invalid climate chunk payload count %d (expected %d).",
				*GetPathName(), PayloadCount, ExpectedPayloadCount);
			Ar.SetError();
			return;
		}
		int32 SerializedPayloadCount = PayloadCount;
		Ar << SerializedPayloadCount;
		for (UE::Serialization::FEditorBulkData& Payload : ClimateChunkPayloads)
		{
			Payload.Serialize(Ar, this);
		}
		return;
	}
	int32 SerializedPayloadCount = 0;
	Ar << SerializedPayloadCount;
	if (SerializedPayloadCount < 0
		|| SerializedPayloadCount > FArcClimateProductStorage::MaxClimateChunkCount
		|| ExpectedPayloadCount > FArcClimateProductStorage::MaxClimateChunkCount)
	{
		UE_LOGF(LogArcEcotope, Error,"%ls contains an unrepresentable climate chunk payload count %d.",
			*GetPathName(), SerializedPayloadCount);
		DiscardProduct();
		Ar.SetError();
		return;
	}
	TArray<UE::Serialization::FEditorBulkData> LoadedPayloads;
	LoadedPayloads.SetNum(SerializedPayloadCount);
	for (UE::Serialization::FEditorBulkData& Payload : LoadedPayloads)
	{
		Payload.Serialize(Ar, this);
	}
	if (SerializedPayloadCount != ExpectedPayloadCount)
	{
		UE_LOGF(LogArcEcotope, Error,"%ls discarded a climate product with %d payloads (expected %d); re-bake is required.",
			*GetPathName(), SerializedPayloadCount, ExpectedPayloadCount);
		DiscardProduct();
		return;
	}
	ClimateChunkPayloads = MoveTemp(LoadedPayloads);
#endif
}

void AArcClimateVolume::PostLoad()
{
	Super::PostLoad();
	if (!HasAnyFlags(RF_ClassDefaultObject) && !ClimateVolumeId.IsValid())
	{
		ClimateVolumeId = FGuid::NewGuid();
	}
#if WITH_EDITOR
	const bool bHasAnyProduct = !ClimateProductMetadata.IsEmpty()
		|| !ClimateChunkManifest.IsEmpty() || !ClimateChunkPayloads.IsEmpty();
	FText ProductError;
	if (bHasAnyProduct && ArcClimateVolumePrivate::CanRepairAuthoredClimate(*this)
		&& !ValidateClimateProduct(ProductError))
	{
		UE_LOGF(LogArcEcotope, Error,"%ls discarded an invalid climate product: %ls. Re-bake is required.",
			*GetPathName(), *ProductError.ToString());
		ClearClimateProduct();
	}
#endif
}

void AArcClimateVolume::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
#if WITH_EDITOR
	const UWorld* World = GetWorld();
	if (World != nullptr && World->WorldType == EWorldType::Editor && World->GetWorldPartition() != nullptr
		&& GetIsSpatiallyLoaded() && CanChangeIsSpatiallyLoadedFlag())
	{
		Modify();
		SetIsSpatiallyLoaded(false);
		UE_LOGF(LogArcEcotope, Log,"Forced climate volume '%ls' to always-loaded in partitioned world '%ls'.",
			*GetName(), *World->GetName());
	}
#endif
}

#if WITH_EDITOR
bool AArcClimateVolume::ReplaceClimateWithValidatedChunks(
	FArcClimateProductMetadata&& InMetadata,
	FArcClimateChunkManifest&& InManifest,
	TArray<FSharedBuffer>&& InPayloads,
	FText& OutError)
{
	check(IsInGameThread());
	TSharedRef<FArcClimateFrozenSource, ESPMode::ThreadSafe> Candidate =
		MakeShared<FArcClimateFrozenSource, ESPMode::ThreadSafe>();
	Candidate->Metadata = InMetadata;
	Candidate->Manifest = InManifest;
	Candidate->Payloads = InPayloads;
	FArcClimateProductReader Reader;
	FString ValidationError;
	if (!Reader.Open(Candidate, ValidationError))
	{
		OutError = FText::FromString(FString::Printf(
			TEXT("Climate staged product failed full validation: %s"), *ValidationError));
		return false;
	}

	TArray<UE::Serialization::FEditorBulkData> NewPayloads;
	NewPayloads.SetNum(InPayloads.Num());
	for (int32 PayloadIndex = 0; PayloadIndex < InPayloads.Num(); ++PayloadIndex)
	{
		NewPayloads[PayloadIndex].UpdatePayload(InPayloads[PayloadIndex], this);
		if (NewPayloads[PayloadIndex].GetPayloadSize() != static_cast<int64>(InPayloads[PayloadIndex].GetSize()))
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("Climate editor bulk payload %d changed size during staging."), PayloadIndex));
			return false;
		}
	}

	ClimateProductMetadata = MoveTemp(InMetadata);
	ClimateChunkManifest = MoveTemp(InManifest);
	ClimateChunkPayloads = MoveTemp(NewPayloads);
	OutError = FText::GetEmpty();
	return true;
}

void AArcClimateVolume::ClearClimateProduct()
{
	ClimateProductMetadata = FArcClimateProductMetadata();
	ClimateChunkManifest.Reset();
	ClimateChunkPayloads.Reset();
}

bool AArcClimateVolume::ValidateClimateProduct(FText& OutError) const
{
	if (ClimateProductMetadata.IsEmpty() && ClimateChunkManifest.IsEmpty() && ClimateChunkPayloads.IsEmpty())
	{
		OutError = FText::FromString(TEXT("Climate volume has no baked climate product."));
		return false;
	}
	FArcClimateFrozenSourcePtr Source;
	return FreezeClimateSource(Source, OutError);
}

bool AArcClimateVolume::FreezeClimateSource(
	FArcClimateFrozenSourcePtr& OutSource,
	FText& OutError) const
{
	check(IsInGameThread());
	OutSource.Reset();
	if (ClimateProductMetadata.IsEmpty())
	{
		OutError = FText::FromString(TEXT("Climate volume has no baked product to freeze."));
		return false;
	}
	if (ClimateChunkPayloads.Num() > FArcClimateProductStorage::MaxClimateChunkCount)
	{
		OutError = FText::FromString(TEXT("Climate bulk payload count exceeds the supported load limit."));
		return false;
	}
	TArray<int64> PayloadSizes;
	PayloadSizes.Reserve(ClimateChunkPayloads.Num());
	uint64 TotalPayloadBytes = 0;
	for (const UE::Serialization::FEditorBulkData& Payload : ClimateChunkPayloads)
	{
		const int64 PayloadSize = Payload.GetPayloadSize();
		if (!ArcClimateVolumePrivate::AccumulatePayloadSize(PayloadSize, TotalPayloadBytes))
		{
			OutError = FText::FromString(TEXT("Climate bulk payload sizes exceed the supported load limit."));
			return false;
		}
		PayloadSizes.Add(PayloadSize);
	}
	FString ValidationError;
	if (!FArcClimateProductStorage::ValidateManifestMetadata(
			ClimateProductMetadata, ClimateChunkManifest, PayloadSizes, ValidationError))
	{
		OutError = FText::FromString(FString::Printf(
			TEXT("Climate product metadata is invalid: %s"), *ValidationError));
		return false;
	}

	TSharedRef<FArcClimateFrozenSource, ESPMode::ThreadSafe> Candidate =
		MakeShared<FArcClimateFrozenSource, ESPMode::ThreadSafe>();
	Candidate->Metadata = ClimateProductMetadata;
	Candidate->Manifest = ClimateChunkManifest;
	Candidate->Payloads.Reserve(ClimateChunkPayloads.Num());
	for (int32 PayloadIndex = 0; PayloadIndex < ClimateChunkPayloads.Num(); ++PayloadIndex)
	{
		FSharedBuffer Payload = ClimateChunkPayloads[PayloadIndex].GetPayload().Get();
		if (!FArcClimateProductStorage::ValidatePayloadBytes(
				ClimateChunkManifest.Chunks[PayloadIndex], Payload, ValidationError))
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("Climate payload %d is invalid: %s"), PayloadIndex, *ValidationError));
			return false;
		}
		Candidate->Payloads.Add(MoveTemp(Payload));
	}
	FArcClimateProductReader Reader;
	if (!Reader.Open(Candidate, ValidationError))
	{
		OutError = FText::FromString(FString::Printf(
			TEXT("Climate frozen source failed full validation: %s"), *ValidationError));
		return false;
	}
	OutSource = Candidate;
	OutError = FText::GetEmpty();
	return true;
}

bool AArcClimateVolume::SampleClimateAtWorldPosition(
	const FVector& WorldPosition,
	const EArcClimateField Field,
	float& OutValue,
	bool& bOutFieldValid,
	FText& OutError) const
{
	bOutFieldValid = false;
	FArcClimateFrozenSourcePtr Source;
	if (!FreezeClimateSource(Source, OutError))
	{
		return false;
	}
	FArcClimateProductReader Reader;
	FString ReaderError;
	if (!Reader.Open(Source, ReaderError)
		|| !Reader.SampleAtWorldPosition(WorldPosition, Field, OutValue, bOutFieldValid, ReaderError))
	{
		OutError = FText::FromString(MoveTemp(ReaderError));
		return false;
	}
	OutError = FText::GetEmpty();
	return true;
}

void AArcClimateVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (bDerivingBakeQuality || PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive
		|| PropertyChangedEvent.MemberProperty == nullptr)
	{
		return;
	}
	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();
	if (MemberName == GET_MEMBER_NAME_CHECKED(AArcClimateVolume, bDrawClimateField)
		|| MemberName == GET_MEMBER_NAME_CHECKED(AArcClimateVolume, ClimateField)
		|| MemberName == GET_MEMBER_NAME_CHECKED(AArcClimateVolume, bClimateFieldGreyscale)
		|| MemberName == GET_MEMBER_NAME_CHECKED(AArcClimateVolume, ClimateFieldStride)
		|| MemberName == GET_MEMBER_NAME_CHECKED(AArcClimateVolume, ClimateFieldCellScale)
		|| MemberName == GET_MEMBER_NAME_CHECKED(AArcClimateVolume, bDrawClimateLabels)
		|| MemberName == GET_MEMBER_NAME_CHECKED(AArcClimateVolume, MaxDrawnClimateCells))
	{
		return;
	}
	if (MemberName == GET_MEMBER_NAME_CHECKED(AArcClimateVolume, BakeQuality))
	{
		ApplyClimateBakeQualityPreset();
	}
	ClearClimateProduct();
}

void AArcClimateVolume::PostEditMove(const bool bFinished)
{
	Super::PostEditMove(bFinished);
	if (bFinished && !HasAnyFlags(RF_ClassDefaultObject))
	{
		ClearClimateProduct();
	}
}

void AArcClimateVolume::PostDuplicate(const bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	ClearClimateProduct();
	if (!bDuplicateForPIE && !HasAnyFlags(RF_ClassDefaultObject))
	{
		ClimateVolumeId = FGuid::NewGuid();
	}
}

void AArcClimateVolume::PostEditImport()
{
	Super::PostEditImport();
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		ClimateVolumeId = FGuid::NewGuid();
		ClearClimateProduct();
	}
}

void AArcClimateVolume::PostEditUndo()
{
	Super::PostEditUndo();
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}
	FText ProductError;
	if (!ValidateClimateProduct(ProductError))
	{
		ClearClimateProduct();
	}
}

EDataValidationResult AArcClimateVolume::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	const TFunction<void(const TCHAR*)> AddError = [&Context, &Result](const TCHAR* Message) -> void
	{
		Context.AddError(FText::FromString(Message));
		Result = EDataValidationResult::Invalid;
	};
	if (!ClimateVolumeId.IsValid())
	{
		AddError(TEXT("ClimateVolumeId must be a valid GUID."));
	}
	if (VolumeBounds == nullptr)
	{
		AddError(TEXT("VolumeBounds is required."));
	}
	else
	{
		const FVector Extent = VolumeBounds->GetUnscaledBoxExtent();
		if (Extent.ContainsNaN() || Extent.X <= 0.0 || Extent.Y <= 0.0 || Extent.Z <= 0.0)
		{
			AddError(TEXT("VolumeBounds must have finite positive extents."));
		}
	}
	if (!FMath::IsFinite(Temperature) || Temperature < 0.0 || Temperature > 1.0)
	{
		AddError(TEXT("Temperature must be finite and inside [0, 1]."));
	}
	if (!FMath::IsFinite(ReferenceElevationM))
	{
		AddError(TEXT("ReferenceElevationM must be finite."));
	}
	if (!FMath::IsFinite(Humidity) || Humidity < 0.0 || Humidity > 1.0)
	{
		AddError(TEXT("Humidity must be finite and inside [0, 1]."));
	}
	if (!FMath::IsFinite(ClimatePriority))
	{
		AddError(TEXT("ClimatePriority must be finite."));
	}
	if (!FMath::IsFinite(GradientBandCm) || GradientBandCm < 0.0)
	{
		AddError(TEXT("GradientBandCm must be finite and non-negative."));
	}
	if (!FMath::IsFinite(GradientSharpness) || GradientSharpness < 0.0001)
	{
		AddError(TEXT("GradientSharpness must be finite and at least 0.0001."));
	}
	if (!FMath::IsFinite(CellSizeCm) || CellSizeCm <= 0.0f)
	{
		AddError(TEXT("CellSizeCm must be finite and positive."));
	}
	if (ChunkBaseCellSpan <= 0)
	{
		AddError(TEXT("ChunkBaseCellSpan must be positive."));
	}
	const FVector Scale = GetActorScale3D();
	if (Scale.ContainsNaN() || Scale.X <= 0.0 || Scale.Y <= 0.0 || Scale.Z <= 0.0
		|| !FMath::IsNearlyEqual(Scale.X, Scale.Y) || !FMath::IsNearlyEqual(Scale.X, Scale.Z))
	{
		AddError(TEXT("Climate volume scale must be finite, positive, and uniform."));
	}
	FText SettingsError;
	if (!FieldSettings.IsValid(SettingsError))
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("FieldSettings is invalid: %s"), *SettingsError.ToString())));
		Result = EDataValidationResult::Invalid;
	}
	if (!HydrologySettings.IsValid(SettingsError))
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("HydrologySettings is invalid: %s"), *SettingsError.ToString())));
		Result = EDataValidationResult::Invalid;
	}
	const UWorld* World = GetWorld();
	if (World != nullptr && World->WorldType == EWorldType::Editor && World->GetWorldPartition() != nullptr
		&& GetIsSpatiallyLoaded())
	{
		AddError(TEXT("Climate volumes must be always-loaded in World Partition worlds."));
	}
	FText ProductError;
	if (!ValidateClimateProduct(ProductError))
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("Climate product is invalid: %s"), *ProductError.ToString())));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif
