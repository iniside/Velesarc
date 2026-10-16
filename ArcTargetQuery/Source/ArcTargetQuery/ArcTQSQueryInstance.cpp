// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSQueryInstance.h"
#include "ArcTQSStep.h"
#include "ArcTQSGenerator.h"
#include "ArcTQSPropertyFunction.h"
#include "ArcTQSQueryDefinition.h"
#include "ArcTargetQueryLogs.h"
#include "HAL/PlatformTime.h"
#include "Debugger/ArcTQSTrace.h"
#include "VisualLogger/VisualLogger.h"
#include "MassActorSubsystem.h"
#include "Mass/EntityManager.h"
#include "GameFramework/Actor.h"

#if !UE_BUILD_SHIPPING

// Get the display name for a step from its instanced struct
static FString GetStepName(const FInstancedStruct& StepStruct)
{
	if (const UScriptStruct* ScriptStruct = StepStruct.GetScriptStruct())
	{
		// Try DisplayName metadata first (what's set in USTRUCT(DisplayName = "..."))
#if WITH_METADATA
		const FString DisplayName = ScriptStruct->GetMetaData(TEXT("DisplayName"));
		if (!DisplayName.IsEmpty())
		{
			return DisplayName;
		}
#endif
		return ScriptStruct->GetName();
	}
	return TEXT("Unknown");
}

#endif // !UE_BUILD_SHIPPING

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
			if (bEmitTrace)
			{
				TRACE_ARCTQS_QUERY_COMPLETED(*this);
			}
			return true;
		}

#if ENABLE_VISUAL_LOG
		{
			FString GeneratorName = TEXT("Unknown");
			if (const UScriptStruct* GenStruct = Generator.GetScriptStruct())
			{
#if WITH_METADATA
				const FString DisplayName = GenStruct->GetMetaData(TEXT("DisplayName"));
				GeneratorName = DisplayName.IsEmpty() ? GenStruct->GetName() : DisplayName;
#else
				GeneratorName = GenStruct->GetName();
#endif
			}

			DebugLog += FString::Printf(TEXT("TQS Query %d: Generator [%s] produced %d items\n"),
				QueryId, *GeneratorName, Items.Num());
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

		if (Status == EArcTQSQueryStatus::Failed)
		{
			TotalExecutionTime += FPlatformTime::Seconds() - StepStart;
#if ENABLE_VISUAL_LOG
			DebugLog += FString::Printf(TEXT("TQS Query %d: step bindings could not be applied — query failed.\n"), QueryId);
			FlushDebugLog();
#endif
			if (bEmitTrace)
			{
				TRACE_ARCTQS_QUERY_COMPLETED(*this);
			}
			return true;
		}

		Status = EArcTQSQueryStatus::Selecting;
	}

	// Phase 3: Selection
	if (Status == EArcTQSQueryStatus::Selecting)
	{
		RunSelection();
		Status = EArcTQSQueryStatus::Completed;
		TotalExecutionTime += FPlatformTime::Seconds() - StepStart;
#if !UE_BUILD_SHIPPING
		TotalExecutionTime -= DebugCollectionOverhead;
#endif
		if (bEmitTrace)
		{
			TRACE_ARCTQS_QUERY_COMPLETED(*this);
		}

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
	ResultIndices.Empty();

#if ENABLE_VISUAL_LOG
	DebugLog.Reset();
#endif
}

const FArcTQSCompiledData* FArcTQSQueryInstance::GetCompiledData() const
{
	const UArcTQSQueryDefinition* QueryDefinition = Definition.Get();
	if (!QueryDefinition || !QueryDefinition->IsExecutable() || QueryDefinition->CompiledData.Revision != CompiledRevision)
	{
		return nullptr;
	}
	return &QueryDefinition->CompiledData;
}

FPropertyBindingDataView FArcTQSQueryInstance::GetSourceView(const FArcTQSDataHandle Handle)
{
	switch (Handle.GetKind())
	{
	case EArcTQSDataKind::Parameters:
		return Parameters.IsValid() ? FPropertyBindingDataView(Parameters.GetMutableValue()) : FPropertyBindingDataView();
	case EArcTQSDataKind::Querier:
		return FPropertyBindingDataView(FArcTQSQuerierInputs::StaticStruct(), &QuerierInputs);
	case EArcTQSDataKind::Item:
		return FPropertyBindingDataView(FArcTQSItemInputs::StaticStruct(), &ItemInputs);
	case EArcTQSDataKind::Function:
		return Functions.IsValidIndex(Handle.GetIndex()) ? FPropertyBindingDataView(Functions[Handle.GetIndex()]) : FPropertyBindingDataView();
	default:
		return FPropertyBindingDataView();
	}
}

