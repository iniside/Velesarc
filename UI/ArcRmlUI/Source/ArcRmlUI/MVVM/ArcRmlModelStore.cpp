// Copyright Lukasz Baran. All Rights Reserved.

#include "MVVM/ArcRmlModelStore.h"

#include "ArcRmlUILog.h"
#include "Components/ArcRmlComponentExpander.h"
#include "Interfaces/ArcRmlStrings.h"
#include "Thread/ArcRmlUIQueues.h"

#include "RmlUi/Core/Context.h"
#include "RmlUi/Core/DataTypeRegister.h"
#include "RmlUi/Core/Element.h"
#include "RmlUi/Core/Event.h"

namespace ArcRmlModelStorePrivate
{
	const char* const CallEventName = "call";
	const char* const DragElementParameter = "drag_element";
	const Rml::String DataAttributePrefix = "data-";

	FArcRmlModelNode ToNode(const FArcRmlValue& Value)
	{
		FArcRmlModelNode Node;
		Node.Kind = Value.Kind;
		switch (Value.Kind)
		{
		case EArcRmlValueKind::Empty:
			break;
		case EArcRmlValueKind::Bool:
			Node.Scalar = Rml::Variant(Value.bBool);
			break;
		case EArcRmlValueKind::Int:
			Node.Scalar = Rml::Variant(static_cast<int64_t>(Value.Int));
			break;
		case EArcRmlValueKind::Float:
			Node.Scalar = Rml::Variant(Value.Float);
			break;
		case EArcRmlValueKind::String:
			Node.Scalar = Rml::Variant(ArcRmlStrings::ToRmlString(Value.String));
			break;
		case EArcRmlValueKind::Colour:
			Node.Scalar = Rml::Variant(Rml::Colourb(Value.Colour.R, Value.Colour.G, Value.Colour.B, Value.Colour.A));
			break;
		case EArcRmlValueKind::Array:
		case EArcRmlValueKind::Struct:
			Node.Children.reserve(Value.Children.Num());
			for (const FArcRmlValue& Child : Value.Children)
			{
				Node.Children.push_back(ToNode(Child));
			}
			if (Value.Kind == EArcRmlValueKind::Struct)
			{
				Node.MemberNames.reserve(Value.MemberNames.Num());
				for (const FString& MemberName : Value.MemberNames)
				{
					Node.MemberNames.push_back(ArcRmlStrings::ToRmlString(MemberName));
				}
			}
			break;
		}
		return Node;
	}

	FArcRmlModelNode* FindMember(FArcRmlModelNode& Node, const Rml::String& Name)
	{
		if (Node.Kind != EArcRmlValueKind::Struct)
		{
			return nullptr;
		}
		const int32 Count = FMath::Min(static_cast<int32>(Node.MemberNames.size()), static_cast<int32>(Node.Children.size()));
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (Node.MemberNames[Index] == Name)
			{
				return &Node.Children[Index];
			}
		}
		return nullptr;
	}

	FArcRmlValue ToValue(const Rml::Variant& Variant)
	{
		FArcRmlValue Value;
		switch (Variant.GetType())
		{
		case Rml::Variant::NONE:
			break;
		case Rml::Variant::BOOL:
			Value.Kind = EArcRmlValueKind::Bool;
			Value.bBool = Variant.Get<bool>();
			break;
		case Rml::Variant::BYTE:
		case Rml::Variant::CHAR:
		case Rml::Variant::INT:
		case Rml::Variant::INT64:
		case Rml::Variant::UINT:
		case Rml::Variant::UINT64:
			Value.Kind = EArcRmlValueKind::Int;
			Value.Int = Variant.Get<int64_t>();
			break;
		case Rml::Variant::FLOAT:
		case Rml::Variant::DOUBLE:
			Value.Kind = EArcRmlValueKind::Float;
			Value.Float = Variant.Get<double>();
			break;
		default:
			Value.Kind = EArcRmlValueKind::String;
			Value.String = ArcRmlStrings::ToFString(Variant.Get<Rml::String>());
			break;
		}
		return Value;
	}

	void CopyDataAttributes(const Rml::Element& Element, TMap<FString, FString>& OutData)
	{
		for (const Rml::Dictionary::value_type& Attribute : Element.GetAttributes())
		{
			if (Attribute.first.rfind(DataAttributePrefix, 0) == 0)
			{
				OutData.Add(ArcRmlStrings::ToFString(Attribute.first), ArcRmlStrings::ToFString(Attribute.second.Get<Rml::String>()));
			}
		}
	}
}

