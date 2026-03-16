/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#pragma once

#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "ArcNeedsFatigueInteropProcessor.generated.h"

/**
 * Periodically syncs FArcFatigueNeedFragment::CurrentValue into the
 * UArcNeedsSurvivalAttributeSet::Fatigue GAS attribute via SetNumericAttributeBase.
 *
 * Runs on game thread (PostPhysics) since it writes to GAS attribute sets through the ASC.
 * Fires approximately once per second to avoid per-frame GAS overhead.
 */
UCLASS(meta = (DisplayName = "Arc Needs Fatigue Interop Processor"))
class ARCNEEDS_API UArcNeedsFatigueInteropProcessor : public UMassProcessor
{
	GENERATED_BODY()

private:
	FMassEntityQuery FatigueQuery;
	float AccumulatedTime = 0.f;

public:
	UArcNeedsFatigueInteropProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
};
