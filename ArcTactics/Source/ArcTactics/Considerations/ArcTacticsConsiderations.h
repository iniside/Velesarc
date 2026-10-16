// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Decision/ArcTacticsNode.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "UtilityAI/ArcUtilityConsideration.h"

#include "ArcTacticsConsiderations.generated.h"

class UArcAbilityDefinition;

UENUM(BlueprintType)
enum class EArcTacticsTargetPresence : uint8
{
	Present,
	Absent
};

USTRUCT(BlueprintType, meta = (Hidden))
struct ARCTACTICS_API FArcTacticsConsiderationNode : public FArcTacticsNode
{
	GENERATED_BODY()

	virtual EArcTacticsNodeRole GetRole() const override;
	virtual FArcUtilityConsiderationResult Score(
		FStructView InstanceData,
		const FArcUtilityTarget& Target,
		const FArcUtilityContext& Context) const;
	virtual EArcTacticsExecutionRequirement GetRequirements(FConstStructView InstanceData) const;
};

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsConstantConsideration : public FArcUtilityConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ArcTacticsParameter, ClampMin = "0.0", ClampMax = "1.0", ToolTip = "The score returned, between 0 and 1."))
	float Value = 1.0f;

	virtual FArcUtilityConsiderationResult Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;

#if WITH_EDITOR
	virtual bool Validate(FString& OutError) const override;
#endif
};

USTRUCT(BlueprintType, meta = (DisplayName = "Constant", ToolTip = "Scores a fixed value."))
struct ARCTACTICS_API FArcTacticsConstantConsiderationNode : public FArcTacticsConsiderationNode
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override;
	virtual FColor GetIconColor() const override;
	virtual FText GetDescription(const FGuid& NodeID, FConstStructView InstanceData, const IPropertyBindingBindingCollectionOwner& BindingOwner, bool bRichText) const override;
#endif
};

USTRUCT(BlueprintType, meta = (Hidden))
struct ARCTACTICS_API FArcTacticsDistanceConsideration : public FArcUtilityConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ArcTacticsParameter, ClampMin = "0.0", ToolTip = "Distance in cm at or below which the score is Near Score."))
	float NearDistance = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ArcTacticsParameter, ClampMin = "0.0", ToolTip = "Distance in cm at or above which the score is Far Score; must exceed Near Distance."))
	float FarDistance = 1000.0f;

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ArcTacticsParameter, ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Score at Near Distance, between 0 and 1."))
	float NearScore = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ArcTacticsParameter, ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Score at Far Distance, between 0 and 1."))
	float FarScore = 0.0f;

	FArcUtilityConsiderationResult ScoreDistanceTo(const FVector& Location, const FArcUtilityContext& Context) const;

#if WITH_EDITOR
	virtual bool Validate(FString& OutError) const override;
#endif
};

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsDistanceToEntityConsideration : public FArcTacticsDistanceConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ArcTacticsInput, ArcTacticsRequiredBinding, ToolTip = "Entity whose position the distance is measured to."))
	FMassEntityHandle Entity;

	virtual FArcUtilityConsiderationResult Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Distance To Entity", ToolTip = "Scores by the distance from the entity to the bound entity, blending from Near Score to Far Score."))
struct ARCTACTICS_API FArcTacticsDistanceToEntityConsiderationNode : public FArcTacticsConsiderationNode
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;
	virtual EArcTacticsExecutionRequirement GetRequirements(FConstStructView InstanceData) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override;
	virtual FColor GetIconColor() const override;
	virtual FText GetDescription(const FGuid& NodeID, FConstStructView InstanceData, const IPropertyBindingBindingCollectionOwner& BindingOwner, bool bRichText) const override;
#endif
};

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsDistanceToLocationConsideration : public FArcTacticsDistanceConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ArcTacticsInput, ArcTacticsRequiredBinding, ToolTip = "World location in cm the distance is measured to."))
	FVector Location = FVector::ZeroVector;

	virtual FArcUtilityConsiderationResult Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Distance To Location", ToolTip = "Scores by the distance from the entity to the bound location, blending from Near Score to Far Score."))
struct ARCTACTICS_API FArcTacticsDistanceToLocationConsiderationNode : public FArcTacticsConsiderationNode
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;
	virtual EArcTacticsExecutionRequirement GetRequirements(FConstStructView InstanceData) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override;
	virtual FColor GetIconColor() const override;
	virtual FText GetDescription(const FGuid& NodeID, FConstStructView InstanceData, const IPropertyBindingBindingCollectionOwner& BindingOwner, bool bRichText) const override;
