// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSCompiler.h"

#if WITH_EDITOR

#include "ArcTQSBindings.h"
#include "ArcTQSGenerator.h"
#include "ArcTQSPropertyFunction.h"
#include "ArcTQSQueryDefinition.h"
#include "ArcTQSStep.h"
#include "GameplayTagContainer.h"
#include "PropertyBindingTypes.h"
#include "StructUtils/PropertyBag.h"
#include "UObject/UnrealType.h"

namespace ArcTQSCompilerPrivate
{
	struct FEndpoint
	{
		FGuid ID;
		FName Name;
		FArcTQSDataHandle Handle;
		const UStruct* Struct = nullptr;
		int32 FunctionNodeIndex = INDEX_NONE;
		TArray<FArcTQSBinding> ForwardBindings;
		TArray<FArcTQSBinding> PerItemBindings;
	};

	struct FFunctionNode
	{
		FGuid ID;
		FConstStructView Node;
		FName OutputName;
		int32 OwnerBindingIndex = INDEX_NONE;
		FGuid OwnerTargetID;
		int32 RootEndpointIndex = INDEX_NONE;
		TArray<int32> Children;
		bool bReadsItem = false;
		bool bPerItem = false;
		int32 CompiledIndex = INDEX_NONE;
	};

	struct FNodeFunctionRanges
	{
		int32 QueryBegin = 0;
		int32 QueryEnd = 0;
		int32 PerItemBegin = 0;
		int32 PerItemEnd = 0;
	};

	bool IsNodeKind(const EArcTQSDataKind Kind)
	{
		return Kind == EArcTQSDataKind::Generator
			|| Kind == EArcTQSDataKind::Step;
	}

	bool TargetsRootProperty(const TArray<FArcTQSBinding>& Bindings, const FName PropertyName)
	{
		return Bindings.ContainsByPredicate([PropertyName](const FArcTQSBinding& Binding)
		{
			return Binding.GetTargetPath().GetSegment(0).GetName() == PropertyName;
		});
	}

	class FCompiler
	{
	public:
		FCompiler(
			const UArcTQSQueryDefinition& InDefinition,
			const uint32 InRevision,
			FArcTQSCompiledData& InOutData,
			TArray<FArcTQSCompilerDiagnostic>& InOutDiagnostics)
			: Definition(InDefinition)
			, Revision(InRevision)
			, Data(InOutData)
			, Diagnostics(InOutDiagnostics)
		{
		}

		bool Run()
		{
			Data.Reset();
			Data.Version = FArcTQSCompiledData::CurrentVersion;
			Data.Revision = Revision;

			AddSourceEndpoints();
			AddNodeEndpoints();
			if (BuildFunctionGraph())
			{
				BuildBindingCollection();
			}
			ValidateMemoryKeys();

			Data.bIsValid = !bFailed;
			if (Data.bIsValid && !Data.ResolvePaths(Definition.Parameters.GetPropertyBagStruct()))
			{
				Error(FGuid(), NSLOCTEXT("ArcTQS", "ResolveCompiledBindings", "Compiled binding paths could not be resolved."));
				Data.bIsValid = false;
			}
			return Data.bIsValid;
		}

	private:
		void AddSourceEndpoints()
		{
			if (const UPropertyBag* ParametersStruct = Definition.Parameters.GetPropertyBagStruct())
			{
				AddEndpoint(Definition.ParametersID, FArcTQSDataHandle(EArcTQSDataKind::Parameters), ParametersStruct);
			}
			AddEndpoint(Definition.QuerierID, FArcTQSDataHandle(EArcTQSDataKind::Querier), FArcTQSQuerierInputs::StaticStruct());
			AddEndpoint(Definition.ItemID, FArcTQSDataHandle(EArcTQSDataKind::Item), FArcTQSItemInputs::StaticStruct());
		}

