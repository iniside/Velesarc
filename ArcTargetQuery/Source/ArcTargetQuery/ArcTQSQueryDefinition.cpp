// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSQueryDefinition.h"

#include "ArcTQSCompiler.h"
#include "ArcTQSGenerator.h"
#include "ArcTQSPropertyFunction.h"
#include "ArcTargetQueryLogs.h"
#include "PropertyBindingDataView.h"
#include "PropertyBindingTypes.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTQSQueryDefinition)

#if WITH_EDITOR
FArcTQSOnDefinitionCompiled ArcTQS::Delegates::OnPostCompile;
FArcTQSOnDefinitionCompiled ArcTQS::Delegates::OnPreviewSetupChanged;

namespace ArcTQSQueryDefinitionPrivate
{
	uint32 NextRevision(const uint32 Revision)
	{
		const uint32 Next = Revision + 1;
		return Next == 0 ? 1 : Next;
	}

	void EnsureUniqueID(FGuid& ID, TSet<FGuid>& UsedIDs)
	{
		if (!ID.IsValid() || UsedIDs.Contains(ID))
		{
			ID = FGuid::NewGuid();
		}
		UsedIDs.Add(ID);
	}

	FString GetNodeDisplayName(const UScriptStruct* Struct)
	{
		return Struct ? Struct->GetDisplayNameText().ToString() : FString(TEXT("Node"));
	}

	void AddDescriptor(
		const UArcTQSQueryDefinition& Definition,
		const FGuid ID,
		TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>>& OutStructDescs)
	{
		TInstancedStruct<FPropertyBindingBindableStructDescriptor> Descriptor;
		if (Definition.GetBindableStructByID(ID, Descriptor))
		{
			OutStructDescs.Add(MoveTemp(Descriptor));
		}
	}
}
#endif

void UArcTQSQueryDefinition::PostInitProperties()
{
	Super::PostInitProperties();
#if WITH_EDITOR
	InitializeEditorBindingsOwner();
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_NeedLoad | RF_WasLoaded))
	{
		RepairNodeIDs();
	}
#endif
}

void UArcTQSQueryDefinition::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITOR
	InitializeEditorBindingsOwner();
	RepairNodeIDs();
	PreviewSetup.Reference.SetDefinition(this);
	if (CompiledData.Version != FArcTQSCompiledData::CurrentVersion)
	{
		TArray<FArcTQSCompilerDiagnostic> Diagnostics;
		CompileEditorData(Diagnostics);
		return;
	}
#endif
	CompiledData.ResolvePaths(Parameters.GetPropertyBagStruct());
}

#if WITH_EDITOR
void UArcTQSQueryDefinition::InitializeEditorBindingsOwner()
{
	TScriptInterface<IPropertyBindingBindingCollectionOwner> Owner;
	Owner.SetObject(this);
	Owner.SetInterface(this);
	EditorBindings.SetBindingsOwner(Owner);
}

