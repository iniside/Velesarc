// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "ArcAttributeObserverTestTypes.generated.h"

// ---------------------------------------------------------------------------
// Test-only attribute fragment types used by ArcAttributeObserverTests.cpp
// ---------------------------------------------------------------------------

USTRUCT(meta = (Hidden))
struct FArcObsTest_FragA : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttribute AttrA = { 10.f, 10.f };

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcObsTest_FragA, AttrA)
};

template <>
struct TMassFragmentTraits<FArcObsTest_FragA> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT(meta = (Hidden))
struct FArcObsTest_FragB : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttribute AttrB = { 20.f, 20.f };

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcObsTest_FragB, AttrB)
};

template <>
struct TMassFragmentTraits<FArcObsTest_FragB> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT(meta = (Hidden))
struct FArcObsTest_FragC : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttribute AttrC = { 30.f, 30.f };

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcObsTest_FragC, AttrC)
};

template <>
struct TMassFragmentTraits<FArcObsTest_FragC> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

// ---------------------------------------------------------------------------
// Test-only handler — captures OnObservedAttributeChanged calls
// ---------------------------------------------------------------------------

USTRUCT(meta = (Hidden, ToolTip = "Test-only handler: counts OnObservedAttributeChanged calls and records the changed attribute and its latest values.\nAssign to the observer attribute, such as FArcObsTest_FragB::AttrB. Target: the observer's set and the observed attribute's set (such as FArcObsTest_FragA::AttrA), with the dependency declared in Inputs; the callback itself reads no other sets.\nSource: no required attribute sets; target means the entity that owns the attribute."))
struct FArcAttributeHandler_Test_Observer : public FArcAttributeHandler
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FArcAttributeRef> Inputs;

	mutable int32 ObservedChangeCount = 0;
	mutable FArcAttributeRef LastChangedRef;
	mutable float LastOldValue = 0.f;
	mutable float LastNewValue = 0.f;

	virtual void OnObservedAttributeChanged(
		FArcAttributeHandlerContext& Context,
		FArcAttribute&               Attribute,
		const FArcAttributeRef&      ChangedRef,
		float                        OldValue,
		float                        NewValue) const override
	{
		++ObservedChangeCount;
		LastChangedRef = ChangedRef;
		LastOldValue   = OldValue;
		LastNewValue   = NewValue;
	}

	virtual void GatherInputs(TArray<FArcAttributeRef>& OutInputs) const override
	{
		OutInputs.Append(Inputs);
	}
};
