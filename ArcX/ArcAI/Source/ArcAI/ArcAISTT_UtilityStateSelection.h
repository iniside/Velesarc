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
#include "NativeGameplayTags.h"
#include "StateTreeConsiderationBase.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeReference.h"

#include "StateTreeTaskBase.h"
#include "Misc/DefinePrivateMemberPtr.h"
#include "ArcAISTT_UtilityStateSelection.generated.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_StateTree_Event);

USTRUCT(BlueprintType)
struct FArcAIUtilityStateEvent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<AActor> TargetActor;
};

enum class EStateTreeRunStatus : uint8;
struct FStateTreeTransitionResult;

struct FArcStateTreeExecutionContext : public FStateTreeExecutionContext
{
	using FStateTreeExecutionContext::CurrentlyProcessedSharedInstanceStorage;
	
	FArcStateTreeExecutionContext(const FStateTreeExecutionContext& InContextToCopy, const UStateTree& InStateTree, FStateTreeInstanceData& InInstanceData)
		: FStateTreeExecutionContext(InContextToCopy, InStateTree, InInstanceData)
	{
		CurrentlyProcessedSharedInstanceStorage = static_cast<const FArcStateTreeExecutionContext&>(InContextToCopy).CurrentlyProcessedSharedInstanceStorage;
		CurrentlyProcessedParentFrame = InContextToCopy.GetCurrentlyProcessedParentFrame(); 
		CurrentlyProcessedFrame = InContextToCopy.GetCurrentlyProcessedFrame();
		//UE_DEFINE_PRIVATE_MEMBER_PTR()
	}
	
	float ArcEvaluateUtility(FStateTreeStateHandle Handle);

	friend struct FArcNodeInstanceDataScope;

	void SetDataScope(const FStateTreeDataHandle InNodeDataHandle, const FStateTreeDataView InNodeInstanceData)
	{
		CurrentNodeDataHandle = InNodeDataHandle;
		CurrentNodeInstanceData = InNodeInstanceData;
	}
	
	struct FArcNodeInstanceDataScope
	{
		FArcNodeInstanceDataScope(FArcStateTreeExecutionContext& InContext, const FStateTreeDataHandle InNodeDataHandle, const FStateTreeDataView InNodeInstanceData)
			: Context(InContext)
		{
			SavedNodeDataHandle = Context.CurrentNodeDataHandle;
			SavedNodeInstanceData = Context.CurrentNodeInstanceData;
			Context.SetDataScope(InNodeDataHandle, InNodeInstanceData);
			
		}

		~FArcNodeInstanceDataScope()
		{
			Context.CurrentNodeDataHandle = SavedNodeDataHandle;
			Context.CurrentNodeInstanceData = SavedNodeInstanceData;
		}

	private:
		FArcStateTreeExecutionContext& Context;
		FStateTreeDataHandle SavedNodeDataHandle;
		FStateTreeDataView SavedNodeInstanceData;
	};
};

USTRUCT()
struct ARCAI_API FArcAISTT_UtilityStateSelectionInstanceData
{
	GENERATED_BODY()

	UPROPERTY()
	FStateTreeStateHandle SourceState = FStateTreeStateHandle::Invalid;
	
	UPROPERTY()
	FStateTreeStateHandle TargetState = FStateTreeStateHandle::Invalid;

	/** The current state being executed. On enter/exit callbacks this is the state of the task. */
	UPROPERTY()
	FStateTreeStateHandle CurrentState = FStateTreeStateHandle::Invalid;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (Optional))
	TArray<TObjectPtr<AActor>> InActors;

	UPROPERTY(EditAnywhere, Category = "Output")
	TObjectPtr<AActor> CurrentActor;

	UPROPERTY(EditAnywhere, Category = "Output")
	TObjectPtr<AActor> SelectedActor;
	
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TArray<FStateTreeStateLink> ConsideredStates;
	
	UPROPERTY()
	bool bStateSelected = false;
};

/**
 * Simple task to wait indefinitely or for a given time (in seconds) before succeeding.
 */
USTRUCT(meta = (DisplayName = "Utility State Selection Task"))
struct ARCAI_API FArcAISTT_UtilityStateSelectionTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()
	
	struct ConState
	{
		FStateTreeStateHandle Handle;
		float Score;
		TWeakObjectPtr<AActor> Target;
	};
	
	using FInstanceDataType = FArcAISTT_UtilityStateSelectionInstanceData;

	FArcAISTT_UtilityStateSelectionTask() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
#if WITH_EDITOR
	//virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
	virtual FName GetIconName() const override
	{
		return FName("StateTreeEditorStyle|Node.Time");
	}
	virtual FColor GetIconColor() const override
	{
		return UE::StateTree::Colors::Grey;
	}
#endif
};

USTRUCT()
struct FArcActorConsiderationInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<AActor> Input = nullptr;
};

/**
 * Consideration using a Float as input to the response curve.
 */
USTRUCT(DisplayName = "Actor Consideration")
struct FArcActorConsideration : public FStateTreeConsiderationCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcActorConsiderationInstanceData;
	
	//~ Begin FStateTreeNodeBase Interface
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

protected:
	//~ Begin FStateTreeConsiderationBase Interface
	virtual float GetScore(FStateTreeExecutionContext& Context) const override;
	//~ End FStateTreeConsiderationBase Interface
};