		void AddNodeEndpoints()
		{
			const FArcTQSGenerator* Generator = Definition.Generator.GetPtr<FArcTQSGenerator>();
			if (!Generator)
			{
				Error(FGuid(), NSLOCTEXT("ArcTQS", "MissingGenerator", "A target query needs a generator derived from FArcTQSGenerator."));
			}
			else
			{
				AddEndpoint(Generator->ID, FArcTQSDataHandle(EArcTQSDataKind::Generator), Definition.Generator.GetScriptStruct());
			}

			if (Definition.Steps.Num() >= static_cast<int32>(MAX_uint16))
			{
				Error(FGuid(), NSLOCTEXT("ArcTQS", "TooManySteps", "The target query contains too many steps."));
				return;
			}
			Data.StepBatches.SetNum(Definition.Steps.Num());
			Data.StepPerItemBatches.SetNum(Definition.Steps.Num());
			for (int32 StepIndex = 0; StepIndex < Definition.Steps.Num(); ++StepIndex)
			{
				const FInstancedStruct& StepStruct = Definition.Steps[StepIndex];
				const FArcTQSStep* Step = StepStruct.GetPtr<FArcTQSStep>();
				if (!Step)
				{
					Error(FGuid(), FText::Format(
						NSLOCTEXT("ArcTQS", "InvalidStep", "Step {0} is empty or does not derive from FArcTQSStep."),
						FText::AsNumber(StepIndex)));
					continue;
				}
				AddEndpoint(Step->ID, FArcTQSDataHandle(EArcTQSDataKind::Step, StepIndex), StepStruct.GetScriptStruct());
			}
		}

		int32 AddEndpoint(const FGuid ID, const FArcTQSDataHandle Handle, const UStruct* Struct)
		{
			if (!ID.IsValid() || !Struct || !Handle.IsValid())
			{
				Error(ID, NSLOCTEXT("ArcTQS", "InvalidEndpoint", "A target query binding endpoint is missing its stable ID or reflected type."));
				return INDEX_NONE;
			}
			if (EndpointIndexByID.Contains(ID))
			{
				Error(ID, NSLOCTEXT("ArcTQS", "DuplicateEndpointID", "Two target query binding endpoints share the same stable ID."));
				return INDEX_NONE;
			}
			FEndpoint Endpoint;
			Endpoint.ID = ID;
			Endpoint.Name = Definition.GetEndpointName(ID);
			Endpoint.Handle = Handle;
			Endpoint.Struct = Struct;
			const int32 EndpointIndex = Endpoints.Add(MoveTemp(Endpoint));
			EndpointIndexByID.Add(ID, EndpointIndex);
			return EndpointIndex;
		}

		void ValidateMemoryKeys()
		{
			if (const FArcTQSGenerator* Generator = Definition.Generator.GetPtr<FArcTQSGenerator>())
			{
				ValidateMemoryKeys(Generator->ID, Definition.GetEndpointName(Generator->ID), Definition.Generator);
			}
			for (const FInstancedStruct& StepStruct : Definition.Steps)
			{
				if (const FArcTQSStep* Step = StepStruct.GetPtr<FArcTQSStep>())
				{
					ValidateMemoryKeys(Step->ID, Definition.GetEndpointName(Step->ID), StepStruct);
				}
			}
			for (const FFunctionNode& FunctionNode : FunctionNodes)
			{
				ValidateMemoryKeys(FunctionNode.ID,
					FName(*FunctionNode.Node.GetScriptStruct()->GetDisplayNameText().ToString()), FunctionNode.Node);
			}
		}

		void ValidateMemoryKeys(const FGuid ID, const FName NodeName, const FConstStructView View)
		{
			if (!View.IsValid())
			{
				return;
			}
			ArcTQS::ForEachMemoryKey(*View.GetScriptStruct(), View.GetMemory(),
				[this, ID, NodeName](const FProperty& Property, const FGameplayTag Key)
			{
				if (Key.IsValid())
				{
					return;
				}
				Error(ID, FText::Format(
					NSLOCTEXT("ArcTQS", "MemoryKeyNotSet", "Memory key '{0}' on '{1}' is not set."),
					FText::FromName(Property.GetFName()), FText::FromName(NodeName)));
			});
		}

