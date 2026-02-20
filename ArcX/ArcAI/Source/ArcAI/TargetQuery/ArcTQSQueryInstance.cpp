// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSQueryInstance.h"
#include "ArcTQSContextProvider.h"
#include "ArcTQSStep.h"
#include "ArcTQSGenerator.h"
#include "HAL/PlatformTime.h"

bool FArcTQSQueryInstance::ExecuteStep(double Deadline)
{
	const double StepStart = FPlatformTime::Seconds();

	if (Status == EArcTQSQueryStatus::Aborted)
	{
		return true;
	}

	// Phase 1: Generate items
	if (Status == EArcTQSQueryStatus::Pending)
	{
		Status = EArcTQSQueryStatus::Generating;
		RunGenerator();

		if (Items.Num() == 0)
		{
			Status = EArcTQSQueryStatus::Failed;
			TotalExecutionTime += FPlatformTime::Seconds() - StepStart;
			return true;
		}

		Status = EArcTQSQueryStatus::Processing;
		TotalExecutionTime += FPlatformTime::Seconds() - StepStart;

		// Check deadline after generation
		if (FPlatformTime::Seconds() >= Deadline)
		{
			return false;
		}
	}

	// Phase 2: Run filter/score steps
	if (Status == EArcTQSQueryStatus::Processing)
	{
		if (!RunProcessingSteps(Deadline))
		{
			TotalExecutionTime += FPlatformTime::Seconds() - StepStart;
			return false; // Yielded, will resume next frame
		}

		Status = EArcTQSQueryStatus::Selecting;
	}

	// Phase 3: Selection
	if (Status == EArcTQSQueryStatus::Selecting)
	{
		RunSelection();
		Status = EArcTQSQueryStatus::Completed;
		TotalExecutionTime += FPlatformTime::Seconds() - StepStart;
		return true;
	}

	return true;
}

void FArcTQSQueryInstance::Abort()
{
	Status = EArcTQSQueryStatus::Aborted;
	Items.Empty();
	Results.Empty();
}

void FArcTQSQueryInstance::RunGenerator()
{
	const FArcTQSGenerator* Gen = Generator.GetPtr<FArcTQSGenerator>();
	if (!Gen)
	{
		Status = EArcTQSQueryStatus::Failed;
		return;
	}

	// Run context provider to dynamically generate context locations (if configured)
	if (const FArcTQSContextProvider* Provider = ContextProvider.GetPtr<FArcTQSContextProvider>())
	{
		Provider->GenerateContextLocations(QueryContext, QueryContext.ContextLocations);
	}

	// Default: if no context locations were provided (neither explicit nor from provider),
	// use querier location as the sole center
	if (QueryContext.ContextLocations.IsEmpty())
	{
		QueryContext.ContextLocations.Add(QueryContext.QuerierLocation);
	}

	Gen->GenerateItems(QueryContext, Items);
}

bool FArcTQSQueryInstance::RunProcessingSteps(double Deadline)
{
	while (CurrentStepIndex < Steps.Num())
	{
		const FArcTQSStep* Step = Steps[CurrentStepIndex].GetPtr<FArcTQSStep>();
		if (!Step)
		{
			++CurrentStepIndex;
			CurrentItemIndex = 0;
			continue;
		}

		// Process items from CurrentItemIndex to end, or until deadline
		while (CurrentItemIndex < Items.Num())
		{
			// Check deadline every 16 items to amortize the syscall cost
			if ((CurrentItemIndex & 0xF) == 0 && CurrentItemIndex > 0 && FPlatformTime::Seconds() >= Deadline)
			{
				return false; // Yield — will resume at this step and item
			}

			FArcTQSTargetItem& Item = Items[CurrentItemIndex];
			if (Item.bValid)
			{
				const float RawScore = Step->ExecuteStep(Item, QueryContext);

				if (Step->StepType == EArcTQSStepType::Filter)
				{
					if (RawScore <= 0.0f)
					{
						Item.bValid = false;
					}
				}
				else
				{
					// Compensatory multiplicative: Pow(clamp(raw, 0, 1), Weight)
					const float ClampedScore = FMath::Clamp(RawScore, 0.0f, 1.0f);
					const float CompensatedScore = FMath::Pow(ClampedScore, Step->Weight);
					Item.Score *= CompensatedScore;
				}
			}
			++CurrentItemIndex;
		}

		// Step completed, move to next
		++CurrentStepIndex;
		CurrentItemIndex = 0;
	}

	return true; // All steps complete
}

void FArcTQSQueryInstance::RunSelection()
{
	// Collect valid items
	TArray<FArcTQSTargetItem*> ValidItems;
	ValidItems.Reserve(Items.Num());
	for (FArcTQSTargetItem& Item : Items)
	{
		if (Item.bValid && Item.Score > 0.0f)
		{
			ValidItems.Add(&Item);
		}
	}

	if (ValidItems.IsEmpty())
	{
		return;
	}

	// Sort by score descending
	Algo::Sort(ValidItems, [](const FArcTQSTargetItem* A, const FArcTQSTargetItem* B)
		{
			return A->Score > B->Score;
		});

	switch (SelectionMode)
	{
	case EArcTQSSelectionMode::HighestScore:
		Results.Add(*ValidItems[0]);
		break;

	case EArcTQSSelectionMode::TopN:
		{
			const int32 Count = FMath::Min(TopN, ValidItems.Num());
			Results.Reserve(Count);
			for (int32 i = 0; i < Count; ++i)
			{
				Results.Add(*ValidItems[i]);
			}
		}
		break;

	case EArcTQSSelectionMode::AllPassing:
		Results.Reserve(ValidItems.Num());
		for (const FArcTQSTargetItem* Item : ValidItems)
		{
			if (Item->Score >= MinPassingScore)
			{
				Results.Add(*Item);
			}
		}
		break;

	case EArcTQSSelectionMode::RandomWeighted:
		{
			// Weighted random selection — single pick
			float TotalWeight = 0.0f;
			for (const FArcTQSTargetItem* Item : ValidItems)
			{
				TotalWeight += Item->Score;
			}

			if (TotalWeight > 0.0f)
			{
				float Random = FMath::FRand() * TotalWeight;
				for (const FArcTQSTargetItem* Item : ValidItems)
				{
					Random -= Item->Score;
					if (Random <= 0.0f)
					{
						Results.Add(*Item);
						break;
					}
				}

				// Fallback in case of floating point issues
				if (Results.IsEmpty())
				{
					Results.Add(*ValidItems.Last());
				}
			}
		}
		break;
	}
}
