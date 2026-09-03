#include "CombatPlayerHUDWidget.h"

#include "CombatCharacterBase.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/SlateBrush.h"

namespace
{
	constexpr float HUDWidth = 360.f;
	constexpr float BarHeight = 18.f;

	UTextBlock* AddText(UWidgetTree* WidgetTree, UVerticalBox* Parent, const FText& Text, float PaddingTop = 0.f)
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(Text);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.95f, 1.f, 1.f)));
		Label->SetShadowOffset(FVector2D(1.f, 1.f));
		if (UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Label))
		{
			Slot->SetPadding(FMargin(0.f, PaddingTop, 0.f, 2.f));
		}
		return Label;
	}

	UProgressBar* AddBar(UWidgetTree* WidgetTree, UVerticalBox* Parent, const FLinearColor& FillColor)
	{
		UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>();
		FProgressBarStyle Style = Bar->GetWidgetStyle();
		Style.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.025f, 0.035f, 0.06f, 0.94f));
		Style.FillImage.TintColor = FSlateColor(FillColor);
		Bar->SetWidgetStyle(Style);
		Bar->SetPercent(1.f);
		if (UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Bar))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}
		return Bar;
	}
}

void UCombatPlayerHUDWidget::InitializeForCharacter(ACombatCharacterBase* InCharacter)
{
	if (ObservedCharacter.Get() == InCharacter)
	{
		return;
	}

	UnbindFromCharacter();
	ObservedCharacter = InCharacter;
	BindToCharacter();
}

void UCombatPlayerHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildLayout();
	BindToCharacter();
}

void UCombatPlayerHUDWidget::NativeDestruct()
{
	UnbindFromCharacter();
	Super::NativeDestruct();
}

void UCombatPlayerHUDWidget::BuildLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
	Background->SetBrushColor(FLinearColor(0.01f, 0.015f, 0.03f, 0.72f));
	Background->SetPadding(FMargin(14.f, 10.f));
	if (UCanvasPanelSlot* BackgroundSlot = Root->AddChildToCanvas(Background))
	{
		BackgroundSlot->SetAnchors(FAnchors(0.f, 1.f));
		BackgroundSlot->SetAlignment(FVector2D(0.f, 1.f));
		BackgroundSlot->SetPosition(FVector2D(28.f, -34.f));
		BackgroundSlot->SetSize(FVector2D(HUDWidth, 142.f));
	}

	UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>();
	Background->SetContent(Panel);

	HealthText = AddText(WidgetTree, Panel, FText::FromString(TEXT("HP 0 / 0")));
	HealthBar = AddBar(WidgetTree, Panel, FLinearColor(0.92f, 0.12f, 0.16f, 1.f));
	StaminaText = AddText(WidgetTree, Panel, FText::FromString(TEXT("STAMINA 0 / 0")), 1.f);
	StaminaBar = AddBar(WidgetTree, Panel, FLinearColor(0.13f, 0.78f, 0.35f, 1.f));
	ComboText = AddText(WidgetTree, Panel, FText::GetEmpty(), 3.f);
	ComboText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.78f, 0.16f, 1.f)));
	ComboText->SetVisibility(ESlateVisibility::Collapsed);
}

void UCombatPlayerHUDWidget::BindToCharacter()
{
	if (bIsBound || !ObservedCharacter.IsValid() || !HealthBar)
	{
		return;
	}

	ACombatCharacterBase* Character = ObservedCharacter.Get();
	Character->OnHealthChanged.AddDynamic(this, &UCombatPlayerHUDWidget::HandleHealthChanged);
	Character->OnStaminaChanged.AddDynamic(this, &UCombatPlayerHUDWidget::HandleStaminaChanged);
	Character->OnComboChanged.AddDynamic(this, &UCombatPlayerHUDWidget::HandleComboChanged);
	bIsBound = true;
	RefreshAll();
}

void UCombatPlayerHUDWidget::UnbindFromCharacter()
{
	if (bIsBound && ObservedCharacter.IsValid())
	{
		ACombatCharacterBase* Character = ObservedCharacter.Get();
		Character->OnHealthChanged.RemoveDynamic(this, &UCombatPlayerHUDWidget::HandleHealthChanged);
		Character->OnStaminaChanged.RemoveDynamic(this, &UCombatPlayerHUDWidget::HandleStaminaChanged);
		Character->OnComboChanged.RemoveDynamic(this, &UCombatPlayerHUDWidget::HandleComboChanged);
	}
	bIsBound = false;
}

void UCombatPlayerHUDWidget::RefreshAll()
{
	if (ACombatCharacterBase* Character = ObservedCharacter.Get())
	{
		HandleHealthChanged(Character->GetHealth(), Character->GetMaxHealth());
		HandleStaminaChanged(Character->GetStamina(), Character->GetMaxStamina());
		HandleComboChanged(Character->GetComboCount());
	}
}

void UCombatPlayerHUDWidget::SetBar(UProgressBar* Bar, UTextBlock* Text, float NewValue, float MaxValue, const FText& Label)
{
	if (Bar)
	{
		Bar->SetPercent(MaxValue > 0.f ? FMath::Clamp(NewValue / MaxValue, 0.f, 1.f) : 0.f);
	}
	if (Text)
	{
		Text->SetText(FText::Format(NSLOCTEXT("CombatHUD", "Value", "{0}  {1} / {2}"), Label,
			FText::AsNumber(FMath::RoundToInt(NewValue)), FText::AsNumber(FMath::RoundToInt(MaxValue))));
	}
}

void UCombatPlayerHUDWidget::HandleHealthChanged(float NewValue, float MaxValue)
{
	SetBar(HealthBar, HealthText, NewValue, MaxValue, NSLOCTEXT("CombatHUD", "Health", "HP"));
}

void UCombatPlayerHUDWidget::HandleStaminaChanged(float NewValue, float MaxValue)
{
	SetBar(StaminaBar, StaminaText, NewValue, MaxValue, NSLOCTEXT("CombatHUD", "Stamina", "STAMINA"));
}

void UCombatPlayerHUDWidget::HandleComboChanged(int32 NewComboCount)
{
	if (!ComboText)
	{
		return;
	}

	ComboText->SetVisibility(NewComboCount > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	ComboText->SetText(NewComboCount > 0
		? FText::Format(NSLOCTEXT("CombatHUD", "Combo", "COMBO x{0}"), FText::AsNumber(NewComboCount))
		: FText::GetEmpty());
}
