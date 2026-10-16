// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSTestingActor.h"
#include "ArcTQSPreviewTargetActor.h"
#include "ArcTQSPreviewTypes.h"
#include "ArcTQSQueryDefinition.h"
#include "ArcTQSQueryExecution.h"
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
	if (QueryInterval > 0.0f)
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - LastQueryTime >= static_cast<double>(QueryInterval))
		{
			RunQuery();
		}
	}
}

void AArcTQSTestingActor::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	CreateQuerierEntity();
	BindPreviewTargets();
#if WITH_EDITOR
	if (!DefinitionCompiledHandle.IsValid())
	{
		DefinitionCompiledHandle = ArcTQS::Delegates::OnPostCompile.AddUObject(this, &AArcTQSTestingActor::HandleDefinitionCompiled);
	}
#endif
	if (QuerierEntity.IsSet())
	{
		RunQuery();
	}
}

void AArcTQSTestingActor::UnregisterAllComponents(const bool bForReregister)
{
#if WITH_EDITOR
	ArcTQS::Delegates::OnPostCompile.Remove(DefinitionCompiledHandle);
	DefinitionCompiledHandle.Reset();
#endif
	UnbindPreviewTargets();
	DestroyQuerierEntity();
	Super::UnregisterAllComponents(bForReregister);
}

#if WITH_EDITOR
void AArcTQSTestingActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (GetWorld())
	{
		BindPreviewTargets();
		RunQuery();
	}
}

void AArcTQSTestingActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	PreviewEntities.Move(QuerierEntity, GetActorTransform());
	RunQuery();
}

void AArcTQSTestingActor::PostEditUndo()
{
	Super::PostEditUndo();
	DestroyQuerierEntity();
	if (IsValid(this) && HasActorRegisteredAllComponents())
	{
		CreateQuerierEntity();
		BindPreviewTargets();
		RunQuery();
	}
}

void AArcTQSTestingActor::HandleDefinitionCompiled(const UArcTQSQueryDefinition& Definition)
{
	if (Query.GetDefinition() != &Definition)
	{
		return;
	}
	Query.SyncParameters();
	RunQuery();
}
#endif

void AArcTQSTestingActor::RunQuery()
{
	if (!GetWorld() || !Query.IsValid())
	{
		return;
	}

	TMap<FGuid, FMassEntityHandle> EntityParameters;
	for (const TPair<FGuid, TObjectPtr<AArcTQSPreviewTargetActor>>& Source : EntityParameterSources)
	{
		if (Source.Value && Source.Value->GetEntity().IsSet())
		{
			EntityParameters.Add(Source.Key, Source.Value->GetEntity());
		}
	}
	const FArcTQSQueryReference RunReference = ArcTQS::MakeReferenceWithEntityParameters(Query, EntityParameters);

	FArcTQSRunOptions Options;
	Options.Seed = FMath::Rand();

	LastQueryTime = FPlatformTime::Seconds();
	bHasData = ArcTQS::RunQuerySync(RunReference, BuildQueryContext(), Options, LastResult) == EArcTQSCreateFailure::None;

	GeneratedCount = LastResult.TotalGenerated;
	ValidCount = LastResult.TotalValid;
	ResultCount = LastResult.ResultIndices.Num();
	LastExecutionTimeMs = static_cast<float>(LastResult.ExecutionTimeMs);

	UpdateDrawing();
}

void AArcTQSTestingActor::CreateQuerierEntity()
{
	UWorld* World = GetWorld();
	if (QuerierEntity.IsSet() || !World || World->WorldType != EWorldType::Editor)
	{
		return;
	}

	FArcTQSPreviewMock Mock;
	Mock.Name = GetFName();
	Mock.Transform = GetActorTransform();
	QuerierEntity = PreviewEntities.Create(*World, Mock);
}

void AArcTQSTestingActor::DestroyQuerierEntity()
{
	PreviewEntities.Release();
	QuerierEntity.Reset();
}

void AArcTQSTestingActor::CollectPreviewTargets(TArray<AArcTQSPreviewTargetActor*>& OutTargets) const
{
	for (AArcTQSPreviewTargetActor* Target : Targets)
	{
		if (IsValid(Target))
		{
			OutTargets.AddUnique(Target);
		}
	}
	for (const TPair<FGuid, TObjectPtr<AArcTQSPreviewTargetActor>>& Source : EntityParameterSources)
	{
		if (IsValid(Source.Value))
		{
			OutTargets.AddUnique(Source.Value.Get());
		}
	}
}

void AArcTQSTestingActor::BindPreviewTargets()
{
	UnbindPreviewTargets();

	TArray<AArcTQSPreviewTargetActor*> PreviewTargets;
	CollectPreviewTargets(PreviewTargets);
	for (AArcTQSPreviewTargetActor* Target : PreviewTargets)
	{
		Target->OnMoved.AddUObject(this, &AArcTQSTestingActor::HandlePreviewTargetMoved);
		BoundTargets.Add(Target);
	}
}

void AArcTQSTestingActor::UnbindPreviewTargets()
{
	for (const TWeakObjectPtr<AArcTQSPreviewTargetActor>& Target : BoundTargets)
	{
		if (AArcTQSPreviewTargetActor* BoundTarget = Target.Get())
		{
			BoundTarget->OnMoved.RemoveAll(this);
		}
	}
	BoundTargets.Reset();
}

void AArcTQSTestingActor::HandlePreviewTargetMoved(AArcTQSPreviewTargetActor* Target)
{
	if (!HasActorRegisteredAllComponents())
	{
		return;
	}
	RunQuery();
}

FArcTQSQueryContext AArcTQSTestingActor::BuildQueryContext() const
{
	FArcTQSQueryContext Ctx;
	Ctx.QuerierLocation = GetActorLocation();
	Ctx.QuerierForward = GetActorForwardVector();
	Ctx.World = GetWorld();
	Ctx.QuerierActor = const_cast<AArcTQSTestingActor*>(this);
	Ctx.QuerierEntity = QuerierEntity;

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

void AArcTQSTestingActor::UpdateDrawing()
{
#if WITH_EDITORONLY_DATA
	if (!RenderingComponent)
	{
		return;
	}
	if (bHasData)
	{
		TArray<AArcTQSPreviewTargetActor*> PreviewTargets;
		CollectPreviewTargets(PreviewTargets);
		TArray<FArcTQSRenderMarker> Markers;
		Markers.Reserve(PreviewTargets.Num());
		for (const AArcTQSPreviewTargetActor* Target : PreviewTargets)
		{
			Markers.Add({Target->GetActorNameOrLabel(), Target->GetActorLocation()});
		}
		RenderingComponent->UpdateQueryData(LastResult, Markers, bDrawLabels, bDrawFilteredItems);
	}
	else
	{
		RenderingComponent->ClearQueryData();
	}
#endif
}