void UArcTQSQueryDefinition::RepairNodeIDs()
{
	TSet<FGuid> UsedIDs;
	ArcTQSQueryDefinitionPrivate::EnsureUniqueID(ParametersID, UsedIDs);
	ArcTQSQueryDefinitionPrivate::EnsureUniqueID(QuerierID, UsedIDs);
	ArcTQSQueryDefinitionPrivate::EnsureUniqueID(ItemID, UsedIDs);
	if (FArcTQSGenerator* GeneratorNode = Generator.GetMutablePtr<FArcTQSGenerator>())
	{
		ArcTQSQueryDefinitionPrivate::EnsureUniqueID(GeneratorNode->ID, UsedIDs);
	}
	for (FInstancedStruct& StepStruct : Steps)
	{
		if (FArcTQSStep* Step = StepStruct.GetMutablePtr<FArcTQSStep>())
		{
			ArcTQSQueryDefinitionPrivate::EnsureUniqueID(Step->ID, UsedIDs);
		}
	}
	const int32 MaxFunctionRepairRounds = EditorBindings.GetNumBindings() + 1;
	for (int32 Round = 0; Round < MaxFunctionRepairRounds; ++Round)
	{
		TSet<FGuid> RoundIDs = UsedIDs;
		TArray<TPair<FGuid, FGuid>> CopiedFunctionIDs;
		EditorBindings.ForEachMutableBinding([&UsedIDs, &RoundIDs, &CopiedFunctionIDs](FPropertyBindingBinding& Binding)
		{
			FArcTQSPropertyFunction* Function = Binding.GetMutablePropertyFunctionNode().GetPtr<FArcTQSPropertyFunction>();
			if (!Function)
			{
				return;
			}
			const FGuid OldID = Function->ID;
			ArcTQSQueryDefinitionPrivate::EnsureUniqueID(Function->ID, RoundIDs);
			if (Function->ID == OldID)
			{
				return;
			}
			Binding.GetMutableSourcePath().SetStructID(Function->ID);
			if (OldID.IsValid() && !UsedIDs.Contains(OldID))
			{
				CopiedFunctionIDs.Emplace(OldID, Function->ID);
			}
		});
		if (CopiedFunctionIDs.IsEmpty())
		{
			break;
		}
		for (const TPair<FGuid, FGuid>& CopiedFunctionID : CopiedFunctionIDs)
		{
			TArray<FArcTQSBinding> InputBindings;
			for (const FArcTQSBinding& Binding : EditorBindings.GetBindings())
			{
				if (Binding.GetTargetPath().GetStructID() == CopiedFunctionID.Key)
				{
					InputBindings.Add(Binding);
				}
			}
			for (FArcTQSBinding& InputBinding : InputBindings)
			{
				InputBinding.GetMutableTargetPath().SetStructID(CopiedFunctionID.Value);
				EditorBindings.AddCompiledBinding(MoveTemp(InputBinding));
			}
		}
	}
	int32 NumBindingsBefore = INDEX_NONE;
	while (NumBindingsBefore != EditorBindings.GetNumBindings())
	{
		NumBindingsBefore = EditorBindings.GetNumBindings();
		TSet<FGuid> LiveIDs;
		for (const FArcTQSBinding& Binding : EditorBindings.GetBindings())
		{
			for (const FGuid& ID : { Binding.GetSourcePath().GetStructID(), Binding.GetTargetPath().GetStructID() })
			{
				TInstancedStruct<FPropertyBindingBindableStructDescriptor> Descriptor;
				if (GetBindableStructByID(ID, Descriptor))
				{
					LiveIDs.Add(ID);
				}
			}
		}
		EditorBindings.RemoveBindings([&LiveIDs](FPropertyBindingBinding& Binding)
		{
			return !LiveIDs.Contains(Binding.GetSourcePath().GetStructID())
				|| !LiveIDs.Contains(Binding.GetTargetPath().GetStructID());
		});
	}
}

bool UArcTQSQueryDefinition::CompileEditorData(TArray<FArcTQSCompilerDiagnostic>& OutDiagnostics)
{
	FArcTQSCompiledData NewCompiledData;
	const bool bCompiled = FArcTQSCompiler::Compile(
		*this,
		ArcTQSQueryDefinitionPrivate::NextRevision(CompiledData.Revision),
		NewCompiledData,
		OutDiagnostics);
	CompiledData = MoveTemp(NewCompiledData);
	return bCompiled;
}

void UArcTQSQueryDefinition::RepairAndCompile()
{
	RepairNodeIDs();
	TArray<FArcTQSCompilerDiagnostic> Diagnostics;
	CompileEditorData(Diagnostics);
	PreviewSetup.Reference.SetDefinition(this);
	ArcTQS::Delegates::OnPostCompile.Broadcast(*this);
}

void UArcTQSQueryDefinition::PreSave(const FObjectPreSaveContext SaveContext)
{
	RepairNodeIDs();
	TArray<FArcTQSCompilerDiagnostic> Diagnostics;
	CompileEditorData(Diagnostics);
	for (const FArcTQSCompilerDiagnostic& Diagnostic : Diagnostics)
	{
		UE_LOGF(LogArcTQS, Warning, "%ls: %ls", *GetPathName(), *Diagnostic.Message.ToString());
	}
	PreviewSetup.Reference.SetDefinition(this);
	ArcTQS::Delegates::OnPostCompile.Broadcast(*this);
	Super::PreSave(SaveContext);
}

void UArcTQSQueryDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(UArcTQSQueryDefinition, PreviewSetup))
	{
		ArcTQS::Delegates::OnPreviewSetupChanged.Broadcast(*this);
		return;
	}
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
	{
		return;
	}
	RepairAndCompile();
}

void UArcTQSQueryDefinition::PostEditImport()
{
	Super::PostEditImport();
	InitializeEditorBindingsOwner();
	RepairAndCompile();
}

void UArcTQSQueryDefinition::PostEditUndo()
{
	Super::PostEditUndo();
	InitializeEditorBindingsOwner();
	RepairAndCompile();
}

void UArcTQSQueryDefinition::PostDuplicate(const EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);
	InitializeEditorBindingsOwner();
	RepairAndCompile();
}

FArcTQSDataHandle UArcTQSQueryDefinition::ResolveEndpoint(const FGuid& ID) const
{
	if (!ID.IsValid())
	{
		return FArcTQSDataHandle();
	}
	if (ID == ParametersID)
	{
		return FArcTQSDataHandle(EArcTQSDataKind::Parameters);
	}
	if (ID == QuerierID)
	{
		return FArcTQSDataHandle(EArcTQSDataKind::Querier);
	}
	if (ID == ItemID)
	{
		return FArcTQSDataHandle(EArcTQSDataKind::Item);
	}
	const FArcTQSGenerator* GeneratorNode = Generator.GetPtr<FArcTQSGenerator>();
	if (GeneratorNode && GeneratorNode->ID == ID)
	{
		return FArcTQSDataHandle(EArcTQSDataKind::Generator);
	}
	for (int32 StepIndex = 0; StepIndex < Steps.Num() && StepIndex < static_cast<int32>(MAX_uint16); ++StepIndex)
	{
		const FArcTQSStep* Step = Steps[StepIndex].GetPtr<FArcTQSStep>();
		if (Step && Step->ID == ID)
		{
			return FArcTQSDataHandle(EArcTQSDataKind::Step, StepIndex);
		}
	}
	const TArray<FArcTQSBinding>& Bindings = EditorBindings.GetBindings();
	for (int32 BindingIndex = 0; BindingIndex < Bindings.Num() && BindingIndex < static_cast<int32>(MAX_uint16); ++BindingIndex)
	{
		const FArcTQSPropertyFunction* Function = Bindings[BindingIndex].GetPropertyFunctionNode().GetPtr<const FArcTQSPropertyFunction>();
		if (Function && Function->ID == ID)
		{
			return FArcTQSDataHandle(EArcTQSDataKind::Function, BindingIndex);
		}
	}
	return FArcTQSDataHandle();
}

const FArcTQSBinding* UArcTQSQueryDefinition::FindFunctionBinding(const FGuid& FunctionID) const
{
	const FArcTQSDataHandle Handle = ResolveEndpoint(FunctionID);
	if (Handle.GetKind() != EArcTQSDataKind::Function)
	{
		return nullptr;
	}
	return &EditorBindings.GetBinding(Handle.GetIndex());
}

EArcTQSDataKind UArcTQSQueryDefinition::ResolveRootNodeKind(const FGuid& ID) const
{
	FGuid CurrentID = ID;
	for (int32 Hop = 0; Hop <= EditorBindings.GetNumBindings(); ++Hop)
	{
		const FArcTQSBinding* FunctionBinding = FindFunctionBinding(CurrentID);
		if (!FunctionBinding)
		{
			return ResolveEndpoint(CurrentID).GetKind();
		}
		CurrentID = FunctionBinding->GetTargetPath().GetStructID();
	}
	return EArcTQSDataKind::Invalid;
}