#endif
};

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsOwnedTagsConsideration : public FArcUtilityConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ArcTacticsInput, ArcTacticsRequiredBinding, ToolTip = "Owned tags the query is tested against."))
	FGameplayTagContainer Tags;

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ArcTacticsParameter, ToolTip = "Tag query the bound tags must satisfy."))
	FGameplayTagQuery Query;

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ArcTacticsParameter, ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Score when the query matches, between 0 and 1."))
	float MatchScore = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ArcTacticsParameter, ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Score when the query does not match, between 0 and 1."))
	float MismatchScore = 0.0f;

	virtual FArcUtilityConsiderationResult Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;

#if WITH_EDITOR
	virtual bool Validate(FString& OutError) const override;
#endif
};

USTRUCT(BlueprintType, meta = (DisplayName = "Owned Tags", ToolTip = "Scores Match Score when the tag query matches the bound tags, otherwise Mismatch Score."))
struct ARCTACTICS_API FArcTacticsOwnedTagsConsiderationNode : public FArcTacticsConsiderationNode
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override;
	virtual FColor GetIconColor() const override;
	virtual FText GetDescription(const FGuid& NodeID, FConstStructView InstanceData, const IPropertyBindingBindingCollectionOwner& BindingOwner, bool bRichText) const override;
#endif
};

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsCostConsideration : public FArcUtilityConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ArcTacticsInput, ArcTacticsRequiredBinding, ToolTip = "Entity that holds the ability."))
	FMassEntityHandle Entity;

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ArcTacticsParameter, ToolTip = "Ability definition whose cost is checked."))
	TObjectPtr<UArcAbilityDefinition> AbilityDefinition = nullptr;

	virtual FArcUtilityConsiderationResult Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Can Afford Cost", ToolTip = "Accepts when the entity can pay the cost of the ability definition, otherwise rejects."))
struct ARCTACTICS_API FArcTacticsCostConsiderationNode : public FArcTacticsConsiderationNode
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;
	virtual EArcTacticsExecutionRequirement GetRequirements(FConstStructView InstanceData) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override;
	virtual FColor GetIconColor() const override;
	virtual FText GetDescription(const FGuid& NodeID, FConstStructView InstanceData, const IPropertyBindingBindingCollectionOwner& BindingOwner, bool bRichText) const override;
#endif
};

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsCooldownConsideration : public FArcUtilityConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ArcTacticsInput, ArcTacticsRequiredBinding, ToolTip = "Entity that holds the ability."))
	FMassEntityHandle Entity;

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ArcTacticsParameter, ToolTip = "Ability definition whose cooldown is checked."))
	TObjectPtr<UArcAbilityDefinition> AbilityDefinition = nullptr;

	virtual FArcUtilityConsiderationResult Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Cooldown Ready", ToolTip = "Accepts when the ability definition is off cooldown, otherwise rejects."))
struct ARCTACTICS_API FArcTacticsCooldownConsiderationNode : public FArcTacticsConsiderationNode
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;
	virtual EArcTacticsExecutionRequirement GetRequirements(FConstStructView InstanceData) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override;
	virtual FColor GetIconColor() const override;
	virtual FText GetDescription(const FGuid& NodeID, FConstStructView InstanceData, const IPropertyBindingBindingCollectionOwner& BindingOwner, bool bRichText) const override;
#endif
};

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsHasEligibleTargetConsideration : public FArcUtilityConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ArcTacticsInput, ArcTacticsRequiredBinding, ToolTip = "Whether an eligible target exists, usually bound from Collect Perception Targets."))
	bool bHasEligibleTarget = false;

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ArcTacticsParameter, ToolTip = "Whether the consideration expects an eligible target to be Present or Absent."))
	EArcTacticsTargetPresence Expectation = EArcTacticsTargetPresence::Present;

	virtual FArcUtilityConsiderationResult Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Has Eligible Target", ToolTip = "Accepts when the bound eligible-target flag matches the expectation, otherwise rejects."))
struct ARCTACTICS_API FArcTacticsHasEligibleTargetConsiderationNode : public FArcTacticsConsiderationNode
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override;
	virtual FColor GetIconColor() const override;
	virtual FText GetDescription(const FGuid& NodeID, FConstStructView InstanceData, const IPropertyBindingBindingCollectionOwner& BindingOwner, bool bRichText) const override;
#endif
};

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsCombatSlotsAvailableConsideration : public FArcUtilityConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ArcTacticsInput, ArcTacticsRequiredBinding, ToolTip = "Entity whose live combat slots are counted."))
	FMassEntityHandle Entity;

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ArcTacticsParameter, ClampMin = "1", ToolTip = "Rejects when this many other attackers already hold live combat slots on the entity. Should match the Slot Max Claimers of the Move To that claims the slot."))
	int32 MaxAttackers = 3;

	virtual FArcUtilityConsiderationResult Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Combat Slots Available", ToolTip = "Accepts when fewer than Max Attackers other entities hold live combat slots on the bound entity, or when this entity already holds one, otherwise rejects."))
