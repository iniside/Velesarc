// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorVisualization/ArcEditorEntityDrawContributor.h"

struct FArcEditorDrawCollector;

class FArcPlacedEntityCollisionDrawContributor : public IArcEditorEntityDrawContributor
{
public:
	virtual FName GetCategory() const override { return TEXT("Collision"); }
	virtual TSubclassOf<UMassEntityTraitBase> GetTraitClass() const override;
	virtual EArcDrawCategoryKind GetCategoryKind() const override { return EArcDrawCategoryKind::Detail; }
	virtual void CollectShapes(
		FArcEditorDrawCollector& Collector,
		const UMassEntityTraitBase* Trait,
		TConstArrayView<FTransform> Transforms,
		const FArcEditorDrawContext& Context) override;
};
