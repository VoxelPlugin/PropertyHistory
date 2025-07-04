// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "SPropertyHistory.h"
#include "IDetailTreeNode.h"
#include "IDetailPropertyRow.h"
#include "IPropertyRowGenerator.h"
#include "ISourceControlRevision.h"
#include "PropertyHistoryUtilities.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Framework/Commands/GenericCommands.h"
#include "InstancedPropertyBagStructureDataProvider.h"

void SPropertyHistory::Construct(const FArguments& Args)
{
	ColumnSizeData = MakeShared<FDetailColumnSizeData>();

	TArray<FName> HiddenColumnsList;
	{
		TArray<FString> HiddenColumnStrings;
		GConfig->GetArray(TEXT("PropertyHistory"), TEXT("HiddenColumns"), HiddenColumnStrings, GEditorPerProjectIni);
		for (const FString& ColumnId : HiddenColumnStrings)
		{
			HiddenColumnsList.Add(FName(ColumnId));
		}
	}

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SAssignNew(ListView, STreeView<TSharedPtr<FPropertyHistoryEntry>>)
			.IsEnabled_Lambda([this]
			{
				if (!PrivateHandler ||
					PrivateHandler->IsLoading())
				{
					return false;
				}

				return true;
			})
			.SelectionMode(ESelectionMode::Single)
			.TreeItemsSource(&Entries)
			.OnGetChildren_Lambda([](const TSharedPtr<FPropertyHistoryEntry>& Item, TArray<TSharedPtr<FPropertyHistoryEntry>>& OutChildren)
			{
				OutChildren = Item->Children;
			})
			.OnMouseButtonDoubleClick_Lambda([this](const TSharedPtr<FPropertyHistoryEntry>&)
			{
				if (!PrivateHandler)
				{
					return;
				}

				PrivateHandler->ShowFullHistory();
			})
			.OnContextMenuOpening_Lambda([this]() -> TSharedRef<SWidget>
			{
				const TArray<TSharedPtr<FPropertyHistoryEntry>>& SelectedItems = ListView->GetSelectedItems();
				if (SelectedItems.Num() != 1)
				{
					return SNullWidget::NullWidget;
				}

				const TSharedPtr<FPropertyHistoryEntry>& SelectedItem = SelectedItems[0];
				if (!SelectedItem->Handle)
				{
					return SNullWidget::NullWidget;
				}

				FUIAction CopyAction;
				FUIAction PasteAction;
				SelectedItem->Handle->CreateDefaultPropertyCopyPasteActions(CopyAction, PasteAction);

				FMenuBuilder MenuBuilder(true, nullptr);

				const TSharedPtr<FUICommandInfo> CopyCommand = FGenericCommands::Get().Copy;

				MenuBuilder.BeginSection("BasicOperations");
				{
					MenuBuilder.AddMenuEntry(
						CopyCommand->GetLabel(),
						CopyCommand->GetDescription(),
						CopyCommand->GetIcon(),
						CopyAction);
				}
				MenuBuilder.EndSection();

				return MenuBuilder.MakeWidget();
			})
			.HeaderRow(
				SAssignNew(HeaderRow, SHeaderRow)
				.CanSelectGeneratedColumn(true)
				.HiddenColumnsList(HiddenColumnsList)
				.OnHiddenColumnsListChanged_Lambda([this]
				{
					TArray<FString> HiddenColumnStrings;
					for (const FName ColumnId : HeaderRow->GetHiddenColumnIds())
					{
						HiddenColumnStrings.Add(ColumnId.ToString());
					}

					GConfig->SetArray(TEXT("PropertyHistory"), TEXT("HiddenColumns"), HiddenColumnStrings, GEditorPerProjectIni);
				})

				+ SHeaderRow::Column("Expander")
				.FixedWidth(20.f)
				.ShouldGenerateWidget(true)
				.DefaultLabel(INVTEXT("Expander"))
				[
					SNew(SSpacer)
				]

				+ SHeaderRow::Column("CL")
				.VAlignHeader(VAlign_Center)
				.FillWidth(1.f)
				.DefaultLabel(INVTEXT("CL"))

				+ SHeaderRow::Column("Revision")
				.VAlignHeader(VAlign_Center)
				.FillWidth(1.5f)
				.DefaultLabel(INVTEXT("Revision"))

				+ SHeaderRow::Column("Value")
				.VAlignHeader(VAlign_Center)
				.FillWidth(5.f)
				.DefaultLabel(INVTEXT("Value"))

				+ SHeaderRow::Column("Author")
				.VAlignHeader(VAlign_Center)
				.HAlignHeader(HAlign_Center)
				.FillWidth(2.f)
				.DefaultLabel(INVTEXT("Author"))

				+ SHeaderRow::Column("Description")
				.VAlignHeader(VAlign_Center)
				.HAlignHeader(HAlign_Center)
				.FillWidth(7.f)
				.DefaultLabel(INVTEXT("Description"))

				+ SHeaderRow::Column("Date")
				.VAlignHeader(VAlign_Center)
				.HAlignHeader(HAlign_Center)
				.FillWidth(2.f)
				.DefaultLabel(INVTEXT("Date"))
			)
			.OnGenerateRow_Lambda([](const TSharedPtr<FPropertyHistoryEntry>& Line, const TSharedRef<STableViewBase>& OwnerTable) -> TSharedRef<STableRow<TSharedPtr<FPropertyHistoryEntry>>>
			{
				if (Line->Revision)
				{
					return SNew(SPropertyEntry, OwnerTable, Line);
				}

				return SNew(SPropertyEntryValue, OwnerTable, Line);
			})
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SScaleBox)
			.IgnoreInheritedScale(true)
			.Visibility_Lambda([this]
			{
				return
					PrivateHandler &&
					PrivateHandler->IsLoading()
					? EVisibility::Visible
					: EVisibility::Collapsed;
			})
			[
				SNew(SThrobber)
			]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		[
			SNew(STextBlock)
			.Text_Lambda([this]
			{
				if (!PrivateHandler ||
					!PrivateHandler->GetError().IsSet())
				{
					return FText();
				}

				return FText::FromString(PrivateHandler->GetError().GetValue());
			})
			.ColorAndOpacity(FStyleColors::Error)
			.Visibility_Lambda([this]
			{
				return
					PrivateHandler &&
					PrivateHandler->GetError().IsSet()
					? EVisibility::Visible
					: EVisibility::Collapsed;
			})
		]
	];
}

