// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Attributes/ArcResourceAttributeHandlers.h"
#include "ArcMassMechanics/Tiers/ArcTierTypes.h"

#include "ArcEnviroAttributeHandlers.generated.h"

USTRUCT(meta = (DisplayName = "Enviro Condition Delta", ToolTip = "In PostExecute, applies the delta meta value to Resource using Op, clamps the result to [MinConst, MaxConst], writes it with SetAttributeDirectAndNotify and resets the meta to zero. Then writes Level = Bands tier of the new value and NegativeLevel = Bands tier of the negated new value, each only when it differs.\nAssign to the delta meta attribute (WetDelta, TemperatureDelta). Target: the meta attribute's set and the sets owning Resource, Level and NegativeLevel. The meta is still reset when the Resource set is missing.\nSource: no required attribute sets."))
struct ARCENVIROEFFECTS_API FArcEnviroAttributeHandler_ConditionDelta : public FArcAttributeHandler
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Resource", meta = (ToolTip = "The condition value the delta applies to, e.g. Wet or Temperature."))
	FArcAttributeRef Resource;

	UPROPERTY(EditAnywhere, Category = "Resource", meta = (ToolTip = "Lower clamp of Resource, e.g. 0 for Wet, -100 for Temperature."))
	float MinConst = 0.f;

	UPROPERTY(EditAnywhere, Category = "Resource", meta = (ToolTip = "Upper clamp of Resource, e.g. 100. Unset means no upper bound."))
	TOptional<float> MaxConst;

	UPROPERTY(EditAnywhere, Category = "Resource", meta = (ToolTip = "How the meta value applies to Resource. Signed deltas such as Wet and Temperature use AddSigned."))
	EArcResourceOp Op = EArcResourceOp::AddSigned;

	UPROPERTY(EditAnywhere, Category = "Level", meta = (ToolTip = "Level attribute set to the Bands tier of the new Resource value, e.g. WetLevel or HotLevel."))
	FArcAttributeRef Level;

	UPROPERTY(EditAnywhere, Category = "Level", meta = (ToolTip = "Optional level attribute set to the Bands tier of the negated new Resource value, e.g. ChilledLevel for Temperature."))
	FArcAttributeRef NegativeLevel;

	UPROPERTY(EditAnywhere, Category = "Level", meta = (ToolTip = "Ascending thresholds for Level and NegativeLevel. Design values: 25, 50, 75."))
	FArcTierBands Bands;

	virtual void PostExecute(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) const override;
};
