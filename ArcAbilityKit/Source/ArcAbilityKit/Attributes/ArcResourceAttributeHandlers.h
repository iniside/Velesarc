// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Attributes/ArcAttribute.h"
#include "Attributes/ArcAttributeHandlerConfig.h"

#include "ArcResourceAttributeHandlers.generated.h"

UENUM()
enum class EArcResourceOp : uint8
{
	Add UMETA(ToolTip = "Adds a positive meta value to the resource. Zero or negative meta values are discarded."),
	Subtract UMETA(ToolTip = "Subtracts a positive meta value from the resource. Zero or negative meta values are discarded."),
	AddSigned UMETA(ToolTip = "Adds the meta value to the resource, keeping its sign.")
};

USTRUCT()
struct ARCABILITYKIT_API FArcResourceChangedPayload
{
	GENERATED_BODY()

	UPROPERTY()
	FArcAttributeRef Resource;

	UPROPERTY()
	float OldValue = 0.f;

	UPROPERTY()
	float NewValue = 0.f;

	UPROPERTY()
	float Requested = 0.f;
};

USTRUCT()
struct ARCABILITYKIT_API FArcHealthChangedPayload
{
	GENERATED_BODY()

	UPROPERTY()
	float OldValue = 0.f;

	UPROPERTY()
	float NewValue = 0.f;

	UPROPERTY()
	float Requested = 0.f;

	UPROPERTY()
	FVector TargetLocation = FVector::ZeroVector;
};

USTRUCT(meta = (ToolTip = "In PostExecute, applies the meta value to Resource using Op, clamps the result to [MinConst, upper bound], writes it with SetAttributeDirectAndNotify and resets the meta to zero. The upper bound is the smaller of MaxAttribute and MaxConst; with neither set the result has no upper bound. When EventTag is set, queues an FArcGameEvent with an FArcResourceChangedPayload and Magnitude = new minus old resource value after every write.\nAssign to the meta attribute. Target: the meta attribute's set and the set owning Resource (and MaxAttribute when set). The meta is still reset when the Resource set is missing.\nSource: no required attribute sets; the event Instigator is the source entity of the modifier."))
struct ARCABILITYKIT_API FArcAttributeHandler_MetaToResource : public FArcAttributeHandler
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Resource")
	FArcAttributeRef Resource;

	UPROPERTY(EditAnywhere, Category = "Resource")
	FArcAttributeRef MaxAttribute;

	UPROPERTY(EditAnywhere, Category = "Resource")
	float MinConst = 0.f;

	UPROPERTY(EditAnywhere, Category = "Resource")
	TOptional<float> MaxConst;

	UPROPERTY(EditAnywhere, Category = "Resource")
	EArcResourceOp Op = EArcResourceOp::Add;

	UPROPERTY(EditAnywhere, Category = "Event")
	FGameplayTag EventTag;

	virtual void PostExecute(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) const override;
};

USTRUCT(meta = (ToolTip = "In PostExecute, applies the damage meta value to Resource using Op, clamps the result to [MinConst, upper bound], writes it with SetAttributeDirectAndNotify and resets the meta to zero. The upper bound is the smaller of MaxAttribute and MaxConst; with neither set the result has no upper bound. When the applied change is not zero, queues ArcEvent.Health.Changed with an FArcHealthChangedPayload, Magnitude = new minus old resource value, and the modifier's captured Source and Target tags.\nAssign to the Damage meta attribute. Target: the meta attribute's set and the set owning Resource (and MaxAttribute when set). The meta is still reset when the Resource set is missing.\nSource: no required attribute sets; the event Instigator is the source entity of the modifier."))
struct ARCABILITYKIT_API FArcAttributeHandler_HealthDamage : public FArcAttributeHandler
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Resource")
	FArcAttributeRef Resource;

	UPROPERTY(EditAnywhere, Category = "Resource")
	FArcAttributeRef MaxAttribute;

	UPROPERTY(EditAnywhere, Category = "Resource")
	float MinConst = 0.f;

	UPROPERTY(EditAnywhere, Category = "Resource")
	TOptional<float> MaxConst;

	UPROPERTY(EditAnywhere, Category = "Resource")
	EArcResourceOp Op = EArcResourceOp::Add;

	virtual void PostExecute(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) const override;
};

USTRUCT(meta = (ToolTip = "Clamps the new value to [MinConst, upper bound] in PreChange. The upper bound is the smaller of MaxAttribute and MaxConst; with neither set only MinConst applies.\nAssign to the clamped attribute. Target: the clamped attribute's set, and the set owning MaxAttribute when set.\nSource: no required attribute sets; target means the entity that owns the attribute."))
struct ARCABILITYKIT_API FArcAttributeHandler_ClampRange : public FArcAttributeHandler
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Range")
	FArcAttributeRef MaxAttribute;

	UPROPERTY(EditAnywhere, Category = "Range")
	float MinConst = 0.f;

	UPROPERTY(EditAnywhere, Category = "Range")
	TOptional<float> MaxConst;

	virtual bool PreChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float& NewValue) const override;
};