void SPropertyHistory::SetHandler(const TSharedPtr<FPropertyHistoryHandler>& Handler)
{
	PrivateHandler = Handler;

	Entries.Empty();
	ListView->RequestTreeRefresh();

	Handler->OnNewEntry.AddLambda(MakeWeakPtrLambda(this, [this]
	{
		Entries = PrivateHandler->Entries;

		if (Entries.Num() > 0)
		{
			if (const TSharedPtr<FPropertyHistoryEntry> Entry = Entries.Last())
			{
				if (!Entry->Node)
				{
					InitializeEntry(Entry);
				}
			}
		}

		ListView->RequestTreeRefresh();
	}));
}

void SPropertyHistory::InitializeEntry(const TSharedPtr<FPropertyHistoryEntry>& Entry) const
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	const TSharedPtr<FInstancePropertyBagStructureDataProvider> StructProvider = MakeShared<FInstancePropertyBagStructureDataProvider>(Entry->Value);

	const TSharedRef<IPropertyRowGenerator> PropertyRowGenerator = PropertyModule.CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());
	PropertyRowGenerator->SetStructure(StructProvider);

	Entry->PropertyRowGenerator = PropertyRowGenerator;
	if (!ensure(PropertyRowGenerator->GetRootTreeNodes().Num() == 1))
	{
		return;
	}

	Entry->Node = PropertyRowGenerator->GetRootTreeNodes()[0];

	{
		TArray<TSharedRef<IDetailTreeNode>> Children;
		Entry->Node->GetChildren(Children);
		if (!ensure(Children.Num() == 1))
		{
			return;
		}

		Entry->Node = Children[0];
	}

	const auto FillChildren = [&](const TSharedPtr<FPropertyHistoryEntry>& CurrentEntry, auto& Lambda) -> void
	{
		CurrentEntry->ColumnSizeData = ColumnSizeData;
		CurrentEntry->Handle = CurrentEntry->Node->CreatePropertyHandle();

		TArray<TSharedRef<IDetailTreeNode>> Children;
		CurrentEntry->Node->GetChildren(Children);

		for (const TSharedRef<IDetailTreeNode>& ChildNode : Children)
		{
			TSharedPtr<FPropertyHistoryEntry> ChildEntry = MakeShared<FPropertyHistoryEntry>();
			ChildEntry->Node = ChildNode;
			Lambda(ChildEntry, Lambda);

			CurrentEntry->Children.Add(ChildEntry);
		}
	};
	FillChildren(Entry, FillChildren);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SPropertyEntry::Construct(
	const FArguments& Args,
	const TSharedRef<STableViewBase>& OwnerTableView,
	const TSharedPtr<FPropertyHistoryEntry>& NewEntry)
{
	WeakEntry = NewEntry;
	FSuperRowType::Construct(Args, OwnerTableView);
}

