// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IPropertyHandle;

namespace ArcEffectSummaryText
{
	/** Returns "FragmentType.PropertyName" or "None" for an FArcAttributeRef property handle. */
	FText FormatAttributeRef(const TSharedPtr<IPropertyHandle>& AttributeHandle);

	/** Returns the display name of the EArcModifierOp enum value held by OperationHandle. */
	FText OperationToText(const TSharedPtr<IPropertyHandle>& OperationHandle);

	/**
	 * Returns a compact summary of an FArcGameplayTagRequirements property handle.
	 * Examples: "Any", "Req:2  Ign:1", "Req:1  +Query", "Ign:3  +Query"
	 */
	FText FormatTagRequirements(const TSharedPtr<IPropertyHandle>& TagFilterHandle);

	/** Multi-line tooltip listing the actual require/ignore tags (and query) of an FArcGameplayTagRequirements handle. */
	FText BuildTagRequirementsTooltip(const TSharedPtr<IPropertyHandle>& TagFilterHandle);

	/** Returns the display name of the inner struct of an FInstancedStruct property handle, or "None". */
	FText InstancedStructTypeName(const TSharedPtr<IPropertyHandle>& InstancedHandle);

	/** One-line magnitude summary for an effect/scoped modifier: Simple -> ScalableFloat string, SetByCaller -> tag, Custom -> instanced-struct type. */
	FText BuildModifierMagnitude(
		const TSharedPtr<IPropertyHandle>& TypeHandle,
		const TSharedPtr<IPropertyHandle>& MagnitudeHandle,
		const TSharedPtr<IPropertyHandle>& SetByCallerTagHandle,
		const TSharedPtr<IPropertyHandle>& CustomMagnitudeHandle);
}
