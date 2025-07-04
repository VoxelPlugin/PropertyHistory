// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DetailColumnSizeData.h"
#include "PropertyHistoryHandler.h"

class SPropertyHistory : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPropertyHistory) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);
	void SetHandler(const TSharedPtr<FPropertyHistoryHandler>& Handler);

private:
	void InitializeEntry(const TSharedPtr<FPropertyHistoryEntry>& Entry) const;

private:
	TSharedPtr<FPropertyHistoryHandler> PrivateHandler;
	TSharedPtr<STreeView<TSharedPtr<FPropertyHistoryEntry>>> ListView;
	TSharedPtr<SHeaderRow> HeaderRow;

	TArray<TSharedPtr<FPropertyHistoryEntry>> Entries;

	TSharedPtr<FDetailColumnSizeData> ColumnSizeData;
};

class SPropertyEntry : public SMultiColumnTableRow<TSharedPtr<FPropertyHistoryEntry>>
{
public:
	void Construct(
		const FArguments& Args,
		const TSharedRef<STableViewBase>& OwnerTableView,
		const TSharedPtr<FPropertyHistoryEntry>& NewEntry);

	//~ Begin SMultiColumnTableRow Interface
	virtual void ConstructChildren(ETableViewMode::Type InOwnerTableMode, const TAttribute<FMargin>& InPadding, const TSharedRef<SWidget>& InContent) override;
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;
	//~ End STableRow Interface

public:
	static FSlateColor GetRowBackgroundColor(int32 IndentLevel, bool bIsHovered);

private:
	TWeakPtr<FPropertyHistoryEntry> WeakEntry;
};

class SPropertyEntryValue : public STableRow<TSharedPtr<FPropertyHistoryEntry>>
{
public:
	void Construct(
		const FArguments& Args,
		const TSharedRef<STableViewBase>& OwnerTableView,
		const TSharedPtr<FPropertyHistoryEntry>& NewEntry);

	//~ Begin STableRow Interface
	virtual void ConstructChildren(ETableViewMode::Type InOwnerTableMode, const TAttribute<FMargin>& InPadding, const TSharedRef<SWidget>& InContent) override;
	//~ End STableRow Interface
};

class SPropertyEntryRowIndent : public SCompoundWidget
{ 
public:
	SLATE_BEGIN_ARGS(SPropertyEntryRowIndent) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<SPropertyEntryValue>& Row);

private:
	//~ Begin SCompoundWidget Interface
	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;
	//~ End SCompoundWidget Interface

	FOptionalSize GetIndentWidth() const;
	FSlateColor GetRowBackgroundColor(int32 IndentLevel) const;

private:
	TWeakPtr<SPropertyEntryValue> WeakRow;
};