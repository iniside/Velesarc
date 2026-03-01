// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassStateTreeTypes.h"
#include "ArcAreaTypes.h"
#include "SmartObjectTypes.h"
#include "GameplayTagContainer.h"
#include "ArcAreaGetAreaInfoPropertyFunction.generated.h"

USTRUCT()
struct FArcAreaGetAreaInfoInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Output)
	FArcAreaHandle AreaHandle;

	UPROPERTY(EditAnywhere, Category = Output)
	FGameplayTagContainer AreaTags;

	UPROPERTY(EditAnywhere, Category = Output)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = Output)
	FSmartObjectHandle SmartObjectHandle;

	UPROPERTY(EditAnywhere, Category = Output)
	FText DisplayName;
};

/**
 * Extracts core area info (handle, tags, location, SmartObject handle, display name).
 */
USTRUCT(meta = (DisplayName = "Arc Get Area Info", Category = "Arc|Area|Self"))
struct ARCAREA_API FArcAreaGetAreaInfoPropertyFunction : public FMassStateTreePropertyFunctionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAreaGetAreaInfoInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual void Execute(FStateTreeExecutionContext& Context) const override;
};