void SPropertyEntry::ConstructChildren(const ETableViewMode::Type InOwnerTableMode, const TAttribute<FMargin>& InPadding, const TSharedRef<SWidget>& InContent)
{
	FSuperRowType::ConstructChildren(InOwnerTableMode, InPadding, InContent);

	ChildSlot
	.Padding(InPadding)
	[
		SNew( SBorder )
		.BorderImage(FAppStyle::Get().GetBrush("DetailsView.GridLine"))
		.Padding(FMargin(0.0f,0.0f,0.0f,1.0f))
		.Clipping(EWidgetClipping::ClipToBounds)
		[
			SNew(SBox)
			.MinDesiredHeight(26.f)
			[
				SNew( SHorizontalBox )
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SNew( SBorder )
					.BorderImage(FAppStyle::Get().GetBrush("DetailsView.Highlight"))
					.Padding_Lambda([this]
					{
						return
							IsHighlighted()
							? FMargin(1)
							: FMargin(0);
					})
					[
						SNew( SBorder )
						.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryMiddle"))
						.BorderBackgroundColor_Lambda([this]
						{
							if (IsHighlighted())
							{
								return FAppStyle::Get().GetSlateColor("Colors.Hover");
							}

							return GetRowBackgroundColor(GetIndentLevel(), this->IsHovered());
						})
						.Padding(0.0f)
						[
							InContent
						]
					]
				]
			]
		]
	];
}

DEFINE_PRIVATE_ACCESS_FUNCTION(SWidget, ComputeDesiredSize)

