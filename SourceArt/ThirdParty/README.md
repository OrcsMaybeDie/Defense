# Pistol VFX source notes

이 디렉터리는 Unreal에서 직접 재생하는 `Content`가 아니라, 외부 효과를 다시 임포트하거나 Niagara로 재구성할 때 필요한 원본 보관용이다.

## Stage 2 - Penetrating Shot

- 원본: `C:\Users\Admin\Desktop\vfxs\evfxshoot`
- 효과: `EVFX04_16_PenetratingShot.efkefc`
- 음원: `EVFX04_16_PenetratingShot.ogg`
- 런타임: 프로젝트의 `Plugins/Effekseer`
- Unreal 음원: `/Game/_Defense/VFX/Weapons/Pistol/Stage2_PenetratingShot/SFX/EVFX04_16_PenetratingShot`
- Unreal 효과: `/Game/_Defense/VFX/Weapons/Pistol/Stage2_PenetratingShot/VFX/EVFX04_16_PenetratingShot`

텍스처, Effekseer 모델, `.efkefc` 순서로 임포트해 리소스가 연결된 상태다. 나중에 텍스처나 모델 연결이 유실된 경우에만 Content Browser에서 `EffekseerEffect`를 우클릭하고 `AssignResources`를 실행한다.

사용 시 매뉴얼 요구에 따라 다음 크레딧을 표기한다.

`EVFX Shoot © Dreams Circle`

## Stage 3 - Storm Tornado

- 원본: `C:\Unity\VFX\New Unity Project\Assets\SpecialSkillsEffectsPack`
- 프리팹: `Effect_01_StormTornado.prefab`
- Unreal 원본 에셋: `/Game/_Defense/VFX/Weapons/Pistol/Stage3_StormTornado/Source`
- Unity 참조 원본: `StormTornado/UnitySource`

Unity 프리팹, ParticleSystem, 머티리얼 및 셰이더는 Unreal에서 직접 실행되지 않는다. `Source` 아래의 메시와 텍스처를 사용해 Unreal Material과 Niagara System을 별도로 재구성해야 한다. Unity 원본과 `.meta` 파일은 파라미터, 의존성, 텍스처 연결을 확인하기 위해 함께 보관한다.
