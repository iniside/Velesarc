// Copyright Lukasz Baran. All Rights Reserved.

#include "Decision/ArcTacticsCompiler.h"

#if WITH_EDITOR

#include "Actions/ArcTacticsSequenceAction.h"
#include "ArcTQSBindings.h"
#include "ArcTQSPropertyFunction.h"
#include "ArcTQSQueryDefinition.h"
#include "ArcTQSQueryReference.h"
#include "Decision/ArcTacticsBindings.h"
#include "Decision/ArcTacticsCompiledData.h"
#include "Decision/ArcTacticsConfig.h"
#include "Decision/ArcTacticsNode.h"
#include "Decision/ArcTacticsSelectors.h"
#include "Considerations/ArcTacticsConsiderations.h"
#include "PropertyBindingDataView.h"
#include "StructUtils/PropertyBag.h"
#include "UtilityAI/ArcUtilityConsideration.h"
#include "UObject/UnrealType.h"

namespace ArcTacticsCompilerPrivate
{
	struct FEndpoint
	{
		FArcTacticsBindingEndpoint Endpoint;
		FName Name;
		FArcTacticsDataHandle Handle;
		const UStruct* Struct = nullptr;
		int32 NodeIndex = INDEX_NONE;
		int32 FunctionNodeIndex = INDEX_NONE;
		int32 FunctionsBegin = 0;
		int32 FunctionsEnd = 0;
		TArray<FArcTacticsBinding> ForwardBindings;
		TArray<FArcTacticsBinding> OutputBindings;
	};

	struct FFunctionNode
	{
		FGuid ID;
		FConstStructView Node;
		FName OutputName;
		FGuid OwnerTargetID;
		TArray<int32> Children;
	};

	class FCompiler
	{
	public:
		FCompiler(
			const UArcTacticsConfig& InConfig,
			const uint32 InRevision,
			FArcTacticsCompiledData& InOutData,
			TArray<FArcTacticsCompilerDiagnostic>& InOutDiagnostics)
			: Config(InConfig)
			, Revision(InRevision)
			, Data(InOutData)
			, Diagnostics(InOutDiagnostics)
		{
		}

		bool Run()
		{
			Data.Reset();
			Data.Version = FArcTacticsCompiledData::CurrentVersion;
			Data.Revision = Revision;
			Data.DefaultDecisionSettings = Config.DecisionSettings;
			Data.DefaultGlobalParameters = Config.GlobalParameters;

			ValidateRootDefaults();
			AddRootEndpoints();
			CompileNodes();
			Data.DefaultInstanceData = Defaults;
			if (BuildFunctionGraph())
			{
				BuildBindingCollection();
			}
			ValidateMemoryKeys();

			Data.bIsValid = !bFailed;
			if (Data.bIsValid && !Data.ResolvePaths())
			{
				Error(FGuid(), NSLOCTEXT("ArcTactics", "ResolveCompiledBindings", "Compiled binding paths could not be resolved."));
			}
			return Data.bIsValid;
		}

	private:
		void ValidateRootDefaults()
		{
			if (!Data.DefaultDecisionSettings.IsValid())
			{
				Error(Config.DecisionSettingsID, NSLOCTEXT("ArcTactics", "InvalidDecisionSettings", "Decision setting defaults are outside their finite supported ranges."));
			}
		}

		void AddRootEndpoints()
		{
			AddEndpoint(Config.ContextID, TEXT("Context"),
				FArcTacticsDataHandle(EArcTacticsBindingEndpointKind::Context), FArcTacticsContextData::StaticStruct());
			AddEndpoint(Config.DecisionSettingsID, TEXT("DecisionSettings"),
				FArcTacticsDataHandle(EArcTacticsBindingEndpointKind::DecisionSettings), FArcTacticsDecisionSettingsParameters::StaticStruct());
			if (const UPropertyBag* GlobalParametersStruct = Config.GlobalParameters.GetPropertyBagStruct())
			{
				AddEndpoint(Config.GlobalParametersID, TEXT("GlobalParameters"),
					FArcTacticsDataHandle(EArcTacticsBindingEndpointKind::GlobalParameters), GlobalParametersStruct);
			}
		}

		void CompileNodes()
		{
			for (int32 Index = 0; Index < Config.GlobalTasks.Num(); ++Index)
			{
				Data.GlobalTasks.Add(CompileNode(
					Config.GlobalTasks[Index], EArcTacticsCompiledNodeKind::GlobalTask, INDEX_NONE, Index));
			}

			for (int32 Index = 0; Index < Config.Actions.Num(); ++Index)
			{
				const FArcTacticsActionDefinition& Source = Config.Actions[Index];
				FArcTacticsCompiledAction& Compiled = Data.Actions.AddDefaulted_GetRef();
				Compiled.Name = Source.Action.Name;
				CompileBranchDefaults(Source.ParametersID, Source.ParameterBagID, Source.Parameters, Source.ParameterBag, Index,
					Compiled.Parameters, Compiled.ParameterBag, Compiled.DefaultParameters, Compiled.DefaultParameterBag);
				Compiled.Action = CompileNode(Source.Action, EArcTacticsCompiledNodeKind::Action, Index, Source.Considerations.Num());
				CompileSteps(Source.Action, Source.Steps, Source.Considerations.Num(), Index, Compiled.Steps);
				CompileActiveSteps(Source.Action, Source.ActiveSteps, Source.Considerations.Num() + 1 + Source.Steps.Num(),
					Index, Compiled.ActiveSteps);
				for (int32 ConsiderationIndex = 0; ConsiderationIndex < Source.Considerations.Num(); ++ConsiderationIndex)
				{
					Compiled.Considerations.Add(CompileNode(Source.Considerations[ConsiderationIndex],
						EArcTacticsCompiledNodeKind::Consideration, Index, ConsiderationIndex));
				}
			}

			for (int32 Index = 0; Index < Config.Selectors.Num(); ++Index)
			{
				const FArcTacticsSelectorDefinition& Source = Config.Selectors[Index];
				FArcTacticsCompiledSelector& Compiled = Data.Selectors.AddDefaulted_GetRef();
				Compiled.Name = Source.Selector.Name;
				CompileBranchDefaults(Source.ParametersID, Source.ParameterBagID, Source.Parameters, Source.ParameterBag,
					Config.Actions.Num() + Index, Compiled.Parameters, Compiled.ParameterBag,
					Compiled.DefaultParameters, Compiled.DefaultParameterBag);
				Compiled.Selector = CompileNode(Source.Selector, EArcTacticsCompiledNodeKind::Selector, Index, INDEX_NONE);
				Compiled.Action = CompileNode(Source.Action, EArcTacticsCompiledNodeKind::Action, Index, Source.Considerations.Num());
				CompileSteps(Source.Action, Source.Steps, Source.Considerations.Num(), Index, Compiled.Steps);
				CompileActiveSteps(Source.Action, Source.ActiveSteps, Source.Considerations.Num() + 1 + Source.Steps.Num(),
					Index, Compiled.ActiveSteps);
				for (int32 ConsiderationIndex = 0; ConsiderationIndex < Source.Considerations.Num(); ++ConsiderationIndex)
				{
					Compiled.Considerations.Add(CompileNode(Source.Considerations[ConsiderationIndex],
						EArcTacticsCompiledNodeKind::Consideration, Index, ConsiderationIndex));
				}
			}
		}

