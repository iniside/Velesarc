// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"

#include "ArcConditionTickProcessors.generated.h"

// ---------------------------------------------------------------------------
// Per-condition tick processors.
//
// Each condition has its own tick processor handling decay, overload timers,
// burnout, and hysteresis. Identical logic, different fragment types.
// ---------------------------------------------------------------------------

#define ARC_DECLARE_CONDITION_TICK_PROCESSOR(Name) \
	UCLASS() \
	class ARCCONDITIONEFFECTS_API UArc##Name##ConditionTickProcessor : public UMassProcessor \
	{ \
		GENERATED_BODY() \
	public: \
		UArc##Name##ConditionTickProcessor(); \
	protected: \
		virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override; \
		virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override; \
	private: \
		FMassEntityQuery EntityQuery; \
	};

ARC_DECLARE_CONDITION_TICK_PROCESSOR(Burning)
ARC_DECLARE_CONDITION_TICK_PROCESSOR(Bleeding)
ARC_DECLARE_CONDITION_TICK_PROCESSOR(Chilled)
ARC_DECLARE_CONDITION_TICK_PROCESSOR(Shocked)
ARC_DECLARE_CONDITION_TICK_PROCESSOR(Poisoned)
ARC_DECLARE_CONDITION_TICK_PROCESSOR(Diseased)
ARC_DECLARE_CONDITION_TICK_PROCESSOR(Weakened)
ARC_DECLARE_CONDITION_TICK_PROCESSOR(Oiled)
ARC_DECLARE_CONDITION_TICK_PROCESSOR(Wet)
ARC_DECLARE_CONDITION_TICK_PROCESSOR(Corroded)
ARC_DECLARE_CONDITION_TICK_PROCESSOR(Blinded)
ARC_DECLARE_CONDITION_TICK_PROCESSOR(Suffocating)
ARC_DECLARE_CONDITION_TICK_PROCESSOR(Exhausted)

#undef ARC_DECLARE_CONDITION_TICK_PROCESSOR