		bool BuildFunctionGraph()
		{
			const int32 NumFailuresBefore = NumErrors;
			CollectFunctions();
			if (NumErrors == NumFailuresBefore)
			{
				ResolveFunctionRoots();
			}
			if (NumErrors == NumFailuresBefore)
			{
				for (int32 FunctionIndex = 0; FunctionIndex < FunctionNodes.Num(); ++FunctionIndex)
				{
					if (const int32* ParentIndex = FunctionIndexByID.Find(FunctionNodes[FunctionIndex].OwnerTargetID))
					{
						FunctionNodes[*ParentIndex].Children.Add(FunctionIndex);
					}
				}
				for (int32 FunctionIndex = 0; FunctionIndex < FunctionNodes.Num(); ++FunctionIndex)
				{
					ClassifyFunction(FunctionIndex);
				}
				OrderFunctions();
			}
			return NumErrors == NumFailuresBefore;
		}

		void CollectFunctions()
		{
			const TArray<FArcTQSBinding>& EditorBindings = Definition.EditorBindings.GetBindings();
			for (int32 BindingIndex = 0; BindingIndex < EditorBindings.Num(); ++BindingIndex)
			{
				const FArcTQSBinding& Binding = EditorBindings[BindingIndex];
				const FConstStructView NodeView = Binding.GetPropertyFunctionNode();
				if (!NodeView.IsValid())
				{
					continue;
				}
				const FArcTQSPropertyFunction* Function = NodeView.GetPtr<const FArcTQSPropertyFunction>();
				const FGuid TargetID = Binding.GetTargetPath().GetStructID();
				if (!Function)
				{
					Error(TargetID, NSLOCTEXT("ArcTQS", "InvalidFunctionNode", "A bound function does not derive from FArcTQSPropertyFunction."));
					continue;
				}
				if (!Function->ID.IsValid() || Binding.GetSourcePath().GetStructID() != Function->ID)
				{
					Error(TargetID, NSLOCTEXT("ArcTQS", "FunctionSourceMismatch", "A function binding must source the output of its own function."));
					continue;
				}
				if (FunctionIndexByID.Contains(Function->ID) || IsReservedID(Function->ID))
				{
					Error(TargetID, NSLOCTEXT("ArcTQS", "DuplicateFunctionID", "Two binding endpoints share the same stable ID as a function."));
					continue;
				}
				int32 NumOutputs = 0;
				const FProperty* OutputProperty = ArcTQS::FindFunctionOutputProperty(NodeView.GetScriptStruct(), &NumOutputs);
				if (!OutputProperty || NumOutputs != 1)
				{
					Error(TargetID, FText::Format(
						NSLOCTEXT("ArcTQS", "FunctionOutputCount", "Function '{0}' must declare exactly one ArcTQSOutput field."),
						NodeView.GetScriptStruct()->GetDisplayNameText()));
					continue;
				}
				FFunctionNode& FunctionNode = FunctionNodes.AddDefaulted_GetRef();
				FunctionNode.ID = Function->ID;
				FunctionNode.Node = NodeView;
				FunctionNode.OutputName = OutputProperty->GetFName();
				FunctionNode.OwnerBindingIndex = BindingIndex;
				FunctionNode.OwnerTargetID = TargetID;
				FunctionIndexByID.Add(Function->ID, FunctionNodes.Num() - 1);
			}

			for (int32 BindingIndex = 0; BindingIndex < EditorBindings.Num(); ++BindingIndex)
			{
				const FArcTQSBinding& Binding = EditorBindings[BindingIndex];
				const FGuid SourceID = Binding.GetSourcePath().GetStructID();
				if (const int32* FunctionIndex = FunctionIndexByID.Find(SourceID))
				{
					if (FunctionNodes[*FunctionIndex].OwnerBindingIndex != BindingIndex)
					{
						Error(Binding.GetTargetPath().GetStructID(), NSLOCTEXT("ArcTQS", "SharedFunctionOutput", "A function output can feed only the binding that owns the function."));
					}
				}
				if (const int32* FunctionIndex = FunctionIndexByID.Find(Binding.GetTargetPath().GetStructID()))
				{
					if (SourceID == Definition.ItemID)
					{
						FunctionNodes[*FunctionIndex].bReadsItem = true;
					}
				}
			}
		}

