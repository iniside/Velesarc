// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSQueryInstance.h"
#include "ArcTQSContextProvider.h"
#include "ArcTQSStep.h"
#include "ArcTQSGenerator.h"
#include "ArcAILogs.h"
#include "HAL/PlatformTime.h"
#include "VisualLogger/VisualLogger.h"
#include "MassActorSubsystem.h"
#include "MassEntityManager.h"
#include "GameFramework/Actor.h"

#if ENABLE_VISUAL_LOG

// Get a readable name for a target item (entity index, actor name, or location)
static FString GetItemDescription(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext)
{
	switch (Item.TargetType)
	{
	case EArcTQSTargetType::MassEntity:
		{
			FString Desc = FString::Printf(TEXT("Entity[%d:%d]"), Item.EntityHandle.Index, Item.EntityHandle.SerialNumber);
			if (QueryContext.EntityManager && QueryContext.EntityManager->IsEntityValid(Item.EntityHandle))
			{
				if (const FMassActorFragment* ActorFrag = QueryContext.EntityManager->GetFragmentDataPtr<FMassActorFragment>(Item.EntityHandle))
				{
					if (const AActor* Actor = ActorFrag->Get())
					{
						Desc += FString::Printf(TEXT(" (%s)"), *Actor->GetName());
					}
				}
			}
			return Desc;
		}

	case EArcTQSTargetType::Actor:
		if (const AActor* Actor = Item.Actor.Get())
		{
			return FString::Printf(TEXT("Actor(%s)"), *Actor->GetName());
		}
		return TEXT("Actor(null)");

	case EArcTQSTargetType::SmartObject:
		return FString::Printf(TEXT("SmartObject(%.0f, %.0f, %.0f)"), Item.Location.X, Item.Location.Y, Item.Location.Z);

	case EArcTQSTargetType::Location:
		return FString::Printf(TEXT("Location(%.0f, %.0f, %.0f)"), Item.Location.X, Item.Location.Y, Item.Location.Z);

	default:
		return FString::Printf(TEXT("Item(%.0f, %.0f, %.0f)"), Item.Location.X, Item.Location.Y, Item.Location.Z);
	}
}

// Get the display name for a step from its instanced struct
static FString GetStepName(const FInstancedStruct& StepStruct)
{
	if (const UScriptStruct* ScriptStruct = StepStruct.GetScriptStruct())
	{
		// Try DisplayName metadata first (what's set in USTRUCT(DisplayName = "..."))
		const FString DisplayName = ScriptStruct->GetMetaData(TEXT("DisplayName"));
		if (!DisplayName.IsEmpty())
		{
			return DisplayName;
		}
		return ScriptStruct->GetName();
	}
	return TEXT("Unknown");
}

void FArcTQSQueryInstance::FlushDebugLog()
{
	if (DebugLog.IsEmpty())
	{
		return;
	}

	if (const AActor* LogOwner = QueryContext.QuerierActor.Get())
	{
		UE_VLOG(LogOwner, LogArcTQS, Log, TEXT("%s"), *DebugLog);
	}

	DebugLog.Reset();
}

#endif // ENABLE_VISUAL_LOG

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

#if ENABLE_VISUAL_LOG
			DebugLog += FString::Printf(TEXT("TQS Query %d: Generator produced 0 items — query failed.\n"), QueryId);
			FlushDebugLog();
#endif
			return true;
		}

#if ENABLE_VISUAL_LOG
		{
			FString GeneratorName = TEXT("Unknown");
			if (const UScriptStruct* GenStruct = Generator.GetScriptStruct())
			{
				const FString DisplayName = GenStruct->GetMetaData(TEXT("DisplayName"));
				GeneratorName = DisplayName.IsEmpty() ? GenStruct->GetName() : DisplayName;
			}

			DebugLog += FString::Printf(TEXT("TQS Query %d: Generator [%s] produced %d items (%d context locations)\n"),
				QueryId, *GeneratorName, Items.Num(), QueryContext.ContextLocations.Num());
		}
#endif

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

#if ENABLE_VISUAL_LOG
		DebugLog += FString::Printf(TEXT("Completed in %.3fms — %d results selected (mode: %s)\n"),
			TotalExecutionTime * 1000.0,
			Results.Num(),
			*UEnum::GetValueAsString(SelectionMode));

		for (int32 i = 0; i < Results.Num(); ++i)
		{
			DebugLog += FString::Printf(TEXT("  Result[%d]: %s  FinalScore=%.4f\n"),
				i, *GetItemDescription(Results[i], QueryContext), Results[i].Score);
		}

		FlushDebugLog();
#endif
		return true;
	}

	return true;
}

