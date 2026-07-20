// Fill out your copyright notice in the Description page of Project Settings.


#include "GameManager/DefenseGameInstance.h"

void UDefenseGameInstance::SetSelectedMapConfigData(UMapConfigData* InMapConfigData)
{
	SelectedMapConfigData = InMapConfigData;
}

UMapConfigData* UDefenseGameInstance::GetSelectedMapConfigData() const
{
	return SelectedMapConfigData ? SelectedMapConfigData : DefaultMapConfigData;
}
