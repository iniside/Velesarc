// Copyright Lukasz Baran. All Rights Reserved.

#include "Considerations/ArcTacticsConsiderations.h"

#include "Abilities/ArcAbilityCooldown.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/ArcMassAbilityCost.h"
#include "Combat/ArcCombatSlots.h"
#include "Decision/ArcTacticsDecision.h"
#include "Engine/World.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Mass/EntityManager.h"
#include "MassCommonFragments.h"

#if WITH_EDITOR
#include "PropertyBindingBindableStructDescriptor.h"
#include "PropertyBindingBindingCollection.h"
#include "PropertyBindingBindingCollectionOwner.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTacticsConsiderations)

namespace ArcTacticsConsiderationsPrivate
{
#if WITH_EDITOR
	FString EscapeRichText(const FStringView Text)
	{
		FString Result(Text);
		Result.ReplaceInline(TEXT("&"), TEXT("&amp;"));
		Result.ReplaceInline(TEXT("<"), TEXT("&lt;"));
		Result.ReplaceInline(TEXT(">"), TEXT("&gt;"));
		Result.ReplaceInline(TEXT("\""), TEXT("&quot;"));
		return Result;
	}

	FText Emphasize(const FText& Text, const bool bRichText)
	{
		const FString Escaped = EscapeRichText(Text.ToString());
		return bRichText
			? FText::FromString(FString::Printf(TEXT("<b>%s</>"), *Escaped))
			: Text;
	}

	FText GetBindingSource(
		const FGuid& NodeID,
		const FName PropertyName,
		const IPropertyBindingBindingCollectionOwner& BindingOwner,
		const bool bRichText)
	{
		const FPropertyBindingBindingCollection* Bindings = BindingOwner.GetEditorPropertyBindings();
		if (!Bindings)
		{
			return FText::GetEmpty();
		}
		const FPropertyBindingPath* SourcePath = Bindings->GetBindingSource(FPropertyBindingPath(NodeID, PropertyName));
		if (!SourcePath)
		{
			return FText::GetEmpty();
		}

		FString SourceName;
		TInstancedStruct<FPropertyBindingBindableStructDescriptor> Descriptor;
		if (BindingOwner.GetBindableStructByID(SourcePath->GetStructID(), Descriptor))
		{
			SourceName = Descriptor.Get().Name.ToString();
		}
		const FString Path = SourcePath->ToString();
		if (!Path.IsEmpty())
		{
			if (!SourceName.IsEmpty())
			{
				SourceName += TEXT(".");
			}
			SourceName += Path;
		}
		return Emphasize(FText::FromString(SourceName), bRichText);
	}

	FText GetSourceOrField(
		const FGuid& NodeID,
		const FName PropertyName,
		const IPropertyBindingBindingCollectionOwner& BindingOwner,
		const bool bRichText)
	{
		const FText Source = GetBindingSource(NodeID, PropertyName, BindingOwner, bRichText);
		return Source.IsEmpty() ? Emphasize(FText::FromName(PropertyName), bRichText) : Source;
	}

	FText DescribeAbility(const UArcAbilityDefinition* Definition, const bool bRichText)
	{
		return Emphasize(
			Definition ? FText::FromString(Definition->GetName()) : NSLOCTEXT("ArcTactics", "NoAbilityDefinition", "no ability"),
			bRichText);
	}

	FText DescribeHistoryTag(const FGameplayTag& Tag, const bool bRichText)
	{
		return Emphasize(
			Tag.IsValid() ? FText::FromName(Tag.GetTagName()) : NSLOCTEXT("ArcTactics", "NoHistoryTag", "no tag"),
			bRichText);
	}

	FText DescribeDistance(
		const FArcTacticsDistanceConsideration& Data,
		const FName SubjectProperty,
		const FGuid& NodeID,
		const IPropertyBindingBindingCollectionOwner& BindingOwner,
		const bool bRichText)
	{
		const FText Subject = GetSourceOrField(NodeID, SubjectProperty, BindingOwner, bRichText);
		const FText Near = FText::AsNumber(Data.NearDistance);
		const FText Far = FText::AsNumber(Data.FarDistance);
		return bRichText
			? FText::Format(NSLOCTEXT("ArcTactics", "DistanceRich", "Distance <s>from self to</> {0} <s>{1}-{2} cm</>"), Subject, Near, Far)
			: FText::Format(NSLOCTEXT("ArcTactics", "DistancePlain", "Distance from self to {0}, {1}-{2} cm"), Subject, Near, Far);
	}
#endif

	FArcUtilityConsiderationResult Accept(const float Score)
	{
		return FArcUtilityConsiderationResult::Accept(FMath::Clamp(Score, 0.0f, 1.0f));
	}

	FArcUtilityConsiderationResult Reject(const FName Reason)
	{
		return FArcUtilityConsiderationResult::Reject(Reason);
	}

	bool IsNormalizedFinite(const float Value)
	{
		return FMath::IsFinite(Value) && Value >= 0.0f && Value <= 1.0f;
	}

