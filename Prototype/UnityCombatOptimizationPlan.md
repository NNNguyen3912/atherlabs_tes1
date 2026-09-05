# Combat architecture notes — `Heaven in your Heart`

Đã đọc `Assets/Scripts/Player`, animation player/katana và các script combat boss. Project dùng Unity `2022.3.62f2`, Input System `1.14.2`, Cinemachine `2.10.5`, URP/VFX Graph.

## Những gì nên giữ

- Animation event đặt trực tiếp trên clip: `AttackStart`, `AttackEnd`, `ComboStart/End`, `OnStartDash/OnEndDash`.
- `TriggerSlashByAnimEvent(int)` và `Hitbox.damageInterval` đã chứng minh được workflow “chạm frame animation là bật hit/VFX”.
- `CharacterTrail` đã có pooling mesh; `CameraController` đã có lock-on và smoothing; `GameFeelManager` đã có hit-stop cơ bản.

## Nút thắt hiện tại

1. **State bị chia nhỏ:** `PlayerCombatMove`, `MeleeAttack`, `SpecialAttack`, `Ultimate`, `Dash`, `SlashEffect` cùng điều khiển `canAttack`, `m_InAttack`, `m_InCombo`, `useEnergySlash`, `m_IsActionLocked`. Một animation bị interrupt dễ để lại cờ sai.
2. **Dữ liệu đòn phụ thuộc index:** `TriggerSlashByAnimEvent(int)` ánh xạ vào thứ tự list `normalSlashes/energySlashes`; đổi thứ tự list có thể đổi VFX/hitbox mà không báo lỗi. Tên event cũng không nhất quán giữa clip player và katana.
3. **Hitbox multi-hit chưa deterministic:** `OnTriggerStay` + `List<Collider>.Contains` + coroutine cho từng target; Dive nhiều hit nên có “hit window/tick” rõ ràng và cooldown theo từng activation, không phụ thuộc frame vật lý.
4. **Tìm object lúc runtime:** `PlayerMove.IgnoreEnemyCollisionLoop` gọi `FindObjectsByType` và `FindGameObjectsWithTag` định kỳ; `ChainBindSkill` lại `FindGameObjectsWithTag` ngay trong animation event. Đây là chi phí và coupling không cần thiết.
5. **Allocation trong feedback:** `CharacterTrail` vẫn tạo `MaterialPropertyBlock` và coroutine tắt trail cho mỗi ghost; VFX/projectile ở nhiều skill dùng `Instantiate/Destroy`. Không đáng lo trong test ít enemy, nhưng sẽ thấy khi spam multi-hit.
6. **Camera/game-feel chưa có một owner:** `CameraController` tự acquire target còn skill có thể chiếm quyền; `GameFeelManager` dùng global `Time.timeScale` và bỏ qua hit-stop mới khi đang chờ.

## Kiến trúc đích cho bài UE

### 1. Một combat component, một state machine

Giữ một owner duy nhất (`CombatComponent`/`CombatAbilityComponent`) với các state tối thiểu: `Idle`, `Attack`, `Dodge`, `HitReact`, `Dead`. Input buffer, cancel window và cleanup nằm ở đây; không để từng đòn tự sửa nhiều boolean.

### 2. Data-driven move definition

Mỗi đòn là một data asset/struct, không phải một class mới:

```text
MoveId, Montage, ChainNext, StaminaCost, GateTag
HitWindows[] { Start, End, Damage, Shape, HitPolicy }
CancelWindows[] { Start, End, CancelTags }
MovementPolicy { RootMotion | ScriptDash, Distance, Duration }
Reaction { Stun, Knockback, LaunchZ }
Feedback { CameraImpulse, HitStop, VFX, SFX }
```

Thêm đòn mới = tạo data + gán montage + đặt notify, không sửa controller C++/Blueprint.

### 3. Semantic animation notifies

Dùng các notify dùng lại được: `CombatWindowBegin`, `CombatHit`, `CombatWindowEnd`, `CombatMove`, `CombatCancelOpen`, `CombatStateEnd`, `CameraImpulse`. Notify truyền `MoveId/HitId` hoặc đọc move đang active; tránh index list kiểu `TriggerSlashByAnimEvent(7)`.

- Ground combo: mỗi clip có một `CombatHit`.
- **Dive:** một montage, 4 `CombatHit` windows/ticks khi nhân vật đang xoay trên không; mỗi activation giữ `AlreadyHitTargets` để tránh double hit trong cùng tick.
- E-chain: E1/E2 chỉ damage/stun nhẹ; **E3/E4 mới có Knockback/Launch reaction** trong data.

### 4. GAS chỉ giữ phần “rule”, không giữ choreography

- Gameplay Ability kiểm tra stamina đầy, cooldown và cancel policy.
- Gameplay Effect xử lý cost/regen/poison/knockback tag.
- Montage + notify quyết định frame hit, đường di chuyển và camera impulse.

## Thứ tự làm phù hợp deadline

1. Chốt data cho 4 ground hit, E1–E4, launcher, Dive 4 tick.
2. Tạo một combat state owner và input buffer; ưu tiên dodge cancel ngay trong attack.
3. Chuẩn hóa semantic notify, sau đó mới gán VFX/SFX/camera impulse.
4. Camera: một combat director nhận target/impulse, smoothing và shake; không cho từng skill tự điều khiển camera lâu dài.
5. Profile một lượt: không `Find*` trong combat loop, không `Instantiate/Destroy` trong hit loop, tắt log per-hit; sau đó quay video và viết tài liệu.

## Tiêu chí “dễ mở rộng”

- Tạo một move mới không cần sửa code xử lý combo.
- Một hit window có thể đổi shape/damage/knockback trong Inspector.
- Dive gây đủ 4 tick lên cùng target theo cooldown rõ ràng.
- Dodge có thể nhận input trong cancel window và không cần chờ animation về idle.
- Mọi skill bị interrupt đều chạy một cleanup duy nhất để trả movement, hitbox, invulnerability và camera quyền điều khiển.

Kết luận: workflow Unity hiện tại có ý tưởng đúng, nhưng nên giữ **event trên animation** và chuyển phần còn lại sang **move data + một state owner**. Đây là lát cắt nhỏ nhất đủ tạo cảm giác ZZZ mà không biến bài test 7 ngày thành một framework mới.