		bool IsReservedID(const FGuid& ID) const
		{
			return EndpointIndexByID.Contains(ID);
		}

		void ResolveFunctionRoots()
		{
			for (FFunctionNode& FunctionNode : FunctionNodes)
			{
				FGuid CurrentID = FunctionNode.OwnerTargetID;
				int32 Hops = 0;
				while (const int32* ParentIndex = FunctionIndexByID.Find(CurrentID))
				{
					if (++Hops > FunctionNodes.Num())
					{
						break;
					}
					CurrentID = FunctionNodes[*ParentIndex].OwnerTargetID;
				}
				if (Hops > FunctionNodes.Num())
				{
					Error(FunctionNode.ID, NSLOCTEXT("ArcTQS", "FunctionCycle", "Bound functions form a cycle."));
					continue;
				}
				const int32* RootIndex = EndpointIndexByID.Find(CurrentID);
				if (!RootIndex || !IsNodeKind(Endpoints[*RootIndex].Handle.GetKind()))
				{
					Error(FunctionNode.ID, NSLOCTEXT("ArcTQS", "FunctionWithoutNode", "A bound function does not feed a query node."));
					continue;
				}
				FunctionNode.RootEndpointIndex = *RootIndex;
			}
		}

		bool ClassifyFunction(const int32 FunctionIndex)
		{
			FFunctionNode& FunctionNode = FunctionNodes[FunctionIndex];
			bool bPerItem = FunctionNode.bReadsItem;
			for (const int32 ChildIndex : FunctionNode.Children)
			{
				bPerItem |= ClassifyFunction(ChildIndex);
			}
			FunctionNode.bPerItem = bPerItem;
			return bPerItem;
		}

		void CollectPostOrder(const int32 FunctionIndex, TArray<int32>& OutOrder) const
		{
			for (const int32 ChildIndex : FunctionNodes[FunctionIndex].Children)
			{
				CollectPostOrder(ChildIndex, OutOrder);
			}
			OutOrder.Add(FunctionIndex);
		}

		void OrderFunctions()
		{
			if (FunctionNodes.Num() >= static_cast<int32>(MAX_uint16))
			{
				Error(FGuid(), NSLOCTEXT("ArcTQS", "TooManyFunctions", "The target query contains too many bound functions."));
				return;
			}
			const int32 NumNodeEndpoints = Endpoints.Num();
			NodeRanges.SetNum(NumNodeEndpoints);
			TArray<int32> CompiledOrder;
			CompiledOrder.Reserve(FunctionNodes.Num());
			for (int32 EndpointIndex = 0; EndpointIndex < NumNodeEndpoints; ++EndpointIndex)
			{
				if (!IsNodeKind(Endpoints[EndpointIndex].Handle.GetKind()))
				{
					continue;
				}
				const FGuid NodeID = Endpoints[EndpointIndex].ID;
				TArray<int32> NodeOrder;
				for (int32 FunctionIndex = 0; FunctionIndex < FunctionNodes.Num(); ++FunctionIndex)
				{
					if (FunctionNodes[FunctionIndex].OwnerTargetID == NodeID)
					{
						CollectPostOrder(FunctionIndex, NodeOrder);
					}
				}
				FNodeFunctionRanges& Ranges = NodeRanges[EndpointIndex];
				Ranges.QueryBegin = CompiledOrder.Num();
				for (const int32 FunctionIndex : NodeOrder)
				{
					if (!FunctionNodes[FunctionIndex].bPerItem)
					{
						CompiledOrder.Add(FunctionIndex);
					}
				}
				Ranges.QueryEnd = CompiledOrder.Num();
				Ranges.PerItemBegin = CompiledOrder.Num();
				for (const int32 FunctionIndex : NodeOrder)
				{
					if (FunctionNodes[FunctionIndex].bPerItem)
					{
						CompiledOrder.Add(FunctionIndex);
					}
				}
				Ranges.PerItemEnd = CompiledOrder.Num();
			}

			Data.Functions.SetNum(CompiledOrder.Num());
			for (int32 CompiledIndex = 0; CompiledIndex < CompiledOrder.Num(); ++CompiledIndex)
			{
				FFunctionNode& FunctionNode = FunctionNodes[CompiledOrder[CompiledIndex]];
				FunctionNode.CompiledIndex = CompiledIndex;
				FArcTQSCompiledFunction& CompiledFunction = Data.Functions[CompiledIndex];
				CompiledFunction.Function = FInstancedStruct(FunctionNode.Node);
				CompiledFunction.bPerItem = FunctionNode.bPerItem;
				const int32 EndpointIndex = AddEndpoint(FunctionNode.ID,
					FArcTQSDataHandle(EArcTQSDataKind::Function, CompiledIndex), FunctionNode.Node.GetScriptStruct());
				if (EndpointIndex != INDEX_NONE)
				{
					Endpoints[EndpointIndex].FunctionNodeIndex = CompiledOrder[CompiledIndex];
				}
			}
		}

