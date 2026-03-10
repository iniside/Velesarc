// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMCP/StateTree/ArcStateTreeTypes.h"

/**
 * Parses a JSON description into FArcSTDescription and validates structural correctness.
 * Does NOT validate against schema or resolve aliases — that's the builder's job.
 */
class FArcStateTreeJsonParser
{
public:
	/** Parse the JSON object into a description. Returns false on structural errors. */
	bool Parse(const TSharedPtr<FJsonObject>& Json, FArcSTDescription& OutDesc);

	/** Get accumulated errors from last Parse call. */
	const TArray<FString>& GetErrors() const { return Errors; }
	const TArray<FString>& GetWarnings() const { return Warnings; }

private:
	bool ParseNode(const TSharedPtr<FJsonObject>& Json, FArcSTNodeDesc& OutNode, const FString& Context);
	bool ParseState(const TSharedPtr<FJsonObject>& Json, FArcSTStateDesc& OutState);
	bool ParseTransition(const TSharedPtr<FJsonObject>& Json, FArcSTTransitionDesc& OutTrans, const FString& StateId);
	bool ParseBinding(const TSharedPtr<FJsonObject>& Json, FArcSTBindingDesc& OutBinding);
	bool ValidateReferences(const FArcSTDescription& Desc);

	void AddError(const FString& Msg) { Errors.Add(Msg); }
	void AddWarning(const FString& Msg) { Warnings.Add(Msg); }

	TArray<FString> Errors;
	TArray<FString> Warnings;
};