struct ARCTACTICS_API FArcTacticsCombatSlotsAvailableConsiderationNode : public FArcTacticsConsiderationNode
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override;
	virtual FColor GetIconColor() const override;
	virtual FText GetDescription(const FGuid& NodeID, FConstStructView InstanceData, const IPropertyBindingBindingCollectionOwner& BindingOwner, bool bRichText) const override;
#endif
};

USTRUCT(BlueprintType, meta = (Hidden, ToolTip = "Shared inputs of the action history considerations."))
struct ARCTACTICS_API FArcTacticsActionHistoryConsideration : public FArcUtilityConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ArcTacticsInput, ArcTacticsRequiredBinding, ToolTip = "Action history of the entity, bound from Context.History."))
	FArcTacticsActionHistory History;

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ArcTacticsParameter, ToolTip = "History tag to look for. A recorded child tag also matches."))
	FGameplayTag Tag;

#if WITH_EDITOR
	virtual bool Validate(FString& OutError) const override;
#endif
};

USTRUCT(BlueprintType, meta = (ToolTip = "Instance data of Action History Recent."))
struct ARCTACTICS_API FArcTacticsActionHistoryRecentConsideration : public FArcTacticsActionHistoryConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ArcTacticsParameter, ClampMin = "1", ToolTip = "Number of newest history entries searched for the tag."))
	int32 LastEntries = 1;

	virtual FArcUtilityConsiderationResult Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;

#if WITH_EDITOR
	virtual bool Validate(FString& OutError) const override;
#endif
};

USTRUCT(BlueprintType, meta = (DisplayName = "Action History Recent", ToolTip = "Accepts when the tag is among the newest Last Entries of the action history, otherwise rejects."))
struct ARCTACTICS_API FArcTacticsActionHistoryRecentConsiderationNode : public FArcTacticsConsiderationNode
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override;
	virtual FColor GetIconColor() const override;
	virtual FText GetDescription(const FGuid& NodeID, FConstStructView InstanceData, const IPropertyBindingBindingCollectionOwner& BindingOwner, bool bRichText) const override;
#endif
};

USTRUCT(BlueprintType, meta = (ToolTip = "Instance data of Action History Within Time."))
struct ARCTACTICS_API FArcTacticsActionHistoryWithinTimeConsideration : public FArcTacticsActionHistoryConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ArcTacticsParameter, ClampMin = "0.0", ToolTip = "Maximum age in seconds of a matching history entry. Must be greater than zero."))
	float MaxAgeSeconds = 5.0f;

	virtual FArcUtilityConsiderationResult Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;

#if WITH_EDITOR
	virtual bool Validate(FString& OutError) const override;
#endif
};

USTRUCT(BlueprintType, meta = (DisplayName = "Action History Within Time", ToolTip = "Accepts when the tag was recorded in the action history no more than Max Age Seconds ago, otherwise rejects."))
struct ARCTACTICS_API FArcTacticsActionHistoryWithinTimeConsiderationNode : public FArcTacticsConsiderationNode
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override;
	virtual FColor GetIconColor() const override;
	virtual FText GetDescription(const FGuid& NodeID, FConstStructView InstanceData, const IPropertyBindingBindingCollectionOwner& BindingOwner, bool bRichText) const override;
#endif
};

USTRUCT(BlueprintType, meta = (ToolTip = "Instance data of Action History Absent."))
struct ARCTACTICS_API FArcTacticsActionHistoryAbsentConsideration : public FArcTacticsActionHistoryConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ArcTacticsParameter, ClampMin = "0", ToolTip = "Number of newest history entries searched for the tag. 0 searches the whole kept history."))
	int32 LastEntries = 0;

	virtual FArcUtilityConsiderationResult Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;

#if WITH_EDITOR
	virtual bool Validate(FString& OutError) const override;
#endif
};

USTRUCT(BlueprintType, meta = (DisplayName = "Action History Absent", ToolTip = "Rejects when the tag is among the searched action history entries, otherwise accepts."))
struct ARCTACTICS_API FArcTacticsActionHistoryAbsentConsiderationNode : public FArcTacticsConsiderationNode
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override;
	virtual FColor GetIconColor() const override;
	virtual FText GetDescription(const FGuid& NodeID, FConstStructView InstanceData, const IPropertyBindingBindingCollectionOwner& BindingOwner, bool bRichText) const override;
#endif
};
