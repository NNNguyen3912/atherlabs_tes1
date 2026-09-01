#include "CombatCharacterBase.h"
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
