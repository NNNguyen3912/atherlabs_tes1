#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EnemyBlueprintAutomationLibrary.generated.h"

/** Editor-only, idempotent repair for the player-reference guard in BP_Enemy's AI Tick graph. */
UCLASS()
class AETHER_TESTEDITOR_API UEnemyBlueprintAutomationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Inserts: bAttacking False -> Branch(IsValid(GetPlayerCharacter)) -> distance/MoveTo branch.
	 * The false path intentionally does nothing, preventing Accessed None in SIE and menu worlds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Editor")
	static FString ApplyEnemyP0PlayerValidGuard();

	/** Starts a normal in-process PIE session, including local PlayerController and screen HUD. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Editor")
	static FString StartNormalPIE();

	UFUNCTION(BlueprintCallable, Category = "Combat|Editor")
	static FString EndPlaySession();
};
