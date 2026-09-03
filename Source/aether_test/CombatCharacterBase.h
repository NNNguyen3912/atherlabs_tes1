#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "CombatAttributeSet.h"
#include "CombatCharacterBase.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UAnimMontage;
class UAnimSequenceBase;
class UCombatPlayerHUDWidget;
class ACombatCharacterBase;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCombatAttributeChanged, float, NewValue, float, MaxValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCombatCharacterDied, ACombatCharacterBase*, DeadCharacter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCombatComboChanged, int32, NewComboCount);

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

	/** GE Instant tru HP nan nhan, magnitude SetByCaller tag Data.Damage — gan GE_Damage trong BP con */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	TSubclassOf<UGameplayEffect> DamageEffect;

	/** Bao khi Health/Stamina doi (NewValue, MaxValue) — HUD va health bar enemy bind vao day */
	UPROPERTY(BlueprintAssignable, Category = "GAS")
	FCombatAttributeChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "GAS")
	FCombatAttributeChanged OnStaminaChanged;

	/** Phat dung mot lan ngay khi Health cham 0. Wave spawner dung event nay, khong can cho corpse Destroy. */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Death")
	FCombatCharacterDied OnCharacterDied;

	/** Combo duoc dem o phia player sau moi cu danh trung dich. */
	UPROPERTY(BlueprintAssignable, Category = "Combat|UI")
	FCombatComboChanged OnComboChanged;

	/** True sau khi Health cham 0 — moi logic combat phai ne */
	UPROPERTY(BlueprintReadOnly, Category = "GAS")
	bool bIsDead = false;

	/** Danh Target: apply DamageEffect cua MINH len ASC cua no. ANS_MeleeHitbox goi ham nay.
	 *  Tra true CHI khi damage thuc su duoc apply (2 ben con song, effect da wire) —
	 *  moi hieu ung an theo (poison, launch, hit react) phai gate bang gia tri nay. */
	UFUNCTION(BlueprintCallable, Category = "GAS")
	bool ApplyDamageToTarget(ACombatCharacterBase* Target, float Damage, bool bPlayHitReaction = true);

	/** BP override: chet (ragdoll, bao wave spawner, disable input...). Chi ban 1 lan. */
	UFUNCTION(BlueprintImplementableEvent, Category = "GAS")
	void OnDeath();

	/** Montage phan ung mac dinh. Gan tren BP_Enemy; de trong neu character khong can hit react. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Reaction")
	TObjectPtr<UAnimMontage> HitReactionMontage;

	/** Montage rieng khi bi launcher hat len. De trong neu character khong can animation nay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Reaction")
	TObjectPtr<UAnimMontage> LaunchReactionMontage;

	/** Fallback sequence used to create a short runtime montage when no authored launch montage is assigned yet. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Reaction")
	TObjectPtr<UAnimSequenceBase> LaunchReactionAnimation;

	/** Cho ANS_MeleeHitbox goi khi launcher danh trung. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Reaction")
	void ApplyCombatLaunch(float LaunchZ);

	/** BP callback de them VFX/camera shake sau khi phan ung hit mac dinh da chay. */
	UFUNCTION(BlueprintImplementableEvent, Category = "GAS")
	void OnMeleeHitReceived(ACombatCharacterBase* Attacker);

	/** Blueprint co the them VFX/sound sau khi montage launcher bat dau. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Reaction")
	void OnCombatLaunched(float LaunchZ);

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

	UFUNCTION(BlueprintPure, Category = "Combat|UI")
	int32 GetComboCount() const { return ComboCount; }

	/** Internal guard used by enemy melee notifies so a Tick-driven Blueprint cannot deal damage every montage loop. */
	bool CanDealMeleeDamage(float Cooldown) const;
	void StartMeleeDamageCooldown(float Cooldown);

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;

	void InitAbilitySystem();

	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleStaminaChanged(const FOnAttributeChangeData& Data);
	void PlayHitReaction();
	void HandleCombatDeath();
	void PlayLaunchReaction();
	void RegisterConfirmedHit();
	void ResetCombo();
	void CreatePlayerHUDIfNeeded();

	/** Keeps an optional world-space/screen-space HPBar widget in sync with GAS. */
	void UpdateHealthWidgets(float NewHealth, float NewMaxHealth);

	/** World time at which this character may deal the next cooldown-protected melee hit. */
	float NextMeleeDamageTime = 0.f;

	/** Opt-in native death hardening. Bat tren BP_Enemy de Tick AI khong the chase sau khi chet. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Death")
	bool bStopMovementOnDeath = false;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Death")
	bool bRagdollOnDeath = false;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Death")
	bool bHideHealthWidgetsOnDeath = false;

	/** 0 = Blueprint/level tu xu ly despawn. Gia tri duong tu dong destroy corpse sau so giay nay. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Death", meta = (ClampMin = "0.0"))
	float DeathDespawnDelay = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|UI")
	TSubclassOf<UCombatPlayerHUDWidget> PlayerHUDClass;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|UI", meta = (ClampMin = "0.1"))
	float ComboResetDelay = 2.f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|UI")
	int32 ComboCount = 0;

	UPROPERTY(Transient)
	TObjectPtr<UCombatPlayerHUDWidget> PlayerHUD;

	FTimerHandle ComboResetTimer;
};
