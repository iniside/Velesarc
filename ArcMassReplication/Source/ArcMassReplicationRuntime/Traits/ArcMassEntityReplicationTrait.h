// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "Fragments/ArcMassReplicationFilter.h"
#include "ArcMassEntityReplicationTrait.generated.h"

class UMassEntityConfigAsset;

UCLASS(BlueprintType, EditInlineNew, CollapseCategories,
	   meta = (DisplayName = "Arc Entity Replication", Category = "Replication"))
class ARCMASSREPLICATIONRUNTIME_API UArcMassEntityReplicationTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Replication")
	float CullDistance = 15000.f;

	UPROPERTY(EditAnywhere, Category = "Replication")
	float CellSize = 10000.f;

	UPROPERTY(EditAnywhere, Category = "Replication", meta = (AllowAbstract = "false"))
	TArray<FArcMassReplicatedFragmentEntry> ReplicatedFragments;

	UPROPERTY(EditAnywhere, Category = "Replication")
	TObjectPtr<UMassEntityConfigAsset> EntityConfigAsset = nullptr;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext,
							   const UWorld& World) const override;
};
