// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/EnemyHPUI.h"

#include "Components/ProgressBar.h"

void UEnemyHPUI::UpdateHPBar(int32 Cur, int32 Max)
{
	float hpPer = static_cast<float>(Cur) / Max;
	HPBar->SetPercent(hpPer);
}
