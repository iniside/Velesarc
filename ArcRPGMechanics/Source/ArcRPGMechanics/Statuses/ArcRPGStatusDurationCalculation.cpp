// Copyright Lukasz Baran. All Rights Reserved.

#include "Statuses/ArcRPGStatusDurationCalculation.h"

#include "ArcRPGMechanicsLogs.h"
#include "Attributes/ArcAttributeSet_RPGCoreStats.h"
#include "Attributes/Handlers/ArcRPGCoreDerivation.h"
#include "Combat/ArcRPGSourceItemSources.h"
#include "Statuses/ArcRPGAttributeSet_StatusDuration.h"
#include "Effects/ArcEffectComponent.h"
#include "Effects/ArcEffectContext.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectSpec.h"
#include "Mass/EntityManager.h"

namespace ArcRPGStatusDurationCalculationPrivate
{
	FArcAttributeCaptureDef GetWisdomCapture()
	{
		return Arc::Capture::MakeCaptureDef<&FArcAttributeSet_RPGCoreStats::GetWisdomAttribute, EArcCaptureSource::Source, false>();
	}
}

void FArcRPGStatusDurationCalculation::GatherCaptures(TArray<FArcAttributeCaptureDef>& OutCaptures, TArray<FArcExtraCaptureDef>& OutExtras) const
{
	using namespace ArcRPGStatusDurationCalculationPrivate;

	OutExtras.AddUnique(ArcRPGSourceItemSources::GetSkillPowerDef());
	OutCaptures.AddUnique(GetWisdomCapture());
	OutCaptures.AddUnique(GET_CAPTURE_SOURCE_LIVE(FArcRPGAttributeSet_StatusDuration, StatusDurationPct));
}

float FArcRPGStatusDurationCalculation::CalculateDuration(const FArcEffectExecutionContext& Context, float BaseDuration) const
{
	using namespace ArcRPGStatusDurationCalculationPrivate;

	const FArcEffectSpec& Spec = Context.GetOwningSpec();
	const FArcEvaluateParameters Params = Spec.MakeEvaluateParameters();

	float SkillPower = 0.f;
	if (!Context.AttemptResolveExtraCapture(ArcRPGSourceItemSources::GetSkillPowerDef(), Params, SkillPower))
	{
		SkillPower = 0.f;
	}

	float Duration = BaseDuration * (1.f + Scale * SkillPower);

	float Wisdom = 0.f;
	if (Context.AttemptCalculateCapturedAttributeMagnitude(GetWisdomCapture(), Params, Wisdom))
	{
		Duration *= 1.f + WisdomK * ArcRPGCoreDerivation::CalculateModifier(Wisdom);
	}

	Duration *= 1.f + Context.CalcCapturedMagnitude(GET_CAPTURE_SOURCE_LIVE(FArcRPGAttributeSet_StatusDuration, StatusDurationPct), Params);

	if (ArcEffectHelpers::GetEffectTags(Spec.Definition).HasAny(WorldTimeEffectTypes))
	{
		UWorld* World = Context.EntityManager ? Context.EntityManager->GetWorld() : nullptr;
		Duration *= GameHoursPerWorldUnit * ArcRPGCoreDerivation::ResolveRealSecondsPerGameHour(World);
	}

	UE_LOGF(LogArcRPGMechanics, Verbose, "RPG status duration %ls on %ls: base %.3f, skill power %.4f, wisdom %.3f -> %.3f s",
		*GetNameSafe(Spec.Definition), *Context.TargetEntity.DebugGetDescription(), BaseDuration, SkillPower, Wisdom, FMath::Max(0.f, Duration));
	return FMath::Max(0.f, Duration);
}
