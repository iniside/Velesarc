// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMCP/StateTree/ArcStateTreeNodeRegistry.h"

#include "StateTreeTaskBase.h"
#include "StateTreeConditionBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeConsiderationBase.h"
#include "StateTreePropertyFunctionBase.h"
#include "StateTreeSchema.h"
#include "UObject/UObjectIterator.h"
#include "StructUtils/InstancedStruct.h"
#include "Algo/Sort.h"

FArcStateTreeNodeRegistry& FArcStateTreeNodeRegistry::Get()
{
	static FArcStateTreeNodeRegistry Instance;
	return Instance;
}

void FArcStateTreeNodeRegistry::Initialize()
{
	if (bInitialized) return;

	struct FBaseInfo
	{
		const UScriptStruct* Base;
		FString Category;
	};

	const TArray<FBaseInfo> Bases = {
		{ FStateTreeTaskBase::StaticStruct(),                    TEXT("task") },
		{ FStateTreeConditionBase::StaticStruct(),               TEXT("condition") },
		{ FStateTreeEvaluatorBase::StaticStruct(),               TEXT("evaluator") },
		{ FStateTreeConsiderationBase::StaticStruct(),           TEXT("consideration") },
		{ FStateTreePropertyFunctionCommonBase::StaticStruct(),  TEXT("property_function") },
	};

	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		const UScriptStruct* Struct = *It;

		for (const FBaseInfo& Info : Bases)
		{
			if (Struct->IsChildOf(Info.Base) && Struct != Info.Base)
			{
				// Skip abstract bases (the four bases themselves are already excluded above)
				if (Struct->HasMetaData(TEXT("Abstract")))
				{
					continue;
				}
				RegisterStruct(Struct, Info.Base, Info.Category);
				break; // a struct only belongs to one category
			}
		}
	}

	bInitialized = true;
	UE_LOG(LogTemp, Log, TEXT("[ArcMCP] StateTree node registry initialized: %d nodes"), ByAlias.Num());
}

void FArcStateTreeNodeRegistry::RegisterStruct(
	const UScriptStruct* Struct,
	const UScriptStruct* BaseStruct,
	const FString& Category)
{
	const FString Alias = GenerateAlias(Struct, Category);

	FArcSTNodeTypeDesc Desc;
	Desc.Alias = Alias;
	Desc.StructName = Struct->GetName();
	Desc.Category = Category;
	Desc.Struct = Struct;

	// Extract struct-level metadata
#if WITH_EDITOR
	{
		const FText DisplayText = Struct->GetDisplayNameText();
		if (!DisplayText.IsEmpty())
		{
			Desc.DisplayName = DisplayText.ToString();
		}
		const FText TooltipText = Struct->GetToolTipText();
		if (!TooltipText.IsEmpty())
		{
			Desc.Description = TooltipText.ToString();
		}
		// Also check USTRUCT meta DisplayName (takes priority over computed display name)
		if (Struct->HasMetaData(TEXT("DisplayName")))
		{
			Desc.DisplayName = Struct->GetMetaData(TEXT("DisplayName"));
		}
	}
#endif

	const bool bIsPropertyFunction = (Category == TEXT("property_function"));

	// Instantiate node to query instance data types
	const int32 Size = Struct->GetStructureSize();
	const int32 Align = Struct->GetMinAlignment();
	uint8* Memory = static_cast<uint8*>(FMemory::Malloc(Size, Align));
	Struct->InitializeStruct(Memory);

	const FStateTreeNodeBase& Node = *reinterpret_cast<const FStateTreeNodeBase*>(Memory);

	if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(Node.GetInstanceDataType()))
	{
		Desc.InstanceDataStruct = InstanceType;
		// For property functions, classify instance data properties as input/output
		Desc.InstanceDataProperties = ReflectProperties(InstanceType, bIsPropertyFunction);
	}
	if (const UScriptStruct* RuntimeType = Cast<const UScriptStruct>(Node.GetExecutionRuntimeDataType()))
	{
		Desc.ExecutionRuntimeDataStruct = RuntimeType;
	}

	Struct->DestroyStruct(Memory);
	FMemory::Free(Memory);

	// Reflect node properties (skip inherited base properties)
	Desc.NodeProperties = ReflectProperties(Struct);

	// TODO: Extract external data handles (requires deeper introspection of TStateTreeExternalDataHandle fields)

	StructNameToAlias.Add(Struct->GetName(), Alias);
	ByAlias.Add(Alias, MoveTemp(Desc));
}

