#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatPlayerHUDWidget.generated.h"

class ACombatCharacterBase;
class UProgressBar;
class UTextBlock;

/**
 * HUD native, khong can Event Graph hay UMG property binding/Tick.
 * Layout duoc tao trong C++ de game co HUD ngay ca khi chua co WBP designer child.
 */
UCLASS()
class AETHER_TEST_API UCombatPlayerHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForCharacter(ACombatCharacterBase* InCharacter);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildLayout();
	void BindToCharacter();
	void UnbindFromCharacter();
	void RefreshAll();

	UFUNCTION()
	void HandleHealthChanged(float NewValue, float MaxValue);

	UFUNCTION()
	void HandleStaminaChanged(float NewValue, float MaxValue);

	UFUNCTION()
	void HandleComboChanged(int32 NewComboCount);

	void SetBar(UProgressBar* Bar, UTextBlock* Text, float NewValue, float MaxValue, const FText& Label);

	TWeakObjectPtr<ACombatCharacterBase> ObservedCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> StaminaBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StaminaText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ComboText;

	bool bIsBound = false;
};
