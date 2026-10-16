// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVM/ArcRmlModelTypes.h"

#include "RmlUi/Core/DataModelHandle.h"
#include "RmlUi/Core/DataVariable.h"

namespace Rml
{
	class Context;
}

class FArcRmlViewChannel;

struct FArcRmlModelNode
{
	EArcRmlValueKind Kind = EArcRmlValueKind::Empty;
	Rml::Variant Scalar;
	Rml::Vector<FArcRmlModelNode> Children;
	Rml::Vector<Rml::String> MemberNames;
};

class FArcRmlModelNodeDefinition final : public Rml::VariableDefinition
{
public:
	FArcRmlModelNodeDefinition();

	virtual bool Get(void* Ptr, Rml::Variant& Variant) override;
	virtual int Size(void* Ptr) override;
	virtual Rml::DataVariable Child(void* Ptr, const Rml::DataAddressEntry& Address) override;
};

class FArcRmlModelStore
{
public:
	explicit FArcRmlModelStore(const TSharedRef<FArcRmlViewChannel, ESPMode::ThreadSafe>& InChannel);

	bool BindModel(Rml::Context& Context, const FString& ModelName, const FArcRmlValue& Snapshot);
	bool UnbindModel(Rml::Context& Context, const FString& ModelName);
	bool ApplyDelta(const FString& ModelName, const TArray<FArcRmlPathEntry>& Path, const FArcRmlValue& Value);
	bool ApplyListOps(const FString& ModelName, const TArray<FArcRmlPathEntry>& Path, const TArray<FArcRmlListOp>& Ops);

private:
	struct FModel
	{
		FString Name;
		Rml::DataModelHandle Handle;
		Rml::UnorderedMap<Rml::String, Rml::UniquePtr<FArcRmlModelNode>> Fields;
	};

	FArcRmlModelNode* FindNode(const FString& ModelName, const TArray<FArcRmlPathEntry>& Path, FModel*& OutModel);
	void HandleCall(Rml::DataModelHandle Handle, Rml::Event& Event, const Rml::VariantList& Arguments, FString ModelName);

	TSharedRef<FArcRmlViewChannel, ESPMode::ThreadSafe> Channel;
	FArcRmlModelNodeDefinition Definition;
	TMap<FString, TUniquePtr<FModel>> Models;
	bool bTransformsRegistered = false;
};
