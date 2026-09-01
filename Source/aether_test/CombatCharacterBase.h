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

	/** GE Instant tru stamina, magnitude SetByCaller tag Data.StaminaCost — gan GE_StaminaCost trong BP con */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	TSubclassOf<UGameplayEffect> StaminaCostEffect;

	/**
	 * Goi truoc moi don danh: du stamina thi tru va tra true, thieu thi khong tru va tra false.
	 * BP: Branch truoc Play Montage.
	 */
	UFUNCTION(BlueprintCallable, Category = "GAS")
	bool TryPayStamina(float Cost);

	/** Apply 1 GE len chinh minh (poison DoT, buff...). Tra handle de remove som neu can. */
	UFUNCTION(BlueprintCallable, Category = "GAS")
	FActiveGameplayEffectHandle ApplyEffectToSelf(TSubclassOf<UGameplayEffect> EffectClass, float Level = 1.f);

	UFUNCTION(BlueprintPure, Category = "GAS")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "GAS")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "GAS")
	float GetStamina() const;

	UFUNCTION(BlueprintPure, Category = "GAS")
	float GetMaxStamina() const;

protected:
	virtual void BeginPlay() override;

	void InitAbilitySystem();
};