	bool HistoryContains(
		const FArcTacticsActionHistory& History,
		const FGameplayTag& Tag,
		const int32 LastEntries,
		const double MinTime)
	{
		const TArray<FArcTacticsActionHistoryEntry>& Entries = History.Entries;
		const int32 FirstIndex = LastEntries > 0 ? FMath::Max(0, Entries.Num() - LastEntries) : 0;
		for (int32 EntryIndex = Entries.Num() - 1; EntryIndex >= FirstIndex; --EntryIndex)
		{
			const FArcTacticsActionHistoryEntry& Entry = Entries[EntryIndex];
			if (Entry.Time >= MinTime && Entry.Tag.MatchesTag(Tag))
			{
				return true;
			}
		}
		return false;
	}
}

EArcTacticsNodeRole FArcTacticsConsiderationNode::GetRole() const
{
	return EArcTacticsNodeRole::Consideration;
}

FArcUtilityConsiderationResult FArcTacticsConsiderationNode::Score(
	const FStructView InstanceData,
	const FArcUtilityTarget& Target,
	const FArcUtilityContext& Context) const
{
	FArcUtilityConsideration* Consideration = InstanceData.GetPtr<FArcUtilityConsideration>();
	return Consideration
		? Consideration->Score(Target, Context)
		: FArcUtilityConsiderationResult::Reject(TEXT("InvalidConsiderationData"));
}

EArcTacticsExecutionRequirement FArcTacticsConsiderationNode::GetRequirements(FConstStructView) const
{
	return EArcTacticsExecutionRequirement::None;
}

FArcUtilityConsiderationResult FArcTacticsConstantConsideration::Score(
	const FArcUtilityTarget&, const FArcUtilityContext&) const
{
	return ArcTacticsConsiderationsPrivate::IsNormalizedFinite(Value)
		? ArcTacticsConsiderationsPrivate::Accept(Value)
		: ArcTacticsConsiderationsPrivate::Reject(TEXT("InvalidConstant"));
}

#if WITH_EDITOR
bool FArcTacticsConstantConsideration::Validate(FString& OutError) const
{
	if (!ArcTacticsConsiderationsPrivate::IsNormalizedFinite(Value))
	{
		OutError = TEXT("Value must be finite and between zero and one.");
		return false;
	}
	return true;
}
#endif

const UScriptStruct* FArcTacticsConstantConsiderationNode::GetInstanceDataType() const
{
	return FArcTacticsConstantConsideration::StaticStruct();
}

FText FArcTacticsConstantConsiderationNode::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "ConstantConsideration", "Constant");
}

FArcUtilityConsiderationResult FArcTacticsDistanceConsideration::ScoreDistanceTo(
	const FVector& Location, const FArcUtilityContext& Context) const
{
	if (!FMath::IsFinite(NearDistance) || !FMath::IsFinite(FarDistance)
		|| NearDistance < 0.0f || FarDistance <= NearDistance
		|| !ArcTacticsConsiderationsPrivate::IsNormalizedFinite(NearScore)
		|| !ArcTacticsConsiderationsPrivate::IsNormalizedFinite(FarScore))
	{
		return ArcTacticsConsiderationsPrivate::Reject(TEXT("InvalidDistanceRange"));
	}
	const FTransformFragment* QuerierTransform = Context.EntityManager
		? Context.EntityManager->GetFragmentDataPtr<FTransformFragment>(Context.QuerierEntity)
		: nullptr;
	if (!QuerierTransform)
	{
		return ArcTacticsConsiderationsPrivate::Reject(TEXT("QuerierTransformMissing"));
	}
	const float Distance = FVector::Dist(QuerierTransform->GetTransform().GetLocation(), Location);
	if (!FMath::IsFinite(Distance))
	{
		return ArcTacticsConsiderationsPrivate::Reject(TEXT("InvalidDistance"));
	}
	const float Alpha = FMath::Clamp((Distance - NearDistance) / (FarDistance - NearDistance), 0.0f, 1.0f);
	return ArcTacticsConsiderationsPrivate::Accept(FMath::Lerp(NearScore, FarScore, Alpha));
}

#if WITH_EDITOR
bool FArcTacticsDistanceConsideration::Validate(FString& OutError) const
{
	if (!FMath::IsFinite(NearDistance) || !FMath::IsFinite(FarDistance)
		|| NearDistance < 0.0f || FarDistance <= NearDistance)
	{
		OutError = TEXT("Distance range must be finite, nonnegative, and increasing.");
		return false;
	}
	if (!ArcTacticsConsiderationsPrivate::IsNormalizedFinite(NearScore)
		|| !ArcTacticsConsiderationsPrivate::IsNormalizedFinite(FarScore))
	{
		OutError = TEXT("Distance response scores must be finite and between zero and one.");
		return false;
	}
	return true;
}
#endif

