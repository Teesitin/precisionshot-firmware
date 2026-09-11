#pragma once

namespace bluetooth {
inline constexpr char DEVICE_NAME[] = "PrecisionShot";
inline constexpr char SERVICE_UUID[] = "8c7a0001-6c3b-4f3d-a8d9-2adbc9f10211";
inline constexpr char TX_UUID[] = "8c7a0002-6c3b-4f3d-a8d9-2adbc9f10211";
inline constexpr char RX_UUID[] = "8c7a0003-6c3b-4f3d-a8d9-2adbc9f10211";
enum class EventType { Connected, Disconnected, Advertising, Received };
struct Event {
  EventType type;
  char text[25];
};
void initialize();
bool poll(Event &event);
void restartAdvertising();
bool notify(const char *text);
bool notificationsEnabled();
}
