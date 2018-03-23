// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////

#include "tink/mac/mac_factory.h"

#include "tink/mac.h"
#include "tink/crypto_format.h"
#include "tink/keyset_handle.h"
#include "tink/mac/hmac_key_manager.h"
#include "tink/mac/mac_config.h"
#include "tink/util/status.h"
#include "tink/util/test_util.h"
#include "gtest/gtest.h"
#include "proto/common.pb.h"
#include "proto/hmac.pb.h"
#include "proto/tink.pb.h"

using crypto::tink::test::AddRawKey;
using crypto::tink::test::AddTinkKey;
using crypto::tink::test::GetKeysetHandle;
using google::crypto::tink::HashType;
using google::crypto::tink::HmacKeyFormat;
using google::crypto::tink::KeyData;
using google::crypto::tink::Keyset;
using google::crypto::tink::KeyStatusType;

namespace util = crypto::tink::util;

namespace crypto {
namespace tink {
namespace {

class MacFactoryTest : public ::testing::Test {
};

TEST_F(MacFactoryTest, testBasic) {
  Keyset keyset;
  auto mac_result = MacFactory::GetPrimitive(*GetKeysetHandle(keyset));
  EXPECT_FALSE(mac_result.ok());
  EXPECT_EQ(util::error::INVALID_ARGUMENT, mac_result.status().error_code());
  EXPECT_PRED_FORMAT2(testing::IsSubstring, "at least one key",
                      mac_result.status().error_message());
}

TEST_F(MacFactoryTest, testPrimitive) {
  // Prepare a format for generating keys for a Keyset.
  HmacKeyManager key_manager;
  const KeyFactory& key_factory = key_manager.get_key_factory();
  std::string key_type = key_manager.get_key_type();

  HmacKeyFormat key_format;
  key_format.set_key_size(16);
  key_format.mutable_params()->set_tag_size(10);
  key_format.mutable_params()->set_hash(HashType::SHA256);

  // Prepare a Keyset.
  Keyset keyset;
  uint32_t key_id_1 = 1234543;
  auto new_key = std::move(key_factory.NewKey(key_format).ValueOrDie());
  AddTinkKey(key_type, key_id_1, *new_key, KeyStatusType::ENABLED,
             KeyData::SYMMETRIC, &keyset);

  uint32_t key_id_2 = 726329;
  new_key = std::move(key_factory.NewKey(key_format).ValueOrDie());
  AddRawKey(key_type, key_id_2, *new_key, KeyStatusType::ENABLED,
            KeyData::SYMMETRIC, &keyset);

  uint32_t key_id_3 = 7213743;
  new_key = std::move(key_factory.NewKey(key_format).ValueOrDie());
  AddTinkKey(key_type, key_id_3, *new_key, KeyStatusType::ENABLED,
             KeyData::SYMMETRIC, &keyset);

  keyset.set_primary_key_id(key_id_3);

  // Initialize the registry.
  ASSERT_TRUE(MacConfig::RegisterStandardKeyTypes().ok());;

  // Create a KeysetHandle and use it with the factory.
  auto mac_result = MacFactory::GetPrimitive(*GetKeysetHandle(keyset));
  EXPECT_TRUE(mac_result.ok()) << mac_result.status();
  auto mac = std::move(mac_result.ValueOrDie());

  // Test the resulting Mac-instance.
  std::string data = "some_data_for_mac";

  auto compute_mac_result = mac->ComputeMac(data);
  EXPECT_TRUE(compute_mac_result.ok()) << compute_mac_result.status();
  std::string mac_value = compute_mac_result.ValueOrDie();
  std::string prefix =
      CryptoFormat::get_output_prefix(keyset.key(2)).ValueOrDie();
  EXPECT_PRED_FORMAT2(testing::IsSubstring, prefix, mac_value);

  util::Status status = mac->VerifyMac(mac_value, data);
  EXPECT_TRUE(status.ok()) << status;

  status = mac->VerifyMac(mac_value, "bad data for mac");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(util::error::INVALID_ARGUMENT, status.error_code());
  EXPECT_PRED_FORMAT2(testing::IsSubstring, "verification failed",
                      status.error_message());

  status = mac->VerifyMac("some bad mac value", data);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(util::error::INVALID_ARGUMENT, status.error_code());
  EXPECT_PRED_FORMAT2(testing::IsSubstring, "verification failed",
                      status.error_message());

  // Create raw MAC value with 2nd key, and verify with Mac-instance.
  auto raw_mac = std::move(
      key_manager.GetPrimitive(keyset.key(1).key_data()).ValueOrDie());
  std::string raw_mac_value = raw_mac->ComputeMac(data).ValueOrDie();
  status = mac->VerifyMac(raw_mac_value, data);
  EXPECT_TRUE(status.ok()) << status;
}

}  // namespace
}  // namespace tink
}  // namespace crypto


int main(int ac, char* av[]) {
  testing::InitGoogleTest(&ac, av);
  return RUN_ALL_TESTS();
}