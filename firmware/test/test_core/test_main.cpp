// Host-side tests for the platform-independent firmware core.
#include <string.h>
#include <unity.h>

#include "../../InstaScreen/backoff.h"
#include "../../InstaScreen/codec.h"
#include "fixture.h"

namespace {

void test_crc32_matches_zlib() {
  const char* s = "123456789";  // standard check value
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, codec::crc32(reinterpret_cast<const uint8_t*>(s), 9));
  TEST_ASSERT_EQUAL_HEX32(0u, codec::crc32(nullptr, 0));
}

void test_crc32_incremental() {
  const uint8_t* d = reinterpret_cast<const uint8_t*>("123456789");
  uint32_t c = codec::crc32(d, 4);
  c = codec::crc32(d + 4, 5, c);
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, c);
}

void test_hex_decode_skips_json_quotes() {
  const char* in = "\"FFD8a0\"";
  uint8_t out[8];
  auto r = codec::hexDecode(in, strlen(in), out, sizeof out);
  TEST_ASSERT_FALSE(r.overflow);
  TEST_ASSERT_EQUAL(3, r.written);
  const uint8_t want[] = {0xFF, 0xD8, 0xA0};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(want, out, 3);
}

void test_hex_decode_reports_overflow() {
  const char* in = "AABBCC";
  uint8_t out[2];
  auto r = codec::hexDecode(in, strlen(in), out, sizeof out);
  TEST_ASSERT_TRUE(r.overflow);
  TEST_ASSERT_EQUAL(2, r.written);
}

void test_base64_decode_padding_and_escapes() {
  // "\/" is a legal JSON escape for '/'; the backslash must be ignored.
  const char* in = "\"/9j/2Q==\"";  // FF D8 FF D9
  const char* escaped = "\"\\/9j\\/2Q==\"";
  const uint8_t want[] = {0xFF, 0xD8, 0xFF, 0xD9};
  uint8_t out[8];

  auto r = codec::base64Decode(in, strlen(in), out, sizeof out);
  TEST_ASSERT_EQUAL(4, r.written);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(want, out, 4);

  r = codec::base64Decode(escaped, strlen(escaped), out, sizeof out);
  TEST_ASSERT_EQUAL(4, r.written);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(want, out, 4);
}

void test_base64_decode_all_lengths() {
  // Every residue mod 3 of the decoded length.
  struct Case { const char* in; size_t n; } cases[] = {
      {"", 0}, {"QQ==", 1}, {"QUI=", 2}, {"QUJD", 3}, {"QUJDRA==", 4},
  };
  uint8_t out[8];
  for (auto& c : cases) {
    auto r = codec::base64Decode(c.in, strlen(c.in), out, sizeof out);
    TEST_ASSERT_EQUAL(c.n, r.written);
    if (c.n) TEST_ASSERT_EQUAL_MEMORY("ABCD", out, c.n);
  }
}

void test_base64_decode_reports_overflow() {
  uint8_t out[2];
  auto r = codec::base64Decode("QUJD", 4, out, sizeof out);
  TEST_ASSERT_TRUE(r.overflow);
}

void test_jpeg_markers() {
  const uint8_t ok[] = {0xFF, 0xD8, 0x00, 0xFF, 0xD9};
  const uint8_t truncated[] = {0xFF, 0xD8, 0x00, 0x12, 0x34};
  TEST_ASSERT_TRUE(codec::looksLikeJpeg(ok, sizeof ok));
  TEST_ASSERT_FALSE(codec::looksLikeJpeg(truncated, sizeof truncated));
  TEST_ASSERT_FALSE(codec::looksLikeJpeg(ok, 2));
}

void test_backoff_doubles_and_caps() {
  Backoff b(1000, 5000);
  TEST_ASSERT_EQUAL(1000, b.fail());
  TEST_ASSERT_EQUAL(2000, b.fail());
  TEST_ASSERT_EQUAL(4000, b.fail());
  TEST_ASSERT_EQUAL(5000, b.fail());
  TEST_ASSERT_EQUAL(5000, b.fail());
  TEST_ASSERT_EQUAL(5, b.consecutiveFailures());
  b.reset();
  TEST_ASSERT_EQUAL(1000, b.fail());
}

// Decodes chunks produced by the Python uploader exactly as the device does
// (ImageStore::fetchChunks), then applies the same integrity checks.
template <size_t N>
void decodeFixture(const char* const (&chunks)[N], bool base64) {
  static uint8_t buf[fixture::kSize];
  size_t written = 0;
  for (size_t i = 0; i < N; ++i) {
    const auto r = base64 ? codec::base64Decode(chunks[i], strlen(chunks[i]), buf + written,
                                                sizeof buf - written)
                          : codec::hexDecode(chunks[i], strlen(chunks[i]), buf + written,
                                             sizeof buf - written);
    TEST_ASSERT_FALSE(r.overflow);
    written += r.written;
  }
  TEST_ASSERT_EQUAL(fixture::kSize, written);
  TEST_ASSERT_EQUAL_HEX32(fixture::kCrc32, codec::crc32(buf, written));
  TEST_ASSERT_TRUE(codec::looksLikeJpeg(buf, written));
}

void test_decodes_uploader_v2_fixture() {
  TEST_ASSERT_EQUAL(fixture::kChunkCount, sizeof fixture::kBase64Chunks / sizeof(char*));
  decodeFixture(fixture::kBase64Chunks, true);
}

void test_decodes_uploader_v1_fixture() { decodeFixture(fixture::kHexChunks, false); }

}  // namespace

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_crc32_matches_zlib);
  RUN_TEST(test_crc32_incremental);
  RUN_TEST(test_hex_decode_skips_json_quotes);
  RUN_TEST(test_hex_decode_reports_overflow);
  RUN_TEST(test_base64_decode_padding_and_escapes);
  RUN_TEST(test_base64_decode_all_lengths);
  RUN_TEST(test_base64_decode_reports_overflow);
  RUN_TEST(test_jpeg_markers);
  RUN_TEST(test_backoff_doubles_and_caps);
  RUN_TEST(test_decodes_uploader_v2_fixture);
  RUN_TEST(test_decodes_uploader_v1_fixture);
  return UNITY_END();
}