FString FArcStateTreeNodeRegistry::GenerateAlias(const UScriptStruct* Struct, const FString& Category)
{
	FString Name = Struct->GetName();

	// Strip common prefixes
	// Longest prefixes first to avoid partial matches (FArc before FArcMass)
	for (const FString& Prefix : { TEXT("FArcAISTT_"), TEXT("FArcMass"), TEXT("FStateTree"), TEXT("FArc"), TEXT("FMass") })
	{
		if (Name.StartsWith(Prefix))
		{
			Name = Name.Mid(Prefix.Len());
			break;
		}
	}

	// Strip category suffix
	static const TMap<FString, FString> SuffixMap = {
		{ TEXT("task"), TEXT("Task") },
		{ TEXT("condition"), TEXT("Condition") },
		{ TEXT("evaluator"), TEXT("Evaluator") },
		{ TEXT("consideration"), TEXT("Consideration") },
		{ TEXT("property_function"), TEXT("PropertyFunction") },
	};
	if (const FString* Suffix = SuffixMap.Find(Category))
	{
		if (Name.EndsWith(*Suffix))
		{
			Name = Name.Left(Name.Len() - Suffix->Len());
		}
	}

	// Convert PascalCase to snake_case
	FString Result;
	for (int32 i = 0; i < Name.Len(); ++i)
	{
		const TCHAR Ch = Name[i];
		if (FChar::IsUpper(Ch) && i > 0 && !FChar::IsUpper(Name[i - 1]))
		{
			Result += TEXT('_');
		}
		Result += FChar::ToLower(Ch);
	}

	// Clean up double underscores
	while (Result.Contains(TEXT("__")))
	{
		Result = Result.Replace(TEXT("__"), TEXT("_"));
	}
	Result.TrimStartAndEndInline();
	if (Result.StartsWith(TEXT("_"))) Result = Result.Mid(1);
	if (Result.EndsWith(TEXT("_"))) Result = Result.Left(Result.Len() - 1);

	// Property functions get a function_ prefix for clarity
	if (Category == TEXT("property_function"))
	{
		Result = TEXT("function_") + Result;
	}

	return Result;
}