		void CompileSteps(
			const FArcTacticsNodeDefinition& Action,
			const TArray<FArcTacticsNodeDefinition>& Steps,
			const int32 ConsiderationCount,
			const int32 OwnerIndex,
			TArray<FArcTacticsNodeHandle>& OutSteps)
		{
			const UScriptStruct* ActionType = Action.Node.GetScriptStruct();
			const bool bIsSequence = ActionType && ActionType->IsChildOf(FArcTacticsSequenceAction::StaticStruct());
			if (bIsSequence && Steps.IsEmpty())
			{
				Error(Action.ID, NSLOCTEXT("ArcTactics", "EmptySequence", "A Tactical Action Sequence needs at least one step."));
			}
			if (!bIsSequence && !Steps.IsEmpty())
			{
				Error(Action.ID, NSLOCTEXT("ArcTactics", "StepsWithoutSequence", "Only a Tactical Action Sequence runs steps."));
			}
			for (int32 StepIndex = 0; StepIndex < Steps.Num(); ++StepIndex)
			{
				OutSteps.Add(CompileNode(Steps[StepIndex], EArcTacticsCompiledNodeKind::Step, OwnerIndex,
					ConsiderationCount + 1 + StepIndex));
			}
		}

		void CompileActiveSteps(
			const FArcTacticsNodeDefinition& Action,
			const TArray<FArcTacticsNodeDefinition>& ActiveSteps,
			const int32 FirstExecutionIndex,
			const int32 OwnerIndex,
			TArray<FArcTacticsNodeHandle>& OutActiveSteps)
		{
			const UScriptStruct* ActionType = Action.Node.GetScriptStruct();
			const bool bIsSequence = ActionType && ActionType->IsChildOf(FArcTacticsSequenceAction::StaticStruct());
			if (!bIsSequence && !ActiveSteps.IsEmpty())
			{
				Error(Action.ID, NSLOCTEXT("ArcTactics", "ActiveStepsWithoutSequence", "Only a Tactical Action Sequence runs active steps."));
			}
			for (int32 ActiveStepIndex = 0; ActiveStepIndex < ActiveSteps.Num(); ++ActiveStepIndex)
			{
				OutActiveSteps.Add(CompileNode(ActiveSteps[ActiveStepIndex], EArcTacticsCompiledNodeKind::ActiveStep,
					OwnerIndex, FirstExecutionIndex + ActiveStepIndex));
			}
		}

		void CompileBranchDefaults(
			const FGuid ParametersID,
			const FGuid ParameterBagID,
			const FArcTacticsBranchParameters& SourceParameters,
			const FInstancedPropertyBag& SourceParameterBag,
			const int32 BranchIndex,
			FArcTacticsDataHandle& OutParametersHandle,
			FArcTacticsDataHandle& OutParameterBagHandle,
			FArcTacticsBranchParameters& OutDefaultParameters,
			FInstancedPropertyBag& OutDefaultParameterBag)
		{
			OutDefaultParameters = SourceParameters;
			OutDefaultParameterBag = SourceParameterBag;
			if (!OutDefaultParameters.IsValid())
			{
				Error(ParametersID, NSLOCTEXT("ArcTactics", "InvalidBranchParameters", "Branch parameter defaults are outside their finite supported ranges."));
			}
			if (BranchIndex < 0 || BranchIndex >= static_cast<int32>(MAX_uint16))
			{
				Error(ParametersID, NSLOCTEXT("ArcTactics", "TooManyBranches", "The tactics configuration contains too many action and selector branches."));
				return;
			}
			OutParametersHandle = FArcTacticsDataHandle(EArcTacticsBindingEndpointKind::BranchParameters, BranchIndex);
			OutParameterBagHandle = FArcTacticsDataHandle(EArcTacticsBindingEndpointKind::BranchParameterBag, BranchIndex);
			AddEndpoint(ParametersID, FName(*Config.GetAuthoringName(ParametersID).ToString()), OutParametersHandle,
				FArcTacticsBranchParameters::StaticStruct());
			if (const UPropertyBag* ParameterBagStruct = SourceParameterBag.GetPropertyBagStruct())
			{
				AddEndpoint(ParameterBagID, FName(*Config.GetAuthoringName(ParameterBagID).ToString()),
					OutParameterBagHandle, ParameterBagStruct);
			}
		}

