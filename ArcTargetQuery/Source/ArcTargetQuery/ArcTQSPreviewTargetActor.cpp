// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSPreviewTargetActor.h"

#include "ArcTQSPreviewTypes.h"
#include "Components/BillboardComponent.h"
#include "Engine/World.h"

AArcTQSPreviewTargetActor::AArcTQSPreviewTargetActor()
{
#if WITH_EDITORONLY_DATA
	UBillboardComponent* SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (SpriteComponent)
	{
		SpriteComponent->bIsScreenSizeScaled = true;
		SetRootComponent(SpriteComponent);
	}
#endif
}

void AArcTQSPreviewTargetActor::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	CreateEntity();
}

void AArcTQSPreviewTargetActor::UnregisterAllComponents(const bool bForReregister)
{
	DestroyEntity();
	Super::UnregisterAllComponents(bForReregister);
}

#if WITH_EDITOR
void AArcTQSPreviewTargetActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	DestroyEntity();
	if (HasActorRegisteredAllComponents())
	{
		CreateEntity();
	}
}

void AArcTQSPreviewTargetActor::PostEditMove(const bool bFinished)
{
	Super::PostEditMove(bFinished);
	PreviewEntities.Move(Entity, GetActorTransform());
	OnMoved.Broadcast(this);
}

void AArcTQSPreviewTargetActor::PostEditUndo()
{
	Super::PostEditUndo();
	DestroyEntity();
	if (IsValid(this) && HasActorRegisteredAllComponents())
	{
		CreateEntity();
	}
}
#endif

void AArcTQSPreviewTargetActor::CreateEntity()
{
	UWorld* World = GetWorld();
	if (Entity.IsSet() || !World || World->WorldType != EWorldType::Editor)
	{
		return;
	}

	FArcTQSPreviewMock Mock;
	Mock.Name = GetFName();
	Mock.Transform = GetActorTransform();
	Mock.OwnedTags = OwnedTags;
	Mock.SpatialIndex = SpatialIndex;
	Entity = PreviewEntities.Create(*World, Mock);
	OnMoved.Broadcast(this);
}

void AArcTQSPreviewTargetActor::DestroyEntity()
{
	if (!Entity.IsSet())
	{
		return;
	}
	PreviewEntities.Release();
	Entity.Reset();
	OnMoved.Broadcast(this);
}