TArray<FArcSTPropertyDesc> FArcStateTreeNodeRegistry::ReflectProperties(const UScriptStruct* Struct, bool bClassifyRoles)
{
	TArray<FArcSTPropertyDesc> Result;
	if (!Struct) return Result;

	// Allocate default instance
	const int32 Size = Struct->GetStructureSize();
	const int32 Align = Struct->GetMinAlignment();
	uint8* DefaultMemory = static_cast<uint8*>(FMemory::Malloc(Size, Align));
	Struct->InitializeStruct(DefaultMemory);

	for (TFieldIterator<FProperty> PropIt(Struct, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		const FProperty* Prop = *PropIt;

		// Skip non-editable / hidden
		if (!Prop->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
		{
			continue;
		}

		FArcSTPropertyDesc Desc;
		Desc.Name = Prop->GetName();
		Desc.TypeName = Prop->GetCPPType();
		Desc.DefaultValue = GetPropertyDefaultString(Prop, DefaultMemory);

#if WITH_EDITOR
		// Extract property-level metadata
		{
			const FText DisplayText = Prop->GetDisplayNameText();
			if (!DisplayText.IsEmpty())
			{
				Desc.DisplayName = DisplayText.ToString();
			}
			const FText TooltipText = Prop->GetToolTipText();
			if (!TooltipText.IsEmpty())
			{
				Desc.Description = TooltipText.ToString();
			}
		}
#endif

		// Classify input/output role for property function instance data
#if WITH_METADATA
		if (bClassifyRoles)
		{
			const FString PropCategory = Prop->GetMetaData(TEXT("Category"));
			if (PropCategory.Equals(TEXT("Output"), ESearchCase::IgnoreCase))
			{
				Desc.Role = TEXT("output");
			}
			else if (PropCategory.Equals(TEXT("Parameter"), ESearchCase::IgnoreCase)
				|| PropCategory.Equals(TEXT("Param"), ESearchCase::IgnoreCase))
			{
				Desc.Role = TEXT("input");
			}
			else if (!PropCategory.IsEmpty())
			{
				// Other categories default to input for property functions
				Desc.Role = TEXT("input");
			}
		}
#endif

		Result.Add(MoveTemp(Desc));
	}

	Struct->DestroyStruct(DefaultMemory);
	FMemory::Free(DefaultMemory);

	return Result;
}

FString FArcStateTreeNodeRegistry::GetPropertyDefaultString(const FProperty* Property, const void* ContainerPtr)
{
	FString Result;
	Property->ExportTextItem_Direct(Result, Property->ContainerPtrToValuePtr<void>(ContainerPtr), nullptr, nullptr, PPF_None);
	return Result;
}

const UScriptStruct* FArcStateTreeNodeRegistry::ResolveNodeType(const FString& AliasOrStructName) const
{
	// Try alias first
	if (const FArcSTNodeTypeDesc* Desc = ByAlias.Find(AliasOrStructName))
	{
		return Desc->Struct;
	}
	// Try struct name
	if (const FString* Alias = StructNameToAlias.Find(AliasOrStructName))
	{
		if (const FArcSTNodeTypeDesc* Desc = ByAlias.Find(*Alias))
		{
			return Desc->Struct;
		}
	}
	return nullptr;
}

UClass* FArcStateTreeNodeRegistry::FindSchemaClass(const FString& SchemaName)
{
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (!It->IsChildOf(UStateTreeSchema::StaticClass())) continue;
		if (It->GetName() == SchemaName || It->GetName() == (TEXT("U") + SchemaName)
			|| (TEXT("U") + It->GetName()) == SchemaName)
		{
			return *It;
		}
	}
	return nullptr;
}

FString FArcStateTreeNodeRegistry::GetCategory(const UScriptStruct* Struct) const
{
	if (const FString* Alias = StructNameToAlias.Find(Struct->GetName()))
	{
		if (const FArcSTNodeTypeDesc* Desc = ByAlias.Find(*Alias))
		{
			return Desc->Category;
		}
	}
	return FString();
}

TArray<const FArcSTNodeTypeDesc*> FArcStateTreeNodeRegistry::GetNodes(
	const UStateTreeSchema* Schema,
	const FString& CategoryFilter,
	const FString& TextFilter) const
{
	TArray<const FArcSTNodeTypeDesc*> Result;

	for (const auto& Pair : ByAlias)
	{
		const FArcSTNodeTypeDesc& Desc = Pair.Value;

		// Category filter
		if (!CategoryFilter.IsEmpty() && Desc.Category != CategoryFilter)
		{
			continue;
		}

		// Schema filter
		if (Schema && Desc.Struct)
		{
			if (!Schema->IsStructAllowed(Desc.Struct))
			{
				continue;
			}
		}

		// Text filter (substring match on alias or struct name)
		if (!TextFilter.IsEmpty())
		{
			if (!Desc.Alias.Contains(TextFilter) && !Desc.StructName.Contains(TextFilter))
			{
				continue;
			}
		}

		Result.Add(&Desc);
	}

	// Sort by alias for stable output
	Algo::Sort(Result, [](const FArcSTNodeTypeDesc* A, const FArcSTNodeTypeDesc* B)
	{
		return A->Alias < B->Alias;
	});

	return Result;
}

const FArcSTNodeTypeDesc* FArcStateTreeNodeRegistry::GetDescriptor(const UScriptStruct* Struct) const
{
	if (const FString* Alias = StructNameToAlias.Find(Struct->GetName()))
	{
		return ByAlias.Find(*Alias);
	}
	return nullptr;
}