FArcRmlModelNodeDefinition::FArcRmlModelNodeDefinition()
	: Rml::VariableDefinition(Rml::DataVariableType::Scalar)
{
}

bool FArcRmlModelNodeDefinition::Get(void* Ptr, Rml::Variant& Variant)
{
	const FArcRmlModelNode* Node = static_cast<const FArcRmlModelNode*>(Ptr);
	if (!Node || Node->Kind == EArcRmlValueKind::Array || Node->Kind == EArcRmlValueKind::Struct)
	{
		return false;
	}
	Variant = Node->Scalar;
	return true;
}

int FArcRmlModelNodeDefinition::Size(void* Ptr)
{
	const FArcRmlModelNode* Node = static_cast<const FArcRmlModelNode*>(Ptr);
	return Node && Node->Kind == EArcRmlValueKind::Array ? static_cast<int>(Node->Children.size()) : 0;
}

Rml::DataVariable FArcRmlModelNodeDefinition::Child(void* Ptr, const Rml::DataAddressEntry& Address)
{
	FArcRmlModelNode* Node = static_cast<FArcRmlModelNode*>(Ptr);
	if (!Node)
	{
		return Rml::DataVariable();
	}

	if (Node->Kind == EArcRmlValueKind::Array)
	{
		const int Count = static_cast<int>(Node->Children.size());
		if (Address.index >= 0 && Address.index < Count)
		{
			return Rml::DataVariable(this, &Node->Children[Address.index]);
		}
		if (Address.name == "size")
		{
			return Rml::MakeLiteralIntVariable(Count);
		}
		return Rml::DataVariable();
	}

	if (FArcRmlModelNode* Member = ArcRmlModelStorePrivate::FindMember(*Node, Address.name))
	{
		return Rml::DataVariable(this, Member);
	}
	return Rml::DataVariable();
}

FArcRmlModelStore::FArcRmlModelStore(const TSharedRef<FArcRmlViewChannel, ESPMode::ThreadSafe>& InChannel)
	: Channel(InChannel)
{
}

bool FArcRmlModelStore::BindModel(Rml::Context& Context, const FString& ModelName, const FArcRmlValue& Snapshot)
{
	if (Models.Contains(ModelName) || Snapshot.Kind != EArcRmlValueKind::Struct)
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: cannot bind data model '%ls'", *ModelName);
		return false;
	}

	Rml::DataModelConstructor Constructor = Context.CreateDataModel(ArcRmlStrings::ToRmlString(ModelName));
	if (!Constructor)
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: RmlUi rejected data model '%ls'", *ModelName);
		return false;
	}

	if (!bTransformsRegistered)
	{
		ArcRmlComponents::RegisterTransforms(*Constructor.GetDataTypeRegister()->GetTransformFuncRegister());
		bTransformsRegistered = true;
	}

	TUniquePtr<FModel> Model = MakeUnique<FModel>();
	Model->Name = ModelName;

	const int32 Count = FMath::Min(Snapshot.MemberNames.Num(), Snapshot.Children.Num());
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const Rml::String FieldName = ArcRmlStrings::ToRmlString(Snapshot.MemberNames[Index]);
		Rml::UniquePtr<FArcRmlModelNode> Node = Rml::MakeUnique<FArcRmlModelNode>(ArcRmlModelStorePrivate::ToNode(Snapshot.Children[Index]));
		Constructor.BindCustomDataVariable(FieldName, Rml::DataVariable(&Definition, Node.get()));
		Model->Fields.emplace(FieldName, std::move(Node));
	}

	const Rml::String EmptyArrayName = ArcRmlStrings::ToRmlString(ArcRmlComponents::EmptyArrayName);
	if (Model->Fields.find(EmptyArrayName) != Model->Fields.end())
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: data model '%ls' has a field named '%ls', which collides with the empty array; it is not bound",
			*ModelName, ArcRmlComponents::EmptyArrayName);
	}
	else
	{
		Rml::UniquePtr<FArcRmlModelNode> EmptyArray = Rml::MakeUnique<FArcRmlModelNode>();
		EmptyArray->Kind = EArcRmlValueKind::Array;
		Constructor.BindCustomDataVariable(EmptyArrayName, Rml::DataVariable(&Definition, EmptyArray.get()));
		Model->Fields.emplace(EmptyArrayName, std::move(EmptyArray));
	}

	Constructor.BindEventCallback(ArcRmlModelStorePrivate::CallEventName,
		[this, ModelName](Rml::DataModelHandle Handle, Rml::Event& Event, const Rml::VariantList& Arguments)
		{
			HandleCall(Handle, Event, Arguments, ModelName);
		});

	Model->Handle = Constructor.GetModelHandle();
	Models.Add(ModelName, MoveTemp(Model));
	return true;
}

