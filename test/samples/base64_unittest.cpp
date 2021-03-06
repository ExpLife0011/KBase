/*
 @ 0xCCCCCCCC
*/

#include "gtest/gtest.h"

#include "kbase/base64.h"

namespace {

const std::pair<std::string, std::string> ciphers[] {
    {"any carnal pleasure.", "YW55IGNhcm5hbCBwbGVhc3VyZS4="},
    {"any carnal pleasure", "YW55IGNhcm5hbCBwbGVhc3VyZQ=="},
    {"any carnal pleasur", "YW55IGNhcm5hbCBwbGVhc3Vy"},
    {"pleasure.", "cGxlYXN1cmUu"},
    {"easure.", "ZWFzdXJlLg=="},
    {"sure.", "c3VyZS4="}
};

}   // namespace

namespace kbase {

TEST(Base64Test, Encode)
{
    for (const auto& cp : ciphers) {
        auto encoded = Base64Encode(cp.first);
        EXPECT_EQ(encoded, cp.second);
    }
}

TEST(Base64Test, Decode)
{
    for (const auto& cp : ciphers) {
        auto decoded = Base64Decode(cp.second);
        EXPECT_EQ(decoded, cp.first);
    }
}

TEST(Base64Test, DecodeInvalid)
{
    std::string s = "aHR0c\\~DovLzEj";
    auto decoded = Base64Decode(s);
    EXPECT_TRUE(decoded.empty());
}

}   // namespace kbase