FArcUtilityConsiderationResult FArcTacticsDistanceToEntityConsideration::Score(
	const FArcUtilityTarget&, const FArcUtilityContext& Context) const
{
	const FTransformFragment* Transform = Context.EntityManager && Context.EntityManager->IsEntityActive(Entity)
		? Context.EntityManager->GetFragmentDataPtr<FTransformFragment>(Entity)
		: nullptr;
	if (!Transform)
	{
		return ArcTacticsConsiderationsPrivate::Reject(TEXT("EntityTransformMissing"));
	}
	return ScoreDistanceTo(Transform->GetTransform().GetLocation(), Context);
}

FArcUtilityConsiderationResult FArcTacticsDistanceToLocationConsideration::Score(
	const FArcUtilityTarget&, const FArcUtilityContext& Context) const
{
	return ScoreDistanceTo(Location, Context);
}

const UScriptStruct* FArcTacticsDistanceToEntityConsiderationNode::GetInstanceDataType() const
{
	return FArcTacticsDistanceToEntityConsideration::StaticStruct();
}

FText FArcTacticsDistanceToEntityConsiderationNode::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "DistanceToEntityConsideration", "Distance To Entity");
}

EArcTacticsExecutionRequirement FArcTacticsDistanceToEntityConsiderationNode::GetRequirements(FConstStructView) const
{
	return EArcTacticsExecutionRequirement::Transform;
}

const UScriptStruct* FArcTacticsDistanceToLocationConsiderationNode::GetInstanceDataType() const
{
	return FArcTacticsDistanceToLocationConsideration::StaticStruct();
}

FText FArcTacticsDistanceToLocationConsiderationNode::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "DistanceToLocationConsideration", "Distance To Location");
}

EArcTacticsExecutionRequirement FArcTacticsDistanceToLocationConsiderationNode::GetRequirements(FConstStructView) const
{
	return EArcTacticsExecutionRequirement::Transform;
}

FArcUtilityConsiderationResult FArcTacticsOwnedTagsConsideration::Score(
	const FArcUtilityTarget&, const FArcUtilityContext&) const
{
	const float TagScore = Query.Matches(Tags) ? MatchScore : MismatchScore;
	return ArcTacticsConsiderationsPrivate::IsNormalizedFinite(TagScore)
		? ArcTacticsConsiderationsPrivate::Accept(TagScore)
		: ArcTacticsConsiderationsPrivate::Reject(TEXT("InvalidTagResponse"));
}

#if WITH_EDITOR
bool FArcTacticsOwnedTagsConsideration::Validate(FString& OutError) const
{
	if (!ArcTacticsConsiderationsPrivate::IsNormalizedFinite(MatchScore)
		|| !ArcTacticsConsiderationsPrivate::IsNormalizedFinite(MismatchScore))
	{
		OutError = TEXT("Tag response scores must be finite and between zero and one.");
		return false;
	}
	return true;
}
#endif

const UScriptStruct* FArcTacticsOwnedTagsConsiderationNode::GetInstanceDataType() const
{
	return FArcTacticsOwnedTagsConsideration::StaticStruct();
}

FText FArcTacticsOwnedTagsConsiderationNode::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "OwnedTagsConsideration", "Owned Tags");
}

FArcUtilityConsiderationResult FArcTacticsCostConsideration::Score(
	const FArcUtilityTarget&, const FArcUtilityContext& Context) const
{
	if (!Context.EntityManager || !Context.EntityManager->IsEntityActive(Entity))
	{
		return ArcTacticsConsiderationsPrivate::Reject(TEXT("NoEntityManagerForCost"));
	}
	FName RejectionReason;
	const FArcActiveAbility* ActiveAbility = ArcTacticsDecision::ResolveAbility(
		*Context.EntityManager, Entity, AbilityDefinition.Get(), false, RejectionReason);
	if (!ActiveAbility)
	{
		return ArcTacticsConsiderationsPrivate::Reject(RejectionReason);
	}
	const UArcAbilityDefinition* Definition = ActiveAbility->Definition.Get();
	if (!Definition->Cost.IsValid())
	{
		return ArcTacticsConsiderationsPrivate::Accept(1.0f);
	}
	const FArcMassAbilityCost* Cost = Definition->Cost.GetPtr<FArcMassAbilityCost>();
	if (!Cost)
	{
		return ArcTacticsConsiderationsPrivate::Reject(TEXT("InvalidCost"));
	}
	const FArcMassAbilityCostContext CostContext{*Context.EntityManager, Entity, ActiveAbility->Handle, Definition};
	return Cost->CheckCost(CostContext)
		? ArcTacticsConsiderationsPrivate::Accept(1.0f)
		: ArcTacticsConsiderationsPrivate::Reject(TEXT("CostUnavailable"));
}

const UScriptStruct* FArcTacticsCostConsiderationNode::GetInstanceDataType() const
{
	return FArcTacticsCostConsideration::StaticStruct();
}

FText FArcTacticsCostConsiderationNode::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "CostConsideration", "Can Afford Cost");
}

