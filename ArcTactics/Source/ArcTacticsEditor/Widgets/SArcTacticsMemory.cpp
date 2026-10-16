// Copyright Lukasz Baran. All Rights Reserved.

#include "Widgets/SArcTacticsMemory.h"

#include "ArcTQSBindings.h"
#include "ArcTQSPropertyFunction.h"
#include "ArcTacticsEditorViewModel.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SArcTacticsMemory"

namespace ArcTacticsMemoryPanelPrivate
{
	constexpr float KeyColumnWidth = 0.25f;
	constexpr float TypeColumnWidth = 0.15f;
	constexpr float UsersColumnWidth = 0.3f;

	TSharedRef<SWidget> MakeColumns(
		const TSharedRef<SWidget>& Key,
		const TSharedRef<SWidget>& Type,
		const TSharedRef<SWidget>& Readers,
		const TSharedRef<SWidget>& Writers)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(KeyColumnWidth)
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				Key
			]
			+ SHorizontalBox::Slot()
			.FillWidth(TypeColumnWidth)
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				Type
			]
			+ SHorizontalBox::Slot()
			.FillWidth(UsersColumnWidth)
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				Readers
			]
			+ SHorizontalBox::Slot()
			.FillWidth(UsersColumnWidth)
			[
				Writers
			];
	}

	FText GetTypeText(const TArray<FString>& Types)
	{
		if (Types.IsEmpty())
		{
			return LOCTEXT("AnyType", "Any");
		}
		TArray<FString> Names;
		for (const FString& TypePath : Types)
		{
			const UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, *TypePath);
			Names.Add(Struct ? Struct->GetName() : TypePath);
		}
		return FText::FromString(FString::Join(Names, TEXT(" / ")));
	}
}

SArcTacticsMemory::~SArcTacticsMemory()
{
	if (const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin())
	{
		Model->OnModelChanged().RemoveAll(this);
	}
}

void SArcTacticsMemory::Construct(
	const FArguments& InArgs,
	const TSharedRef<FArcTacticsEditorViewModel> InViewModel)
{
	ViewModel = InViewModel;
	OnMemoryUserClicked = InArgs._OnMemoryUserClicked;
	InViewModel->OnModelChanged().AddSP(this, &SArcTacticsMemory::HandleModelChanged);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.0f, 4.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("MemoryKeys", "Memory Keys"))
			.TextStyle(FAppStyle::Get(), TEXT("DetailsView.CategoryTextStyle"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.0f, 0.0f, 10.0f, 2.0f)
		[
			ArcTacticsMemoryPanelPrivate::MakeColumns(
				SNew(STextBlock).Text(LOCTEXT("KeyColumn", "Key")).ColorAndOpacity(FSlateColor::UseSubduedForeground()),
				SNew(STextBlock).Text(LOCTEXT("TypeColumn", "Type")).ColorAndOpacity(FSlateColor::UseSubduedForeground()),
				SNew(STextBlock).Text(LOCTEXT("ReadersColumn", "Readers")).ColorAndOpacity(FSlateColor::UseSubduedForeground()),
				SNew(STextBlock).Text(LOCTEXT("WritersColumn", "Writers")).ColorAndOpacity(FSlateColor::UseSubduedForeground()))
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(4.0f, 0.0f, 4.0f, 4.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(RowsBox, SVerticalBox)
			]
		]
	];

	RebuildRows();
}

void SArcTacticsMemory::HandleModelChanged(const EArcTacticsAuthoringChange)
{
	RebuildRows();
}

