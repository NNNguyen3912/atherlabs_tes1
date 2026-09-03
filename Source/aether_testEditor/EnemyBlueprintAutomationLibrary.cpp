#include "EnemyBlueprintAutomationLibrary.h"

#include "Editor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphSchema_K2_Actions.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_IfThenElse.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "LevelEditor.h"
#include "IAssetViewport.h"
#include "PlayInEditorDataTypes.h"
#include "ScopedTransaction.h"

FString UEnemyBlueprintAutomationLibrary::ApplyEnemyP0PlayerValidGuard()
{
	if (GEditor && GEditor->PlayWorld)
	{
		return TEXT("Refused: stop PIE/SIE before modifying BP_Enemy.");
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Game/Combat/GAS/BP_Enemy.BP_Enemy"));
	UEdGraph* Graph = Blueprint && !Blueprint->UbergraphPages.IsEmpty() ? Blueprint->UbergraphPages[0] : nullptr;
	if (!Graph)
	{
		return TEXT("Refused: BP_Enemy EventGraph was not found.");
	}

	const UFunction* GetPlayerCharacterFunction = UGameplayStatics::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UGameplayStatics, GetPlayerCharacter));
	const UFunction* IsValidFunction = UKismetSystemLibrary::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, IsValid));
	if (!GetPlayerCharacterFunction || !IsValidFunction)
	{
		return TEXT("Refused: Engine utility functions required for P0 were not found.");
	}

	UK2Node_CallFunction* GetPlayerCharacterNode = nullptr;
	TArray<UK2Node_CallFunction*> IsValidNodes;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);
		if (!CallNode)
		{
			continue;
		}

		if (CallNode->GetTargetFunction() == GetPlayerCharacterFunction)
		{
			GetPlayerCharacterNode = CallNode;
		}

		if (CallNode->GetTargetFunction() == IsValidFunction)
		{
			IsValidNodes.Add(CallNode);
		}
	}

	if (!GetPlayerCharacterNode)
	{
		return TEXT("Refused: GetPlayerCharacter(0) node was not found in BP_Enemy EventGraph.");
	}
	for (UK2Node_CallFunction* IsValidNode : IsValidNodes)
	{
		UEdGraphPin* ObjectPin = IsValidNode ? IsValidNode->FindPin(TEXT("Object")) : nullptr;
		UEdGraphPin* ReturnPin = IsValidNode ? IsValidNode->GetReturnValuePin() : nullptr;
		if (!ObjectPin || !ReturnPin)
		{
			continue;
		}
		const bool bReadsPlayer = ObjectPin->LinkedTo.ContainsByPredicate([GetPlayerCharacterNode](const UEdGraphPin* Pin)
		{
			return Pin && Pin->GetOwningNode() == GetPlayerCharacterNode;
		});
		const bool bFeedsBranch = ReturnPin->LinkedTo.ContainsByPredicate([](const UEdGraphPin* Pin)
		{
			return Pin && Cast<UK2Node_IfThenElse>(Pin->GetOwningNode()) != nullptr;
		});
		if (bReadsPlayer && bFeedsBranch)
		{
			return TEXT("Already applied: BP_Enemy already has a GetPlayerCharacter validity guard.");
		}
	}

	// This is the known range/attack branch in BP_Enemy. The GUID is used only after validating the node type and input wire.
	const FGuid RangeBranchGuid(0x777AC0A3, 0x456F9B45, 0xA0547B87, 0xAEF7CF81);
	UK2Node_IfThenElse* RangeBranch = nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && Node->NodeGuid == RangeBranchGuid)
		{
			RangeBranch = Cast<UK2Node_IfThenElse>(Node);
			break;
		}
	}
	if (!RangeBranch)
	{
		return TEXT("Refused: BP_Enemy's distance/attack branch changed since audit; no graph wire was modified.");
	}

	UEdGraphPin* RangeExecInput = RangeBranch->GetExecPin();
	if (!RangeExecInput || RangeExecInput->LinkedTo.Num() != 1)
	{
		return TEXT("Refused: distance/attack branch did not have exactly one input execution wire.");
	}
	UEdGraphPin* PreviousExec = RangeExecInput->LinkedTo[0];
	UEdGraphPin* PlayerReturn = GetPlayerCharacterNode->GetReturnValuePin();
	if (!PreviousExec || !PlayerReturn)
	{
		return TEXT("Refused: required BP_Enemy pins were unavailable.");
	}

	const FScopedTransaction Transaction(NSLOCTEXT("Combat", "AddEnemyP0PlayerGuard", "Add Enemy Player Validity Guard"));
	Blueprint->Modify();
	Graph->Modify();
	RangeBranch->Modify();

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	UK2Node_CallFunction* IsValidNode = FEdGraphSchemaAction_K2NewNode::SpawnNode<UK2Node_CallFunction>(
		Graph,
		FVector2D(RangeBranch->NodePosX - 180, RangeBranch->NodePosY - 165),
		EK2NewNodeFlags::None,
		[IsValidFunction](UK2Node_CallFunction* NewNode)
		{
			NewNode->SetFromFunction(IsValidFunction);
		});
	UK2Node_IfThenElse* GuardBranch = FEdGraphSchemaAction_K2NewNode::SpawnNode<UK2Node_IfThenElse>(
		Graph,
		FVector2D(RangeBranch->NodePosX - 20, RangeBranch->NodePosY - 165), EK2NewNodeFlags::None);

	if (!IsValidNode || !GuardBranch)
	{
		return TEXT("Refused: could not create the P0 guard nodes.");
	}

	GuardBranch->NodeComment = TEXT("CODEX_P0_PLAYER_VALID_GUARD");
	UEdGraphPin* IsValidObject = IsValidNode->FindPin(TEXT("Object"));
	UEdGraphPin* IsValidReturn = IsValidNode->GetReturnValuePin();
	if (!IsValidObject || !IsValidReturn || !Schema->TryCreateConnection(PlayerReturn, IsValidObject))
	{
		return TEXT("Refused: P0 nodes were created but the player validity data link could not be completed; undo the transaction and inspect BP_Enemy.");
	}
	Schema->BreakSinglePinLink(PreviousExec, RangeExecInput);
	if (!Schema->TryCreateConnection(PreviousExec, GuardBranch->GetExecPin()) ||
		!Schema->TryCreateConnection(IsValidReturn, GuardBranch->GetConditionPin()) ||
		!Schema->TryCreateConnection(GuardBranch->GetThenPin(), RangeExecInput))
	{
		return TEXT("Refused: P0 nodes were created but their execution links could not be completed; undo the transaction and inspect BP_Enemy.");
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	return TEXT("Applied: added Branch(IsValid(GetPlayerCharacter(0))) before BP_Enemy's distance/attack branch.");
}

FString UEnemyBlueprintAutomationLibrary::StartNormalPIE()
{
	if (!GEditor)
	{
		return TEXT("Refused: UnrealEd is unavailable.");
	}
	if (GEditor->PlayWorld)
	{
		return TEXT("Already running: PIE/SIE is active.");
	}

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	FRequestPlaySessionParams SessionParams;
	SessionParams.DestinationSlateViewport = LevelEditorModule.GetFirstActiveViewport();
	GEditor->RequestPlaySession(SessionParams);
	return TEXT("Requested: normal PIE session.");
}

FString UEnemyBlueprintAutomationLibrary::EndPlaySession()
{
	if (!GEditor || !GEditor->PlayWorld)
	{
		return TEXT("No PIE/SIE session is active.");
	}
	GEditor->RequestEndPlayMap();
	return TEXT("Requested: end PIE/SIE session.");
}