		FArcTacticsNodeHandle CompileNode(
			const FArcTacticsNodeDefinition& Definition,
			const EArcTacticsCompiledNodeKind Kind,
			const int32 OwnerIndex,
			const int32 ExecutionIndex)
		{
			if (!Definition.ID.IsValid())
			{
				Error(Definition.ID, NSLOCTEXT("ArcTactics", "MissingNodeID", "A tactics node has no stable authoring ID."));
			}
			if (!Definition.Node.IsValid())
			{
				Error(Definition.ID, NSLOCTEXT("ArcTactics", "MissingNode", "A tactics node definition is empty."));
				return FArcTacticsNodeHandle();
			}
			const FArcTacticsNode* Node = Definition.Node.GetPtr<FArcTacticsNode>();
			if (!Node || Definition.Node.GetScriptStruct() == FArcTacticsNode::StaticStruct())
			{
				Error(Definition.ID, NSLOCTEXT("ArcTactics", "InvalidNode", "A tactics node must contain a concrete FArcTacticsNode type."));
				return FArcTacticsNodeHandle();
			}
			EArcTacticsNodeRole ExpectedRole = EArcTacticsNodeRole::Invalid;
			if (Kind == EArcTacticsCompiledNodeKind::GlobalTask)
			{
				ExpectedRole = EArcTacticsNodeRole::GlobalTask;
			}
			else if (Kind == EArcTacticsCompiledNodeKind::Action)
			{
				ExpectedRole = EArcTacticsNodeRole::Action;
			}
			else if (Kind == EArcTacticsCompiledNodeKind::Consideration)
			{
				ExpectedRole = EArcTacticsNodeRole::Consideration;
			}
			else if (Kind == EArcTacticsCompiledNodeKind::Selector)
			{
				ExpectedRole = EArcTacticsNodeRole::Selector;
			}
			else if (Kind == EArcTacticsCompiledNodeKind::Step)
			{
				ExpectedRole = EArcTacticsNodeRole::Step;
			}
			else if (Kind == EArcTacticsCompiledNodeKind::ActiveStep)
			{
				ExpectedRole = EArcTacticsNodeRole::ActiveStep;
			}
			if (Kind == EArcTacticsCompiledNodeKind::Consideration
				&& !Definition.Node.GetScriptStruct()->IsChildOf(FArcTacticsConsiderationNode::StaticStruct()))
			{
				Error(Definition.ID, NSLOCTEXT("ArcTactics", "InvalidConsiderationNode", "A consideration must contain an FArcTacticsConsiderationNode type."));
				return FArcTacticsNodeHandle();
			}
			if (Kind == EArcTacticsCompiledNodeKind::Selector
				&& !Definition.Node.GetScriptStruct()->IsChildOf(FArcTacticsSelectorNode::StaticStruct()))
			{
				Error(Definition.ID, NSLOCTEXT("ArcTactics", "InvalidSelectorNode", "A selector must contain an FArcTacticsSelectorNode type."));
				return FArcTacticsNodeHandle();
			}
			if (Kind == EArcTacticsCompiledNodeKind::Step
				&& !Definition.Node.GetScriptStruct()->IsChildOf(FArcTacticsStepNode::StaticStruct()))
			{
				Error(Definition.ID, NSLOCTEXT("ArcTactics", "InvalidStepNode", "A step must contain an FArcTacticsStepNode type."));
				return FArcTacticsNodeHandle();
			}
			if (Kind == EArcTacticsCompiledNodeKind::ActiveStep
				&& !Definition.Node.GetScriptStruct()->IsChildOf(FArcTacticsActiveStepNode::StaticStruct()))
			{
				Error(Definition.ID, NSLOCTEXT("ArcTactics", "InvalidActiveStepNode", "An active step must contain an FArcTacticsActiveStepNode type."));
				return FArcTacticsNodeHandle();
			}
			if (Node->GetRole() != ExpectedRole)
			{
				Error(Definition.ID, NSLOCTEXT("ArcTactics", "MismatchedNodeRole", "A tactics node type is authored in an incompatible section."));
				return FArcTacticsNodeHandle();
			}

			const UScriptStruct* InstanceType = Node->GetInstanceDataType();
			if (!InstanceType)
			{
				Error(Definition.ID, NSLOCTEXT("ArcTactics", "MissingInstanceType", "A tactics node returned no instance-data type."));
				return FArcTacticsNodeHandle();
			}
			if (Kind == EArcTacticsCompiledNodeKind::Consideration
				&& !InstanceType->IsChildOf(FArcUtilityConsideration::StaticStruct()))
			{
				Error(Definition.ID, NSLOCTEXT("ArcTactics", "InvalidConsiderationInstance", "A consideration must use FArcUtilityConsideration instance data."));
				return FArcTacticsNodeHandle();
			}

			FInstancedStruct DefaultData = Definition.InstanceData;
			if (!DefaultData.IsValid())
			{
				DefaultData.InitializeAs(InstanceType);
			}
			else if (DefaultData.GetScriptStruct() != InstanceType)
			{
				Error(Definition.ID, NSLOCTEXT("ArcTactics", "MismatchedInstanceType", "A tactics node's authored instance data does not match its declared type."));
				DefaultData.InitializeAs(InstanceType);
			}
			if (Kind == EArcTacticsCompiledNodeKind::Consideration)
			{
				const FArcUtilityConsideration* Consideration = DefaultData.GetPtr<FArcUtilityConsideration>();
				if (!Consideration)
				{
					Error(Definition.ID, NSLOCTEXT("ArcTactics", "MissingConsiderationDefaults", "A consideration has no valid default instance data."));
					return FArcTacticsNodeHandle();
				}
				bool bValidDefaults = true;
				FString ValidationError;
				if (!Consideration->Validate(ValidationError))
				{
					Error(Definition.ID, FText::Format(
						NSLOCTEXT("ArcTactics", "InvalidConsiderationDefaults", "A consideration's defaults are invalid: {0}"),
						FText::FromString(ValidationError)));
					bValidDefaults = false;
				}
				if (!bValidDefaults)
				{
					return FArcTacticsNodeHandle();
				}
			}

			ValidateQueryReferences(Definition.ID, DefaultData);

			const int32 NodeIndex = Data.Nodes.Num();
			if (NodeIndex >= static_cast<int32>(MAX_uint16))
			{
				Error(Definition.ID, NSLOCTEXT("ArcTactics", "TooManyNodes", "The tactics configuration contains too many compiled nodes."));
				return FArcTacticsNodeHandle();
			}

			Defaults.Add(MoveTemp(DefaultData));
			const FName NodeName = Definition.Name.IsNone() ? FName(*Node->GetDisplayName().ToString()) : Definition.Name;
			FArcTacticsCompiledNode& Compiled = Data.Nodes.AddDefaulted_GetRef();
			Compiled.Name = NodeName;
			Compiled.Node = Definition.Node;
			Compiled.InstanceData = FArcTacticsDataHandle(EArcTacticsBindingEndpointKind::Node, NodeIndex);
			Compiled.Kind = Kind;
			Compiled.OwnerIndex = OwnerIndex;
			Compiled.ExecutionIndex = ExecutionIndex;
			AddEndpoint(Definition.ID, NodeName, Compiled.InstanceData, InstanceType, NodeIndex);
			return FArcTacticsNodeHandle(NodeIndex);
		}

