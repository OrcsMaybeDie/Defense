// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/EnemyHPUI.h"

#include "Components/ProgressBar.h"

void UEnemyHPUI::UpdateHPBar(const float Cur, const float Max)
{
	if (!HPBar)
	{
		return;
	}

	const float HPPercent = Max > 0.0f ? Cur / Max : 0.0f;
	HPBar->SetPercent(FMath::Clamp(HPPercent, 0.0f, 1.0f));
}
