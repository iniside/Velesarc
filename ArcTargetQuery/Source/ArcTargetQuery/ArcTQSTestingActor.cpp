// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSTestingActor.h"
#include "ArcTQSQueryDefinition.h"
#include "ArcTQSQueryInstance.h"
#include "ArcTQSQuerySubsystem.h"
#include "ArcTQSRenderingComponent.h"
#include "MassEntitySubsystem.h"
#include "HAL/PlatformTime.h"
#include "Components/BillboardComponent.h"
#include "Engine/World.h"


AArcTQSTestingActor::AArcTQSTestingActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

#if WITH_EDITORONLY_DATA
	// Add a billboard sprite so the actor is visible and selectable in the editor
	UBillboardComponent* SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (SpriteComponent)
	{
		SpriteComponent->bIsScreenSizeScaled = true;
		SetRootComponent(SpriteComponent);
	}

	// Create the debug rendering component
	RenderingComponent = CreateEditorOnlyDefaultSubobject<UArcTQSRenderingComponent>(TEXT("TQSRender"));
	if (RenderingComponent)
	{
		RenderingComponent->SetupAttachment(RootComponent);
	}
#endif
}

void AArcTQSTestingActor::BeginPlay()
{
	Super::BeginPlay();
	RunQuery();
}

void AArcTQSTestingActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Auto re-query on interval
	if (QueryInterval > 0.0f && ActiveQueryId == INDEX_NONE)
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - LastQueryTime >= static_cast<double>(QueryInterval))
		{
			RunQuery();
		}
	}
}

#if WITH_EDITOR
void AArcTQSTestingActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Re-run the query when any property changes
	if (GetWorld())
	{
		AbortActiveQuery();
		RunQuery();
	}
}

void AArcTQSTestingActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	if (bFinished)
	{
		AbortActiveQuery();
		RunQuery();
	}
}
#endif

void AArcTQSTestingActor::RunQuery()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UArcTQSQuerySubsystem* TQSSubsystem = World->GetSubsystem<UArcTQSQuerySubsystem>();
	if (!TQSSubsystem)
	{
		return;
	}

	AbortActiveQuery();

	FArcTQSQueryContext QueryContext = BuildQueryContext();

	int32 NewQueryId = INDEX_NONE;

	if (bUseInlineDefinition)
	{
		if (!InlineGenerator.IsValid())
		{
			return;
		}

		// Copy inline data — the subsystem takes ownership via move
		FInstancedStruct ContextProviderCopy = InlineContextProvider;
		FInstancedStruct GeneratorCopy = InlineGenerator;
		TArray<FInstancedStruct> StepsCopy = InlineSteps;

		NewQueryId = TQSSubsystem->RunQuery(
			MoveTemp(ContextProviderCopy),
			MoveTemp(GeneratorCopy),
			MoveTemp(StepsCopy),
			InlineSelectionMode,
			InlineTopN,
			InlineMinPassingScore,
			InlineTopPercent,
			QueryContext,
			FArcTQSQueryFinished::CreateUObject(this, &AArcTQSTestingActor::OnQueryCompleted));
	}
	else
	{
		if (!QueryDefinition)
		{
			return;
		}

		NewQueryId = TQSSubsystem->RunQuery(
			QueryDefinition.Get(),
			QueryContext,
			FArcTQSQueryFinished::CreateUObject(this, &AArcTQSTestingActor::OnQueryCompleted));
	}

	if (NewQueryId != INDEX_NONE)
	{
		ActiveQueryId = NewQueryId;
		LastQueryTime = FPlatformTime::Seconds();
	}
}

FArcTQSQueryContext AArcTQSTestingActor::BuildQueryContext() const
{
	FArcTQSQueryContext Ctx;
	Ctx.QuerierLocation = GetActorLocation();
	Ctx.QuerierForward = GetActorForwardVector();
	Ctx.World = GetWorld();
	Ctx.QuerierActor = const_cast<AArcTQSTestingActor*>(this);

	// Provide entity manager so mass entity generators/steps work
	if (UWorld* World = GetWorld())
	{
		if (UMassEntitySubsystem* MES = World->GetSubsystem<UMassEntitySubsystem>())
		{
			Ctx.EntityManager = &MES->GetMutableEntityManager();
		}
	}

	return Ctx;
}

void AArcTQSTestingActor::OnQueryCompleted(FArcTQSQueryInstance& CompletedQuery)
{
	ActiveQueryId = INDEX_NONE;

	// Cache results for drawing
	CachedData.AllItems = CompletedQuery.Items;
	CachedData.Results = CompletedQuery.Results;
	CachedData.QuerierLocation = CompletedQuery.QueryContext.QuerierLocation;
	CachedData.ContextLocations = CompletedQuery.QueryContext.ContextLocations;
	CachedData.ExecutionTimeMs = CompletedQuery.TotalExecutionTime * 1000.0;
	CachedData.Status = CompletedQuery.Status;
	CachedData.SelectionMode = CompletedQuery.SelectionMode;
	CachedData.TotalGenerated = CompletedQuery.Items.Num();
	CachedData.Timestamp = FPlatformTime::Seconds();

	int32 NumValid = 0;
	for (const FArcTQSTargetItem& Item : CompletedQuery.Items)
	{
		if (Item.bValid)
		{
			++NumValid;
		}
	}
	CachedData.TotalValid = NumValid;

	// Update status properties
	GeneratedCount = CachedData.TotalGenerated;
	ValidCount = CachedData.TotalValid;
	ResultCount = CachedData.Results.Num();
	LastExecutionTimeMs = static_cast<float>(CachedData.ExecutionTimeMs);
	ApproximateQueryTimeMs = FPlatformTime::Seconds() - LastQueryTime;
	
	bHasData = true;
	UpdateDrawing();
}

void AArcTQSTestingActor::AbortActiveQuery()
{
	if (ActiveQueryId != INDEX_NONE)
	{
		if (UWorld* World = GetWorld())
		{
			if (UArcTQSQuerySubsystem* TQSSubsystem = World->GetSubsystem<UArcTQSQuerySubsystem>())
			{
				TQSSubsystem->AbortQuery(ActiveQueryId);
			}
		}
		ActiveQueryId = INDEX_NONE;
	}
}

void AArcTQSTestingActor::UpdateDrawing()
{
#if WITH_EDITORONLY_DATA
	if (RenderingComponent && bHasData)
	{
		RenderingComponent->UpdateQueryData(CachedData, bDrawLabels, bDrawFilteredItems);

//#if WITH_EDITOR
//		if (GEditor)
//		{
//			GEditor->RedrawLevelEditingViewports();
//		}
//#endif
	}
#endif
}
