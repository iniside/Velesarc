// Copyright Lukasz Baran. All Rights Reserved.

#include "ViewModels/ArcBuilderViewModel.h"

#include "ArcBuilderSubsystem.h"
#include "ArcBuildingDefinition.h"

void UArcBuilderViewModel::SetBuilderSubsystem(UArcBuilderSubsystem* InSubsystem)
{
	BuilderSubsystem = InSubsystem;
}

void UArcBuilderViewModel::RefreshState(bool bInSessionActive, bool bInIsPlacing, bool bInRequirementsMet)
{
	UE_MVVM_SET_PROPERTY_VALUE(bSessionActive, bInSessionActive);
	UE_MVVM_SET_PROPERTY_VALUE(bIsPlacing, bInIsPlacing);
	UE_MVVM_SET_PROPERTY_VALUE(bPlacementRequirementsMet, bInRequirementsMet);
}

void UArcBuilderViewModel::SetBuildings(TArray<TObjectPtr<UArcBuildingViewModel>>&& NewBuildings)
{
	UE_MVVM_SET_PROPERTY_VALUE(Buildings, NewBuildings);
}

void UArcBuilderViewModel::SelectBuilding(UArcBuildingViewModel* Building)
{
	if (SelectedBuilding == Building)
	{
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(SelectedBuilding, Building);

	for (const TObjectPtr<UArcBuildingViewModel>& Entry : Buildings)
	{
		if (Entry != nullptr)
		{
			Entry->SetbIsSelected(Entry == Building);
		}
	}

	OnSelectedBuildingChangedDelegate.Broadcast(Building);
}

bool UArcBuilderViewModel::StartPlacement()
{
	UArcBuilderSubsystem* Subsystem = BuilderSubsystem.Get();
	if (Subsystem == nullptr || SelectedBuilding == nullptr || SelectedBuilding->GetDefinition() == nullptr)
	{
		return false;
	}

	Subsystem->StartPlacement(SelectedBuilding->GetDefinition());
	return true;
}

bool UArcBuilderViewModel::ConfirmPlacement()
{
	UArcBuilderSubsystem* Subsystem = BuilderSubsystem.Get();
	if (Subsystem == nullptr)
	{
		return false;
	}

	Subsystem->ConfirmPlacement();
	return true;
}

bool UArcBuilderViewModel::CancelPlacement()
{
	UArcBuilderSubsystem* Subsystem = BuilderSubsystem.Get();
	if (Subsystem == nullptr)
	{
		return false;
	}

	Subsystem->CancelPlacement();
	return true;
}
