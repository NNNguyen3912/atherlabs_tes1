#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "CombatAttributeSet.h"
#include "CombatCharacterBase.generated.h"

class UGameplayAbility;
class UGameplayEffect;

/**
 * Nhan vat nen co GAS: dung chung cho player va enemy.
 * ASC dat truc tiep tren Character (du an single-player).
 */
UCLASS()
class AETHER_TEST_API ACombatCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ACombatCharacterBase();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return ASC; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> ASC;

	UPROPERTY()
	TObjectPtr<UCombatAttributeSet> Attributes;

	/** Ability cap san khi spawn — khai bao trong Blueprint con */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	/** Effect chay luc khoi tao (init chi so, hoi stamina...) — khai bao trong Blueprint con */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	TArray<TSubclassOf<UGameplayEffect>> StartupEffects;

protected:
	virtual void BeginPlay() override;

	void InitAbilitySystem();
};
