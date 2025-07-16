/**
 * This file is part of ArcX.
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

#include "Abilities/Tasks/AbilityTask.h"
#include "Animation/AnimInstance.h"
#include "ArcMacroDefines.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "InputAction.h"

#include "ArcComboComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogArcCombo
	, Log
	, Log);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArcComboStepDelegate
	, FGameplayTag
	, EventTag);

UCLASS()
class ARCCORE_API UArcAT_WaitComboAction : public UAbilityTask
{
	GENERATED_BODY()

private:
	FDelegateHandle InputDelegateHandle;

	UPROPERTY(BlueprintAssignable)
	FArcComboStepDelegate OnNextStep;

	UPROPERTY(BlueprintAssignable)
	FArcComboStepDelegate OnComboWindowStart;

	UPROPERTY(BlueprintAssignable)
	FArcComboStepDelegate OnComboWindowEnd;

	UPROPERTY(BlueprintAssignable)
	FArcComboStepDelegate OnComboFinished;

	FOnMontageEnded MontageEndedDelegate;
	/*
	 * Called when GameplayEvent tag is broadcasted from currently executing step.
	 */
	UPROPERTY(BlueprintAssignable)
	FArcComboStepDelegate OnComboEvent;

	UPROPERTY()
	TObjectPtr<class UArcComboGraph> ComboGraphTest;

	FGameplayTagContainer EventTags;
	FDelegateHandle EventHandle;

	TArray<uint32> BoundHandles;

	int32 PressedHandle;
	int32 TriggeredHandle;
	int32 ReleasedHandle;

	int32 CurrentComboNode = INDEX_NONE;

	bool bComboWindowActive = false;