bool FArcRmlModelStore::UnbindModel(Rml::Context& Context, const FString& ModelName)
{
	if (!Models.Contains(ModelName))
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: cannot unbind data model '%ls', it is not bound", *ModelName);
		return false;
	}

	const TUniquePtr<FModel> Model = Models.FindAndRemoveChecked(ModelName);
	if (!Context.RemoveDataModel(ArcRmlStrings::ToRmlString(ModelName)))
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: RmlUi has no data model '%ls' to remove", *ModelName);
	}
	UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: removed data model '%ls'", *ModelName);
	return true;
}

FArcRmlModelNode* FArcRmlModelStore::FindNode(const FString& ModelName, const TArray<FArcRmlPathEntry>& Path, FModel*& OutModel)
{
	OutModel = nullptr;
	const TUniquePtr<FModel>* FoundModel = Models.Find(ModelName);
	if (!FoundModel || Path.Num() == 0)
	{
		return nullptr;
	}

	FModel& Model = **FoundModel;
	Rml::UnorderedMap<Rml::String, Rml::UniquePtr<FArcRmlModelNode>>::iterator FoundField = Model.Fields.find(ArcRmlStrings::ToRmlString(Path[0].Name));
	if (FoundField == Model.Fields.end())
	{
		return nullptr;
	}

	FArcRmlModelNode* Node = FoundField->second.get();
	for (int32 Index = 1; Index < Path.Num() && Node; ++Index)
	{
		const FArcRmlPathEntry& Entry = Path[Index];
		if (Entry.Index != INDEX_NONE)
		{
			const bool bInRange = Node->Kind == EArcRmlValueKind::Array && Entry.Index < static_cast<int32>(Node->Children.size());
			Node = bInRange ? &Node->Children[Entry.Index] : nullptr;
		}
		else
		{
			Node = ArcRmlModelStorePrivate::FindMember(*Node, ArcRmlStrings::ToRmlString(Entry.Name));
		}
	}

	if (Node)
	{
		OutModel = &Model;
	}
	return Node;
}

bool FArcRmlModelStore::ApplyDelta(const FString& ModelName, const TArray<FArcRmlPathEntry>& Path, const FArcRmlValue& Value)
{
	FModel* Model = nullptr;
	FArcRmlModelNode* Node = FindNode(ModelName, Path, Model);
	if (!Node)
	{
		return false;
	}

	*Node = ArcRmlModelStorePrivate::ToNode(Value);
	Model->Handle.DirtyVariable(ArcRmlStrings::ToRmlString(Path[0].Name));
	return true;
}