EArcTacticsExecutionRequirement FArcTacticsCostConsiderationNode::GetRequirements(FConstStructView) const
{
	return EArcTacticsExecutionRequirement::AbilityCollection;
}

FArcUtilityConsiderationResult FArcTacticsCooldownConsideration::Score(
	const FArcUtilityTarget&, const FArcUtilityContext& Context) const
{
	if (!Context.EntityManager || !Context.EntityManager->IsEntityActive(Entity))
	{
		return ArcTacticsConsiderationsPrivate::Reject(TEXT("NoEntityManagerForCooldown"));
	}
	FName RejectionReason;
	const FArcActiveAbility* ActiveAbility = ArcTacticsDecision::ResolveAbility(
		*Context.EntityManager, Entity, AbilityDefinition.Get(), false, RejectionReason);
	if (!ActiveAbility)
	{
		return ArcTacticsConsiderationsPrivate::Reject(RejectionReason);
	}
	const UArcAbilityDefinition* Definition = ActiveAbility->Definition.Get();
	if (!Definition->Cooldown.IsValid())
	{
		return ArcTacticsConsiderationsPrivate::Accept(1.0f);
	}
	const FArcAbilityCooldown* Cooldown = Definition->Cooldown.GetPtr<FArcAbilityCooldown>();
	if (!Cooldown)
	{
		return ArcTacticsConsiderationsPrivate::Reject(TEXT("InvalidCooldown"));
	}
	return Cooldown->CheckCooldown(*Context.EntityManager, Entity, ActiveAbility->Handle)
		? ArcTacticsConsiderationsPrivate::Accept(1.0f)
		: ArcTacticsConsiderationsPrivate::Reject(TEXT("CooldownActive"));
}

const UScriptStruct* FArcTacticsCooldownConsiderationNode::GetInstanceDataType() const
{
	return FArcTacticsCooldownConsideration::StaticStruct();
}

FText FArcTacticsCooldownConsiderationNode::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "CooldownConsideration", "Cooldown Ready");
}

EArcTacticsExecutionRequirement FArcTacticsCooldownConsiderationNode::GetRequirements(FConstStructView) const
{
	return EArcTacticsExecutionRequirement::AbilityCollection;
}

FArcUtilityConsiderationResult FArcTacticsHasEligibleTargetConsideration::Score(
	const FArcUtilityTarget&, const FArcUtilityContext&) const
{
	const bool bExpectsPresent = Expectation == EArcTacticsTargetPresence::Present;
	if (bHasEligibleTarget == bExpectsPresent)
	{
		return ArcTacticsConsiderationsPrivate::Accept(1.0f);
	}
	return ArcTacticsConsiderationsPrivate::Reject(
		bExpectsPresent ? TEXT("NoEligibleTarget") : TEXT("EligibleTargetPresent"));
}

const UScriptStruct* FArcTacticsHasEligibleTargetConsiderationNode::GetInstanceDataType() const
{
	return FArcTacticsHasEligibleTargetConsideration::StaticStruct();
}

FText FArcTacticsHasEligibleTargetConsiderationNode::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "HasEligibleTargetConsideration", "Has Eligible Target");
}

FArcUtilityConsiderationResult FArcTacticsCombatSlotsAvailableConsideration::Score(
	const FArcUtilityTarget&, const FArcUtilityContext& Context) const
{
	const UWorld* World = Context.World.Get();
	if (!Context.EntityManager || !World || !Context.EntityManager->IsEntityActive(Entity))
	{
		return ArcTacticsConsiderationsPrivate::Reject(TEXT("CombatSlotEntityMissing"));
	}
	const double Now = World->GetTimeSeconds();
	FVector OwnSlotPoint = FVector::ZeroVector;
	if (ArcCombatSlots::GetLiveSlotPoint(*Context.EntityManager, Context.QuerierEntity, Entity, Now, OwnSlotPoint))
	{
		return ArcTacticsConsiderationsPrivate::Accept(1.0f);
	}
	return ArcCombatSlots::CountLiveClaimers(*Context.EntityManager, Entity, Context.QuerierEntity, Now) >= MaxAttackers
		? ArcTacticsConsiderationsPrivate::Reject(TEXT("CombatSlotsFull"))
		: ArcTacticsConsiderationsPrivate::Accept(1.0f);
}

const UScriptStruct* FArcTacticsCombatSlotsAvailableConsiderationNode::GetInstanceDataType() const
{
	return FArcTacticsCombatSlotsAvailableConsideration::StaticStruct();
}

FText FArcTacticsCombatSlotsAvailableConsiderationNode::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "CombatSlotsAvailableConsideration", "Combat Slots Available");
}

#if WITH_EDITOR
bool FArcTacticsActionHistoryConsideration::Validate(FString& OutError) const
{
	if (!Tag.IsValid())
	{
		OutError = TEXT("Tag must be set.");
		return false;
	}
	return true;
}

