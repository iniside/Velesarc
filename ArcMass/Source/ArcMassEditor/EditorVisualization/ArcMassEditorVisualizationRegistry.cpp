// Copyright Lukasz Baran. All Rights Reserved.

#include "EditorVisualization/ArcMassEditorVisualizationRegistry.h"

#include "EditorVisualization/ArcEditorEntityDrawContributor.h"
#include "Preview/ArcMassPreviewContributorInterface.h"

#include "Algo/BinarySearch.h"
#include "MassLogging.h"

FArcMassEditorVisualizationRegistry& FArcMassEditorVisualizationRegistry::Get()
{
	static FArcMassEditorVisualizationRegistry* Instance = new FArcMassEditorVisualizationRegistry();
	return *Instance;
}

void FArcMassEditorVisualizationRegistry::RegisterPreviewContributor(FName Id, EArcPreviewContext Contexts, FPreviewFactory Factory)
{
	if (Id.IsNone() || !Factory)
	{
		UE_LOGF(LogMass, Warning,"FArcMassEditorVisualizationRegistry: rejected a preview contributor registration with an empty Id or factory.");
		return;
	}

	// Sorted by Id so the contributor order is reproducible across editor runs.
	const int32 Index = Algo::LowerBoundBy(PreviewEntries, Id, &FPreviewEntry::Id, FNameLexicalLess());

	if (PreviewEntries.IsValidIndex(Index) && PreviewEntries[Index].Id == Id)
	{
		UE_LOGF(LogMass, Warning,"FArcMassEditorVisualizationRegistry: preview contributor '%ls' re-registered, the previous factory is replaced.", *Id.ToString());
		PreviewEntries[Index].Contexts = Contexts;
		PreviewEntries[Index].Factory = MoveTemp(Factory);
		return;
	}

	FPreviewEntry NewEntry;
	NewEntry.Id = Id;
	NewEntry.Contexts = Contexts;
	NewEntry.Factory = MoveTemp(Factory);
	PreviewEntries.Insert(MoveTemp(NewEntry), Index);
}

void FArcMassEditorVisualizationRegistry::UnregisterPreviewContributor(FName Id)
{
	const int32 Index = Algo::LowerBoundBy(PreviewEntries, Id, &FPreviewEntry::Id, FNameLexicalLess());
	if (!PreviewEntries.IsValidIndex(Index) || PreviewEntries[Index].Id != Id)
	{
		return;
	}

	if (OpenPreviewViewportCount > 0)
	{
		UE_LOGF(LogMass, Warning,"FArcMassEditorVisualizationRegistry: preview contributor '%ls' unregistered while %d preview viewport(s) still hold contributors it created.", *Id.ToString(), OpenPreviewViewportCount);
	}

	PreviewEntries.RemoveAt(Index);
}

void FArcMassEditorVisualizationRegistry::CreatePreviewContributors(EArcPreviewContext Context, TArray<TUniquePtr<IArcMassPreviewContributor>>& OutContributors) const
{
	OutContributors.Reset();

	for (const FPreviewEntry& Entry : PreviewEntries)
	{
		if (!EnumHasAnyFlags(Entry.Contexts, Context) || !Entry.Factory)
		{
			continue;
		}

		TUniquePtr<IArcMassPreviewContributor> Contributor = Entry.Factory();
		if (Contributor.IsValid())
		{
			OutContributors.Add(MoveTemp(Contributor));
		}
	}
}

void FArcMassEditorVisualizationRegistry::NotifyPreviewViewportOpened()
{
	++OpenPreviewViewportCount;
}

void FArcMassEditorVisualizationRegistry::NotifyPreviewViewportClosed()
{
	OpenPreviewViewportCount = FMath::Max(0, OpenPreviewViewportCount - 1);
}

void FArcMassEditorVisualizationRegistry::RegisterDrawContributor(FName Id, TSharedRef<IArcEditorEntityDrawContributor> Contributor)
{
	if (Id.IsNone())
	{
		UE_LOGF(LogMass, Warning,"FArcMassEditorVisualizationRegistry: rejected a draw contributor registration with an empty Id.");
		return;
	}

	const int32 Index = Algo::LowerBoundBy(DrawEntries, Id, &FDrawEntry::Id, FNameLexicalLess());

	if (DrawEntries.IsValidIndex(Index) && DrawEntries[Index].Id == Id)
	{
		UE_LOGF(LogMass, Warning,"FArcMassEditorVisualizationRegistry: draw contributor '%ls' re-registered, the previous instance is replaced.", *Id.ToString());
		DrawEntries[Index].Contributor = Contributor;
	}
	else
	{
		FDrawEntry NewEntry;
		NewEntry.Id = Id;
		NewEntry.Contributor = Contributor;
		DrawEntries.Insert(MoveTemp(NewEntry), Index);
	}

	DrawContributorsChangedDelegate.Broadcast();
}

void FArcMassEditorVisualizationRegistry::UnregisterDrawContributor(FName Id)
{
	const int32 Index = Algo::LowerBoundBy(DrawEntries, Id, &FDrawEntry::Id, FNameLexicalLess());
	if (!DrawEntries.IsValidIndex(Index) || DrawEntries[Index].Id != Id)
	{
		return;
	}

	DrawEntries.RemoveAt(Index);
	DrawContributorsChangedDelegate.Broadcast();
}

void FArcMassEditorVisualizationRegistry::GetDrawContributors(TArray<TSharedPtr<IArcEditorEntityDrawContributor>>& OutContributors) const
{
	OutContributors.Reset();
	OutContributors.Reserve(DrawEntries.Num());

	for (const FDrawEntry& Entry : DrawEntries)
	{
		OutContributors.Add(Entry.Contributor);
	}
}

FSimpleMulticastDelegate& FArcMassEditorVisualizationRegistry::OnDrawContributorsChanged()
{
	return DrawContributorsChangedDelegate;
}
