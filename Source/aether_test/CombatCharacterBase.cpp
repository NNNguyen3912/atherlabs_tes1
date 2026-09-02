#include "CombatCharacterBase.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GameplayAbilitySpec.h"

ACombatCharacterBase::ACombatCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;

	ASC = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("ASC"));
	Attributes = CreateDefaultSubobject<UCombatAttributeSet>(TEXT("Attributes"));
}

void ACombatCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	InitAbilitySystem();
}

void ACombatCharacterBase::InitAbilitySystem()
{
	if (!ASC)
	{
		return;
	}

	ASC->InitAbilityActorInfo(this, this);

	ASC->GetGameplayAttributeValueChangeDelegate(Attributes->GetHealthAttribute())
		.AddUObject(this, &ACombatCharacterBase::HandleHealthChanged);
	ASC->GetGameplayAttributeValueChangeDelegate(Attributes->GetStaminaAttribute())
		.AddUObject(this, &ACombatCharacterBase::HandleStaminaChanged);

	for (const TSubclassOf<UGameplayEffect>& EffectClass : StartupEffects)
	{
		if (!EffectClass)
		{
			continue;
		}
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(this);
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.f, Context);
		if (Spec.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (!AbilityClass)
		{
			continue;
		}
		ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
	}
}

bool ACombatCharacterBase::TryPayStamina(float Cost)
{
	if (Cost <= 0.f)
	{
		return true;
	}
	if (!ASC || !Attributes || !StaminaCostEffect)
	{
		// Chua wire GE_StaminaCost thi khong chan combo — chi bao loi ra log
		UE_LOG(LogTemp, Warning, TEXT("TryPayStamina: StaminaCostEffect chua duoc gan tren %s"), *GetName());
		return true;
	}
	if (Attributes->GetStamina() < Cost)
	{
		return false;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(StaminaCostEffect, 1.f, Context);
	if (!Spec.IsValid())
	{
		return true;
	}
	// GE cong them gia tri am = tru stamina
	Spec.Data->SetSetByCallerMagnitude(
		FGameplayTag::RequestGameplayTag(TEXT("Data.StaminaCost")), -Cost);
	ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	return true;
}

FActiveGameplayEffectHandle ACombatCharacterBase::ApplyEffectToSelf(TSubclassOf<UGameplayEffect> EffectClass, float Level)
{
	if (!ASC || !EffectClass)
	{
		return FActiveGameplayEffectHandle();
	}
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, Level, Context);
	if (!Spec.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}
	return ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

bool ACombatCharacterBase::ApplyDamageToTarget(ACombatCharacterBase* Target, float Damage)
{
	if (!Target || Target->bIsDead || bIsDead || Damage <= 0.f)
	{
		return false;
	}
	if (!ASC || !DamageEffect)
	{
		UE_LOG(LogTemp, Warning, TEXT("ApplyDamageToTarget: DamageEffect chua duoc gan tren %s"), *GetName());
		return false;
	}
	UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
	if (!TargetASC)
	{
		return false;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);
	Context.AddInstigator(this, this);
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(DamageEffect, 1.f, Context);
	if (!Spec.IsValid())
	{
		return false;
	}
	// GE cong gia tri am vao Health = tru mau
	Spec.Data->SetSetByCallerMagnitude(
		FGameplayTag::RequestGameplayTag(TEXT("Data.Damage")), -Damage);
	ASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);

	Target->PlayHitReaction();
	Target->OnMeleeHitReceived(this);
	return true;
}

void ACombatCharacterBase::PlayHitReaction()
{
	if (!bIsDead && HitReactionMontage)
	{
		PlayAnimMontage(HitReactionMontage);
	}
}

void ACombatCharacterBase::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	const float MaxHealth = GetMaxHealth();
	UpdateHealthWidgets(Data.NewValue, MaxHealth);
	OnHealthChanged.Broadcast(Data.NewValue, MaxHealth);
	if (Data.NewValue <= 0.f && !bIsDead)
	{
		bIsDead = true;
		OnDeath();
	}
}

void ACombatCharacterBase::UpdateHealthWidgets(float NewHealth, float NewMaxHealth)
{
	const float HealthPercent = NewMaxHealth > 0.f
		? FMath::Clamp(NewHealth / NewMaxHealth, 0.f, 1.f)
		: 0.f;

	TInlineComponentArray<UWidgetComponent*> WidgetComponents(this);
	GetComponents(WidgetComponents);
	for (UWidgetComponent* WidgetComponent : WidgetComponents)
	{
		if (!WidgetComponent)
		{
			continue;
		}

		if (UUserWidget* Widget = WidgetComponent->GetUserWidgetObject())
		{
			if (UProgressBar* HealthBar = Cast<UProgressBar>(Widget->GetWidgetFromName(TEXT("HPBar"))))
			{
				HealthBar->SetPercent(HealthPercent);
			}
		}
	}
}

void ACombatCharacterBase::HandleStaminaChanged(const FOnAttributeChangeData& Data)
{
	OnStaminaChanged.Broadcast(Data.NewValue, GetMaxStamina());
}

float ACombatCharacterBase::GetHealth() const
{
	return Attributes ? Attributes->GetHealth() : 0.f;
}

float ACombatCharacterBase::GetMaxHealth() const
{
	return Attributes ? Attributes->GetMaxHealth() : 0.f;
}

float ACombatCharacterBase::GetStamina() const
{
	return Attributes ? Attributes->GetStamina() : 0.f;
}

float ACombatCharacterBase::GetMaxStamina() const
{
	return Attributes ? Attributes->GetMaxStamina() : 0.f;
}

bool ACombatCharacterBase::CanDealMeleeDamage(float Cooldown) const
{
	if (Cooldown <= 0.f)
	{
		return true;
	}

	const UWorld* World = GetWorld();
	return !World || World->GetTimeSeconds() >= NextMeleeDamageTime;
}

void ACombatCharacterBase::StartMeleeDamageCooldown(float Cooldown)
{
	if (Cooldown <= 0.f)
	{
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		NextMeleeDamageTime = World->GetTimeSeconds() + Cooldown;
	}
}
