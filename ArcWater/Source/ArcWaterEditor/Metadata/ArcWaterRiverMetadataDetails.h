// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "ScopedTransaction.h"
#include "SplineMetadataDetailsFactory.h"

#include "ArcWaterRiverMetadataDetails.generated.h"

namespace ETextCommit { enum Type : int; }

class IDetailGroup;
class UArcWaterRiverMetadata;
class USplineComponent;

/** No explicit registration: SplineComponentDetails discovers this through TObjectIterator and matches GetMetadataClass() by exact class equality. */
UCLASS()
class UArcWaterRiverMetadataDetailsFactory : public USplineMetadataDetailsFactoryBase
{
	GENERATED_UCLASS_BODY()

public:
	virtual ~UArcWaterRiverMetadataDetailsFactory() = default;
	virtual TSharedPtr<ISplineMetadataDetails> Create() override;
	virtual UClass* GetMetadataClass() const override;
};

class FArcWaterRiverMetadataDetails : public ISplineMetadataDetails, public TSharedFromThis<FArcWaterRiverMetadataDetails>
{
public:
	virtual ~FArcWaterRiverMetadataDetails() = default;

	virtual FName GetName() const override { return FName(TEXT("ArcWaterRiverMetadataDetails")); }
	virtual FText GetDisplayName() const override;
	virtual void Update(USplineComponent* InSplineComponent, const TSet<int32>& InSelectedKeys) override;
	virtual void GenerateChildContent(IDetailGroup& InGroup) override;

	TOptional<float> HalfWidthValue;
	TOptional<float> FlowSpeedValue;

	USplineComponent* SplineComp = nullptr;
	TSet<int32> SelectedKeys;

private:
	void OnBeginSliderMovement();
	void OnEndSliderMovement(float NewValue);

	EVisibility IsEnabled() const { return (SelectedKeys.Num() > 0) ? EVisibility::Visible : EVisibility::Collapsed; }

	TOptional<float> GetHalfWidth() const { return HalfWidthValue; }
	void OnSetHalfWidth(float NewValue, ETextCommit::Type CommitInfo);

	TOptional<float> GetFlowSpeed() const { return FlowSpeedValue; }
	void OnSetFlowSpeed(float NewValue, ETextCommit::Type CommitInfo);

	UArcWaterRiverMetadata* GetMetadata() const;

	TUniquePtr<FScopedTransaction> EditSliderValueTransaction;
};