bool FArcTQSQueryInstance::ApplyBindings(
	const FArcTQSCompiledData& CompiledData,
	const FPropertyBindingIndex16 BatchIndex,
	const FPropertyBindingDataView TargetView)
{
	if (!BatchIndex.IsValid())
	{
		return true;
	}
	const FPropertyBindingCopyInfoBatch& Batch = CompiledData.Bindings.GetBatch(BatchIndex);
	if (!TargetView.IsValid() || Batch.TargetStruct.Get().Struct != TargetView.GetStruct())
	{
		return false;
	}
	if (!EvaluateFunctions(CompiledData, Batch))
	{
		return false;
	}
	const TConstArrayView<FPropertyBindingCopyInfo> Copies = CompiledData.Bindings.GetBatchCopies(Batch);
	const int32 BindingsBegin = Batch.BindingsBegin.Get();
	if (Batch.BindingsEnd.Get() - BindingsBegin != Copies.Num())
	{
		return false;
	}
	for (int32 CopyIndex = 0; CopyIndex < Copies.Num(); ++CopyIndex)
	{
		const FArcTQSBinding& Binding = CompiledData.Bindings.GetBinding(BindingsBegin + CopyIndex);
		const FPropertyBindingDataView SourceView = GetSourceView(Binding.GetSourceDataHandle());
		if (!SourceView.IsValid() || !CompiledData.Bindings.CopyProperty(Copies[CopyIndex], SourceView, TargetView))
		{
			return false;
		}
	}
	return true;
}

bool FArcTQSQueryInstance::EvaluateFunctions(const FArcTQSCompiledData& CompiledData, const FPropertyBindingCopyInfoBatch& Batch)
{
	if (!Batch.PropertyFunctionsBegin.IsValid() || !Batch.PropertyFunctionsEnd.IsValid())
	{
		return true;
	}
	FArcTQSFunctionContext FunctionContext;
	FunctionContext.EntityManager = QueryContext.EntityManager;
	FunctionContext.World = QueryContext.World.Get();
	FunctionContext.Self = QueryContext.QuerierEntity;
	for (int32 FunctionIndex = Batch.PropertyFunctionsBegin.Get(); FunctionIndex < Batch.PropertyFunctionsEnd.Get(); ++FunctionIndex)
	{
		if (!CompiledData.Functions.IsValidIndex(FunctionIndex) || !Functions.IsValidIndex(FunctionIndex))
		{
			return false;
		}
		const FArcTQSCompiledFunction& CompiledFunction = CompiledData.Functions[FunctionIndex];
		FInstancedStruct& Function = Functions[FunctionIndex];
		const UScriptStruct* FunctionStruct = CompiledFunction.Function.GetScriptStruct();
		if (!FunctionStruct || Function.GetScriptStruct() != FunctionStruct)
		{
			return false;
		}
		FunctionStruct->CopyScriptStruct(Function.GetMutableMemory(), CompiledFunction.Function.GetMemory());
		if (!ApplyBindings(CompiledData, CompiledFunction.InputBatch, FPropertyBindingDataView(Function)))
		{
			return false;
		}
		Function.GetMutable<FArcTQSPropertyFunction>().Execute(FunctionContext);
	}
	return true;
}

void FArcTQSQueryInstance::FillItemInputs(const FArcTQSTargetItem& Item)
{
	ItemInputs.Location = Item.Location;
	ItemInputs.Entity = Item.EntityHandle;
}