		void ValidateQueryReferences(const FGuid NodeID, FInstancedStruct& InstanceData)
		{
			ArcTacticsNode::ForEachQueryReference(*InstanceData.GetScriptStruct(), InstanceData.GetMutableMemory(),
				[this, NodeID](FArcTQSQueryReference& Reference)
			{
				const UArcTQSQueryDefinition* QueryDefinition = Reference.GetDefinition();
				if (QueryDefinition && !QueryDefinition->IsExecutable())
				{
					Error(NodeID, FText::Format(
						NSLOCTEXT("ArcTactics", "QueryDefinitionNotExecutable", "Query definition '{0}' is not executable; fix and compile it first."),
						FText::FromString(QueryDefinition->GetName())));
				}
			});
		}

		void ValidateMemoryKeys()
		{
			TMap<FGameplayTag, FString> TypeByKey;
			for (const FEndpoint& Endpoint : Endpoints)
			{
				if (Defaults.IsValidIndex(Endpoint.NodeIndex))
				{
					AddMemoryKeys(Endpoint.Endpoint.ID, Endpoint.Name, FConstStructView(Defaults[Endpoint.NodeIndex]), TypeByKey);
				}
			}
			for (const FFunctionNode& FunctionNode : FunctionNodes)
			{
				AddMemoryKeys(FunctionNode.ID, FName(*FunctionNode.Node.GetScriptStruct()->GetDisplayNameText().ToString()),
					FunctionNode.Node, TypeByKey);
			}
		}

		void AddMemoryKeys(const FGuid ID, const FName NodeName, const FConstStructView View,
			TMap<FGameplayTag, FString>& TypeByKey)
		{
			if (!View.IsValid())
			{
				return;
			}
			ArcTQS::ForEachMemoryKey(*View.GetScriptStruct(), View.GetMemory(),
				[this, ID, NodeName, &TypeByKey](const FProperty& Property, const FGameplayTag Key)
			{
				if (!Key.IsValid())
				{
					Error(ID, FText::Format(
						NSLOCTEXT("ArcTactics", "MemoryKeyNotSet", "Memory key '{0}' on '{1}' is not set."),
						FText::FromName(Property.GetFName()), FText::FromName(NodeName)));
					return;
				}
				const FString& Type = Property.GetMetaData(TEXT("ArcMemoryType"));
				if (Type.IsEmpty())
				{
					return;
				}
				const FString* Existing = TypeByKey.Find(Key);
				if (!Existing)
				{
					TypeByKey.Add(Key, Type);
				}
				else if (*Existing != Type)
				{
					Error(ID, FText::Format(
						NSLOCTEXT("ArcTactics", "MemoryKeyTypeConflict", "Memory key '{0}' is used as {1} and as {2} in this configuration."),
						FText::FromString(Key.ToString()), FText::FromString(*Existing), FText::FromString(Type)));
				}
			});
		}

		bool BuildFunctionGraph()
		{
			const int32 NumErrorsBefore = NumErrors;
			CollectFunctions();
			if (NumErrors == NumErrorsBefore)
			{
				ResolveFunctionRoots();
			}
			if (NumErrors == NumErrorsBefore)
			{
				for (int32 FunctionIndex = 0; FunctionIndex < FunctionNodes.Num(); ++FunctionIndex)
				{
					if (const int32* ParentIndex = FunctionIndexByID.Find(FunctionNodes[FunctionIndex].OwnerTargetID))
					{
						FunctionNodes[*ParentIndex].Children.Add(FunctionIndex);
					}
				}
				OrderFunctions();
			}
			return NumErrors == NumErrorsBefore;
		}

