// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Decision/ArcTacticsConfig.h"
#include "Simulator/ArcTacticsSimulator.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;
struct FArcResponseCurve;

class SArcTacticsSimulator final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcTacticsSimulator) {}
	SLATE_END_ARGS()

	virtual ~SArcTacticsSimulator() override;
	void Construct(const FArguments& InArgs, UArcTacticsConfig* InConfig);

private:
	void HandleAuthoringChanged(EArcTacticsAuthoringChange Change);
	void SyncRowWidgets(bool bForce);
	void GatherStructure(TArray<FGuid>& OutStructure) const;

	TSharedRef<SWidget> MakeHeader();
	TSharedRef<SWidget> MakeRowWidget(const FArcTacticsSimulatorRow& Row);
	TSharedRef<SWidget> MakeRowHeader(const FGuid& RowID);
	TSharedRef<SWidget> MakeConsiderationWidget(const FGuid& RowID, const FGuid& ConsiderationID);

	const FArcTacticsSimulatorConsideration* FindConsideration(const FGuid& RowID, const FGuid& ConsiderationID) const;
	const FArcResponseCurve* GetConsiderationCurve(FGuid ConsiderationID) const;

	FText GetMinScoreText() const;
	FText GetNothingPickedText() const;
	FText GetRowNameText(FGuid RowID) const;
	FText GetRowPriorityText(FGuid RowID) const;
	FText GetRowScoreText(FGuid RowID) const;
	TOptional<float> GetRowScorePercent(FGuid RowID) const;
	FText GetRowOutcomeText(FGuid RowID) const;
	FSlateColor GetRowOutcomeColor(FGuid RowID) const;
	FText GetRowChanceText(FGuid RowID) const;
	FText GetRowMonteCarloText(FGuid RowID) const;
	EVisibility GetMonteCarloVisibility() const;
	EVisibility GetRowBoundVisibility(FGuid RowID) const;
	FText GetConsiderationNameText(FGuid RowID, FGuid ConsiderationID) const;
	EVisibility GetConsiderationBoundVisibility(FGuid RowID, FGuid ConsiderationID) const;
	FText GetConsiderationValuesText(FGuid RowID, FGuid ConsiderationID) const;
	float GetConsiderationInput(FGuid ConsiderationID) const;
	TOptional<float> GetConsiderationMarker(FGuid ConsiderationID) const;
	ECheckBoxState GetVetoState(FGuid ConsiderationID) const;
	ECheckBoxState GetLockState(FGuid ConsiderationID) const;

	TWeakObjectPtr<UArcTacticsConfig> Config;
	FArcTacticsSimulator Simulator;
	TSharedPtr<SVerticalBox> RowsBox;
	TArray<FGuid> BuiltStructure;
	TSet<FGuid> ExpandedRows;
};
