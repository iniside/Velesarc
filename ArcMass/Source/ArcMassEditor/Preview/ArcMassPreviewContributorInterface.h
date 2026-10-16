// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HitProxies.h"
#include "UObject/ObjectPtr.h"

class AActor;
class FPreviewScene;
class FPrimitiveDrawInterface;
class UMassEntityConfigAsset;
class UMassEntityTraitBase;
class UPrimitiveComponent;

// StaticGetType() must be exported for HHitProxy::IsA()/StaticGetType() to link across the DLL boundary.
struct HArcMassPreviewHitProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(ARCMASSEDITOR_API);

	int32 ContributorIndex;
	int32 ElementIndex;

	HArcMassPreviewHitProxy(int32 InContributorIndex, int32 InElementIndex)
		: HHitProxy(HPP_Foreground)
		, ContributorIndex(InContributorIndex)
		, ElementIndex(InElementIndex)
	{}
};

class ARCMASSEDITOR_API IArcMassPreviewContributor
{
public:
	virtual ~IArcMassPreviewContributor() = default;
	virtual bool CanContribute(const UMassEntityTraitBase* Trait) const = 0;
	virtual void Apply(const UMassEntityTraitBase* Trait, FPreviewScene& Scene, AActor* PreviewActor, UMassEntityConfigAsset* InOwningConfig) = 0;
	virtual void Clear(FPreviewScene& Scene) = 0;
	virtual void Draw(FPrimitiveDrawInterface* PDI, int32 ContributorIndex) {}
	virtual FBox GetBounds() const { return FBox(ForceInit); }
	virtual FTransform GetSelectableTransform(int32 Index) const { return FTransform::Identity; }
	virtual void SetSelectableTransform(int32 Index, const FTransform& NewTransform) {}
	virtual UObject* GetMutableTraitObject() const { return nullptr; }
	virtual void SetSelectionHighlight(int32 Index) {}
	virtual int32 GetPreviewPrimitiveCount() const { return 0; }
	virtual UPrimitiveComponent* GetPreviewPrimitive(int32 Index) const { return nullptr; }

	void SetEditedConfig(UMassEntityConfigAsset* InEditedConfig) { EditedConfig = InEditedConfig; }

	// Traits resolved from a parent config are drawn but never selectable: the gizmo would write into the parent asset.
	bool IsTraitEditable() const { return OwningConfig != nullptr && OwningConfig == EditedConfig; }

protected:
	TObjectPtr<UMassEntityConfigAsset> OwningConfig = nullptr;
	TObjectPtr<UMassEntityConfigAsset> EditedConfig = nullptr;
};

namespace ArcMassPreview
{
	ARCMASSEDITOR_API void ApplyConfigToContributors(
		const UMassEntityConfigAsset& Config,
		TArray<TUniquePtr<IArcMassPreviewContributor>>& Contributors,
		FPreviewScene& Scene,
		AActor* PreviewActor);
}
