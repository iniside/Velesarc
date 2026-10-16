// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorVisualization/ArcEditorEntityDrawContributor.h"

struct FArcEditorDrawCollector;

class FArcWorkAreaBoundsDrawContributor : public IArcEditorEntityDrawContributor
{
public:
	virtual FName GetCategory() const override { return TEXT("WorkArea"); }
	virtual TSubclassOf<UMassEntityTraitBase> GetTraitClass() const override;
	virtual EArcDrawCategoryKind GetCategoryKind() const override { return EArcDrawCategoryKind::Representation; }
	virtual void CollectShapes(
		FArcEditorDrawCollector& Collector,
		const UMassEntityTraitBase* Trait,
		TConstArrayView<FTransform> Transforms,
		const FArcEditorDrawContext& Context) override;
};
