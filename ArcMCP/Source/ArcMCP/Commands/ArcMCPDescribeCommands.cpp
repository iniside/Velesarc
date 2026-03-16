// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMCP/Commands/ArcMCPDescribeCommands.h"

#include "Commands/ECACommand.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UObject/UObjectIterator.h"
#include "ArcCore/Items/Fragments/ArcItemFragment.h"
#include "ArcCore/Items/Fragments/ArcFragmentDescription.h"

REGISTER_ECA_COMMAND(FArcMCPCommand_DescribeFragments)

FECACommandResult FArcMCPCommand_DescribeFragments::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString StructNameFilter;
	GetStringParam(Params, TEXT("struct_name"), StructNameFilter, /*bRequired=*/ false);
	StructNameFilter.RemoveFromStart(TEXT("F"));

	FString BaseTypeFilter;
	GetStringParam(Params, TEXT("base_type"), BaseTypeFilter, /*bRequired=*/ false);

	const UScriptStruct* FragmentBase = FArcItemFragment::StaticStruct();
	const UScriptStruct* ScalableBase = FArcScalableFloatItemFragment::StaticStruct();

	TArray<TSharedPtr<FJsonValue>> ResultArray;

	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		UScriptStruct* Struct = *It;

		const bool bIsFragment = Struct->IsChildOf(FragmentBase) && Struct != FragmentBase;
		const bool bIsScalable = Struct->IsChildOf(ScalableBase) && Struct != ScalableBase;

		if (!bIsFragment && !bIsScalable)
		{
			continue;
		}

		// Skip hidden/base structs
		if (Struct->HasMetaData(TEXT("Hidden")))
		{
			continue;
		}

		// Apply base_type filter
		if (!BaseTypeFilter.IsEmpty())
		{
			if (BaseTypeFilter == TEXT("fragment") && !bIsFragment)
			{
				continue;
			}
			if (BaseTypeFilter == TEXT("scalable_float") && !bIsScalable)
			{
				continue;
			}
		}

		// Apply struct_name filter
		if (!StructNameFilter.IsEmpty() && Struct->GetName() != StructNameFilter)
		{
			continue;
		}

		// Allocate a default instance, call GetDescription, then destroy
		const int32 StructSize = Struct->GetStructureSize();
		const int32 StructAlign = Struct->GetMinAlignment();
		uint8* Memory = static_cast<uint8*>(FMemory::Malloc(StructSize, StructAlign));
		Struct->InitializeStruct(Memory);

		FArcFragmentDescription Desc;
		if (bIsFragment)
		{
			const FArcItemFragment* Fragment = reinterpret_cast<const FArcItemFragment*>(Memory);
			Desc = Fragment->GetDescription(Struct);
		}
		else
		{
			const FArcScalableFloatItemFragment* Fragment = reinterpret_cast<const FArcScalableFloatItemFragment*>(Memory);
			Desc = Fragment->GetDescription(Struct);
		}

		Struct->DestroyStruct(Memory);
		FMemory::Free(Memory);

		ResultArray.Add(MakeShared<FJsonValueObject>(Desc.ToJson()));
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetArrayField(TEXT("fragments"), ResultArray);
	Result->SetNumberField(TEXT("count"), ResultArray.Num());

	return FECACommandResult::Success(Result);
}