FName UArcTQSQueryDefinition::GetEndpointName(const FGuid& ID) const
{
	const FArcTQSDataHandle Handle = ResolveEndpoint(ID);
	switch (Handle.GetKind())
	{
	case EArcTQSDataKind::Parameters:
		return TEXT("Parameters");
	case EArcTQSDataKind::Querier:
		return TEXT("Querier");
	case EArcTQSDataKind::Item:
		return TEXT("Item");
	case EArcTQSDataKind::Generator:
		return FName(*ArcTQSQueryDefinitionPrivate::GetNodeDisplayName(Generator.GetScriptStruct()));
	case EArcTQSDataKind::Step:
		return FName(*FString::Printf(TEXT("Step %d %s"), Handle.GetIndex(),
			*ArcTQSQueryDefinitionPrivate::GetNodeDisplayName(Steps[Handle.GetIndex()].GetScriptStruct())));
	case EArcTQSDataKind::Function:
		return FName(*ArcTQSQueryDefinitionPrivate::GetNodeDisplayName(
			EditorBindings.GetBinding(Handle.GetIndex()).GetPropertyFunctionNode().GetScriptStruct()));
	default:
		return NAME_None;
	}
}

FConstStructView UArcTQSQueryDefinition::FindNode(const FGuid& ID) const
{
	const FArcTQSDataHandle Handle = ResolveEndpoint(ID);
	switch (Handle.GetKind())
	{
	case EArcTQSDataKind::Generator:
		return FConstStructView(Generator);
	case EArcTQSDataKind::Step:
		return FConstStructView(Steps[Handle.GetIndex()]);
	case EArcTQSDataKind::Function:
		return EditorBindings.GetBinding(Handle.GetIndex()).GetPropertyFunctionNode();
	default:
		return FConstStructView();
	}
}

bool UArcTQSQueryDefinition::GetBindingDataView(
	const FPropertyBindingBinding& InBinding,
	const EBindingSide InSide,
	FPropertyBindingDataView& OutDataView)
{
	const FGuid ID = InSide == EBindingSide::Source
		? InBinding.GetSourcePath().GetStructID()
		: InBinding.GetTargetPath().GetStructID();
	return GetBindingDataViewByID(ID, OutDataView);
}

void UArcTQSQueryDefinition::GetBindableStructs(
	const FGuid InTargetStructID,
	TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>>& OutStructDescs) const
{
	OutStructDescs.Reset();
	const EArcTQSDataKind TargetKind = ResolveRootNodeKind(InTargetStructID);
	const FGuid SourceIDs[] = { ParametersID, QuerierID, ItemID };
	for (const FGuid& SourceID : SourceIDs)
	{
		if (ArcTQS::IsBindingSourceAccessible(ResolveEndpoint(SourceID).GetKind(), TargetKind))
		{
			ArcTQSQueryDefinitionPrivate::AddDescriptor(*this, SourceID, OutStructDescs);
		}
	}
}

bool UArcTQSQueryDefinition::GetBindableStructByID(
	const FGuid InStructID,
	TInstancedStruct<FPropertyBindingBindableStructDescriptor>& OutStructDesc) const
{
	const FArcTQSDataHandle Handle = ResolveEndpoint(InStructID);
	const UStruct* Struct = nullptr;
	switch (Handle.GetKind())
	{
	case EArcTQSDataKind::Parameters:
		Struct = Parameters.GetPropertyBagStruct();
		break;
	case EArcTQSDataKind::Querier:
		Struct = FArcTQSQuerierInputs::StaticStruct();
		break;
	case EArcTQSDataKind::Item:
		Struct = FArcTQSItemInputs::StaticStruct();
		break;
	case EArcTQSDataKind::Generator:
	case EArcTQSDataKind::Step:
	case EArcTQSDataKind::Function:
		Struct = FindNode(InStructID).GetScriptStruct();
		break;
	default:
		break;
	}
	if (!Struct)
	{
		return false;
	}
	OutStructDesc = TInstancedStruct<FPropertyBindingBindableStructDescriptor>::Make(
		GetEndpointName(InStructID), Struct, InStructID);
	return true;
}