		void BuildBindingCollection()
		{
			for (const FEndpoint& Endpoint : Endpoints)
			{
				Data.Bindings.AddBindableStruct(FArcTQSBindableStructDescriptor(
					Endpoint.Name, Endpoint.Struct, Endpoint.ID, Endpoint.Handle));
			}
			for (const FArcTQSBinding& EditorBinding : Definition.EditorBindings.GetBindings())
			{
				CompileBinding(EditorBinding);
			}
			ValidateRequiredBindings();

			for (int32 EndpointIndex = 0; EndpointIndex < Endpoints.Num(); ++EndpointIndex)
			{
				FEndpoint& Endpoint = Endpoints[EndpointIndex];
				const EArcTQSDataKind Kind = Endpoint.Handle.GetKind();
				const FNodeFunctionRanges Ranges = NodeRanges.IsValidIndex(EndpointIndex) ? NodeRanges[EndpointIndex] : FNodeFunctionRanges();
				if (Kind == EArcTQSDataKind::Generator)
				{
					CreateBatch(Endpoint, Endpoint.ForwardBindings, Ranges.QueryBegin, Ranges.QueryEnd, Data.GeneratorBatch);
				}
				else if (Kind == EArcTQSDataKind::Step && Data.StepBatches.IsValidIndex(Endpoint.Handle.GetIndex()))
				{
					const int32 StepIndex = Endpoint.Handle.GetIndex();
					CreateBatch(Endpoint, Endpoint.ForwardBindings, Ranges.QueryBegin, Ranges.QueryEnd, Data.StepBatches[StepIndex]);
					CreateBatch(Endpoint, Endpoint.PerItemBindings, Ranges.PerItemBegin, Ranges.PerItemEnd, Data.StepPerItemBatches[StepIndex]);
				}
				else if (Kind == EArcTQSDataKind::Function && Data.Functions.IsValidIndex(Endpoint.Handle.GetIndex()))
				{
					CreateBatch(Endpoint, Endpoint.ForwardBindings, 0, 0, Data.Functions[Endpoint.Handle.GetIndex()].InputBatch);
				}
			}
		}

		EArcTQSDataKind GetRootKind(const FEndpoint& Endpoint) const
		{
			if (Endpoint.Handle.GetKind() != EArcTQSDataKind::Function)
			{
				return Endpoint.Handle.GetKind();
			}
			const int32 RootEndpointIndex = FunctionNodes[Endpoint.FunctionNodeIndex].RootEndpointIndex;
			return Endpoints[RootEndpointIndex].Handle.GetKind();
		}

