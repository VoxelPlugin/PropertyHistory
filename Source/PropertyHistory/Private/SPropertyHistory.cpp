// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "SPropertyHistory.h"
#include "PropertyHistoryUtilities.h"
#include "ISourceControlRevision.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SScaleBox.h"

void SPropertyHistory::Construct(const FArguments& Args)
{
	WeakWindow = Args._Window;
	Handler = Args._Handler;

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(FAppStyle::GetBrush("Menu.Background"))
		]
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(FAppStyle::GetOptionalBrush("Menu.Outline", nullptr))
		]
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.Padding(0.f)
			.BorderImage(FStyleDefaults::GetNoBrush())
			.ForegroundColor(FAppStyle::GetSlateColor("DefaultForeground"))
			[
				SNew(SBox)
				.MaxDesiredHeight(720)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
							.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FDockTabStyle>("Docking.Tab").CloseButtonStyle)
							.OnClicked_Lambda([this]
							{
								const TSharedPtr<SWindow> Window = WeakWindow.Pin();
								if (!ensure(Window))
								{
									return FReply::Handled();
								}

								Window->RequestDestroyWindow();
								return FReply::Handled();
							})
							.Cursor(EMouseCursor::Default)
						]
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.f)
					.Padding(2.f)
					[
						SAssignNew(ListView, SListView<TSharedPtr<FPropertyHistoryEntry>>)
						.ListViewStyle(&FAppStyle::Get().GetWidgetStyle<FTableViewStyle>("PropertyTable.InViewport.ListView"))
						.SelectionMode(ESelectionMode::Single)
						.ListItemsSource(&Handler->Entries)
						.OnMouseButtonDoubleClick_Lambda([this](const TSharedPtr<FPropertyHistoryEntry>&)
						{
							Handler->ShowFullHistory();
						})
						.HeaderRow(
							SNew(SHeaderRow)
							+ SHeaderRow::Column("CL")
							.FixedWidth(60.f)
							[
								SNew(SBox)
								.HAlign(HAlign_Center)
								[
									SNew(STextBlock)
									.Text(INVTEXT("CL"))
								]
							]
							+ SHeaderRow::Column("Value")
							.FixedWidth(300.f)
							[
								SNew(SBox)
								.HAlign(HAlign_Center)
								[
									SNew(STextBlock)
									.Text(INVTEXT("Value"))
								]
							]
							+ SHeaderRow::Column("Author")
							.FixedWidth(100.f)
							[
								SNew(SBox)
								.HAlign(HAlign_Center)
								[
									SNew(STextBlock)
									.Text(INVTEXT("Author"))
								]
							]
							+ SHeaderRow::Column("Description")
							.FillWidth(1.f)
							[
								SNew(SBox)
								.HAlign(HAlign_Center)
								[
									SNew(STextBlock)
									.Text(INVTEXT("Description"))
								]
							]
						)
						.OnGenerateRow_Lambda([](const TSharedPtr<FPropertyHistoryEntry>& Line, const TSharedRef<STableViewBase>& OwnerTable)
						{
							return SNew(SPropertyEntry, OwnerTable, *Line);
						})
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SScaleBox)
						.IgnoreInheritedScale(true)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.Visibility_Lambda([this]
						{
							return Handler->IsLoading() ? EVisibility::Visible : EVisibility::Collapsed;
						})
						[
							SNew(SCircularThrobber)
						]
					]
					+ SVerticalBox::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text_Lambda([this]
						{
							if (!Handler->GetError().IsSet())
							{
								return FText();
							}

							return FText::FromString(Handler->GetError().GetValue());
						})
						.ColorAndOpacity(FColor::Red)
						.Visibility_Lambda([this]
						{
							return Handler->GetError().IsSet() ? EVisibility::Visible : EVisibility::Collapsed;
						})
					]
				]
			]
		]
	];

	Handler->OnNewEntry.AddLambda(MakeWeakPtrLambda(this, [this]
	{
		ListView->RequestListRefresh();
	}));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SPropertyEntry::Construct(
	const FArguments& Args,
	const TSharedRef<STableViewBase>& OwnerTableView,
	const FPropertyHistoryEntry& NewEntry)
{
	Entry = NewEntry;

	FSuperRowType::Construct(FSuperRowType::FArguments(), OwnerTableView);
}

TSharedRef<SWidget> SPropertyEntry::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (ColumnName == "CL")
	{
		return
			SNew(SBox)
			.Padding(4.f, 0.f)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::FromInt(Entry.Revision->GetCheckInIdentifier())))
				.ColorAndOpacity(FSlateColor::UseForeground())
			];
	}

	if (ColumnName == "Value")
	{
		const FPropertyBagPropertyDesc* PropertyDesc = Entry.Value.FindPropertyDescByName("Value");
		if (!ensure(PropertyDesc))
		{
			return SNullWidget::NullWidget;
		}

		FString ToolTip;
		const FString String = INLINE_LAMBDA -> FString
		{
			if (PropertyDesc->IsObjectType())
			{
				const TValueOrError<UObject*, EPropertyBagResult> WrappedObject = Entry.Value.GetValueObject("Value");
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

			const TValueOrError<FString, EPropertyBagResult> Value = Entry.Value.GetValueSerializedString("Value");
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

	if (ColumnName == "Author")
	{
		return
			SNew(SBox)
			.Padding(4.f, 0.f)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Entry.Revision->GetUserName()))
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
				.Text(FText::FromString(Entry.Revision->GetDescription().TrimStartAndEnd()))
				.ToolTipText(FText::FromString(Entry.Revision->GetDescription().TrimStartAndEnd()))
				.ColorAndOpacity(FSlateColor::UseForeground())
			];
	}

	return SNullWidget::NullWidget;
}