bool FArcTacticsActionHistoryRecentConsideration::Validate(FString& OutError) const
{
	if (!Super::Validate(OutError))
	{
		return false;
	}
	if (LastEntries < 1)
	{
		OutError = TEXT("Last Entries must be at least one.");
		return false;
	}
	return true;
}

bool FArcTacticsActionHistoryWithinTimeConsideration::Validate(FString& OutError) const
{
	if (!Super::Validate(OutError))
	{
		return false;
	}
	if (!FMath::IsFinite(MaxAgeSeconds) || MaxAgeSeconds <= 0.0f)
	{
		OutError = TEXT("Max Age Seconds must be finite and greater than zero.");
		return false;
	}
	return true;
}

bool FArcTacticsActionHistoryAbsentConsideration::Validate(FString& OutError) const
{
	if (!Super::Validate(OutError))
	{
		return false;
	}
	if (LastEntries < 0)
	{
		OutError = TEXT("Last Entries must not be negative.");
		return false;
	}
	return true;
}
#endif

FArcUtilityConsiderationResult FArcTacticsActionHistoryRecentConsideration::Score(
	const FArcUtilityTarget&, const FArcUtilityContext&) const
{
	if (LastEntries < 1)
	{
		return ArcTacticsConsiderationsPrivate::Reject(TEXT("InvalidHistoryLastEntries"));
	}
	return ArcTacticsConsiderationsPrivate::HistoryContains(History, Tag, LastEntries, TNumericLimits<double>::Lowest())
		? ArcTacticsConsiderationsPrivate::Accept(1.0f)
		: ArcTacticsConsiderationsPrivate::Reject(TEXT("HistoryTagMissing"));
}

const UScriptStruct* FArcTacticsActionHistoryRecentConsiderationNode::GetInstanceDataType() const
{
	return FArcTacticsActionHistoryRecentConsideration::StaticStruct();
}

FText FArcTacticsActionHistoryRecentConsiderationNode::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "ActionHistoryRecentConsideration", "Action History Recent");
}

FArcUtilityConsiderationResult FArcTacticsActionHistoryWithinTimeConsideration::Score(
	const FArcUtilityTarget&, const FArcUtilityContext& Context) const
{
	const UWorld* World = Context.World.Get();
	if (!World)
	{
		return ArcTacticsConsiderationsPrivate::Reject(TEXT("HistoryWorldMissing"));
	}
	if (!FMath::IsFinite(MaxAgeSeconds) || MaxAgeSeconds <= 0.0f)
	{
		return ArcTacticsConsiderationsPrivate::Reject(TEXT("InvalidHistoryMaxAge"));
	}
	const double MinTime = World->GetTimeSeconds() - static_cast<double>(MaxAgeSeconds);
	return ArcTacticsConsiderationsPrivate::HistoryContains(History, Tag, 0, MinTime)
		? ArcTacticsConsiderationsPrivate::Accept(1.0f)
		: ArcTacticsConsiderationsPrivate::Reject(TEXT("HistoryTagMissing"));
}

const UScriptStruct* FArcTacticsActionHistoryWithinTimeConsiderationNode::GetInstanceDataType() const
{
	return FArcTacticsActionHistoryWithinTimeConsideration::StaticStruct();
}

FText FArcTacticsActionHistoryWithinTimeConsiderationNode::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "ActionHistoryWithinTimeConsideration", "Action History Within Time");
}

FArcUtilityConsiderationResult FArcTacticsActionHistoryAbsentConsideration::Score(
	const FArcUtilityTarget&, const FArcUtilityContext&) const
{
	if (LastEntries < 0)
	{
		return ArcTacticsConsiderationsPrivate::Reject(TEXT("InvalidHistoryLastEntries"));
	}
	return ArcTacticsConsiderationsPrivate::HistoryContains(History, Tag, LastEntries, TNumericLimits<double>::Lowest())
		? ArcTacticsConsiderationsPrivate::Reject(TEXT("HistoryTagPresent"))
		: ArcTacticsConsiderationsPrivate::Accept(1.0f);
}

const UScriptStruct* FArcTacticsActionHistoryAbsentConsiderationNode::GetInstanceDataType() const
{
	return FArcTacticsActionHistoryAbsentConsideration::StaticStruct();
}

FText FArcTacticsActionHistoryAbsentConsiderationNode::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "ActionHistoryAbsentConsideration", "Action History Absent");
}

#if WITH_EDITOR
FName FArcTacticsConstantConsiderationNode::GetIconName() const
{
	return TEXT("AppStyle|Icons.Check");
}

FColor FArcTacticsConstantConsiderationNode::GetIconColor() const
{
	return FColor(135, 180, 108);
}

FText FArcTacticsConstantConsiderationNode::GetDescription(
	const FGuid&,
	const FConstStructView InstanceData,
	const IPropertyBindingBindingCollectionOwner&,
	const bool bRichText) const
{
	const FArcTacticsConstantConsideration* Data = InstanceData.GetPtr<FArcTacticsConstantConsideration>();
	if (!Data)
	{
		return FText::GetEmpty();
	}
	const FText Value = ArcTacticsConsiderationsPrivate::Emphasize(FText::AsNumber(Data->Value), bRichText);
	return FText::Format(NSLOCTEXT("ArcTactics", "ConstantConsiderationDescription", "Constant {0}"), Value);
}

