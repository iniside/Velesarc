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

#include "MassEntityTypes.h"
#include "MassProcessor.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTemplateRegistry.h"
#include "ArcNeedsFragment.generated.h"

USTRUCT(BlueprintType)
struct ARCAI_API FArcNeedItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FName NeedName = NAME_None;

	UPROPERTY(EditAnywhere, meta=(ClampMin=0, ClampMax=100, UIMin=0, UIMax=100))
	float CurrentValue = 0;
	
	UPROPERTY(EditAnywhere)
	float ChangeRate = 0;

	
	bool operator==(const FArcNeedItem& Other) const
	{
		return NeedName == Other.NeedName;
	}

	bool operator==(const FName& Other) const
	{
		return NeedName == Other;
	}
};

USTRUCT()
struct ARCAI_API FArcNeedsFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TArray<FArcNeedItem> Needs;
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories)
class ARCAI_API UArcNeedsTraitBase : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:

	/** Appends items into the entity template required for the trait. */
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
	{
		BuildContext.AddFragment<FArcNeedsFragment>();
	}
};

USTRUCT()
struct ARCAI_API FArcDummyFragment : public FMassFragment
{
	GENERATED_BODY()
};


UCLASS(meta = (DisplayName = "Arc Needs Processor"))
class ARCAI_API UArcNeedsProcessor : public UMassProcessor
{
	GENERATED_BODY()
	
private:
	FMassEntityQuery NeedsQuery;
	
public:
	UArcNeedsProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
};

/*
 * Simple way to register actor with needs system. By no means the only way, if you have custom spawn system
 * for actors in which you can inject needed data and register with Mass, it would be better than using this component.
 */
UCLASS(Blueprintable, BlueprintType, meta=(BlueprintSpawnableComponent))
class UArcNeedsComponent : public UActorComponent
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, Category=Config)
	TArray<FArcNeedItem> Needs;
	
	UArcNeedsComponent();

	virtual void BeginPlay() override;
};

UCLASS(Blueprintable, BlueprintType, meta=(BlueprintSpawnableComponent))
class UArcRestoreNeedComponent : public UActorComponent
{
	GENERATED_BODY()
protected:
	UPROPERTY(EditDefaultsOnly, Category=Config)
	FName NeedName;

	UPROPERTY(EditDefaultsOnly, Category=Config, meta=(ClampMin=0, ClampMax=100, UIMin=0, UIMax=100))
	float RestoreValue = 0.f;

	UPROPERTY(EditDefaultsOnly, Category=Config)
	float RestoreCooldown = 0.0f;

	double LastUseTime = 0;
	
	UArcRestoreNeedComponent();
public:
	UFUNCTION(BlueprintCallable, Category="Arc AI")
	void Restore(class AActor* For);

	bool GetCanBeUsed() const;

	float GetRestoreValue() const
	{
		return RestoreValue;
	}

	double GetLastUseTime() const
	{
		return LastUseTime;
	};
};
