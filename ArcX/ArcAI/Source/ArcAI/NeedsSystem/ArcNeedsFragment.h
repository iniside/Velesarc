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
#include "MassEQSBlueprintLibrary.h"
#include "MassNavigationTypes.h"
#include "MassStateTreeSchema.h"
#include "MassSubsystemBase.h"
#include "StateTreeConsiderationBase.h"
#include "StateTreePropertyFunctionBase.h"
#include "StateTreeTaskBase.h"
#include "Considerations/StateTreeCommonConsiderations.h"
#include "Tasks/ArcMassStateTreeRunEnvQueryTask.h"
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

template<>
struct TMassFragmentTraits<FArcNeedsFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

USTRUCT()
struct ARCAI_API FArcNeedFragment : public FMassFragment
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere)
	uint8 NeedType = 0;
	
	UPROPERTY(EditAnywhere)
	float ChangeRate = 0;
	
	UPROPERTY(EditAnywhere)
	float CurrentValue = 0;
};


USTRUCT()
struct ARCAI_API FArcHungerNeedFragment : public FArcNeedFragment
{
	GENERATED_BODY()
};

UCLASS(meta = (DisplayName = "Arc Hunger Need Processor"))
class ARCAI_API UArcHungerNeedProcessor : public UMassProcessor
{
	GENERATED_BODY()
	
private:
	FMassEntityQuery NeedsQuery;
	
public:
	UArcHungerNeedProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
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

USTRUCT()
struct FArcNeedsConsiderationInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FName NeedName = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<AActor> Actor = nullptr;
};

/**
 * Consideration using a Float as input to the response curve.
 */
USTRUCT(DisplayName = "Needs Consideration")
struct FArcNeedstConsideration : public FStateTreeConsiderationCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcNeedsConsiderationInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
protected:
	//~ Begin FStateTreeConsiderationBase Interface
	virtual float GetScore(FStateTreeExecutionContext& Context) const override;
	//~ End FStateTreeConsiderationBase Interface

public:
	UPROPERTY(EditAnywhere, Category = "Default")
	FStateTreeConsiderationResponseCurve ResponseCurve;
};

USTRUCT()
struct FArcMassNeedsConsiderationInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FName NeedName = NAME_None;
};

/**
 * Consideration using a Float as input to the response curve.
 */
USTRUCT(DisplayName = "Mass Needs Consideration")
struct FArcMassNeedsConsideration : public FStateTreeConsiderationCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassNeedsConsiderationInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
protected:
	//~ Begin FStateTreeConsiderationBase Interface
	virtual float GetScore(FStateTreeExecutionContext& Context) const override;
	//~ End FStateTreeConsiderationBase Interface

public:
	UPROPERTY(EditAnywhere, Category = "Default")
	FStateTreeConsiderationResponseCurve ResponseCurve;
};

USTRUCT()
struct ARCAI_API FArcScoreEntity
{
	GENERATED_BODY()

public:
	virtual void Check(FMassEntityManager& EntityManager, FMassEntityHandle OwningEntity, FMassEntityHandle InteractingEntity) {}

	virtual ~FArcScoreEntity() = default;
};
// shared fragment ?

USTRUCT()
struct ARCAI_API FArcNeedStateTreeFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TObjectPtr<UStateTree> StateTree;

	// What Entity with this fragment need ?
	UPROPERTY(EditAnywhere)
	FInstancedStruct OwnerNeed;
};

template<>
struct TMassFragmentTraits<FArcNeedStateTreeFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

class UStateTree;

USTRUCT()
struct FArcMassInjectStateTreeTaskInstanceData
{
	GENERATED_BODY()

	/** Delay before the task ends. Default (0 or any negative) will run indefinitely, so it requires a transition in the state tree to stop it. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UStateTree> StateTree = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag StateTag;

	FStateTreeReference StateTreeRef;
	FStateTreeReferenceOverrides LinkedStateTreeOverrides;
};


USTRUCT(meta = (DisplayName = "Arc Mass Inject StateTree"))
struct FArcMassInjectStateTreeTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassInjectStateTreeTaskInstanceData;
public:
	FArcMassInjectStateTreeTask();

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}
	
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

class UStateTree;

USTRUCT()
struct FArcProvideNextStateTreeTaskInstanceData
{
	GENERATED_BODY()

public:
	// Target From which we will try get State Tree(s)
	UPROPERTY(EditAnywhere, Category = Parameter)
	FMassEnvQueryEntityInfoBlueprintWrapper TargetInput;
	
	/** Delay before the task ends. Default (0 or any negative) will run indefinitely, so it requires a transition in the state tree to stop it. */
	UPROPERTY(EditAnywhere, Category = Out, meta = (RefType = "/Script/StateTreeModule.StateTree"))
	FStateTreePropertyRef StateTree;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	TArray<TObjectPtr<UStateTree>> StateTrees;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnQueueFinished;
	
	UPROPERTY()
	TArray<TObjectPtr<UStateTree>> QueuedStateTrees;
	
};

USTRUCT(meta = (DisplayName = "Arc Provide Next StateTree Task"))
struct ARCAI_API FArcProvideNextStateTreeTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()
	
	using FInstanceDataType = FArcProvideNextStateTreeTaskInstanceData;
	
public:
	FArcProvideNextStateTreeTask();

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}
	
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