FName FArcTacticsDistanceToEntityConsiderationNode::GetIconName() const
{
	return TEXT("AppStyle|Icons.Search");
}

FColor FArcTacticsDistanceToEntityConsiderationNode::GetIconColor() const
{
	return FColor(86, 160, 216);
}

FText FArcTacticsDistanceToEntityConsiderationNode::GetDescription(
	const FGuid& NodeID,
	const FConstStructView InstanceData,
	const IPropertyBindingBindingCollectionOwner& BindingOwner,
	const bool bRichText) const
{
	const FArcTacticsDistanceToEntityConsideration* Data = InstanceData.GetPtr<FArcTacticsDistanceToEntityConsideration>();
	return Data
		? ArcTacticsConsiderationsPrivate::DescribeDistance(
			*Data, GET_MEMBER_NAME_CHECKED(FArcTacticsDistanceToEntityConsideration, Entity), NodeID, BindingOwner, bRichText)
		: FText::GetEmpty();
}

FName FArcTacticsDistanceToLocationConsiderationNode::GetIconName() const
{
	return TEXT("AppStyle|Icons.Search");
}

FColor FArcTacticsDistanceToLocationConsiderationNode::GetIconColor() const
{
	return FColor(86, 160, 216);
}

FText FArcTacticsDistanceToLocationConsiderationNode::GetDescription(
	const FGuid& NodeID,
	const FConstStructView InstanceData,
	const IPropertyBindingBindingCollectionOwner& BindingOwner,
	const bool bRichText) const
{
	const FArcTacticsDistanceToLocationConsideration* Data = InstanceData.GetPtr<FArcTacticsDistanceToLocationConsideration>();
	return Data
		? ArcTacticsConsiderationsPrivate::DescribeDistance(
			*Data, GET_MEMBER_NAME_CHECKED(FArcTacticsDistanceToLocationConsideration, Location), NodeID, BindingOwner, bRichText)
		: FText::GetEmpty();
}

FName FArcTacticsOwnedTagsConsiderationNode::GetIconName() const
{
	return TEXT("AppStyle|Icons.Tag");
}

FColor FArcTacticsOwnedTagsConsiderationNode::GetIconColor() const
{
	return FColor(178, 111, 214);
}

FText FArcTacticsOwnedTagsConsiderationNode::GetDescription(
	const FGuid& NodeID,
	const FConstStructView InstanceData,
	const IPropertyBindingBindingCollectionOwner& BindingOwner,
	const bool bRichText) const
{
	const FArcTacticsOwnedTagsConsideration* Data = InstanceData.GetPtr<FArcTacticsOwnedTagsConsideration>();
	if (!Data)
	{
		return FText::GetEmpty();
	}
	const FText Tags = ArcTacticsConsiderationsPrivate::GetSourceOrField(
		NodeID,
		GET_MEMBER_NAME_CHECKED(FArcTacticsOwnedTagsConsideration, Tags),
		BindingOwner,
		bRichText);
	const FText MatchScore = FText::AsNumber(Data->MatchScore);
	const FText MismatchScore = FText::AsNumber(Data->MismatchScore);
	return bRichText
		? FText::Format(NSLOCTEXT("ArcTactics", "OwnedTagsRich", "Match {0} <s>score {1}, otherwise {2}</>"), Tags, MatchScore, MismatchScore)
		: FText::Format(NSLOCTEXT("ArcTactics", "OwnedTagsPlain", "Match {0}: score {1}, otherwise {2}"), Tags, MatchScore, MismatchScore);
}

FName FArcTacticsCostConsiderationNode::GetIconName() const
{
	return TEXT("AppStyle|Icons.Download");
}

FColor FArcTacticsCostConsiderationNode::GetIconColor() const
{
	return FColor(224, 155, 65);
}

FText FArcTacticsCostConsiderationNode::GetDescription(
	const FGuid& NodeID,
	const FConstStructView InstanceData,
	const IPropertyBindingBindingCollectionOwner& BindingOwner,
	const bool bRichText) const
{
	const FArcTacticsCostConsideration* Data = InstanceData.GetPtr<FArcTacticsCostConsideration>();
	if (!Data)
	{
		return FText::GetEmpty();
	}
	const FText Ability = ArcTacticsConsiderationsPrivate::DescribeAbility(Data->AbilityDefinition.Get(), bRichText);
	const FText Entity = ArcTacticsConsiderationsPrivate::GetSourceOrField(
		NodeID,
		GET_MEMBER_NAME_CHECKED(FArcTacticsCostConsideration, Entity),
		BindingOwner,
		bRichText);
	return bRichText
		? FText::Format(NSLOCTEXT("ArcTactics", "CostRich", "Can {0} <s>afford</> {1}"), Entity, Ability)
		: FText::Format(NSLOCTEXT("ArcTactics", "CostPlain", "Can {0} afford {1}"), Entity, Ability);
}

