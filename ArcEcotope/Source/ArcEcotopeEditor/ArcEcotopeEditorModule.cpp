// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEcotopeEditorModule.h"

#include "ArcEcotopeModule.h"
#include "Climate/ArcClimateVolume.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Details/ArcClimateVolumeDetails.h"
#include "Details/ArcEcotopeVolumeDetails.h"
#include "Diagnostics/ArcForestDiagnostics.h"
#include "Editor.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "ILevelEditor.h"
#include "ISCSEditorUICustomization.h"
#include "Instances/ArcEcotopeInstancePartitionActor.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Selection.h"
#include "Simulation/ArcForestSimulationController.h"
#include "Simulation/ArcForestSimulationState.h"
#include "Simulation/ArcEcotopeVolume.h"
#include "SubobjectDataSubsystem.h"

namespace ArcEcotopeEditorModulePrivate
{
	class FVolumeSubobjectEditorCustomization final : public ISCSEditorUICustomization
	{
	public:
		UE_EXPERIMENTAL_OVERRIDE(5.8, "Signature may change in the future")
		virtual bool OverrideDesiredVisibility(
			const UE::Editor::FSubobjectEditorContext& Context,
			EVisibility& VisibilityOut) const override
		{
			if (Cast<AArcEcotopeVolume>(Context.ObjectContext) == nullptr)
			{
				return false;
			}
			VisibilityOut = EVisibility::Collapsed;
			return true;
		}
	};

	void LogVolumeStatistics(const AArcEcotopeVolume& Volume, const TCHAR* Reason)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ArcEcotope_LogVolumeStatistics);
		if (!UE_LOG_ACTIVE(LogArcEcotope, Log))
		{
			return;
		}
		const double StartSeconds = FPlatformTime::Seconds();

		int32 IndividualCount = 0;
		int32 CohortCount = 0;
		int32 PatchCount = 0;
		int32 DeadwoodCount = 0;
		if (const UArcForestSimulationState* State = Volume.SimulationState)
		{
			IndividualCount = State->Snapshot.Individuals.Num();
			CohortCount = State->Snapshot.Cohorts.Num();
			PatchCount = State->Snapshot.Patches.Num();
			DeadwoodCount = State->Snapshot.Deadwood.Num();
		}

		int64 TotalInstances = 0;
		int32 StaticComponentCount = 0;
		int32 SkinnedComponentCount = 0;
		int32 UnloadedActorCount = 0;
		FString ComponentText;
		for (const FArcEcotopeEmittedActor& Record : Volume.InstancePartitionActors)
		{
			const AArcEcotopeInstancePartitionActor* PartitionActor =
				Cast<AArcEcotopeInstancePartitionActor>(Record.Actor.Get());
			if (PartitionActor == nullptr)
			{
				// Reported rather than resolved: a diagnostics log must not stream a cell in.
				++UnloadedActorCount;
				ComponentText += FString::Printf(TEXT("\n    actor %-40s <unloaded>"), *Record.Actor.ToString());
				continue;
			}
			ComponentText += FString::Printf(TEXT("\n    actor %s"), *PartitionActor->GetName());
			for (const UInstancedStaticMeshComponent* Component : PartitionActor->StaticMeshInstances)
			{
				const int32 InstanceCount = IsValid(Component) ? Component->GetInstanceCount() : 0;
				TotalInstances += InstanceCount;
				++StaticComponentCount;
				ComponentText += FString::Printf(
					TEXT("\n      ISM  %-40s instances=%d"),
					IsValid(Component) ? *Component->GetName() : TEXT("<invalid>"),
					InstanceCount);
			}
			for (const UInstancedSkinnedMeshComponent* Component : PartitionActor->SkinnedMeshInstances)
			{
				const int32 InstanceCount = IsValid(Component) ? Component->GetInstanceCount() : 0;
				TotalInstances += InstanceCount;
				++SkinnedComponentCount;
				ComponentText += FString::Printf(
					TEXT("\n      ISKM %-40s instances=%d"),
					IsValid(Component) ? *Component->GetName() : TEXT("<invalid>"),
					InstanceCount);
			}
		}

		UE_LOGF(
			LogArcEcotope,
			Log,"Volume '%ls' (%ls): individuals=%d cohorts=%d patches=%d deadwood=%d | actors=%d unloaded=%d entityActors=%d ISM=%d ISKM=%d instances=%lld | gathered in %.2f ms%ls",
			*Volume.GetName(),
			Reason,
			IndividualCount,
			CohortCount,
			PatchCount,
			DeadwoodCount,
			Volume.InstancePartitionActors.Num(),
			UnloadedActorCount,
			Volume.EmittedEntityActors.Num(),
			StaticComponentCount,
			SkinnedComponentCount,
			TotalInstances,
			UE::ArcForest::GetElapsedMilliseconds(StartSeconds),
			*ComponentText);
	}

	int32 GetSelectedVolumes(TArray<AArcEcotopeVolume*>& OutVolumes)
	{
		OutVolumes.Reset();
		USelection* SelectedActors = GEditor != nullptr ? GEditor->GetSelectedActors() : nullptr;
		if (SelectedActors == nullptr)
		{
			return 0;
		}
		return SelectedActors->GetSelectedObjects<AArcEcotopeVolume>(OutVolumes);
	}

	void DumpSelectedVolumeStatistics()
	{
		TArray<AArcEcotopeVolume*> Volumes;
		if (GetSelectedVolumes(Volumes) == 0)
		{
			UE_LOGF(LogArcEcotope, Log,"arcecotope.DumpStats: no ArcEcotope volume is selected.");
			return;
		}
		for (const AArcEcotopeVolume* Volume : Volumes)
		{
			if (IsValid(Volume))
			{
				LogVolumeStatistics(*Volume, TEXT("arcecotope.DumpStats"));
			}
		}
		UE::ArcForest::LogSharedHelperCounterTotals(TEXT("arcecotope.DumpStats"));
	}

	FAutoConsoleCommand DumpStatsCommand(
		TEXT("arcecotope.DumpStats"),
		TEXT("Logs record, component and instance counts for the selected ArcEcotope volumes."),
		FConsoleCommandDelegate::CreateStatic(&DumpSelectedVolumeStatistics));

}

