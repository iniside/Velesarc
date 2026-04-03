// Copyright Lukasz Baran. All Rights Reserved.

#include "Preview/ArcNiagaraVisPreviewContributor.h"

#include "AdvancedPreviewScene.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "ArcMass/NiagaraVisualization/ArcNiagaraVisTrait.h"

bool FArcNiagaraVisPreviewContributor::CanContribute(const UMassEntityTraitBase* Trait) const
{
	const UArcNiagaraVisTrait* NiagaraTrait = Cast<UArcNiagaraVisTrait>(Trait);
	return NiagaraTrait && NiagaraTrait->IsValid();
}

void FArcNiagaraVisPreviewContributor::Apply(const UMassEntityTraitBase* Trait, FAdvancedPreviewScene& Scene, AActor* PreviewActor)
{
	CachedTrait = const_cast<UArcNiagaraVisTrait*>(CastChecked<UArcNiagaraVisTrait>(Trait));

	UNiagaraSystem* System = CachedTrait->GetNiagaraConfig().NiagaraSystem.LoadSynchronous();
	if (!System)
	{
		return;
	}

	if (!PreviewComponent)
	{
		PreviewComponent = NewObject<UNiagaraComponent>(PreviewActor);
		PreviewActor->AddOwnedComponent(PreviewComponent);
		PreviewComponent->RegisterComponent();
		Scene.AddComponent(PreviewComponent, CachedTrait->GetComponentTransform());
	}

	PreviewComponent->SetAsset(System);
	PreviewComponent->SetRelativeTransform(CachedTrait->GetComponentTransform());
	PreviewComponent->SetForceSolo(true);
	PreviewComponent->Activate(true);
}

void FArcNiagaraVisPreviewContributor::Clear(FAdvancedPreviewScene& Scene)
{
	if (PreviewComponent)
	{
		PreviewComponent->Deactivate();
		Scene.RemoveComponent(PreviewComponent);
		PreviewComponent = nullptr;
	}
	CachedTrait = nullptr;
}

FBox FArcNiagaraVisPreviewContributor::GetBounds() const
{
	if (PreviewComponent)
	{
		return PreviewComponent->Bounds.GetBox();
	}
	return FBox(ForceInit);
}

int32 FArcNiagaraVisPreviewContributor::GetSelectableCount() const
{
	return (PreviewComponent && CachedTrait) ? 1 : 0;
}

FTransform FArcNiagaraVisPreviewContributor::GetSelectableTransform(int32 Index) const
{
	if (Index == 0 && PreviewComponent)
	{
		return PreviewComponent->GetRelativeTransform();
	}
	return FTransform::Identity;
}

void FArcNiagaraVisPreviewContributor::SetSelectableTransform(int32 Index, const FTransform& NewTransform)
{
	if (Index == 0 && CachedTrait && PreviewComponent)
	{
		CachedTrait->SetComponentTransform(NewTransform);
		CachedTrait->MarkPackageDirty();
		PreviewComponent->SetRelativeTransform(NewTransform);
		PreviewComponent->MarkRenderStateDirty();
	}
}

UObject* FArcNiagaraVisPreviewContributor::GetMutableTraitObject() const
{
	return CachedTrait;
}

UPrimitiveComponent* FArcNiagaraVisPreviewContributor::GetPreviewPrimitive(int32 Index) const
{
	return (Index == 0) ? PreviewComponent.Get() : nullptr;
}
