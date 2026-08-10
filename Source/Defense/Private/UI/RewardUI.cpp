// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/RewardUI.h"

#include "Components/TextBlock.h"

void URewardUI::SetRewardAmount(const int32 RewardAmount)
{
	if (RewardText)
	{
		RewardText->SetText(FText::AsNumber(RewardAmount));
	}
}