void FArcTQSQueryInstance::RunGenerator()
{
	QueryContext.RandomStream = &RandomStream;
	QueryContext.UnavailableRequirements = UnavailableRequirements;

#if !UE_BUILD_SHIPPING
	StepResults.SetNum(Steps.Num());
	for (int32 StepIndex = 0; StepIndex < Steps.Num(); ++StepIndex)
	{
		FArcTQSStepResult& StepResult = StepResults[StepIndex];
		StepResult.StepName = GetStepName(Steps[StepIndex]);
		if (const FArcTQSStep* Step = Steps[StepIndex].GetPtr<FArcTQSStep>())
		{
			StepResult.StepType = Step->StepType;
		}
	}
#endif

	const FArcTQSCompiledData* CompiledDataPtr = GetCompiledData();
	const FArcTQSGenerator* Gen = Generator.GetPtr<FArcTQSGenerator>();
	if (!CompiledDataPtr || !Gen || CompiledDataPtr->Functions.Num() != Functions.Num())
	{
		Status = EArcTQSQueryStatus::Failed;
		return;
	}
	const FArcTQSCompiledData& CompiledData = *CompiledDataPtr;

	QuerierInputs.Entity = QueryContext.QuerierEntity;
	QuerierInputs.Location = QueryContext.QuerierLocation;
	QuerierInputs.Forward = QueryContext.QuerierForward;
	QuerierInputs.Actor = QueryContext.QuerierActor;

	if (!ApplyBindings(CompiledData, CompiledData.GeneratorBatch, FPropertyBindingDataView(Generator))
		|| CompiledData.StepBatches.Num() != Steps.Num()
		|| CompiledData.StepPerItemBatches.Num() != Steps.Num())
	{
		Status = EArcTQSQueryStatus::Failed;
		return;
	}
	for (int32 StepIndex = 0; StepIndex < Steps.Num(); ++StepIndex)
	{
		if (!ApplyBindings(CompiledData, CompiledData.StepBatches[StepIndex], FPropertyBindingDataView(Steps[StepIndex])))
		{
			Status = EArcTQSQueryStatus::Failed;
			return;
		}
	}

	if (EnumHasAnyFlags(Gen->GetWorldRequirements(), UnavailableRequirements))
	{
		bGeneratorLevelOnly = true;
		return;
	}

	Gen->GenerateItems(QueryContext, Items);

#if !UE_BUILD_SHIPPING
	FilteredByStep.Init(INDEX_NONE, Items.Num());
#endif
}