void FArcEcotopeEditorModule::StartupModule()
{
	if (!IsRunningGame())
	{
		FPropertyEditorModule& PropertyEditor =
			FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		FRegisterCustomClassLayoutParams LayoutParams;
		// FActorDetails registers later, from DetailCustomizations; only an explicit high Order queries this layout after it.
		LayoutParams.OptionalOrder = MAX_int32;
		PropertyEditor.RegisterCustomClassLayout(
			AArcEcotopeVolume::StaticClass()->GetFName(),
			FOnGetDetailCustomizationInstance::CreateStatic(&FArcEcotopeVolumeDetails::MakeInstance),
			LayoutParams);
		PropertyEditor.RegisterCustomClassLayout(
			AArcClimateVolume::StaticClass()->GetFName(),
			FOnGetDetailCustomizationInstance::CreateStatic(&FArcClimateVolumeDetails::MakeInstance),
			LayoutParams);
		PropertyEditor.NotifyCustomizationModuleChanged();
		bDetailsCustomizationRegistered = true;

		SubobjectEditorCustomization = MakeShared<ArcEcotopeEditorModulePrivate::FVolumeSubobjectEditorCustomization>();
		FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		LevelEditor.OnLevelEditorCreated().AddRaw(this, &FArcEcotopeEditorModule::RegisterSubobjectEditorCustomization);
		RegisterSubobjectEditorCustomization(LevelEditor.GetFirstLevelEditor());

		SelectionChangedHandle = USelection::SelectionChangedEvent.AddRaw(
			this, &FArcEcotopeEditorModule::OnEditorSelectionChanged);
	}

	SimulationController = MakeUnique<FArcForestSimulationController>();
	SimulationController->Register();
}

void FArcEcotopeEditorModule::ShutdownModule()
{
	if (SelectionChangedHandle.IsValid())
	{
		USelection::SelectionChangedEvent.Remove(SelectionChangedHandle);
		SelectionChangedHandle.Reset();
	}

	if (SimulationController)
	{
		SimulationController->Unregister();
		SimulationController.Reset();
	}

	if (bDetailsCustomizationRegistered
		&& !IsEngineExitRequested()
		&& FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FPropertyEditorModule& PropertyEditor =
			FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		PropertyEditor.UnregisterCustomClassLayout(AArcEcotopeVolume::StaticClass()->GetFName());
		PropertyEditor.UnregisterCustomClassLayout(AArcClimateVolume::StaticClass()->GetFName());
		PropertyEditor.NotifyCustomizationModuleChanged();
		bDetailsCustomizationRegistered = false;
	}

	if (SubobjectEditorCustomization.IsValid()
		&& FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
	{
		FLevelEditorModule& LevelEditor =
			FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		LevelEditor.OnLevelEditorCreated().RemoveAll(this);
		if (TSharedPtr<ILevelEditor> FirstLevelEditor = LevelEditor.GetFirstLevelEditor())
		{
			FirstLevelEditor->RemoveActorDetailsSCSEditorUICustomization(SubobjectEditorCustomization);
		}
	}
	SubobjectEditorCustomization.Reset();
}

void FArcEcotopeEditorModule::RegisterSubobjectEditorCustomization(TSharedPtr<ILevelEditor> InLevelEditor)
{
	if (InLevelEditor.IsValid() && SubobjectEditorCustomization.IsValid())
	{
		InLevelEditor->AddActorDetailsSCSEditorUICustomization(SubobjectEditorCustomization);
	}
}

void FArcEcotopeEditorModule::OnEditorSelectionChanged(UObject* NewSelection)
{
	(void)NewSelection;
	TArray<AArcEcotopeVolume*> Volumes;
	if (ArcEcotopeEditorModulePrivate::GetSelectedVolumes(Volumes) == 0)
	{
		LastReportedVolumes.Reset();
		return;
	}
	// The editor broadcasts several times per user-visible selection change.
	bool bSameSelection = LastReportedVolumes.Num() == Volumes.Num();
	for (int32 Index = 0; bSameSelection && Index < Volumes.Num(); ++Index)
	{
		bSameSelection = LastReportedVolumes[Index].Get() == Volumes[Index];
	}
	if (bSameSelection)
	{
		return;
	}
	LastReportedVolumes.Reset(Volumes.Num());
	for (AArcEcotopeVolume* Volume : Volumes)
	{
		LastReportedVolumes.Add(Volume);
	}
	for (const AArcEcotopeVolume* Volume : Volumes)
	{
		if (IsValid(Volume))
		{
			ArcEcotopeEditorModulePrivate::LogVolumeStatistics(*Volume, TEXT("selection changed"));
		}
	}
}

FArcEcotopeEditorModule& FArcEcotopeEditorModule::Get()
{
	return FModuleManager::LoadModuleChecked<FArcEcotopeEditorModule>(TEXT("ArcEcotopeEditor"));
}

bool FArcEcotopeEditorModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded(TEXT("ArcEcotopeEditor"));
}

FArcForestSimulationController& FArcEcotopeEditorModule::GetSimulationController() const
{
	check(SimulationController);
	return *SimulationController;
}

IMPLEMENT_MODULE(FArcEcotopeEditorModule, ArcEcotopeEditor)