		void CollectFunctions()
		{
			const TArray<FArcTacticsBinding>& EditorBindings = Config.EditorBindings.GetBindings();
			TArray<int32> OwnerBindingIndices;
			for (int32 BindingIndex = 0; BindingIndex < EditorBindings.Num(); ++BindingIndex)
			{
				const FArcTacticsBinding& Binding = EditorBindings[BindingIndex];
				const FConstStructView NodeView = Binding.GetPropertyFunctionNode();
				if (!NodeView.IsValid())
				{
					continue;
				}
				const FGuid TargetID = Binding.GetTargetPath().GetStructID();
				const FArcTQSPropertyFunction* Function = NodeView.GetPtr<const FArcTQSPropertyFunction>();
				if (!Function)
				{
					Error(TargetID, NSLOCTEXT("ArcTactics", "InvalidFunctionNode", "A bound function does not derive from FArcTQSPropertyFunction."));
					continue;
				}
				if (Binding.IsOutputBinding())
				{
					Error(TargetID, NSLOCTEXT("ArcTactics", "FunctionOutputBinding", "An output binding cannot use a function."));
					continue;
				}
				if (!Function->ID.IsValid() || Binding.GetSourcePath().GetStructID() != Function->ID)
				{
					Error(TargetID, NSLOCTEXT("ArcTactics", "FunctionSourceMismatch", "A function binding must source the output of its own function."));
					continue;
				}
				if (FunctionIndexByID.Contains(Function->ID) || EndpointIndexByID.Contains(Function->ID))
				{
					Error(TargetID, NSLOCTEXT("ArcTactics", "DuplicateFunctionID", "Two binding endpoints share the same stable ID as a function."));
					continue;
				}
				int32 NumOutputs = 0;
				const FProperty* OutputProperty = ArcTQS::FindFunctionOutputProperty(NodeView.GetScriptStruct(), &NumOutputs);
				if (!OutputProperty || NumOutputs != 1)
				{
					Error(TargetID, FText::Format(
						NSLOCTEXT("ArcTactics", "FunctionOutputCount", "Function '{0}' must declare exactly one ArcTQSOutput field."),
						NodeView.GetScriptStruct()->GetDisplayNameText()));
					continue;
				}
				FFunctionNode& FunctionNode = FunctionNodes.AddDefaulted_GetRef();
				FunctionNode.ID = Function->ID;
				FunctionNode.Node = NodeView;
				FunctionNode.OutputName = OutputProperty->GetFName();
				FunctionNode.OwnerTargetID = TargetID;
				FunctionIndexByID.Add(Function->ID, FunctionNodes.Num() - 1);
				OwnerBindingIndices.Add(BindingIndex);
			}

			for (int32 BindingIndex = 0; BindingIndex < EditorBindings.Num(); ++BindingIndex)
			{
				const FArcTacticsBinding& Binding = EditorBindings[BindingIndex];
				const int32* FunctionIndex = FunctionIndexByID.Find(Binding.GetSourcePath().GetStructID());
				if (FunctionIndex && OwnerBindingIndices[*FunctionIndex] != BindingIndex)
				{
					Error(Binding.GetTargetPath().GetStructID(), NSLOCTEXT("ArcTactics", "SharedFunctionOutput", "A function output can feed only the binding that owns the function."));
				}
			}
		}

