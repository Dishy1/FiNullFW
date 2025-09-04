## FiNULL Fire Extinguisher FW
- **ESP-MESH 기반**: RSSI 기반 루트(Exit) 자동 선출기능과 노드가 1분마다 FSM 정보 + HWID + 온도를 공유합니다.
- **JSON 데이어 송신 (Core 1)**: Root Node에서 다른 Mesh의 데이터를 수신한 뒤 이를 REST `/api/nodes`, `/api/config`를 통해 제공합니다.
- **화재 소화 FSM(Core 0)**: 0 정상 → 1 화재 → 2 만료 → 3 안전핀제거 → 4 바닥회전 → 5 로봇팔조준 → 6 레버작동. 상태 1~6은 비상 상황이므로 사이렌 / LED / 피난유도등이 동시 작동하며 주변 사람들에게 알립니다.
- **센서/액추에이터**: MQ-7(ADC1 CH6 GPIO34), MLX90640 (I2C 21/20, 상대 X 추정은 stub), TB6612FNG 모터 A/B, 릴레이(12/14/27 Active-Low), 서보(19/18) - 원래 DC 기반 Actuator였으나 I2C MLX와 혼선으로 인해 일단 Servo로 변경하였음.
- HWID/만료일 변경은 HTTP `POST /api/config`에서 가능합니다. (JSON: `{ "hwid":"NODE-1", "expiry":"2026.12.31" }`)

## API 요약
- `GET /api/nodes` : 각 노드 리스트
- `GET /api/config` : 로컬 장치 설정 조회(hwid, expiry)
- `POST /api/config` : 설정 변경(JSON)
- 메세지 포맷 :
```json
{ "type":"heartbeat", "hwid":"HW-112233445566", "expiry":"2026.12.31", "temp":32.5, "simple_state":1, "fsm_state":3 }
```