public:
	/**
	 * @brief Waits for direct Input from ability, either enable globally or per ability
	 * using Replicate Input Directly.
	 * @param TaskInstanceName Instance name, sometimes used to recycle tasks
	 * @return Task Object
	 */
	UFUNCTION(BlueprintCallable
		, Category = "Arc Core|Ability|Tasks"
		, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UArcAT_WaitComboAction* WaitComboAction(UGameplayAbility* OwningAbility
												   , FName TaskInstanceName
												   , const FGameplayTagContainer& InEventTags
												   , class UArcComboGraph* InComboGraph);

	virtual void Activate() override;

	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	UFUNCTION()
	void OnMontageEnded(UAnimMontage* Montage
						, bool bInterrupted);

	void HandleGameplayEvent(FGameplayTag EventTag
							 , const FGameplayEventData* Payload);

	void HandleComboEvent(FGameplayTag EventTag);

	void HandleComboWindowStart();

	void HandleComboWindowEnd();

	void HandleOnInputPressed(const FInputActionInstance& InputActionInstance);
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcComboNodeExtension
{
	GENERATED_BODY()

public:
	virtual void ComboStateEnter()
	{
	}

	virtual void ComboStateLeave()
	{
	}

	virtual ~FArcComboNodeExtension()
	{
	}
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcComboNode
{
	GENERATED_BODY()
	friend class UArcComboGraph;

public:
	UPROPERTY()
	FGuid NodeId;

	UPROPERTY(EditAnywhere
		, Category = "Arc Core")
	FName ItemName;

	UPROPERTY(EditAnywhere
		, Category = "Arc Core")
	FGameplayTag StepTag;

	UPROPERTY(EditAnywhere
		, Category = "Arc Core")
	FGameplayTag WindowStartTag;

	UPROPERTY(EditAnywhere
		, Category = "Arc Core")
	FGameplayTag WindowEndTag;

	UPROPERTY(EditAnywhere
		, Category = "Arc Core")
	FGameplayTagContainer GrantedTags;

	UPROPERTY(EditAnywhere
		, Category = "Arc Core")
	TObjectPtr<class UInputAction> ComboInput = nullptr;

	UPROPERTY(EditAnywhere
		, Category = "Arc Core")
	TObjectPtr<class UAnimMontage> ComboMontage = nullptr;

	UPROPERTY()
	TArray<FGuid> ParentNodes;

	UPROPERTY()
	TArray<FGuid> ChildNodes;

	static FArcComboNode Invalid;

	static FArcComboNode MakeNode()
	{
		FArcComboNode Node;
		Node.NodeId = FGuid::NewGuid();
		return Node;
	}

	TArray<const FArcComboNode*> GetChildNodes(const class UArcComboGraph* TreeData) const;

	TArray<const FArcComboNode*> GetParentNodes(const class UArcComboGraph* TreeData) const;

	void AddChild(const FGuid& InChild)
	{
		ChildNodes.AddUnique(InChild);
	}

	void AddParent(const FGuid& Guid)
	{
		ParentNodes.AddUnique(Guid);
	}

	void ClearAllChildren()
	{
		ChildNodes.Empty();
	}

	void ClearAllParents()
	{
		ParentNodes.Empty();
	}

	bool IsValid() const
	{
		return NodeId.IsValid();
	}

	const FGuid& GetId() const
	{
		return NodeId;
	}

	const TArray<FGuid>& GetParentNodesIds() const
	{
		return ParentNodes;
	}

	const TArray<FGuid>& GetChildNodesIds() const
	{
		return ChildNodes;
	}

	bool operator==(const FGuid& Guid) const
	{
		return NodeId == Guid;
	}

	bool operator==(const FArcComboNode& Other) const
	{
		return NodeId == Other.NodeId;
	}

	bool operator==(UInputAction* Other) const
	{
		return ComboInput == Other;
	}

	bool operator==(const UInputAction* Other) const
	{
		return ComboInput == Other;
	}

	friend uint32 GetTypeHash(const FArcComboNode& Node)
	{
		return GetTypeHash(Node.NodeId);
	}
};

UCLASS(BlueprintType)
class ARCCORE_API UArcComboGraph : public UDataAsset
{
	GENERATED_BODY()

public:
	friend struct FArcComboNode;

	UPROPERTY(EditAnywhere
		, Category = "Arc Core")
	TObjectPtr<class UInputMappingContext> InputMapping;

	UPROPERTY()
	TArray<FArcComboNode> Nodes;

#if WITH_EDITORONLY_DATA

public:
	/** Graph for Behavior Tree */
	UPROPERTY()
	TObjectPtr<class UEdGraph> ATGraph;

	FGuid AddNewNode()
	{
		FArcComboNode Node = FArcComboNode::MakeNode();
		Nodes.Add(Node);
		return Node.GetId();
	}

	void AddChild(const FGuid& InNode
				  , const FGuid& InChild)
	{
		FArcComboNode* Node = Nodes.FindByKey(InNode);
		Node->AddChild(InChild);
	}

	void AddParent(const FGuid& InNode
				   , const FGuid& InParent)
	{
		FArcComboNode* Node = Nodes.FindByKey(InNode);
		Node->AddParent(InParent);
	}

	const TArray<FArcComboNode>& GetNodes() const
	{
		return Nodes;
	}

	void RemoveNode(const FGuid& Guid);
#endif

public:
	const FArcComboNode* FindNode(const FGuid& InNodeId) const
	{
		return Nodes.FindByKey(InNodeId);
	}

	FArcComboNode* FindNode(const FGuid& InNodeId)
	{
		return Nodes.FindByKey(InNodeId);
	}

	TArray<const FArcComboNode*> GetChildNodes(const FGuid& InNodeId) const;

	TArray<const FArcComboNode*> GetParentNodes(const FGuid& InNodeId) const;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FArcComboWindowDelegate
	, bool /*bIsActive*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FArcComboEventDelegate
	, FGameplayTag);
UCLASS(ClassGroup = (Arc)
	, meta = (BlueprintSpawnableComponent))
class ARCCORE_API UArcComboComponent : public UActorComponent
{
	GENERATED_BODY()
	friend class UArcAnimNotify_ComboEvent;
	friend class UArcAnimNotify_ComboWindowStart;
	friend class UArcAnimNotify_ComboWindowEnd;

protected:
	UPROPERTY(EditAnywhere
		, Category = "Arc Core")
	TObjectPtr<class UArcComboGraph> ComboDataTest;

	DEFINE_ARC_DELEGATE(FArcComboWindowDelegate
		, OnComboWindow);
	DEFINE_ARC_DELEGATE(FArcComboEventDelegate
		, OnComboEvent);

	DEFINE_ARC_DELEGATE(FSimpleMulticastDelegate
		, OnComboWindowStart);

public:
	void BroadcastComboWindowStart()
	{
		OnComboWindowStartDelegate.Broadcast();
	}

	DEFINE_ARC_DELEGATE(FSimpleMulticastDelegate
		, OnComboWindowEnd);

public:
	void BroadcastComboWindowEnd()
	{
		OnComboWindowEndDelegate.Broadcast();
	}

public:
	// Sets default values for this component's properties
	UArcComboComponent();

	static UArcComboComponent* FindComboComponent(class AActor* InOwner)
	{
		return InOwner->FindComponentByClass<UArcComboComponent>();
	}

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime
							   , ELevelTick TickType
							   , FActorComponentTickFunction* ThisTickFunction) override;

	FArcComboNode GetNext(const FArcComboNode& Current);
};