void SArcTacticsMemory::RebuildRows()
{
	if (!RowsBox.IsValid())
	{
		return;
	}
	RowsBox->ClearChildren();
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	const UArcTacticsConfig* Config = Model.IsValid() ? Model->GetConfig() : nullptr;
	if (!Config)
	{
		return;
	}

	TMap<FGameplayTag, FMemoryRow> RowsByKey;
	const auto AddUses = [&RowsByKey](
		const FConstStructView View,
		const FGuid NavigateID,
		const FText& Label,
		const bool bFocusProperty)
	{
		if (!View.IsValid())
		{
			return;
		}
		ArcTQS::ForEachMemoryKey(*View.GetScriptStruct(), View.GetMemory(),
			[&RowsByKey, NavigateID, &Label, bFocusProperty](const FProperty& Property, const FGameplayTag Key)
		{
			if (!Key.IsValid())
			{
				return;
			}
			TArray<FString> Access;
			Property.GetMetaData(TEXT("ArcMemoryAccess")).ParseIntoArray(Access, TEXT("|"));
			const bool bRead = Access.Contains(TEXT("Read"));
			const bool bWrite = Access.Contains(TEXT("Write"));
			const bool bForget = Access.Contains(TEXT("Forget"));

			FMemoryRow& Row = RowsByKey.FindOrAdd(Key);
			Row.Key = Key;
			const FString& Type = Property.GetMetaData(TEXT("ArcMemoryType"));
			if (!Type.IsEmpty())
			{
				Row.Types.AddUnique(Type);
			}
			FMemoryUser User;
			User.NavigateID = NavigateID;
			User.PropertyName = bFocusProperty ? Property.GetFName() : NAME_None;
			User.Label = Label;
			if (bRead)
			{
				Row.Readers.Add(User);
			}
			if (bWrite || bForget)
			{
				User.bForgetOnly = !bWrite;
				Row.Writers.Add(User);
			}
			Row.bWritten |= bWrite;
		});
	};
	const auto AddNode = [Config, &AddUses](const FArcTacticsNodeDefinition& Definition)
	{
		AddUses(FConstStructView(Definition.InstanceData), Definition.ID, Config->GetAuthoringName(Definition.ID), true);
	};
	const auto AddNodes = [&AddNode](const TArray<FArcTacticsNodeDefinition>& Definitions)
	{
		for (const FArcTacticsNodeDefinition& Definition : Definitions)
		{
			AddNode(Definition);
		}
	};

	AddNodes(Config->GlobalTasks);
	for (const FArcTacticsActionDefinition& Action : Config->Actions)
	{
		AddNode(Action.Action);
		AddNodes(Action.Steps);
		AddNodes(Action.ActiveSteps);
		AddNodes(Action.Considerations);
	}
	for (const FArcTacticsSelectorDefinition& Selector : Config->Selectors)
	{
		AddNode(Selector.Selector);
		AddNode(Selector.Action);
		AddNodes(Selector.Steps);
		AddNodes(Selector.ActiveSteps);
		AddNodes(Selector.Considerations);
	}
	for (const FArcTacticsBinding& Binding : Config->EditorBindings.GetBindings())
	{
		const FConstStructView FunctionView = Binding.GetPropertyFunctionNode();
		const FArcTQSPropertyFunction* Function = FunctionView.GetPtr<const FArcTQSPropertyFunction>();
		if (!Function)
		{
			continue;
		}
		AddUses(
			FunctionView,
			Function->ID,
			FText::Format(
				LOCTEXT("FunctionUserFormat", "{0}: {1}"),
				Config->GetAuthoringName(Config->ResolveFunctionRoot(Function->ID)),
				FunctionView.GetScriptStruct()->GetDisplayNameText()),
			false);
	}

	if (RowsByKey.IsEmpty())
	{
		RowsBox->AddSlot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoMemoryKeys", "No node or function of this configuration reads or writes NPC memory."))
			.AutoWrapText(true)
		];
		return;
	}

	RowsByKey.KeySort([](const FGameplayTag& A, const FGameplayTag& B)
	{
		return A.GetTagName().LexicalLess(B.GetTagName());
	});
	for (const TPair<FGameplayTag, FMemoryRow>& Pair : RowsByKey)
	{
		const FMemoryRow& Row = Pair.Value;
		const TSharedRef<SVerticalBox> WritersBox = SNew(SVerticalBox);
		if (!Row.bWritten)
		{
			WritersBox->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("WrittenElsewhere", "written elsewhere"))
				.ToolTipText(LOCTEXT("WrittenElsewhereTooltip", "No node or function of this configuration writes this key; a trait, another configuration or code must write it."))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
		}
		if (!Row.Writers.IsEmpty())
		{
			WritersBox->AddSlot()
			.AutoHeight()
			[
				MakeUsersWidget(Row.Writers, FText::GetEmpty())
			];
		}

		RowsBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 2.0f)
		[
			SNew(SBorder)
			.Padding(6.0f)
			.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
			[
				ArcTacticsMemoryPanelPrivate::MakeColumns(
					SNew(STextBlock)
					.Text(FText::FromName(Row.Key.GetTagName()))
					.AutoWrapText(true),
					SNew(STextBlock)
					.Text(ArcTacticsMemoryPanelPrivate::GetTypeText(Row.Types))
					.ToolTipText(FText::FromString(FString::Join(Row.Types, TEXT("\n"))))
					.ColorAndOpacity(Row.Types.Num() > 1 ? FSlateColor(FLinearColor::Red) : FSlateColor::UseForeground())
					.AutoWrapText(true),
					MakeUsersWidget(Row.Readers, LOCTEXT("NoReaders", "not read here")),
					WritersBox)
			]
		];
	}
}

TSharedRef<SWidget> SArcTacticsMemory::MakeUsersWidget(
	const TArray<FMemoryUser>& Users,
	const FText EmptyText) const
{
	if (Users.IsEmpty())
	{
		return SNew(STextBlock)
			.Text(EmptyText)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground());
	}
	const TSharedRef<SWrapBox> WrapBox = SNew(SWrapBox).UseAllottedSize(true);
	for (const FMemoryUser& User : Users)
	{
		WrapBox->AddSlot()
		.Padding(0.0f, 0.0f, 4.0f, 2.0f)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))
			.ToolTipText(User.bForgetOnly
				? LOCTEXT("ForgetUserTooltip", "Forgets this key. Click to select the node.")
				: LOCTEXT("UserTooltip", "Click to select the node."))
			.OnClicked(this, &SArcTacticsMemory::HandleUserClicked, User.NavigateID, User.PropertyName)
			[
				SNew(STextBlock)
				.Text(User.bForgetOnly
					? FText::Format(LOCTEXT("ForgetUserFormat", "{0} (forget)"), User.Label)
					: User.Label)
			]
		];
	}
	return WrapBox;
}

FReply SArcTacticsMemory::HandleUserClicked(const FGuid NavigateID, const FName PropertyName) const
{
	OnMemoryUserClicked.ExecuteIfBound(NavigateID, PropertyName);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