FName FArcTacticsCooldownConsiderationNode::GetIconName() const
{
	return TEXT("AppStyle|Icons.Settings");
}

FColor FArcTacticsCooldownConsiderationNode::GetIconColor() const
{
	return FColor(88, 182, 190);
}

FText FArcTacticsCooldownConsiderationNode::GetDescription(
	const FGuid& NodeID,
	const FConstStructView InstanceData,
	const IPropertyBindingBindingCollectionOwner& BindingOwner,
	const bool bRichText) const
{
	const FArcTacticsCooldownConsideration* Data = InstanceData.GetPtr<FArcTacticsCooldownConsideration>();
	if (!Data)
	{
		return FText::GetEmpty();
	}
	const FText Ability = ArcTacticsConsiderationsPrivate::DescribeAbility(Data->AbilityDefinition.Get(), bRichText);
	const FText Entity = ArcTacticsConsiderationsPrivate::GetSourceOrField(
		NodeID,
		GET_MEMBER_NAME_CHECKED(FArcTacticsCooldownConsideration, Entity),
		BindingOwner,
		bRichText);
	return bRichText
		? FText::Format(NSLOCTEXT("ArcTactics", "CooldownRich", "Cooldown ready for {0} <s>on</> {1}"), Ability, Entity)
		: FText::Format(NSLOCTEXT("ArcTactics", "CooldownPlain", "Cooldown ready for {0} on {1}"), Ability, Entity);
}

FName FArcTacticsHasEligibleTargetConsiderationNode::GetIconName() const
{
	return TEXT("AppStyle|Icons.Filter");
}

FColor FArcTacticsHasEligibleTargetConsiderationNode::GetIconColor() const
{
	return FColor(213, 115, 132);
}

FText FArcTacticsHasEligibleTargetConsiderationNode::GetDescription(
	const FGuid& NodeID,
	const FConstStructView InstanceData,
	const IPropertyBindingBindingCollectionOwner& BindingOwner,
	const bool bRichText) const
{
	const FArcTacticsHasEligibleTargetConsideration* Data = InstanceData.GetPtr<FArcTacticsHasEligibleTargetConsideration>();
	if (!Data)
	{
		return FText::GetEmpty();
	}
	const FText Source = ArcTacticsConsiderationsPrivate::GetSourceOrField(
		NodeID,
		GET_MEMBER_NAME_CHECKED(FArcTacticsHasEligibleTargetConsideration, bHasEligibleTarget),
		BindingOwner,
		bRichText);
	const FText Expectation = StaticEnum<EArcTacticsTargetPresence>()->GetDisplayNameTextByValue(static_cast<int64>(Data->Expectation));
	const FText DisplayExpectation = ArcTacticsConsiderationsPrivate::Emphasize(Expectation, bRichText);
	return bRichText
		? FText::Format(NSLOCTEXT("ArcTactics", "EligibleTargetRich", "Expect {0} <s>from</> {1}"), DisplayExpectation, Source)
		: FText::Format(NSLOCTEXT("ArcTactics", "EligibleTargetPlain", "Expect {0} from {1}"), Expectation, Source);
}

FName FArcTacticsCombatSlotsAvailableConsiderationNode::GetIconName() const
{
	return TEXT("AppStyle|Icons.Filter");
}

FColor FArcTacticsCombatSlotsAvailableConsiderationNode::GetIconColor() const
{
	return FColor(206, 92, 76);
}

FText FArcTacticsCombatSlotsAvailableConsiderationNode::GetDescription(
	const FGuid& NodeID,
	const FConstStructView InstanceData,
	const IPropertyBindingBindingCollectionOwner& BindingOwner,
	const bool bRichText) const
{
	const FArcTacticsCombatSlotsAvailableConsideration* Data =
		InstanceData.GetPtr<FArcTacticsCombatSlotsAvailableConsideration>();
	if (!Data)
	{
		return FText::GetEmpty();
	}
	const FText Entity = ArcTacticsConsiderationsPrivate::GetSourceOrField(
		NodeID,
		GET_MEMBER_NAME_CHECKED(FArcTacticsCombatSlotsAvailableConsideration, Entity),
		BindingOwner,
		bRichText);
	const FText MaxAttackers = FText::AsNumber(Data->MaxAttackers);
	return bRichText
		? FText::Format(NSLOCTEXT("ArcTactics", "CombatSlotsRich", "Fewer than {0} <s>attackers on</> {1}"), MaxAttackers, Entity)
		: FText::Format(NSLOCTEXT("ArcTactics", "CombatSlotsPlain", "Fewer than {0} attackers on {1}"), MaxAttackers, Entity);
}

FName FArcTacticsActionHistoryRecentConsiderationNode::GetIconName() const
{
	return TEXT("AppStyle|Icons.Tag");
}

FColor FArcTacticsActionHistoryRecentConsiderationNode::GetIconColor() const
{
	return FColor(122, 150, 222);
}