TSharedRef<SWidget> SPropertyEntry::GenerateWidgetForColumn(const FName& ColumnName)
{
	const TSharedPtr<FPropertyHistoryEntry> Entry = WeakEntry.Pin();
	if (!Entry)
	{
		return SNullWidget::NullWidget;
	}

	if (ColumnName == "Expander")
	{
		return
			SNew(SExpanderArrow, SharedThis(this))
			.IndentAmount(16)
			.ShouldDrawWires(false);
	}

	const TSharedRef<SWidget> ContentWidget = INLINE_LAMBDA -> TSharedRef<SWidget>
	{
		if (ColumnName == "CL")
		{
			return
				SNew(SBox)
				.Padding(4.f, 0.f)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::FromInt(Entry->Revision->GetCheckInIdentifier())))
					.ColorAndOpacity(FSlateColor::UseForeground())
				];
		}

		if (ColumnName == "Revision")
		{
			return
				SNew(SBox)
				.Padding(4.f, 0.f)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Entry->Revision->GetRevision()))
					.ColorAndOpacity(FSlateColor::UseForeground())
				];
		}

		if (ColumnName == "Value")
		{
			const FPropertyBagPropertyDesc* PropertyDesc = Entry->Value.FindPropertyDescByName("Value");
			if (!ensure(PropertyDesc))
			{
				return SNullWidget::NullWidget;
			}

			if (!Entry->Node)
			{
				return SNullWidget::NullWidget;
			}

			if (const TSharedPtr<IDetailPropertyRow> Row = Entry->Node->GetRow())
			{
				Row->ShowPropertyButtons(false);
			}

			const FNodeWidgets NodeWidgets = Entry->Node->CreateNodeWidgets();
			if (!NodeWidgets.ValueWidget)
			{
				return SNullWidget::NullWidget;
			}

			const FVector2D ValueDesiredSize = PrivateAccess::ComputeDesiredSize(*NodeWidgets.ValueWidget)(1.f);
			if (FMath::IsNearlyZero(ValueDesiredSize.X))
			{
				FString ToolTip;
				const FString String = INLINE_LAMBDA -> FString
				{
					if (PropertyDesc->IsObjectType())
					{
						const TValueOrError<UObject*, EPropertyBagResult> WrappedObject = Entry->Value.GetValueObject("Value");
						if (!WrappedObject.IsValid())
						{
							return "<Error>";
						}

						const UObject* Object = WrappedObject.GetValue();
						if (!Object)
						{
							return "null";
						}

						ToolTip = Object->GetPathName();

						return Object->GetName();
					}

					const TValueOrError<FString, EPropertyBagResult> Value = Entry->Value.GetValueSerializedString("Value");
					if (!Value.IsValid())
					{
						return "<Error>";
					}

					return Value.GetValue();
				};

				if (ToolTip.IsEmpty())
				{
					ToolTip = String;
				}

				return
					SNew(SBox)
					.Padding(4.f, 0.f)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(String))
						.ToolTipText(FText::FromString(ToolTip))
						.ColorAndOpacity(FSlateColor::UseForeground())
					];
			}

			NodeWidgets.ValueWidget->SetEnabled(false);

			return
				SNew(SBox)
				.Clipping(EWidgetClipping::OnDemand)
				.HAlign(NodeWidgets.ValueWidgetLayoutData.HorizontalAlignment)
				.VAlign(NodeWidgets.ValueWidgetLayoutData.VerticalAlignment)
				.MinDesiredWidth(NodeWidgets.ValueWidgetLayoutData.MinWidth)
				.MaxDesiredWidth(NodeWidgets.ValueWidgetLayoutData.MaxWidth)
				.Padding(4.f, 0.f)
				[
					NodeWidgets.ValueWidget.ToSharedRef()
				];
		}

		if (ColumnName == "Author")
		{
			return
				SNew(SBox)
				.Padding(4.f, 0.f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Entry->Revision->GetUserName()))
					.ColorAndOpacity(FSlateColor::UseForeground())
				];
		}

		if (ColumnName == "Description")
		{
			return
				SNew(SBox)
				.Padding(4.f, 0.f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Entry->Revision->GetDescription().TrimStartAndEnd()))
					.ToolTipText(FText::FromString(Entry->Revision->GetDescription().TrimStartAndEnd()))
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					.ColorAndOpacity(FSlateColor::UseForeground())
				];
		}

		if (ColumnName == "Date")
		{
			return
				SNew(SBox)
				.Padding(4.f, 0.f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Entry->Revision->GetDate().ToString()))
					.ToolTipText(FText::FromString(Entry->Revision->GetDate().ToString()))
					.ColorAndOpacity(FSlateColor::UseForeground())
				];
		}

		return SNullWidget::NullWidget;
	};

	return
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.f)
		.MaxWidth(1.f)
		[
			SNew(SSeparator)
			.Thickness(1.f)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		[
			ContentWidget
		];
}

