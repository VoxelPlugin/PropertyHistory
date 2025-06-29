// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "CoreMinimal.h"
#include "ToolMenus.h"
#include "PropertyHistoryHandler.h"
#include "PropertyHistoryUtilities.h"
#include "RevisionControlStyle/RevisionControlStyle.h"
#include "Editor/PropertyEditor/Private/SDetailSingleItemRow.h"

DEFINE_PRIVATE_ACCESS_FUNCTION(SDetailSingleItemRow, GetPropertyNode);

class FPropertyHistoryModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(UE::PropertyEditor::RowContextMenuName);

		Menu->AddDynamicSection(NAME_None, MakeLambdaDelegate([](UToolMenu* ToolMenu)
		{
			const FWidgetPath Path = FSlateApplication::Get().LocateWindowUnderMouse(
				FSlateApplication::Get().GetCursorPos(),
				FSlateApplication::Get().GetInteractiveTopLevelWindows(),
				true);

			TSharedPtr<SDetailSingleItemRow> Row;
			for (int32 PathIndex = Path.Widgets.Num() - 1; PathIndex >= 0; PathIndex--)
			{
				const TSharedRef<SWidget> Widget = Path.Widgets[PathIndex].Widget;
				if (Widget->GetType() != "SDetailSingleItemRow")
				{
					continue;
				}

				Row = StaticCastSharedRef<SDetailSingleItemRow>(Widget);
				break;
			}

			if (!Row)
			{
				return;
			}

			const TSharedPtr<FPropertyNode> Node = PrivateAccess::GetPropertyNode(*Row)();
			if (!Node)
			{
				return;
			}

			TArray<const FProperty*> Properties;
			{
				TSharedPtr<FPropertyNode> LocalNode = Node;
				while (LocalNode)
				{
					const FProperty* Property = LocalNode->GetProperty();
					if (!Property)
					{
						break;
					}

					Properties.Add(Property);
					LocalNode = LocalNode->GetParentNodeSharedPtr();
				}
			}

			if (Properties.Num() == 0)
			{
				return;
			}

			UObject* Object = nullptr;
			if (Node->GetSingleObject(Object) != FPropertyAccess::Success)
			{
				return;
			}

			if (!Object)
			{
				return;
			}

			UClass* OwnerClass = Cast<UClass>(Properties.Last()->GetOwnerUObject());
			if (!OwnerClass ||
				!Object->IsA(OwnerClass))
			{
				return;
			}

			const TSharedRef<FPropertyHistoryHandler> Handler = MakeShared<FPropertyHistoryHandler>(Properties);
			if (!Handler->Initialize(*Object))
			{
				return;
			}

			FToolMenuSection& Section = ToolMenu->FindOrAddSection("History");

			Section.AddMenuEntry(
				"SeeHistory",
				INVTEXT("See history"),
				INVTEXT("See this property history"),
				FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.Actions.History"),
				FUIAction(
					MakeWeakObjectPtrDelegate(Object, [Handler]
					{
						Handler->ShowHistory();
					})));
		}));
	}
};

IMPLEMENT_MODULE(FPropertyHistoryModule, PropertyHistory);