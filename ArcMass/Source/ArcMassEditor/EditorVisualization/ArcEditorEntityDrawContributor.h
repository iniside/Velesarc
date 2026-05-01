#pragma once

#include "CoreMinimal.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityTraitBase.h"

class UArcEditorEntityDrawComponent;

struct FArcEditorDrawContext
{
	UWorld* World = nullptr;

	struct FPlacedEntityInfo
	{
		const UMassEntityConfigAsset* Config = nullptr;
		TArray<FTransform> Transforms;
	};

	TArray<FPlacedEntityInfo> AllPlacedEntities;
};

class ARCMASSEDITOR_API IArcEditorEntityDrawContributor
{
public:
	virtual ~IArcEditorEntityDrawContributor() = default;

	virtual FName GetCategory() const = 0;

	virtual TSubclassOf<UMassEntityTraitBase> GetTraitClass() const = 0;

	virtual bool IsEnabledByDefault() const { return true; }

	virtual void CollectShapes(
		UArcEditorEntityDrawComponent& DrawComponent,
		const UMassEntityTraitBase* Trait,
		TConstArrayView<FTransform> Transforms,
		const FArcEditorDrawContext& Context) = 0;
};