		void CompileBinding(const FArcTQSBinding& EditorBinding)
		{
			const FPropertyBindingPath& SourcePath = EditorBinding.GetSourcePath();
			const FPropertyBindingPath& TargetPath = EditorBinding.GetTargetPath();
			const int32* SourceIndex = EndpointIndexByID.Find(SourcePath.GetStructID());
			const int32* TargetIndex = EndpointIndexByID.Find(TargetPath.GetStructID());
			if (!SourceIndex || !TargetIndex)
			{
				Error(TargetPath.GetStructID(), NSLOCTEXT("ArcTQS", "MissingBindingEndpoint", "A binding endpoint does not resolve to a query input, node or function."));
				return;
			}
			const FEndpoint& Source = Endpoints[*SourceIndex];
			FEndpoint& Target = Endpoints[*TargetIndex];
			const EArcTQSDataKind SourceKind = Source.Handle.GetKind();
			const EArcTQSDataKind TargetKind = Target.Handle.GetKind();
			const bool bTargetIsFunction = TargetKind == EArcTQSDataKind::Function;
			if ((!IsNodeKind(TargetKind) && !bTargetIsFunction) || IsNodeKind(SourceKind))
			{
				Error(Target.ID, NSLOCTEXT("ArcTQS", "InvalidBindingDirection", "Bindings must copy from a query input or function into a node or function."));
				return;
			}
			const EArcTQSDataKind RootKind = GetRootKind(Target);
			const bool bSourceIsFunction = SourceKind == EArcTQSDataKind::Function;
			if (!bSourceIsFunction && !ArcTQS::IsBindingSourceAccessible(SourceKind, RootKind))
			{
				Error(Target.ID, FText::Format(
					NSLOCTEXT("ArcTQS", "InaccessibleBindingSource", "'{0}' cannot be bound into this node."),
					FText::FromName(Source.Name)));
				return;
			}
			const bool bPerItem = SourceKind == EArcTQSDataKind::Item
				|| (bSourceIsFunction && FunctionNodes[Source.FunctionNodeIndex].bPerItem);
			if (bPerItem && RootKind != EArcTQSDataKind::Step)
			{
				Error(Target.ID, NSLOCTEXT("ArcTQS", "PerItemOutsideStep", "Per-item values can only be bound into steps."));
				return;
			}
			if (TargetPath.IsPathEmpty())
			{
				Error(Target.ID, NSLOCTEXT("ArcTQS", "EmptyBindingTarget", "A binding has no target field."));
				return;
			}

			FString PathError;
			TArray<FPropertyBindingPathIndirection> TargetIndirections;
			if (!TargetPath.ResolveIndirections(Target.Struct, TargetIndirections, &PathError, true) || TargetIndirections.IsEmpty())
			{
				Error(Target.ID, FText::Format(
					NSLOCTEXT("ArcTQS", "InvalidBindingTargetPath", "Binding target path is invalid: {0}"),
					FText::FromString(PathError)));
				return;
			}
			const bool bTargetIsLiteralOnly = TargetIndirections.ContainsByPredicate([](const FPropertyBindingPathIndirection& Indirection)
			{
				const FProperty* Property = Indirection.GetProperty();
				return Property && ArcTQS::IsLiteralOnlyProperty(*Property);
			});
			if (bTargetIsLiteralOnly)
			{
				Error(Target.ID, FText::Format(
					NSLOCTEXT("ArcTQS", "BindingTargetLiteralOnly", "Binding target '{0}' is a memory key and accepts only a literal value."),
					FText::FromString(TargetPath.ToString())));
				return;
			}
			const FProperty* TargetRootProperty = TargetIndirections[0].GetProperty();
			if (!TargetRootProperty || !ArcTQS::IsBindableProperty(*TargetRootProperty))
			{
				Error(Target.ID, FText::Format(
					NSLOCTEXT("ArcTQS", "BindingTargetNotEditable", "Binding target '{0}' is not an editable field."),
					FText::FromString(TargetPath.ToString())));
				return;
			}
			if (bTargetIsFunction && ArcTQS::IsFunctionOutputProperty(*TargetRootProperty))
			{
				Error(Target.ID, NSLOCTEXT("ArcTQS", "FunctionOutputAsInput", "A function output cannot be bound as an input."));
				return;
			}
			if (bSourceIsFunction
				&& (SourcePath.NumSegments() == 0 || SourcePath.GetSegment(0).GetName() != FunctionNodes[Source.FunctionNodeIndex].OutputName))
			{
				Error(Target.ID, NSLOCTEXT("ArcTQS", "FunctionSourceNotOutput", "A function binding must source the function's output field."));
				return;
			}

			TArray<FPropertyBindingPathIndirection> SourceIndirections;
			if (!SourcePath.ResolveIndirections(Source.Struct, SourceIndirections, &PathError, true))
			{
				Error(Target.ID, FText::Format(
					NSLOCTEXT("ArcTQS", "InvalidBindingSourcePath", "Binding source path is invalid: {0}"),
					FText::FromString(PathError)));
				return;
			}
			const FPropertyBindingPathIndirection SourceLeaf = SourceIndirections.IsEmpty()
				? FPropertyBindingPathIndirection(Source.Struct)
				: SourceIndirections.Last();
			FArcTQSBinding CompiledBinding(SourcePath, TargetPath);
			FPropertyBindingCopyInfo CopyInfo;
			if (!Data.Bindings.ResolveBindingCopyInfo(CompiledBinding, SourceLeaf, TargetIndirections.Last(), CopyInfo))
			{
				Error(Target.ID, FText::Format(
					NSLOCTEXT("ArcTQS", "IncompatibleBinding", "Binding '{0}' has incompatible source and target types."),
					FText::FromString(TargetPath.ToString())));
				return;
			}
			CompiledBinding.SetDataHandles(Source.Handle, Target.Handle);
			if (bPerItem && !bTargetIsFunction)
			{
				Target.PerItemBindings.Add(MoveTemp(CompiledBinding));
			}
			else
			{
				Target.ForwardBindings.Add(MoveTemp(CompiledBinding));
			}
		}

