// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PropertyHistoryHandler.h"

class SPropertyHistory : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPropertyHistory) {}
		SLATE_ARGUMENT(TSharedPtr<SWindow>, Window)
		SLATE_ARGUMENT(TSharedPtr<FPropertyHistoryHandler>, Handler);
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

private:
	TWeakPtr<SWindow> WeakWindow;
	TSharedPtr<FPropertyHistoryHandler> Handler;
	TSharedPtr<SListView<TSharedPtr<FPropertyHistoryEntry>>> ListView;
};

class SPropertyEntry : public SMultiColumnTableRow<TSharedPtr<FPropertyHistoryEntry>>
{
public:
	SLATE_BEGIN_ARGS(SPropertyEntry) {}
	SLATE_END_ARGS()

	void Construct(
		const FArguments& Args,
		const TSharedRef<STableViewBase>& OwnerTableView,
		const FPropertyHistoryEntry& NewEntry);

	//~ Begin STableRow Interface
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;
	//~ End STableRow Interface

private:
	FPropertyHistoryEntry Entry;
};