bool FArcRmlModelStore::ApplyListOps(const FString& ModelName, const TArray<FArcRmlPathEntry>& Path, const TArray<FArcRmlListOp>& Ops)
{
	FModel* Model = nullptr;
	FArcRmlModelNode* Node = FindNode(ModelName, Path, Model);
	if (!Node || Node->Kind != EArcRmlValueKind::Array)
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: list operations on model '%ls' target a path that is not an array", *ModelName);
		return false;
	}

	Rml::Vector<FArcRmlModelNode>& Rows = Node->Children;
	bool bApplied = false;
	for (const FArcRmlListOp& Op : Ops)
	{
		const int32 Count = static_cast<int32>(Rows.size());
		bool bValid = false;
		switch (Op.Type)
		{
		case EArcRmlListOpType::Insert:
			bValid = Op.Index >= 0 && Op.Index <= Count;
			if (bValid)
			{
				Rows.insert(Rows.begin() + Op.Index, ArcRmlModelStorePrivate::ToNode(Op.Value));
			}
			break;
		case EArcRmlListOpType::Remove:
			bValid = Op.Index >= 0 && Op.Index < Count;
			if (bValid)
			{
				Rows.erase(Rows.begin() + Op.Index);
			}
			break;
		case EArcRmlListOpType::Move:
			bValid = Op.Index >= 0 && Op.Index < Count && Op.ToIndex >= 0 && Op.ToIndex < Count;
			if (bValid && Op.Index != Op.ToIndex)
			{
				FArcRmlModelNode Moved = MoveTemp(Rows[Op.Index]);
				Rows.erase(Rows.begin() + Op.Index);
				Rows.insert(Rows.begin() + Op.ToIndex, MoveTemp(Moved));
			}
			break;
		case EArcRmlListOpType::Update:
			bValid = Op.Index >= 0 && Op.Index < Count;
			if (bValid)
			{
				Rows[Op.Index] = ArcRmlModelStorePrivate::ToNode(Op.Value);
			}
			break;
		}

		if (!bValid)
		{
			UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: list operation %d at %d -> %d is out of range (%d rows) on model '%ls'", static_cast<int32>(Op.Type), Op.Index,
				Op.ToIndex, Count, *ModelName);
			break;
		}
		bApplied = true;
	}

	if (bApplied)
	{
		Model->Handle.DirtyVariable(ArcRmlStrings::ToRmlString(Path[0].Name));
	}
	return bApplied;
}

void FArcRmlModelStore::HandleCall(Rml::DataModelHandle Handle, Rml::Event& Event, const Rml::VariantList& Arguments, FString ModelName)
{
	if (Arguments.size() < 2)
	{
		UE_LOGF(LogArcRmlUI, Warning, "ArcRmlUI: call() on model '%ls' needs a child path and a function name", *ModelName);
		return;
	}

	FArcRmlOutboundEvent Outbound;
	Outbound.Type = EArcRmlOutboundType::ViewModelCall;
	FArcRmlViewModelCall& Call = Outbound.Call;
	Call.ModelName = MoveTemp(ModelName);
	Call.ChildPath = ArcRmlStrings::ToFString(Arguments[0].Get<Rml::String>());
	Call.FunctionName = ArcRmlStrings::ToFString(Arguments[1].Get<Rml::String>());
	for (size_t Index = 2; Index < Arguments.size(); ++Index)
	{
		Call.Arguments.Add(ArcRmlModelStorePrivate::ToValue(Arguments[Index]));
	}

	if (const Rml::Element* Dragged = static_cast<const Rml::Element*>(Event.GetParameter<void*>(ArcRmlModelStorePrivate::DragElementParameter, nullptr)))
	{
		ArcRmlModelStorePrivate::CopyDataAttributes(*Dragged, Call.DragData);
	}
	if (const Rml::Element* Current = Event.GetCurrentElement())
	{
		ArcRmlModelStorePrivate::CopyDataAttributes(*Current, Call.ElementData);
	}

	Channel->PushOutbound(MoveTemp(Outbound));
}