		void ValidateRequiredBindings()
		{
			for (const FEndpoint& Endpoint : Endpoints)
			{
				if (!IsNodeKind(Endpoint.Handle.GetKind()) && Endpoint.Handle.GetKind() != EArcTQSDataKind::Function)
				{
					continue;
				}
				for (TFieldIterator<FProperty> PropertyIt(Endpoint.Struct); PropertyIt; ++PropertyIt)
				{
					const FProperty* Property = *PropertyIt;
					if (!ArcTQS::IsRequiredBinding(*Property))
					{
						continue;
					}
					const FName PropertyName = Property->GetFName();
					if (!TargetsRootProperty(Endpoint.ForwardBindings, PropertyName)
						&& !TargetsRootProperty(Endpoint.PerItemBindings, PropertyName))
					{
						Error(Endpoint.ID, FText::Format(
							NSLOCTEXT("ArcTQS", "MissingRequiredBinding", "Required input '{0}' has no binding."),
							FText::FromName(PropertyName)));
					}
				}
			}
		}

		void CreateBatch(
			const FEndpoint& Target,
			TArray<FArcTQSBinding>& PendingBindings,
			const int32 FunctionsBegin,
			const int32 FunctionsEnd,
			FPropertyBindingIndex16& OutBatchIndex)
		{
			OutBatchIndex = FPropertyBindingIndex16();
			const bool bHasFunctions = FunctionsEnd > FunctionsBegin;
			if (PendingBindings.IsEmpty() && !bHasFunctions)
			{
				return;
			}
			const FArcTQSBindableStructDescriptor* TargetDescriptor = Data.Bindings.FindBindableStruct(Target.Handle);
			if (!TargetDescriptor)
			{
				Error(Target.ID, NSLOCTEXT("ArcTQS", "MissingCompiledTarget", "A compiled binding target has no data descriptor."));
				return;
			}
			struct FSortedBinding
			{
				FArcTQSBinding Binding;
				TArray<FPropertyBindingPathIndirection> DestinationIndirections;
			};
			TArray<FSortedBinding> SortedBindings;
			SortedBindings.Reserve(PendingBindings.Num());
			for (const FArcTQSBinding& Binding : PendingBindings)
			{
				FSortedBinding& SortedBinding = SortedBindings.AddDefaulted_GetRef();
				SortedBinding.Binding = Binding;
				FString PathError;
				if (!Binding.GetTargetPath().ResolveIndirections(Target.Struct, SortedBinding.DestinationIndirections, &PathError, true))
				{
					Error(Target.ID, FText::Format(
						NSLOCTEXT("ArcTQS", "InvalidCopyDestination", "A compiled binding destination path is invalid: {0}"),
						FText::FromString(PathError)));
					return;
				}
			}
			SortedBindings.StableSort([](const FSortedBinding& Left, const FSortedBinding& Right) -> bool
			{
				const int32 CommonPathLength = FMath::Min(Left.DestinationIndirections.Num(), Right.DestinationIndirections.Num());
				for (int32 Index = 0; Index < CommonPathLength; ++Index)
				{
					const FPropertyBindingPathIndirection& LeftIndirection = Left.DestinationIndirections[Index];
					const FPropertyBindingPathIndirection& RightIndirection = Right.DestinationIndirections[Index];
					if (LeftIndirection.GetPropertyOffset() != RightIndirection.GetPropertyOffset())
					{
						return LeftIndirection.GetPropertyOffset() < RightIndirection.GetPropertyOffset();
					}
					if (LeftIndirection.GetArrayIndex() != RightIndirection.GetArrayIndex())
					{
						return LeftIndirection.GetArrayIndex() < RightIndirection.GetArrayIndex();
					}
				}
				return Left.DestinationIndirections.Num() < Right.DestinationIndirections.Num();
			});

			const int32 BindingsBegin = Data.Bindings.GetNumBindings();
			const int32 BindingsEnd = BindingsBegin + SortedBindings.Num();
			const int32 BatchIndex = Data.Bindings.GetNumCopyBatches();
			if (!FPropertyBindingIndex16::IsValidIndex(BindingsBegin)
				|| !FPropertyBindingIndex16::IsValidIndex(BindingsEnd)
				|| !FPropertyBindingIndex16::IsValidIndex(BatchIndex))
			{
				Error(Target.ID, NSLOCTEXT("ArcTQS", "TooManyBindings", "The target query contains too many compiled bindings or batches."));
				return;
			}
			FPropertyBindingCopyInfoBatch& Batch = Data.Bindings.AddCopyBatch();
			Batch.TargetStruct = TInstancedStruct<FPropertyBindingBindableStructDescriptor>::Make(*TargetDescriptor);
			Batch.BindingsBegin = FPropertyBindingIndex16(BindingsBegin);
			for (FSortedBinding& SortedBinding : SortedBindings)
			{
				Data.Bindings.AddCompiledBinding(MoveTemp(SortedBinding.Binding));
			}
			Batch.BindingsEnd = FPropertyBindingIndex16(BindingsEnd);
			Batch.PropertyFunctionsBegin = bHasFunctions ? FPropertyBindingIndex16(FunctionsBegin) : FPropertyBindingIndex16();
			Batch.PropertyFunctionsEnd = bHasFunctions ? FPropertyBindingIndex16(FunctionsEnd) : FPropertyBindingIndex16();
			OutBatchIndex = FPropertyBindingIndex16(BatchIndex);
			PendingBindings.Reset();
		}