bool UArcTQSQueryDefinition::GetBindingDataViewByID(const FGuid InStructID, FPropertyBindingDataView& OutDataView) const
{
	const FArcTQSDataHandle Handle = ResolveEndpoint(InStructID);
	switch (Handle.GetKind())
	{
	case EArcTQSDataKind::Parameters:
		if (!Parameters.IsValid())
		{
			return false;
		}
		OutDataView = FPropertyBindingDataView(const_cast<FInstancedPropertyBag&>(Parameters).GetMutableValue());
		return true;
	case EArcTQSDataKind::Querier:
		OutDataView = FPropertyBindingDataView(
			FArcTQSQuerierInputs::StaticStruct(), const_cast<FArcTQSQuerierInputs*>(&EditorQuerierInputs));
		return true;
	case EArcTQSDataKind::Item:
		OutDataView = FPropertyBindingDataView(
			FArcTQSItemInputs::StaticStruct(), const_cast<FArcTQSItemInputs*>(&EditorItemInputs));
		return true;
	case EArcTQSDataKind::Generator:
	case EArcTQSDataKind::Step:
	case EArcTQSDataKind::Function:
		{
			const FConstStructView Node = FindNode(InStructID);
			if (!Node.IsValid())
			{
				return false;
			}
			OutDataView = FPropertyBindingDataView(Node.GetScriptStruct(), const_cast<uint8*>(Node.GetMemory()));
			return true;
		}
	default:
		return false;
	}
}

FPropertyBindingBindingCollection* UArcTQSQueryDefinition::GetEditorPropertyBindings()
{
	return &EditorBindings;
}

const FPropertyBindingBindingCollection* UArcTQSQueryDefinition::GetEditorPropertyBindings() const
{
	return &EditorBindings;
}

bool UArcTQSQueryDefinition::CanCreateParameter(const FGuid InStructID) const
{
	return InStructID.IsValid() && InStructID == ParametersID;
}

void UArcTQSQueryDefinition::CreateParametersForStruct(
	const FGuid InStructID,
	const TArrayView<UE::PropertyBinding::FPropertyCreationDescriptor> InOutCreationDescs)
{
	if (InOutCreationDescs.IsEmpty() || !CanCreateParameter(InStructID))
	{
		return;
	}
	UE::PropertyBinding::CreateUniquelyNamedPropertiesInPropertyBag(InOutCreationDescs, Parameters);
	MarkPackageDirty();
	RepairAndCompile();
}

FGuid UArcTQSQueryDefinition::GetFallbackStructID() const
{
	return FGuid();
}

void UArcTQSQueryDefinition::AppendBindablePropertyFunctionStructs(
	TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>>& InOutStructs) const
{
	const UScriptStruct* BaseStruct = FArcTQSPropertyFunction::StaticStruct();
	TArray<const UScriptStruct*> FunctionStructs;
	for (TObjectIterator<UScriptStruct> StructIt; StructIt; ++StructIt)
	{
		const UScriptStruct* Struct = *StructIt;
		if (Struct != BaseStruct
			&& Struct->IsChildOf(BaseStruct)
			&& !Struct->HasAnyFlags(RF_NewerVersionExists)
			&& !Struct->HasMetaData(TEXT("Hidden"))
			&& ArcTQS::FindFunctionOutputProperty(Struct))
		{
			FunctionStructs.Add(Struct);
		}
	}
	FunctionStructs.Sort([](const UScriptStruct& A, const UScriptStruct& B)
	{
		return A.GetPathName() < B.GetPathName();
	});
	for (const UScriptStruct* Struct : FunctionStructs)
	{
		TInstancedStruct<FPropertyBindingBindableStructDescriptor>& Descriptor = InOutStructs.Add_GetRef(
			TInstancedStruct<FPropertyBindingBindableStructDescriptor>::Make(
				FName(*Struct->GetDisplayNameText().ToString()), Struct, FGuid::NewDeterministicGuid(Struct->GetPathName())));
		Descriptor.GetMutable().Category = Struct->GetMetaData(TEXT("Category"));
	}
}

void UArcTQSQueryDefinition::OnPropertyBindingChanged(
	const FPropertyBindingPath& InSourcePath,
	const FPropertyBindingPath& InTargetPath)
{
	MarkPackageDirty();
	RepairAndCompile();
}
#endif
