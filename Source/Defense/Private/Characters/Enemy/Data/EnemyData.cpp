// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Enemy/Data/EnemyData.h"

UEnemyAttackData::UEnemyAttackData()
{
	EnemyType = EEnemyType::Attack;
}

UEnemyDestroyData::UEnemyDestroyData()
{
	EnemyType = EEnemyType::Destroy;
}
