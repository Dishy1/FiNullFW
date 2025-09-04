# FireMesh (ESP32 WROOM, ESP-IDF v5.4)

## 요구사항 매핑
- **ESP-MESH 기반**: 모든 노드는 동일 펌웨어. RSSI 기반 루트(Exit) 자동 선출. 각 노드는 1분마다 `simple_state`(0:정상,1:화재,2:소화기만료) + HWID, 온도 등을 전송.
- **웹 호스팅(Core 1)**: 루트 노드만 SPIFFS(/www)에 있는 정적 파일(= FiNULLweb 빌드 결과)을 httpd로 서빙 + REST `/api/nodes`, `/api/config` 제공.
- **화재 소화 FSM(Core 0)**: 0 정상 → 1 화재 → 2 만료 → 3 안전핀제거 → 4 바닥회전 → 5 로봇팔조준 → 6 레버작동. 상태 1~6 동안 별도 알림태스크가 사이렌/LED를 계속 구동하며 비상버튼(Active-Low, GPIO36)이 눌리면 즉시 0으로 복귀.
- **센서/액추에이터**: MQ-7(ADC1 CH6 GPIO34), MLX90640 (I2C 21/20, 상대 X 추정은 stub), TB6612FNG 모터 A/B, 릴레이(12/14/27 Active-Low), 서보(19/18).

## 빌드 & 플래시
```bash
idf.py set-target esp32
idf.py build flash monitor
```

### 설정
- `sdkconfig.defaults`에 라우터 SSID/PASS 입력(또는 `idf.py menuconfig` → Component config → Mesh/Wi-Fi)
- HWID/만료일 변경은 HTTP `POST /api/config` (JSON: `{ "hwid":"NODE-1", "expiry":"2026.12.31" }`)

### SPIFFS에 웹 리소스 올리기
- FiNULLweb를 빌드하여 산출물(보통 `dist` 또는 `build`) 내용을 `main/www/`에 복사.
- `partitions.csv`에 `spiffs` 파티션이 있으며, 위 `web.c`가 `/spiffs/www`로 마운트하여 서빙.

## 주의사항
- MLX98640 표기는 MLX90640으로 가정. 실제 센서 드라이버 연동 시 `sensors.c`의 `sensors_get_mlx_relative_x()` 구현 교체.
- GPIO36(비상버튼)은 내부 풀업 미지원. 외부 풀업 회로 필요.
- TB6612FNG STBY(GPIO25)는 High 유지.
- 서보 펄스는 LEDC 50Hz/16-bit로 생성 (0..180° → 0.5..2.5ms).

## API 요약
- `GET /api/nodes` : 노드 리스트(JSON)
- `GET /api/config` : 로컬 장치 설정 조회(hwid, expiry)
- `POST /api/config` : 설정 변경(JSON)

## 메시지 포맷(메시)
```json
{ "type":"heartbeat", "hwid":"HW-112233445566", "expiry":"2026.12.31", "temp":32.5, "simple_state":1, "fsm_state":3 }
```

## 향후 확장
- WebSocket/SSE 푸시 알림
- RTC/타임싱크(만료일 정확 판정)
- MLX90640 실 프레임 기반 Fire centroid X 계산
- 인증/ACL 및 OTA