void FArcTQSQueryInstance::Abort()
{
	Status = EArcTQSQueryStatus::Aborted;
	Items.Empty();
	Results.Empty();

#if ENABLE_VISUAL_LOG
	DebugLog.Reset();
#endif
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

#if ENABLE_VISUAL_LOG
		const FString StepName = GetStepName(Steps[CurrentStepIndex]);
		int32 FilteredCount = 0;
#endif

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

#if ENABLE_VISUAL_LOG
						++FilteredCount;
						DebugLog += FString::Printf(TEXT("  [%d] %s | Step %d [%s] FILTERED OUT (raw=%.4f)\n"),
							CurrentItemIndex, *GetItemDescription(Item, QueryContext),
							CurrentStepIndex, *StepName, RawScore);
#endif
					}
				}
				else
				{
					// Compensatory multiplicative: Pow(clamp(raw, 0, 1), Weight)
					const float ClampedScore = FMath::Clamp(RawScore, 0.0f, 1.0f);
					const float CompensatedScore = FMath::Pow(ClampedScore, Step->Weight);
					Item.Score *= CompensatedScore;

#if ENABLE_VISUAL_LOG
					DebugLog += FString::Printf(TEXT("  [%d] %s | Step %d [%s] raw=%.4f comp=%.4f (w=%.2f) => cumulative=%.4f\n"),
						CurrentItemIndex, *GetItemDescription(Item, QueryContext),
						CurrentStepIndex, *StepName,
						RawScore, CompensatedScore, Step->Weight, Item.Score);
#endif
				}
			}
			++CurrentItemIndex;
		}

#if ENABLE_VISUAL_LOG
		if (Step->StepType == EArcTQSStepType::Filter)
		{
			DebugLog += FString::Printf(TEXT("Step %d [%s] (Filter) — %d items filtered out\n"),
				CurrentStepIndex, *StepName, FilteredCount);
		}
		else
		{
			DebugLog += FString::Printf(TEXT("Step %d [%s] (Score, weight=%.2f) — processed %d items\n"),
				CurrentStepIndex, *StepName, Step->Weight, Items.Num());
		}
#endif

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
#if ENABLE_VISUAL_LOG
		DebugLog += FString::Printf(TEXT("No valid items remaining after scoring — 0 results.\n"));
#endif
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
			// Weighted random selection — single pick from ALL valid items
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

	case EArcTQSSelectionMode::RandomFromTop5Percent:
	case EArcTQSSelectionMode::RandomFromTop25Percent:
	case EArcTQSSelectionMode::RandomFromTopPercent:
		{
			// Determine the percentage to use
			float Percent;
			if (SelectionMode == EArcTQSSelectionMode::RandomFromTop5Percent)
			{
				Percent = 5.0f;
			}
			else if (SelectionMode == EArcTQSSelectionMode::RandomFromTop25Percent)
			{
				Percent = 25.0f;
			}
			else
			{
				Percent = FMath::Clamp(TopPercent, 1.0f, 100.0f);
			}

			// Already sorted descending — take top Percent% (at least 1)
			const int32 PoolSize = FMath::Max(1, FMath::CeilToInt(ValidItems.Num() * Percent / 100.0f));
			const int32 PickIndex = FMath::RandRange(0, PoolSize - 1);
			Results.Add(*ValidItems[PickIndex]);
		}
		break;

	case EArcTQSSelectionMode::WeightedRandomFromTop5Percent:
	case EArcTQSSelectionMode::WeightedRandomFromTop25Percent:
	case EArcTQSSelectionMode::WeightedRandomFromTopPercent:
		{
			// Determine the percentage to use
			float Percent;
			if (SelectionMode == EArcTQSSelectionMode::WeightedRandomFromTop5Percent)
			{
				Percent = 5.0f;
			}
			else if (SelectionMode == EArcTQSSelectionMode::WeightedRandomFromTop25Percent)
			{
				Percent = 25.0f;
			}
			else
			{
				Percent = FMath::Clamp(TopPercent, 1.0f, 100.0f);
			}

			// Already sorted descending — take top Percent% (at least 1)
			const int32 PoolSize = FMath::Max(1, FMath::CeilToInt(ValidItems.Num() * Percent / 100.0f));

			// Weighted random within the top pool — higher scores more likely
			float TotalWeight = 0.0f;
			for (int32 i = 0; i < PoolSize; ++i)
			{
				TotalWeight += ValidItems[i]->Score;
			}

			if (TotalWeight > 0.0f)
			{
				float Random = FMath::FRand() * TotalWeight;
				for (int32 i = 0; i < PoolSize; ++i)
				{
					Random -= ValidItems[i]->Score;
					if (Random <= 0.0f)
					{
						Results.Add(*ValidItems[i]);
						break;
					}
				}

				// Fallback
				if (Results.IsEmpty())
				{
					Results.Add(*ValidItems[PoolSize - 1]);
				}
			}
			else
			{
				// All zero scores — uniform pick
				Results.Add(*ValidItems[FMath::RandRange(0, PoolSize - 1)]);
			}
		}
		break;
	}
}