		void Error(const FGuid NodeID, const FText& Message)
		{
			FArcTQSCompilerDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
			Diagnostic.NodeID = NodeID;
			Diagnostic.Message = Message;
			Diagnostic.bIsError = true;
			bFailed = true;
			++NumErrors;
		}

		const UArcTQSQueryDefinition& Definition;
		uint32 Revision = 0;
		FArcTQSCompiledData& Data;
		TArray<FArcTQSCompilerDiagnostic>& Diagnostics;
		TArray<FEndpoint> Endpoints;
		TMap<FGuid, int32> EndpointIndexByID;
		TArray<FFunctionNode> FunctionNodes;
		TMap<FGuid, int32> FunctionIndexByID;
		TArray<FNodeFunctionRanges> NodeRanges;
		int32 NumErrors = 0;
		bool bFailed = false;
	};
}

bool FArcTQSCompiler::Compile(
	const UArcTQSQueryDefinition& Definition,
	const uint32 Revision,
	FArcTQSCompiledData& OutData,
	TArray<FArcTQSCompilerDiagnostic>& OutDiagnostics)
{
	OutDiagnostics.Reset();
	ArcTQSCompilerPrivate::FCompiler Compiler(Definition, Revision, OutData, OutDiagnostics);
	return Compiler.Run();
}

#endif