FText FArcTacticsActionHistoryRecentConsiderationNode::GetDescription(
	const FGuid& NodeID,
	const FConstStructView InstanceData,
	const IPropertyBindingBindingCollectionOwner& BindingOwner,
	const bool bRichText) const
{
	const FArcTacticsActionHistoryRecentConsideration* Data =
		InstanceData.GetPtr<FArcTacticsActionHistoryRecentConsideration>();
	if (!Data)
	{
		return FText::GetEmpty();
	}
	const FText Tag = ArcTacticsConsiderationsPrivate::DescribeHistoryTag(Data->Tag, bRichText);
	const FText History = ArcTacticsConsiderationsPrivate::GetSourceOrField(
		NodeID,
		GET_MEMBER_NAME_CHECKED(FArcTacticsActionHistoryConsideration, History),
		BindingOwner,
		bRichText);
	const FText LastEntries = FText::AsNumber(Data->LastEntries);
	return bRichText
		? FText::Format(NSLOCTEXT("ArcTactics", "HistoryRecentRich", "{0} <s>in last {1} of</> {2}"), Tag, LastEntries, History)
		: FText::Format(NSLOCTEXT("ArcTactics", "HistoryRecentPlain", "{0} in last {1} of {2}"), Tag, LastEntries, History);
}

FName FArcTacticsActionHistoryWithinTimeConsiderationNode::GetIconName() const
{
	return TEXT("AppStyle|Icons.Tag");
}

FColor FArcTacticsActionHistoryWithinTimeConsiderationNode::GetIconColor() const
{
	return FColor(122, 150, 222);
}

FText FArcTacticsActionHistoryWithinTimeConsiderationNode::GetDescription(
	const FGuid& NodeID,
	const FConstStructView InstanceData,
	const IPropertyBindingBindingCollectionOwner& BindingOwner,
	const bool bRichText) const
{
	const FArcTacticsActionHistoryWithinTimeConsideration* Data =
		InstanceData.GetPtr<FArcTacticsActionHistoryWithinTimeConsideration>();
	if (!Data)
	{
		return FText::GetEmpty();
	}
	const FText Tag = ArcTacticsConsiderationsPrivate::DescribeHistoryTag(Data->Tag, bRichText);
	const FText History = ArcTacticsConsiderationsPrivate::GetSourceOrField(
		NodeID,
		GET_MEMBER_NAME_CHECKED(FArcTacticsActionHistoryConsideration, History),
		BindingOwner,
		bRichText);
	const FText MaxAge = FText::AsNumber(Data->MaxAgeSeconds);
	return bRichText
		? FText::Format(NSLOCTEXT("ArcTactics", "HistoryWithinTimeRich", "{0} <s>within {1} s in</> {2}"), Tag, MaxAge, History)
		: FText::Format(NSLOCTEXT("ArcTactics", "HistoryWithinTimePlain", "{0} within {1} s in {2}"), Tag, MaxAge, History);
}

FName FArcTacticsActionHistoryAbsentConsiderationNode::GetIconName() const
{
	return TEXT("AppStyle|Icons.Filter");
}

FColor FArcTacticsActionHistoryAbsentConsiderationNode::GetIconColor() const
{
	return FColor(122, 150, 222);
}

FText FArcTacticsActionHistoryAbsentConsiderationNode::GetDescription(
	const FGuid& NodeID,
	const FConstStructView InstanceData,
	const IPropertyBindingBindingCollectionOwner& BindingOwner,
	const bool bRichText) const
{
	const FArcTacticsActionHistoryAbsentConsideration* Data =
		InstanceData.GetPtr<FArcTacticsActionHistoryAbsentConsideration>();
	if (!Data)
	{
		return FText::GetEmpty();
	}
	const FText Tag = ArcTacticsConsiderationsPrivate::DescribeHistoryTag(Data->Tag, bRichText);
	const FText History = ArcTacticsConsiderationsPrivate::GetSourceOrField(
		NodeID,
		GET_MEMBER_NAME_CHECKED(FArcTacticsActionHistoryConsideration, History),
		BindingOwner,
		bRichText);
	if (Data->LastEntries <= 0)
	{
		return bRichText
			? FText::Format(NSLOCTEXT("ArcTactics", "HistoryAbsentAllRich", "{0} <s>not in</> {1}"), Tag, History)
			: FText::Format(NSLOCTEXT("ArcTactics", "HistoryAbsentAllPlain", "{0} not in {1}"), Tag, History);
	}
	const FText LastEntries = FText::AsNumber(Data->LastEntries);
	return bRichText
		? FText::Format(NSLOCTEXT("ArcTactics", "HistoryAbsentRich", "{0} <s>not in last {1} of</> {2}"), Tag, LastEntries, History)
		: FText::Format(NSLOCTEXT("ArcTactics", "HistoryAbsentPlain", "{0} not in last {1} of {2}"), Tag, LastEntries, History);
}
#endif