FSlateColor SPropertyEntry::GetRowBackgroundColor(const int32 IndentLevel, const bool bIsHovered)
{
	int32 ColorIndex = 0;
	int32 Increment = 1;

	for (int i = 0; i < IndentLevel; ++i)
	{
		ColorIndex += Increment;

		if (ColorIndex == 0 || ColorIndex == 3)
		{
			Increment = -Increment;
		}
	}

	static const uint8 ColorOffsets[] =
	{
		0, 4, (4 + 2), (6 + 4), (10 + 6)
	};

	const FSlateColor BaseSlateColor = bIsHovered ? 
		FAppStyle::Get().GetSlateColor("Colors.Header") : 
		FAppStyle::Get().GetSlateColor("Colors.Panel");

	const FColor BaseColor = BaseSlateColor.GetSpecifiedColor().ToFColor(true);

	const FColor ColorWithOffset(
		BaseColor.R + ColorOffsets[ColorIndex], 
		BaseColor.G + ColorOffsets[ColorIndex], 
		BaseColor.B + ColorOffsets[ColorIndex]);

	return FSlateColor(FLinearColor::FromSRGBColor(ColorWithOffset));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SPropertyEntryValue::Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView, const TSharedPtr<FPropertyHistoryEntry>& NewEntry)
{
	SetOwnerTableView(OwnerTableView);

	const FNodeWidgets NodeWidgets = NewEntry->Node->CreateNodeWidgets();

	TSharedPtr<SWidget> ContentWidget = SNullWidget::NullWidget;

	const TSharedRef<SHorizontalBox> NameColumnBox =
		SNew(SHorizontalBox)
		.Clipping(EWidgetClipping::OnDemand);

	NameColumnBox->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Fill)
		.Padding(0.0f)
		.AutoWidth()
		[
			SNew(SPropertyEntryRowIndent, SharedThis(this))
		];

	NameColumnBox->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(2.0f,0.0f,0.0f,0.0f)
		.AutoWidth()
		[
			SNew(SExpanderArrow, SharedThis(this))
			.BaseIndentLevel(GetIndentLevel())
			.IndentAmount(0.f)
		];

	const auto GetMaxWidth = [](const FNodeWidgetLayoutData& Data) -> FOptionalSize
	{
		const float MaxWidth = Data.MaxWidth.Get(0);
		if (Data.MaxWidth.Get(0) > Data.MinWidth.Get(0))
		{
			return MaxWidth;
		}

		return -1.f;
	};

	if (NodeWidgets.WholeRowWidget)
	{
		NameColumnBox->AddSlot()
		.HAlign(NodeWidgets.WholeRowWidgetLayoutData.HorizontalAlignment)
		.VAlign(NodeWidgets.WholeRowWidgetLayoutData.VerticalAlignment)
		.Padding(2.f, 0.f, 2.f, 0.f)
		[
			SNew(SBox)
			.MinDesiredWidth(NodeWidgets.WholeRowWidgetLayoutData.MinWidth)
			.MaxDesiredWidth(GetMaxWidth(NodeWidgets.WholeRowWidgetLayoutData))
			[
				NodeWidgets.WholeRowWidget.ToSharedRef()
			]
		];

		ContentWidget = NameColumnBox;
	}
	else
	{
		NodeWidgets.ValueWidget->SetEnabled(false);

		NameColumnBox->AddSlot()
		.HAlign(NodeWidgets.NameWidgetLayoutData.HorizontalAlignment)
		.VAlign(NodeWidgets.NameWidgetLayoutData.VerticalAlignment)
		.Padding(2.f, 0.f, 2.f, 0.f)
		[
			SNew(SBox)
			.MinDesiredWidth(NodeWidgets.NameWidgetLayoutData.MinWidth)
			.MaxDesiredWidth(GetMaxWidth(NodeWidgets.NameWidgetLayoutData))
			[
				NodeWidgets.NameWidget.ToSharedRef()
			]
		];

		ContentWidget =
			SNew(SSplitter)
			.Style(FAppStyle::Get(), "DetailsView.Splitter")
			.PhysicalSplitterHandleSize(1.0f)
			.HitDetectionSplitterHandleSize(5.0f)
			.HighlightedHandleIndex(NewEntry->ColumnSizeData->GetHoveredSplitterIndex())
			.OnHandleHovered(NewEntry->ColumnSizeData->GetOnSplitterHandleHovered())
			+ SSplitter::Slot()
			.Value(NewEntry->ColumnSizeData->GetNameColumnWidth())
			.OnSlotResized(NewEntry->ColumnSizeData->GetOnNameColumnResized())
			[
				NameColumnBox
			]
			+ SSplitter::Slot()
			.Value(NewEntry->ColumnSizeData->GetValueColumnWidth())
			.OnSlotResized(NewEntry->ColumnSizeData->GetOnValueColumnResized())
			[
				SNew(SBox)
				.Clipping(EWidgetClipping::OnDemand)
				.HAlign(NodeWidgets.ValueWidgetLayoutData.HorizontalAlignment)
				.VAlign(NodeWidgets.ValueWidgetLayoutData.VerticalAlignment)
				.MinDesiredWidth(NodeWidgets.ValueWidgetLayoutData.MinWidth)
				.MaxDesiredWidth(GetMaxWidth(NodeWidgets.ValueWidgetLayoutData))
				.Padding(12.f, 0.f, 2.f, 0.f)
				[
					NodeWidgets.ValueWidget.ToSharedRef()
				]
			];
	}

	FArguments NewArgs = Args;
	STableRow<TSharedPtr<FPropertyHistoryEntry>>::Construct(
		NewArgs
		.ShowSelection(false)
		.Content()
		[
			SNew( SBorder )
			.BorderImage(FAppStyle::Get().GetBrush("DetailsView.GridLine"))
			.Padding(FMargin(0.0f,0.0f,0.0f,1.0f))
			.Clipping(EWidgetClipping::ClipToBounds)
			[
				SNew(SBox)
				.MinDesiredHeight(26.f)
				[
					SNew( SHorizontalBox )
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					[
						SNew( SBorder )
						.BorderImage(FAppStyle::Get().GetBrush("DetailsView.Highlight"))
						.Padding_Lambda([this]
						{
							return
								IsHighlighted()
								? FMargin(1)
								: FMargin(0);
						})
						[
							SNew( SBorder )
							.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryMiddle"))
							.BorderBackgroundColor_Lambda([this]
							{
								if (IsHighlighted())
								{
									return FAppStyle::Get().GetSlateColor("Colors.Hover");
								}

								return SPropertyEntry::GetRowBackgroundColor(GetIndentLevel(), this->IsHovered());
							})
							.Padding(0.0f)
							[
								ContentWidget.ToSharedRef()
							]
						]
					]
				]
			]
		]
		, OwnerTableView);
}

