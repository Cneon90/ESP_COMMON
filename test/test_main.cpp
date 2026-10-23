// File: test/test_main.cpp
/// @file test_main.cpp
/// @brief Точка входа Unity-тестов: собирает все тестовые группы набора.
///
/// PlatformIO не генерирует runner автоматически — main() и общие
/// setUp/tearDown определены здесь; тестовые функции — в test_frame.cpp,
/// test_handshake.cpp и test_ping.cpp.

#include <unity.h>

// Общие хуки Unity (ровно одно определение на весь набор).
void setUp() {}
void tearDown() {}

// --- test_frame.cpp ---
extern void test_crc16_check_value_123456789(void);
extern void test_crc16_streaming_equals_oneshot(void);
extern void test_frame_roundtrip(void);
extern void test_frame_zero_payload_roundtrip(void);
extern void test_frame_header_layout(void);
extern void test_frame_decode_truncated_header(void);
extern void test_frame_decode_truncated_body(void);
extern void test_frame_decode_bad_magic(void);
extern void test_frame_decode_crc_corruption_in_body(void);
extern void test_frame_decode_crc_corruption_in_crc_field(void);
extern void test_frame_decode_oversize_length(void);
extern void test_frame_decode_rejects_unknown_crypto_mode(void);
extern void test_frame_stream_two_frames(void);
extern void test_frame_encode_rejects_oversize_payload(void);

// --- test_handshake.cpp ---
extern void test_hkdf_rfc5869_case1(void);
extern void test_hkdf_deterministic_and_salt_sensitive(void);
extern void test_handshake_full_exchange_symmetric(void);
extern void test_handshake_full_path_via_io(void);
extern void test_handshake_bad_auth(void);
extern void test_handshake_bad_crc_rejected(void);
extern void test_handshake_negotiation_picks_strongest_common(void);
extern void test_handshake_negotiation_plain_only(void);
extern void test_handshake_no_common_mode(void);
extern void test_crypto_factory_modes(void);
extern void test_crypto_iv_layout(void);
extern void test_aes128_roundtrip_and_seq_binding(void);
extern void test_aes256_roundtrip_and_seq_binding(void);
extern void test_chacha20_roundtrip_and_seq_binding(void);
extern void test_crypto_rejects_wrong_key_length(void);
extern void test_mbedtls_aes128_ctr_nist_vector(void);
extern void test_mbedtls_chacha20_rfc7539_vector(void);

// --- test_ping.cpp ---
extern void test_ping_command_echoes_data(void);
extern void test_ping_command_empty_payload(void);
extern void test_dispatcher_dispatches_known_cmd(void);
extern void test_dispatcher_unknown_cmd_not_implemented(void);
extern void test_dispatcher_re_registration_replaces(void);
extern void test_registry_installs_into_dispatcher(void);
extern void test_session_roundtrip_ping(void);
extern void test_session_roundtrip_unknown_cmd(void);
extern void test_session_replay_rejected(void);
extern void test_session_server_serves_same_command_set_both_roles(void);
extern void test_config_section_helpers(void);
extern void test_config_get_set_roundtrip(void);
extern void test_config_set_wrong_size_rejected(void);
extern void test_config_set_unknown_section_rejected(void);
extern void test_reboot_command_schedules_delay(void);
extern void test_config_commands_via_session_roundtrip(void);

int main() {
  UNITY_BEGIN();

  // FrameCodec + CRC
  RUN_TEST(test_crc16_check_value_123456789);
  RUN_TEST(test_crc16_streaming_equals_oneshot);
  RUN_TEST(test_frame_roundtrip);
  RUN_TEST(test_frame_zero_payload_roundtrip);
  RUN_TEST(test_frame_header_layout);
  RUN_TEST(test_frame_decode_truncated_header);
  RUN_TEST(test_frame_decode_truncated_body);
  RUN_TEST(test_frame_decode_bad_magic);
  RUN_TEST(test_frame_decode_crc_corruption_in_body);
  RUN_TEST(test_frame_decode_crc_corruption_in_crc_field);
  RUN_TEST(test_frame_decode_oversize_length);
  RUN_TEST(test_frame_decode_rejects_unknown_crypto_mode);
  RUN_TEST(test_frame_stream_two_frames);
  RUN_TEST(test_frame_encode_rejects_oversize_payload);

  // Handshake + крипто
  RUN_TEST(test_hkdf_rfc5869_case1);
  RUN_TEST(test_hkdf_deterministic_and_salt_sensitive);
  RUN_TEST(test_handshake_full_exchange_symmetric);
  RUN_TEST(test_handshake_full_path_via_io);
  RUN_TEST(test_handshake_bad_auth);
  RUN_TEST(test_handshake_bad_crc_rejected);
  RUN_TEST(test_handshake_negotiation_picks_strongest_common);
  RUN_TEST(test_handshake_negotiation_plain_only);
  RUN_TEST(test_handshake_no_common_mode);
  RUN_TEST(test_crypto_factory_modes);
  RUN_TEST(test_crypto_iv_layout);
  RUN_TEST(test_aes128_roundtrip_and_seq_binding);
  RUN_TEST(test_aes256_roundtrip_and_seq_binding);
  RUN_TEST(test_chacha20_roundtrip_and_seq_binding);
  RUN_TEST(test_crypto_rejects_wrong_key_length);
  RUN_TEST(test_mbedtls_aes128_ctr_nist_vector);
  RUN_TEST(test_mbedtls_chacha20_rfc7539_vector);

  // Команды + Session
  RUN_TEST(test_ping_command_echoes_data);
  RUN_TEST(test_ping_command_empty_payload);
  RUN_TEST(test_dispatcher_dispatches_known_cmd);
  RUN_TEST(test_dispatcher_unknown_cmd_not_implemented);
  RUN_TEST(test_dispatcher_re_registration_replaces);
  RUN_TEST(test_registry_installs_into_dispatcher);
  RUN_TEST(test_session_roundtrip_ping);
  RUN_TEST(test_session_roundtrip_unknown_cmd);
  RUN_TEST(test_session_replay_rejected);
  RUN_TEST(test_session_server_serves_same_command_set_both_roles);

  // Настройки: SET_CONFIG / GET_CONFIG / REBOOT
  RUN_TEST(test_config_section_helpers);
  RUN_TEST(test_config_get_set_roundtrip);
  RUN_TEST(test_config_set_wrong_size_rejected);
  RUN_TEST(test_config_set_unknown_section_rejected);
  RUN_TEST(test_reboot_command_schedules_delay);
  RUN_TEST(test_config_commands_via_session_roundtrip);

  return UNITY_END();
}
