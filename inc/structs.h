#pragma pack(push, 1)
typedef struct {
  uint8_t version;
  uint8_t id;
  uint8_t length;
  uint8_t armed;
  uint16_t throttlePercent;  // 0 to 1000
  uint16_t crc;
}STR_CTRL2HUB_MSG;

typedef struct {
  uint8_t version;
  uint8_t id;
  uint8_t length;
  uint8_t armed;
  uint32_t voltage;
  uint32_t totalMah;
  uint32_t totalCurrent;
  uint16_t avgRpm;
  uint8_t avgCapTemp;
  uint8_t avgFetTemp;
  int16_t baroTemp;
  uint32_t baroPressure;
  uint16_t crc;
}STR_HUB2CTRL_MSG;

typedef struct {
  uint8_t version_major;
  uint8_t version_minor;
  uint16_t armed_time;
  uint8_t screen_rotation;
  uint16_t crc;
}STR_DEVICE_DATA;
#pragma pack(pop)
// TODO: Handle multiple versions of device data and migrate

static STR_CTRL2HUB_MSG controlData;
static STR_HUB2CTRL_MSG hubData;
static STR_DEVICE_DATA deviceData;
