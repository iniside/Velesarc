// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Styling/SlateTypes.h"
#include "Types/SlateEnums.h"

class AArcEcotopeVolume;
class IPropertyUtilities;
class SWidget;

/** Thin Details-panel adapter for the editor-only ArcForest orchestration APIs. */
class FArcEcotopeVolumeDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	AArcEcotopeVolume* GetVolume() const;
	bool CanStartOperation() const;
	bool CanMutateState() const;

	void BuildActionsCategory(IDetailLayoutBuilder& DetailBuilder);
	void BuildStatusCategory(IDetailLayoutBuilder& DetailBuilder);
	void BuildCategoryTabs(IDetailLayoutBuilder& DetailBuilder, const TArray<FName>& TabNames);
	TSharedRef<SWidget> MakeCategoryTab(FName TabName, const FText& Label);

	FName GetSelectedTab() const;
	ECheckBoxState IsTabSelected(FName TabName) const;
	void OnTabSelected(ECheckBoxState NewState, FName TabName);

	FReply BakeSite();
	FReply Validate();
	FReply StartBootstrapAndSpinUp();
	FReply StartBareGroundAndSpinUp();
	FReply SimulateConfiguredYears();
	FReply Cancel();
	FReply ClearState();
	FReply RebuildInstances();

	FText GetCurrentYearText() const;
	FText GetProgressText() const;
	FText GetLastErrorText() const;

	void SetLastError(const FText& Error) const;
	void ClearLastError() const;

	TOptional<FVector::FReal> GetBoxExtentAxis(int32 AxisIndex) const;
	TOptional<FVector::FReal> GetBoxExtentX() const;
	TOptional<FVector::FReal> GetBoxExtentY() const;
	TOptional<FVector::FReal> GetBoxExtentZ() const;
	void CommitBoxExtentAxis(int32 AxisIndex, FVector::FReal NewValue);
	void OnBoxExtentXCommitted(FVector::FReal NewValue, ETextCommit::Type CommitType);
	void OnBoxExtentYCommitted(FVector::FReal NewValue, ETextCommit::Type CommitType);
	void OnBoxExtentZCommitted(FVector::FReal NewValue, ETextCommit::Type CommitType);

	TWeakObjectPtr<AArcEcotopeVolume> SelectedVolume;
	TSharedPtr<IPropertyUtilities> PropertyUtilities;
};
