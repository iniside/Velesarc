// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcConditionGameplayTags.h"
#include "ArcConditionTypes.h"

namespace ArcConditionTags
{
	// Active tags
	UE_DEFINE_GAMEPLAY_TAG(Active_Burning,		"ArcCondition.Active.Burning");
	UE_DEFINE_GAMEPLAY_TAG(Active_Bleeding,		"ArcCondition.Active.Bleeding");
	UE_DEFINE_GAMEPLAY_TAG(Active_Chilled,		"ArcCondition.Active.Chilled");
	UE_DEFINE_GAMEPLAY_TAG(Active_Shocked,		"ArcCondition.Active.Shocked");
	UE_DEFINE_GAMEPLAY_TAG(Active_Poisoned,		"ArcCondition.Active.Poisoned");
	UE_DEFINE_GAMEPLAY_TAG(Active_Diseased,		"ArcCondition.Active.Diseased");
	UE_DEFINE_GAMEPLAY_TAG(Active_Weakened,		"ArcCondition.Active.Weakened");
	UE_DEFINE_GAMEPLAY_TAG(Active_Oiled,		"ArcCondition.Active.Oiled");
	UE_DEFINE_GAMEPLAY_TAG(Active_Wet,			"ArcCondition.Active.Wet");
	UE_DEFINE_GAMEPLAY_TAG(Active_Corroded,		"ArcCondition.Active.Corroded");
	UE_DEFINE_GAMEPLAY_TAG(Active_Blinded,		"ArcCondition.Active.Blinded");
	UE_DEFINE_GAMEPLAY_TAG(Active_Suffocating,	"ArcCondition.Active.Suffocating");
	UE_DEFINE_GAMEPLAY_TAG(Active_Exhausted,	"ArcCondition.Active.Exhausted");

	// Overload tags
	UE_DEFINE_GAMEPLAY_TAG(Overload_Burning,	"ArcCondition.Overload.Burning");
	UE_DEFINE_GAMEPLAY_TAG(Overload_Bleeding,	"ArcCondition.Overload.Bleeding");
	UE_DEFINE_GAMEPLAY_TAG(Overload_Chilled,	"ArcCondition.Overload.Chilled");
	UE_DEFINE_GAMEPLAY_TAG(Overload_Shocked,	"ArcCondition.Overload.Shocked");
	UE_DEFINE_GAMEPLAY_TAG(Overload_Poisoned,	"ArcCondition.Overload.Poisoned");
	UE_DEFINE_GAMEPLAY_TAG(Overload_Diseased,	"ArcCondition.Overload.Diseased");
	UE_DEFINE_GAMEPLAY_TAG(Overload_Weakened,	"ArcCondition.Overload.Weakened");
	UE_DEFINE_GAMEPLAY_TAG(Overload_Oiled,		"ArcCondition.Overload.Oiled");
	UE_DEFINE_GAMEPLAY_TAG(Overload_Wet,		"ArcCondition.Overload.Wet");
	UE_DEFINE_GAMEPLAY_TAG(Overload_Corroded,	"ArcCondition.Overload.Corroded");
	UE_DEFINE_GAMEPLAY_TAG(Overload_Blinded,	"ArcCondition.Overload.Blinded");
	UE_DEFINE_GAMEPLAY_TAG(Overload_Suffocating,"ArcCondition.Overload.Suffocating");
	UE_DEFINE_GAMEPLAY_TAG(Overload_Exhausted,	"ArcCondition.Overload.Exhausted");

	// SetByCaller tag
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Saturation, "ArcCondition.SetByCaller.Saturation");

	FGameplayTag GetActiveTag(EArcConditionType Type)
	{
		switch (Type)
		{
		case EArcConditionType::Burning:		return Active_Burning;
		case EArcConditionType::Bleeding:		return Active_Bleeding;
		case EArcConditionType::Chilled:		return Active_Chilled;
		case EArcConditionType::Shocked:		return Active_Shocked;
		case EArcConditionType::Poisoned:		return Active_Poisoned;
		case EArcConditionType::Diseased:		return Active_Diseased;
		case EArcConditionType::Weakened:		return Active_Weakened;
		case EArcConditionType::Oiled:			return Active_Oiled;
		case EArcConditionType::Wet:			return Active_Wet;
		case EArcConditionType::Corroded:		return Active_Corroded;
		case EArcConditionType::Blinded:		return Active_Blinded;
		case EArcConditionType::Suffocating:	return Active_Suffocating;
		case EArcConditionType::Exhausted:		return Active_Exhausted;
		default:								return FGameplayTag();
		}
	}

	FGameplayTag GetOverloadTag(EArcConditionType Type)
	{
		switch (Type)
		{
		case EArcConditionType::Burning:		return Overload_Burning;
		case EArcConditionType::Bleeding:		return Overload_Bleeding;
		case EArcConditionType::Chilled:		return Overload_Chilled;
		case EArcConditionType::Shocked:		return Overload_Shocked;
		case EArcConditionType::Poisoned:		return Overload_Poisoned;
		case EArcConditionType::Diseased:		return Overload_Diseased;
		case EArcConditionType::Weakened:		return Overload_Weakened;
		case EArcConditionType::Oiled:			return Overload_Oiled;
		case EArcConditionType::Wet:			return Overload_Wet;
		case EArcConditionType::Corroded:		return Overload_Corroded;
		case EArcConditionType::Blinded:		return Overload_Blinded;
		case EArcConditionType::Suffocating:	return Overload_Suffocating;
		case EArcConditionType::Exhausted:		return Overload_Exhausted;
		default:								return FGameplayTag();
		}
	}
}
