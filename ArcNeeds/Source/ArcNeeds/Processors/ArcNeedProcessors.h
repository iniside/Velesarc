/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
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
#include "Fragments/ArcNeedFragment.h"
#include "ArcNeedProcessors.generated.h"

UCLASS(meta = (DisplayName = "Arc Hunger Need Processor"))
class ARCNEEDS_API UArcHungerNeedProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcHungerNeedProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery NeedsQuery;
};

UCLASS(meta = (DisplayName = "Arc Thirst Need Processor"))
class ARCNEEDS_API UArcThirstNeedProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcThirstNeedProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery NeedsQuery;
};

UCLASS(meta = (DisplayName = "Arc Fatigue Need Processor"))
class ARCNEEDS_API UArcFatigueNeedProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcFatigueNeedProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery NeedsQuery;
};