		void ResolveFunctionRoots()
		{
			for (const FFunctionNode& FunctionNode : FunctionNodes)
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
					Error(FunctionNode.ID, NSLOCTEXT("ArcTactics", "FunctionCycle", "Bound functions form a cycle."));
					continue;
				}
				const int32* RootIndex = EndpointIndexByID.Find(CurrentID);
				if (!RootIndex || Endpoints[*RootIndex].Endpoint.Kind == EArcTacticsBindingEndpointKind::Context)
				{
					Error(FunctionNode.ID, NSLOCTEXT("ArcTactics", "FunctionWithoutRoot", "A bound function does not feed a tactics node, parameters or decision settings."));
				}
			}
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
				Error(FGuid(), NSLOCTEXT("ArcTactics", "TooManyFunctions", "The tactics configuration contains too many bound functions."));
				return;
			}
			TArray<int32> CompiledOrder;
			CompiledOrder.Reserve(FunctionNodes.Num());
			const int32 NumRootEndpoints = Endpoints.Num();
			for (int32 EndpointIndex = 0; EndpointIndex < NumRootEndpoints; ++EndpointIndex)
			{
				FEndpoint& Endpoint = Endpoints[EndpointIndex];
				Endpoint.FunctionsBegin = CompiledOrder.Num();
				for (int32 FunctionIndex = 0; FunctionIndex < FunctionNodes.Num(); ++FunctionIndex)
				{
					if (FunctionNodes[FunctionIndex].OwnerTargetID == Endpoint.Endpoint.ID)
					{
						CollectPostOrder(FunctionIndex, CompiledOrder);
					}
				}
				Endpoint.FunctionsEnd = CompiledOrder.Num();
			}

			Data.Functions.SetNum(CompiledOrder.Num());
			for (int32 CompiledIndex = 0; CompiledIndex < CompiledOrder.Num(); ++CompiledIndex)
			{
				const FFunctionNode& FunctionNode = FunctionNodes[CompiledOrder[CompiledIndex]];
				Data.Functions[CompiledIndex].Function = FInstancedStruct(FunctionNode.Node);
				const int32 EndpointIndex = AddEndpoint(FunctionNode.ID,
					FName(*FunctionNode.Node.GetScriptStruct()->GetDisplayNameText().ToString()),
					FArcTacticsDataHandle(EArcTacticsBindingEndpointKind::Function, CompiledIndex),
					FunctionNode.Node.GetScriptStruct());
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
				Data.Bindings.AddBindableStruct(FArcTacticsBindableStructDescriptor(
					Endpoint.Name, Endpoint.Struct, Endpoint.Endpoint.ID, Endpoint.Handle));
			}
			for (const FArcTacticsBinding& EditorBinding : Config.EditorBindings.GetBindings())
			{
				CompileBinding(EditorBinding);
			}
			ValidateRequiredBindings();

			CreateForwardBatch(Config.GlobalParametersID, Data.GlobalInitializationBindingsBatch);
			CreateForwardBatch(Config.DecisionSettingsID, Data.DecisionSettingsBindingsBatch);
			for (int32 Index = 0; Index < Config.Actions.Num(); ++Index)
			{
				FArcTacticsCompiledAction& Compiled = Data.Actions[Index];
				CreateForwardBatch(Config.Actions[Index].ParameterBagID, Compiled.ParameterBagInitializationBindingsBatch);
				CreateForwardBatch(Config.Actions[Index].ParametersID, Compiled.ParametersInitializationBindingsBatch);
			}
			for (int32 Index = 0; Index < Config.Selectors.Num(); ++Index)
			{
				FArcTacticsCompiledSelector& Compiled = Data.Selectors[Index];
				CreateForwardBatch(Config.Selectors[Index].ParameterBagID, Compiled.ParameterBagInitializationBindingsBatch);
				CreateForwardBatch(Config.Selectors[Index].ParametersID, Compiled.ParametersInitializationBindingsBatch);
			}
			for (FEndpoint& Endpoint : Endpoints)
			{
				if (Endpoint.Handle.GetKind() == EArcTacticsBindingEndpointKind::Function
					&& Data.Functions.IsValidIndex(Endpoint.Handle.GetIndex()))
				{
					CreateBatch(Endpoint, Endpoint.ForwardBindings, 0, 0, Data.Functions[Endpoint.Handle.GetIndex()].InputBatch);
					continue;
				}
				if (Endpoint.NodeIndex == INDEX_NONE || !Data.Nodes.IsValidIndex(Endpoint.NodeIndex))
				{
					continue;
				}
				FArcTacticsCompiledNode& Node = Data.Nodes[Endpoint.NodeIndex];
				CreateBatch(Endpoint, Endpoint.ForwardBindings, Endpoint.FunctionsBegin, Endpoint.FunctionsEnd, Node.BindingsBatch);
				CreateBatch(Endpoint, Endpoint.OutputBindings, 0, 0, Node.OutputBindingsBatch);
			}
		}

		void CompileBinding(const FArcTacticsBinding& EditorBinding)
		{
			const int32* SourceIndex = EndpointIndexByID.Find(EditorBinding.GetSourcePath().GetStructID());
			const int32* TargetIndex = EndpointIndexByID.Find(EditorBinding.GetTargetPath().GetStructID());
			if (!SourceIndex || !TargetIndex)
			{
				Error(FGuid(), NSLOCTEXT("ArcTactics", "MissingBindingEndpoint", "A binding endpoint does not resolve to compiled execution data."));
				return;
			}
			const FEndpoint& Source = Endpoints[*SourceIndex];
			FEndpoint& Target = Endpoints[*TargetIndex];
			FString PathError;
			FArcTacticsBinding CompiledBinding(EditorBinding.GetSourcePath(), EditorBinding.GetTargetPath(), EditorBinding.IsOutputBinding());
			const EArcTacticsBindingValidationResult ValidationResult = Data.Bindings.ValidateEditorBinding(
				CompiledBinding, Source.Struct, Source.Endpoint, Target.Struct, Target.Endpoint,
				Config.IsBindingSourceAccessible(Source.Endpoint.ID, Target.Endpoint.ID, CompiledBinding.IsOutputBinding()),
				&PathError);
			switch (ValidationResult)
			{
			case EArcTacticsBindingValidationResult::Valid:
				break;
			case EArcTacticsBindingValidationResult::InvalidTarget:
				Error(Target.Endpoint.ID, NSLOCTEXT("ArcTactics", "InvalidBindingTarget", "The binding target is not executable tactics data."));
				return;
			case EArcTacticsBindingValidationResult::InvalidScope:
				Error(Target.Endpoint.ID, NSLOCTEXT("ArcTactics", "InvalidBindingScope", "A binding source is outside the target's accessible scope or execution phase."));
				return;
			case EArcTacticsBindingValidationResult::InvalidSourcePath:
				Error(Target.Endpoint.ID, FText::Format(NSLOCTEXT("ArcTactics", "InvalidBindingSourcePath", "Binding source path is invalid: {0}"), FText::FromString(PathError)));
				return;
			case EArcTacticsBindingValidationResult::InvalidTargetPath:
				Error(Target.Endpoint.ID, FText::Format(NSLOCTEXT("ArcTactics", "InvalidBindingTargetPath", "Binding target path is invalid: {0}"), FText::FromString(PathError)));
				return;
			case EArcTacticsBindingValidationResult::InvalidRole:
				Error(Target.Endpoint.ID, NSLOCTEXT("ArcTactics", "InvalidBindingRole", "A binding path uses an exposed property with a missing or conflicting tactics role."));
				return;
			case EArcTacticsBindingValidationResult::SourceNotOutput:
				Error(Target.Endpoint.ID, NSLOCTEXT("ArcTactics", "BindingSourceNotOutput", "Bindings from nodes must source an Output property."));
				return;
			case EArcTacticsBindingValidationResult::TargetNotInput:
				Error(Target.Endpoint.ID, NSLOCTEXT("ArcTactics", "BindingTargetNotInput", "Forward bindings may only target Input or Parameter properties."));
				return;
			case EArcTacticsBindingValidationResult::InvalidOutputTarget:
				Error(Target.Endpoint.ID, NSLOCTEXT("ArcTactics", "InvalidOutputBinding", "Output bindings must publish a node Output into its own writable parameter bag."));
				return;
			case EArcTacticsBindingValidationResult::LiteralOnlyTarget:
				Error(Target.Endpoint.ID, FText::Format(
					NSLOCTEXT("ArcTactics", "BindingTargetLiteralOnly", "Binding target '{0}' is a memory key and accepts only a literal value."),
					FText::FromString(CompiledBinding.GetTargetPath().ToString())));
				return;
			case EArcTacticsBindingValidationResult::IncompatibleTypes:
				Error(Target.Endpoint.ID, NSLOCTEXT("ArcTactics", "IncompatibleBinding", "A binding source and target have incompatible reflected types in the actual copy direction."));
				return;
			}
			if (CompiledBinding.IsOutputBinding()
				&& Data.Nodes.IsValidIndex(Target.NodeIndex)
				&& Data.Nodes[Target.NodeIndex].Kind == EArcTacticsCompiledNodeKind::ActiveStep)
			{
				Error(Target.Endpoint.ID, NSLOCTEXT("ArcTactics", "ActiveStepOutputBinding", "Active steps have no outputs; an output binding cannot target an active step."));
				return;
			}
			if (Source.Handle.GetKind() == EArcTacticsBindingEndpointKind::Function
				&& (CompiledBinding.GetSourcePath().NumSegments() == 0
					|| CompiledBinding.GetSourcePath().GetSegment(0).GetName()
						!= FunctionNodes[Source.FunctionNodeIndex].OutputName))
			{
				Error(Target.Endpoint.ID, NSLOCTEXT("ArcTactics", "FunctionSourceNotOutput", "A function binding must source the function's output field."));
				return;
			}
			CompiledBinding.SetDataHandles(Source.Handle, Target.Handle);
			if (CompiledBinding.IsOutputBinding())
			{
				Target.OutputBindings.Add(MoveTemp(CompiledBinding));
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
				if (Endpoint.Endpoint.Kind != EArcTacticsBindingEndpointKind::Node
					&& Endpoint.Endpoint.Kind != EArcTacticsBindingEndpointKind::Function)
				{
					continue;
				}
				for (TFieldIterator<FProperty> PropertyIt(Endpoint.Struct); PropertyIt; ++PropertyIt)
				{
					const FProperty* Property = *PropertyIt;
					if (!ArcTacticsNode::IsRequiredInput(*Property))
					{
						continue;
					}
					bool bFound = false;
					for (const FArcTacticsBinding& Binding : Endpoint.ForwardBindings)
					{
						if (!Binding.GetTargetPath().IsPathEmpty()
							&& Binding.GetTargetPath().GetSegment(0).GetName() == Property->GetFName())
						{
							bFound = true;
							break;
						}
					}
					if (!bFound)
					{
						Error(Endpoint.Endpoint.ID, FText::Format(
							NSLOCTEXT("ArcTactics", "MissingRequiredBinding", "Required Input '{0}' has no binding."),
							FText::FromName(Property->GetFName())));
					}
				}
			}
		}

		void CreateForwardBatch(const FGuid EndpointID, FPropertyBindingIndex16& OutBatchIndex)
		{
			FEndpoint* Endpoint = FindMutableEndpoint(EndpointID);
			if (Endpoint)
			{
				CreateBatch(*Endpoint, Endpoint->ForwardBindings, Endpoint->FunctionsBegin, Endpoint->FunctionsEnd, OutBatchIndex);
			}
		}

		void CreateBatch(
			const FEndpoint& Target,
			TArray<FArcTacticsBinding>& PendingBindings,
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
			const FArcTacticsBindableStructDescriptor* TargetDescriptor = Data.Bindings.FindBindableStruct(Target.Handle);
			if (!TargetDescriptor)
			{
				Error(Target.Endpoint.ID, NSLOCTEXT("ArcTactics", "MissingCompiledTarget", "A compiled binding target has no data descriptor."));
				return;
			}
			struct FSortedBinding
			{
				FArcTacticsBinding Binding;
				TArray<FPropertyBindingPathIndirection> DestinationIndirections;
			};
			TArray<FSortedBinding> SortedBindings;
			SortedBindings.Reserve(PendingBindings.Num());
			for (const FArcTacticsBinding& Binding : PendingBindings)
			{
				const UStruct* DestinationStruct = Target.Struct;
				const FPropertyBindingPath* DestinationPath = &Binding.GetTargetPath();
				if (Binding.IsOutputBinding())
				{
					const FArcTacticsBindableStructDescriptor* SourceDescriptor = Data.Bindings.FindBindableStruct(Binding.GetSourceDataHandle());
					if (!SourceDescriptor)
					{
						Error(Target.Endpoint.ID, NSLOCTEXT("ArcTactics", "MissingOutputDestination", "An output binding destination has no data descriptor."));
						return;
					}
					DestinationStruct = SourceDescriptor->Struct;
					DestinationPath = &Binding.GetSourcePath();
				}
				FSortedBinding& SortedBinding = SortedBindings.AddDefaulted_GetRef();
				SortedBinding.Binding = Binding;
				FString PathError;
				if (!DestinationPath->ResolveIndirections(
					DestinationStruct, SortedBinding.DestinationIndirections, &PathError, true))
				{
					Error(Target.Endpoint.ID, FText::Format(
						NSLOCTEXT("ArcTactics", "InvalidCopyDestination", "A compiled binding destination path is invalid: {0}"),
						FText::FromString(PathError)));
					return;
				}
			}
			SortedBindings.StableSort([](const FSortedBinding& Left, const FSortedBinding& Right) -> bool
			{
				const int32 CommonPathLength = FMath::Min(
					Left.DestinationIndirections.Num(), Right.DestinationIndirections.Num());
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
			PendingBindings.Reset(SortedBindings.Num());
			for (FSortedBinding& SortedBinding : SortedBindings)
			{
				PendingBindings.Add(MoveTemp(SortedBinding.Binding));
			}
			const int32 BindingsBegin = Data.Bindings.GetNumBindings();
			const int32 BindingsEnd = BindingsBegin + PendingBindings.Num();
			const int32 BatchIndex = Data.Bindings.GetNumCopyBatches();
			if (!FPropertyBindingIndex16::IsValidIndex(BindingsBegin)
				|| !FPropertyBindingIndex16::IsValidIndex(BindingsEnd)
				|| !FPropertyBindingIndex16::IsValidIndex(BatchIndex))
			{
				Error(Target.Endpoint.ID, NSLOCTEXT("ArcTactics", "TooManyBindings", "The tactics configuration contains too many compiled bindings or batches."));
				return;
			}
			FPropertyBindingCopyInfoBatch& Batch = Data.Bindings.AddCopyBatch();
			Batch.TargetStruct = TInstancedStruct<FPropertyBindingBindableStructDescriptor>::Make(*TargetDescriptor);
			Batch.BindingsBegin = FPropertyBindingIndex16(BindingsBegin);
			for (FArcTacticsBinding& Binding : PendingBindings)
			{
				Data.Bindings.AddCompiledBinding(MoveTemp(Binding));
			}
			Batch.BindingsEnd = FPropertyBindingIndex16(BindingsEnd);
			Batch.PropertyFunctionsBegin = bHasFunctions ? FPropertyBindingIndex16(FunctionsBegin) : FPropertyBindingIndex16();
			Batch.PropertyFunctionsEnd = bHasFunctions ? FPropertyBindingIndex16(FunctionsEnd) : FPropertyBindingIndex16();
			OutBatchIndex = FPropertyBindingIndex16(BatchIndex);
		}

		int32 AddEndpoint(const FGuid ID, const FName Name, const FArcTacticsDataHandle Handle,
			const UStruct* Struct, const int32 NodeIndex = INDEX_NONE)
		{
			if (!ID.IsValid() || !Struct || !Handle.IsValid())
			{
				Error(ID, NSLOCTEXT("ArcTactics", "InvalidEndpoint", "A tactics binding endpoint is missing its stable ID, reflected type, or data handle."));
				return INDEX_NONE;
			}
			if (EndpointIndexByID.Contains(ID))
			{
				Error(ID, NSLOCTEXT("ArcTactics", "DuplicateNodeID", "Two tactics binding roots share the same stable authoring ID."));
				return INDEX_NONE;
			}
			for (const FEndpoint& ExistingEndpoint : Endpoints)
			{
				if (ExistingEndpoint.Handle == Handle)
				{
					Error(ID, NSLOCTEXT("ArcTactics", "DuplicateDataHandle", "Two tactics binding roots resolve to the same compiled data handle."));
					return INDEX_NONE;
				}
			}
			FArcTacticsBindingEndpoint BindingEndpoint;
			if (!Config.ResolveBindingEndpoint(ID, BindingEndpoint) || BindingEndpoint.Kind != Handle.GetKind())
			{
				Error(ID, NSLOCTEXT("ArcTactics", "MismatchedEndpoint", "A tactics binding root does not match its authoring endpoint contract."));
				return INDEX_NONE;
			}
			FEndpoint Endpoint;
			Endpoint.Endpoint = BindingEndpoint;
			Endpoint.Name = Name;
			Endpoint.Handle = Handle;
			Endpoint.Struct = Struct;
			Endpoint.NodeIndex = NodeIndex;
			ValidateRoles(Endpoint);
			const int32 EndpointIndex = Endpoints.Add(MoveTemp(Endpoint));
			EndpointIndexByID.Add(ID, EndpointIndex);
			return EndpointIndex;
		}

		void ValidateRoles(const FEndpoint& Endpoint)
		{
			if (Endpoint.Endpoint.Kind == EArcTacticsBindingEndpointKind::GlobalParameters
				|| Endpoint.Endpoint.Kind == EArcTacticsBindingEndpointKind::BranchParameterBag)
			{
				return;
			}
			for (TFieldIterator<FProperty> PropertyIt(Endpoint.Struct); PropertyIt; ++PropertyIt)
			{
				const FProperty* Property = *PropertyIt;
				const bool bExposed = Property->HasAnyPropertyFlags(CPF_Edit | CPF_EditConst | CPF_BlueprintVisible)
					|| Property->HasMetaData(TEXT("ArcTacticsInput"))
					|| Property->HasMetaData(TEXT("ArcTacticsOutput"))
					|| Property->HasMetaData(TEXT("ArcTacticsParameter"));
				if (bExposed && ArcTacticsNode::ResolvePropertyRole(*Property) == EArcTacticsPropertyRole::Invalid)
				{
					Error(Endpoint.Endpoint.ID, FText::Format(
						NSLOCTEXT("ArcTactics", "InvalidExposedPropertyRole", "Exposed execution property '{0}' must have exactly one tactics role."),
						FText::FromName(Property->GetFName())));
				}
			}
		}

		const FEndpoint* FindEndpoint(const FGuid ID) const
		{
			const int32* Index = EndpointIndexByID.Find(ID);
			return Index ? &Endpoints[*Index] : nullptr;
		}

		FEndpoint* FindMutableEndpoint(const FGuid ID)
		{
			const int32* Index = EndpointIndexByID.Find(ID);
			return Index ? &Endpoints[*Index] : nullptr;
		}

		void Error(const FGuid NodeID, const FText& Message)
		{
			FArcTacticsCompilerDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
			Diagnostic.NodeID = NodeID;
			Diagnostic.Message = Message;
			Diagnostic.bIsError = true;
			bFailed = true;
			++NumErrors;
		}

		const UArcTacticsConfig& Config;
		uint32 Revision = 0;
		FArcTacticsCompiledData& Data;
		TArray<FArcTacticsCompilerDiagnostic>& Diagnostics;
		TArray<FInstancedStruct> Defaults;
		TArray<FEndpoint> Endpoints;
		TMap<FGuid, int32> EndpointIndexByID;
		TArray<FFunctionNode> FunctionNodes;
		TMap<FGuid, int32> FunctionIndexByID;
		int32 NumErrors = 0;
		bool bFailed = false;
	};
}

bool FArcTacticsCompiler::Compile(
	const UArcTacticsConfig& Config,
	const uint32 Revision,
	FArcTacticsCompiledData& OutData,
	TArray<FArcTacticsCompilerDiagnostic>& OutDiagnostics)
{
	OutDiagnostics.Reset();
	ArcTacticsCompilerPrivate::FCompiler Compiler(Config, Revision, OutData, OutDiagnostics);
	return Compiler.Run();
}

#endif
