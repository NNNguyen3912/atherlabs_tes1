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