bool FArcTQSQueryInstance::RunProcessingSteps(double Deadline)
{
	const FArcTQSCompiledData* CompiledData = GetCompiledData();
	if (!CompiledData || CompiledData->StepPerItemBatches.Num() != Steps.Num())
	{
		Status = EArcTQSQueryStatus::Failed;
		return true;
	}
	while (CurrentStepIndex < Steps.Num())
	{
		FInstancedStruct& StepStruct = Steps[CurrentStepIndex];
		const FArcTQSStep* Step = StepStruct.GetPtr<FArcTQSStep>();
		const bool bSkipped = Step && EnumHasAnyFlags(Step->GetWorldRequirements(), UnavailableRequirements);
		if (!Step || bSkipped)
		{
#if !UE_BUILD_SHIPPING
			{
				const double DebugStart = FPlatformTime::Seconds();
				FArcTQSStepResult& StepResult = StepResults[CurrentStepIndex];
				StepResult.State = bSkipped ? EArcTQSStepState::Skipped : EArcTQSStepState::Null;
				StepResult.CumulativeScores.SetNumUninitialized(Items.Num());
				for (int32 ItemIdx = 0; ItemIdx < Items.Num(); ++ItemIdx)
				{
					StepResult.CumulativeScores[ItemIdx] = Items[ItemIdx].Score;
				}
				DebugCollectionOverhead += FPlatformTime::Seconds() - DebugStart;
			}
#endif

#if ENABLE_VISUAL_LOG
			DebugLog += FString::Printf(TEXT("Step %d [%s] %s\n"),
				CurrentStepIndex, *GetStepName(StepStruct), bSkipped ? TEXT("skipped: requires unavailable world services") : TEXT("is null"));
#endif
			++CurrentStepIndex;
			CurrentItemIndex = 0;
			continue;
		}
		const FPropertyBindingIndex16 ItemBatch = CompiledData->StepPerItemBatches[CurrentStepIndex];
		const FPropertyBindingDataView StepView(StepStruct);

#if ENABLE_VISUAL_LOG
		const FString StepName = GetStepName(Steps[CurrentStepIndex]);
		int32 FilteredCount = 0;
#endif

#if !UE_BUILD_SHIPPING
		FArcTQSStepResult& StepResult = StepResults[CurrentStepIndex];
		if (CurrentItemIndex == 0)
		{
			StepResult.RawScores.SetNumZeroed(Items.Num());
		}
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
				if (ItemBatch.IsValid())
				{
					FillItemInputs(Item);
					if (!ApplyBindings(*CompiledData, ItemBatch, StepView))
					{
						Status = EArcTQSQueryStatus::Failed;
						return true;
					}
				}
				const float RawScore = Step->ExecuteStep(Item, QueryContext);

#if !UE_BUILD_SHIPPING
				StepResult.RawScores[CurrentItemIndex] = RawScore;
#endif

				if (Step->StepType == EArcTQSStepType::Filter)
				{
					if (RawScore <= 0.0f)
					{
						Item.bValid = false;

#if !UE_BUILD_SHIPPING
						++StepResult.FilteredCount;
						FilteredByStep[CurrentItemIndex] = CurrentStepIndex;
#endif

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
					const float CurveScore = Step->EvaluateScore(RawScore);
					Item.Score *= CurveScore;

#if ENABLE_VISUAL_LOG
					DebugLog += FString::Printf(TEXT("  [%d] %s | Step %d [%s] raw=%.4f curved=%.4f => cumulative=%.4f\n"),
						CurrentItemIndex, *GetItemDescription(Item, QueryContext),
						CurrentStepIndex, *StepName,
						RawScore, CurveScore, Item.Score);
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
			DebugLog += FString::Printf(TEXT("Step %d [%s] (Score) — processed %d items\n"),
				CurrentStepIndex, *StepName, Items.Num());
		}
#endif

#if !UE_BUILD_SHIPPING
		{
			const double DebugStart = FPlatformTime::Seconds();
			StepResult.State = EArcTQSStepState::Executed;
			StepResult.CumulativeScores.SetNumUninitialized(Items.Num());
			for (int32 ItemIdx = 0; ItemIdx < Items.Num(); ++ItemIdx)
			{
				StepResult.CumulativeScores[ItemIdx] = Items[ItemIdx].Score;
			}
			DebugCollectionOverhead += FPlatformTime::Seconds() - DebugStart;
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

	const FArcTQSTargetItem* ItemsBegin = Items.GetData();
	const TFunction<void(const FArcTQSTargetItem*)> AddResult = [this, ItemsBegin](const FArcTQSTargetItem* Item)
		{
			Results.Add(*Item);
			ResultIndices.Add(static_cast<int32>(Item - ItemsBegin));
		};

	switch (SelectionMode)
	{
	case EArcTQSSelectionMode::HighestScore:
		AddResult(ValidItems[0]);
		break;

	case EArcTQSSelectionMode::TopN:
		{
			const int32 Count = FMath::Min(TopN, ValidItems.Num());
			Results.Reserve(Count);
			for (int32 i = 0; i < Count; ++i)
			{
				AddResult(ValidItems[i]);
			}
		}
		break;

	case EArcTQSSelectionMode::AllPassing:
		Results.Reserve(ValidItems.Num());
		for (const FArcTQSTargetItem* Item : ValidItems)
		{
			if (Item->Score >= MinPassingScore)
			{
				AddResult(Item);
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
				float Random = RandomStream.FRand() * TotalWeight;
				for (const FArcTQSTargetItem* Item : ValidItems)
				{
					Random -= Item->Score;
					if (Random <= 0.0f)
					{
						AddResult(Item);
						break;
					}
				}

				// Fallback in case of floating point issues
				if (Results.IsEmpty())
				{
					AddResult(ValidItems.Last());
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
			const int32 PickIndex = RandomStream.RandRange(0, PoolSize - 1);
			AddResult(ValidItems[PickIndex]);
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
				float Random = RandomStream.FRand() * TotalWeight;
				for (int32 i = 0; i < PoolSize; ++i)
				{
					Random -= ValidItems[i]->Score;
					if (Random <= 0.0f)
					{
						AddResult(ValidItems[i]);
						break;
					}
				}

				// Fallback
				if (Results.IsEmpty())
				{
					AddResult(ValidItems[PoolSize - 1]);
				}
			}
			else
			{
				// All zero scores — uniform pick
				AddResult(ValidItems[RandomStream.RandRange(0, PoolSize - 1)]);
			}
		}
		break;
	}
}
