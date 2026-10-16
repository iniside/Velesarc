// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "River/ArcWaterRiverModifierTypes.h"
#include "Stamps/ArcTerrainStampComponent.h"

#include "ArcWaterRiverSplineModifier.generated.h"

class UArcWaterRiverModifier;
struct FArcWaterRiverSplineFieldScratch;

UCLASS(Transient)
class ARCWATER_API UArcWaterRiverSplineModifier final : public UArcTerrainStampComponent
{
	GENERATED_BODY()

public:
	UArcWaterRiverSplineModifier();

	bool Initialize(UArcWaterRiverModifier& Parent,
		const FArcWaterRiverGeneratedReach& InReach, double InSampleSpacingCm, FText& OutError);

	FIntPoint GetFieldDims() const { return FieldDims; }
	double GetSampleSpacingCm() const { return SampleSpacingCm; }
	int64 GetEstimatedRasterBytes() const;

	virtual bool GetSourceDescriptor(FArcStampSourceDesc& OutDesc) const override;
	virtual const UArcTerrainStampComponent* GetCompositeOrderAnchor() const override;
	virtual void OnUnregister() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
	TSharedRef<FArcWaterRiverSplineFieldScratch> AcquireScratch() const;

	UPROPERTY(Transient)
	TObjectPtr<UArcWaterRiverModifier> ParentModifier;

	UPROPERTY(Transient)
	FArcWaterRiverGeneratedReach Reach;

	UPROPERTY(Transient)
	FIntPoint FieldDims = FIntPoint::ZeroValue;

	UPROPERTY(Transient)
	FGuid HeightContentId;

	UPROPERTY(Transient)
	FGuid MaskContentId;

	double SampleSpacingCm = 0.0;
	mutable TSharedPtr<FArcWaterRiverSplineFieldScratch> FieldScratch;
};
