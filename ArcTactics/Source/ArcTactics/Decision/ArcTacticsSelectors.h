// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Decision/ArcTacticsNode.h"
#include "GlobalTasks/ArcTacticsDataTasks.h"
#include "Mass/ArcTacticsOption.h"
#include "Mass/EntityHandle.h"

#include "ArcTacticsSelectors.generated.h"

USTRUCT(BlueprintType, meta = (Hidden))
struct ARCTACTICS_API FArcTacticsSelectorNode : public FArcTacticsNode
{
	GENERATED_BODY()

	virtual EArcTacticsNodeRole GetRole() const override;
	virtual int32 GetItemCount(FConstStructView InstanceData) const;
	virtual bool SelectItem(FStructView InstanceData, int32 ItemIndex, FArcTacticsCandidateIdentity& OutIdentity) const;
};

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsEntitySelectorInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input", meta = (ArcTacticsInput, ArcTacticsRequiredBinding, ToolTip = "Entity handles the selector iterates; each one becomes a candidate."))
	TArray<FMassEntityHandle> Items;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTacticsOutput, ToolTip = "The entity currently being evaluated."))
	FMassEntityHandle CurrentEntity;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Entity Selector", ToolTip = "Scores its action once for each entity in Items, exposing the one being evaluated as CurrentEntity; each entity is a separate candidate."))
struct ARCTACTICS_API FArcTacticsEntitySelector : public FArcTacticsSelectorNode
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;
	virtual int32 GetItemCount(FConstStructView InstanceData) const override;
	virtual bool SelectItem(FStructView InstanceData, int32 ItemIndex, FArcTacticsCandidateIdentity& OutIdentity) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override;
	virtual FColor GetIconColor() const override;
	virtual FText GetDescription(const FGuid& NodeID, FConstStructView InstanceData, const IPropertyBindingBindingCollectionOwner& BindingOwner, bool bRichText) const override;
#endif
};

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsLocationSelectorInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input", meta = (ArcTacticsInput, ArcTacticsRequiredBinding, ToolTip = "World locations the selector iterates; each one becomes a candidate."))
	TArray<FVector> Items;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTacticsOutput, ToolTip = "The location currently being evaluated."))
	FVector CurrentLocation = FVector::ZeroVector;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Location Selector", ToolTip = "Scores its action once for each location in Items, exposing the one being evaluated as CurrentLocation; each location is a separate candidate."))
struct ARCTACTICS_API FArcTacticsLocationSelector : public FArcTacticsSelectorNode
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;
	virtual int32 GetItemCount(FConstStructView InstanceData) const override;
	virtual bool SelectItem(FStructView InstanceData, int32 ItemIndex, FArcTacticsCandidateIdentity& OutIdentity) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override;
	virtual FColor GetIconColor() const override;
	virtual FText GetDescription(const FGuid& NodeID, FConstStructView InstanceData, const IPropertyBindingBindingCollectionOwner& BindingOwner, bool bRichText) const override;
#endif
};

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsTargetSelectorInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input", meta = (ArcTacticsInput, ArcTacticsRequiredBinding, ToolTip = "Targets the selector iterates; each one becomes a candidate identified by its entity."))
	TArray<FArcTacticsTarget> Items;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTacticsOutput, ToolTip = "The target currently being evaluated."))
	FArcTacticsTarget CurrentTarget;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Target Selector", ToolTip = "Scores its action once for each target in Items, exposing the one being evaluated as CurrentTarget; each target is a separate candidate."))
struct ARCTACTICS_API FArcTacticsTargetSelector : public FArcTacticsSelectorNode
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;
	virtual int32 GetItemCount(FConstStructView InstanceData) const override;
	virtual bool SelectItem(FStructView InstanceData, int32 ItemIndex, FArcTacticsCandidateIdentity& OutIdentity) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override;
	virtual FColor GetIconColor() const override;
	virtual FText GetDescription(const FGuid& NodeID, FConstStructView InstanceData, const IPropertyBindingBindingCollectionOwner& BindingOwner, bool bRichText) const override;
#endif
};
