// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Climate/ArcClimateProduct.h"
#include "Climate/ArcClimateSettings.h"
#include "GameFramework/Actor.h"
#include "Misc/DataValidation.h"

#if WITH_EDITORONLY_DATA
#include "Serialization/EditorBulkData.h"
#endif

#include "ArcClimateVolume.generated.h"

class UBoxComponent;

UENUM(BlueprintType)
enum class EArcClimateBakeQuality : uint8
{
	Low,
	Medium,
	High,
};

UCLASS(BlueprintType)
class ARCECOTOPE_API AArcClimateVolume : public AActor
{
	GENERATED_BODY()

public:
	AArcClimateVolume();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|State")
	FGuid ClimateVolumeId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climate")
	TObjectPtr<UBoxComponent> VolumeBounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Temperature = 0.5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate", meta = (Units = "m"))
	double ReferenceElevationM = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Humidity = 0.5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate")
	double ClimatePriority = 100.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate", meta = (ClampMin = "0.0", Units = "cm"))
	double GradientBandCm = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate", meta = (ClampMin = "0.0001"))
	double GradientSharpness = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate")
	bool bContributesClimate = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Quality", meta = (ClampMin = "1.0", Units = "cm"))
	float CellSizeCm = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Quality", meta = (ClampMin = "1"))
	int32 ChunkBaseCellSpan = 256;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quality")
	EArcClimateBakeQuality BakeQuality = EArcClimateBakeQuality::Medium;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate", meta = (ShowOnlyInnerProperties))
	FArcClimateFieldSettings FieldSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climate", meta = (ShowOnlyInnerProperties))
	FArcClimateHydrologySettings HydrologySettings;

	UPROPERTY(EditAnywhere, Category = "Debug|Climate", DisplayName = "Draw Climate Field",
		meta = (ToolTip = "View-only overlay of one baked climate field; changing it never invalidates the climate bake."))
	bool bDrawClimateField = false;

	UPROPERTY(EditAnywhere, Category = "Debug|Climate", DisplayName = "Climate Field",
		meta = (EditCondition = "bDrawClimateField", InvalidEnumValues = "Count"))
	EArcClimateField ClimateField = EArcClimateField::Temperature;

	UPROPERTY(EditAnywhere, Category = "Debug|Climate", DisplayName = "Climate Field Greyscale",
		meta = (EditCondition = "bDrawClimateField"))
	bool bClimateFieldGreyscale = true;

	UPROPERTY(EditAnywhere, Category = "Debug|Climate", DisplayName = "Climate Field Stride",
		meta = (EditCondition = "bDrawClimateField", ClampMin = "0",
		ToolTip = "0 auto-fits the sheet to Max Drawn Climate Cells; above 0 samples every Nth baked cell."))
	int32 ClimateFieldStride = 0;

	UPROPERTY(EditAnywhere, Category = "Debug|Climate", DisplayName = "Climate Field Cell Scale",
		meta = (EditCondition = "bDrawClimateField", ClampMin = "0.05", ClampMax = "1.0"))
	float ClimateFieldCellScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Debug|Climate", DisplayName = "Draw Climate Labels",
		meta = (EditCondition = "bDrawClimateField"))
	bool bDrawClimateLabels = false;

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Debug|Climate", DisplayName = "Max Drawn Climate Cells",
		meta = (EditCondition = "bDrawClimateField", ClampMin = "0"))
	int32 MaxDrawnClimateCells = 50000;

	bool HasDebugOverlayEnabled() const { return bDrawClimateField; }

	UPROPERTY(VisibleAnywhere, NonTransactional, Category = "Debug|State")
	FArcClimateProductMetadata ClimateProductMetadata;

#if WITH_EDITORONLY_DATA
	UPROPERTY(NonTransactional)
	FArcClimateChunkManifest ClimateChunkManifest;
#endif

	void ApplyClimateBakeQualityPreset();

	virtual void PostInitProperties() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void PostRegisterAllComponents() override;

#if WITH_EDITOR
	bool ReplaceClimateWithValidatedChunks(
		FArcClimateProductMetadata&& InMetadata,
		FArcClimateChunkManifest&& InManifest,
		TArray<FSharedBuffer>&& InPayloads,
		FText& OutError);

	void ClearClimateProduct();
	bool ValidateClimateProduct(FText& OutError) const;
	bool FreezeClimateSource(FArcClimateFrozenSourcePtr& OutSource, FText& OutError) const;
	bool SampleClimateAtWorldPosition(
		const FVector& WorldPosition,
		EArcClimateField Field,
		float& OutValue,
		bool& bOutFieldValid,
		FText& OutError) const;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostEditImport() override;
	virtual void PostEditUndo() override;
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	bool bDerivingBakeQuality = false;

#if WITH_EDITORONLY_DATA
	TArray<UE::Serialization::FEditorBulkData> ClimateChunkPayloads;
#endif
};