void SPropertyEntryValue::ConstructChildren(ETableViewMode::Type InOwnerTableMode, const TAttribute<FMargin>& InPadding, const TSharedRef<SWidget>& InContent)
{
	ChildSlot
	.Padding(InPadding)
	[
		InContent
	];

	InnerContentSlot = &ChildSlot.AsSlot();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SPropertyEntryRowIndent::Construct(const FArguments& InArgs, const TSharedRef<SPropertyEntryValue>& Row)
{
	WeakRow = Row;

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(this, &SPropertyEntryRowIndent::GetIndentWidth)
	];
}

int32 SPropertyEntryRowIndent::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const TSharedPtr<SPropertyEntryValue> Row = WeakRow.Pin();
	if (!Row.IsValid())
	{
		return LayerId;
	}

	const FSlateBrush* BackgroundBrush = FAppStyle::Get().GetBrush("DetailsView.CategoryMiddle");
	const FSlateBrush* DropShadowBrush = FAppStyle::Get().GetBrush("DetailsView.ArrayDropShadow");

	const int32 IndentLevel = Row->GetIndentLevel();
	for (int32 Index = 0; Index < IndentLevel; Index++)
	{
		FSlateColor BackgroundColor = GetRowBackgroundColor(Index);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(FVector2f(16.f, AllottedGeometry.GetLocalSize().Y), FSlateLayoutTransform(FVector2f(16.f * Index, 0.f))),
			BackgroundBrush,
			ESlateDrawEffect::None,
			BackgroundColor.GetColor(InWidgetStyle)
		);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(FVector2f(16.f, AllottedGeometry.GetLocalSize().Y), FSlateLayoutTransform(FVector2f(16.f * Index, 0.f))),
			DropShadowBrush
		);
	}
		
	return LayerId + 1;
}

FOptionalSize SPropertyEntryRowIndent::GetIndentWidth() const
{
	int32 IndentLevel = 0;
	if (const TSharedPtr<SPropertyEntryValue> Row = WeakRow.Pin())
	{
		IndentLevel = Row->GetIndentLevel();
	}

	return IndentLevel * 16.0f;
}

FSlateColor SPropertyEntryRowIndent::GetRowBackgroundColor(const int32 IndentLevel) const
{
	const TSharedPtr<SPropertyEntryValue> Row = WeakRow.Pin();
	return SPropertyEntry::GetRowBackgroundColor(
		IndentLevel,
		Row && Row->IsHovered());
}