// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Input/Reply.h"

class AArcTownVolume;
class FArcTownSimulationController;
class UArcTownSimulationState;

/** Thin Details-panel adapter for the editor-only ArcTown site, simulation and emission APIs. */
class FArcTownVolumeDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	AArcTownVolume* GetVolume() const;
	FArcTownSimulationController* GetController() const;
	bool CanMutateState() const;
	bool CanStartSimulation() const;
	UArcTownSimulationState* EnsureSimulationState(AArcTownVolume& Town) const;

	FReply SampleSite();
	FReply Simulate();
	FReply ResetSimulation();
	FReply Rebuild();
	FReply Clear();
	FReply SweepOrphans();

	FText GetSimulateLabelText() const;
	FText GetSiteStatusText() const;
	FText GetSimulationYearText() const;
	FText GetNodeCountText() const;
	FText GetRoadCountText() const;
	FText GetSimulatedBuildingCountText() const;
	FText GetUnreachableTripCountText() const;
	FText GetProgressText() const;
	FText GetEmittedBuildingCountText() const;
	FText GetCellCountText() const;
	FText GetRoadSplineCountText() const;
	FText GetOrphansSweptText() const;
	FText GetLastErrorText() const;

	void SetLastError(const FText& Error) const;
	void ClearLastError() const;

	TWeakObjectPtr<AArcTownVolume> SelectedVolume;
	mutable TWeakPtr<FArcTownSimulationController> CachedController;
};
