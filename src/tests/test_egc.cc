#include "base/internal/strutils.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

std::vector<int32_t> boundaries(EGCIterator& iterator) {
  std::vector<int32_t> result;
  for (auto offset = iterator->first(); offset != icu::BreakIterator::DONE;
       offset = iterator->next()) {
    result.push_back(offset);
  }
  return result;
}

TEST(EgcResetTest, UnicodeSuffixRetainsIcuPath) {
  const std::string source = "中|plain";
  EGCIterator iterator(source.data(), static_cast<int32_t>(source.size()));
  ASSERT_TRUE(iterator.ok());
  ASSERT_FALSE(iterator.is_ascii());
  iterator.reset(source.data() + 4, 5);
  ASSERT_TRUE(iterator.ok());
  EXPECT_FALSE(iterator.is_ascii());
  EXPECT_EQ((std::vector<int32_t>{0, 1, 2, 3, 4, 5}), boundaries(iterator));
}

TEST(EgcResetTest, CrLfSuffixRetainsIcuPath) {
  const std::string source = "\r\nplain";
  EGCIterator iterator(source.data(), static_cast<int32_t>(source.size()));
  ASSERT_TRUE(iterator.ok());
  ASSERT_FALSE(iterator.is_ascii());
  iterator.reset(source.data() + 2, 5);
  ASSERT_TRUE(iterator.ok());
  EXPECT_FALSE(iterator.is_ascii());
  EXPECT_EQ((std::vector<int32_t>{0, 1, 2, 3, 4, 5}), boundaries(iterator));
}

void expect_subranges_match_fresh_iterator(const std::string& source,
                                            int32_t first_start = 0,
                                            int32_t last_start = -1) {
  const auto size = static_cast<int32_t>(source.size());
  if (last_start < 0)
    last_start = size;
  for (int32_t start = first_start; start <= last_start; ++start) {
    for (int32_t length = 0; length <= size - start; ++length) {
      SCOPED_TRACE(::testing::Message() << source << ":" << start << ":" << length);
      EGCIterator iterator(source.data(), size);
      iterator->last();
      iterator.reset(source.data() + start, length);
      EGCIterator fresh(source.data() + start, length);
      ASSERT_TRUE(iterator.ok());
      ASSERT_TRUE(fresh.ok());
      EXPECT_EQ(boundaries(fresh), boundaries(iterator));
    }
  }
}

TEST(EgcResetTest, SubrangesMatchFreshIteratorAscii) {
  expect_subranges_match_fresh_iterator("ascii");
}

TEST(EgcResetTest, SubrangesMatchFreshIteratorCjk) {
  expect_subranges_match_fresh_iterator("中|文|尾");
}

TEST(EgcResetTest, SubrangesMatchFreshIteratorCombining) {
  expect_subranges_match_fresh_iterator("é|x");
}

class EgcEmojiSubrangeTest : public ::testing::TestWithParam<int32_t> {};

TEST_P(EgcEmojiSubrangeTest, MatchesFreshIterator) {
  const auto start = GetParam();
  expect_subranges_match_fresh_iterator("👩‍💻|🙂|x", start, start);
}

INSTANTIATE_TEST_SUITE_P(AllByteStarts, EgcEmojiSubrangeTest,
                         ::testing::Values(0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
                                           10, 11, 12, 13, 14, 15, 16, 17,
                                           18));

TEST(EgcResetTest, SubrangesMatchFreshIteratorCrLf) {
  expect_subranges_match_fresh_iterator("a\r\nb\r\nc");
}

TEST(EgcResetTest, IteratorDoesNotRetainDestroyedSource) {
  {
    const std::string source = "👩‍💻|🙂|x";
    EGCIterator iterator(source.data(), static_cast<int32_t>(source.size()));
    EXPECT_EQ((std::vector<int32_t>{0, 11, 12, 16, 17, 18}), boundaries(iterator));
  }

  {
    const std::string replacement = "中|文|尾";
    EGCIterator iterator(replacement.data(), static_cast<int32_t>(replacement.size()));
    EXPECT_EQ((std::vector<int32_t>{0, 3, 4, 7, 8, 11}), boundaries(iterator));
  }
}

TEST(EgcResetTest, SmartIteratorClearsCachedCursorAndCount) {
  const std::string source = "中|é|plain";
  EGCSmartIterator iterator(source.data(), static_cast<int32_t>(source.size()));
  ASSERT_TRUE(iterator.ok());
  EXPECT_EQ(9u, iterator.count());
  EXPECT_GE(iterator.index_to_offset(-1), 0);
  iterator.reset(source.data() + 8, 5);
  ASSERT_TRUE(iterator.ok());
  EXPECT_EQ(5u, iterator.count());
  EXPECT_EQ(0, iterator.first());
  EXPECT_EQ(1, iterator.next());
  EXPECT_EQ(4, iterator.index_to_offset(-1));
}

TEST(EgcResetTest, CrossBufferAndNegativeLengthReset) {
  const std::string unicode = "中|tail";
  const std::string ascii = "other";
  EGCIterator iterator(unicode.data(), static_cast<int32_t>(unicode.size()));
  iterator.reset(ascii.data(), static_cast<int32_t>(ascii.size()));
  ASSERT_TRUE(iterator.ok());
  EXPECT_TRUE(iterator.is_ascii());
  iterator.reset(unicode.data(), -2);
  EXPECT_FALSE(iterator.ok());
  iterator.reset(unicode.data(), -1);
  ASSERT_TRUE(iterator.ok());
  EXPECT_FALSE(iterator.is_ascii());
  EXPECT_EQ((std::vector<int32_t>{0, 3, 4, 5, 6, 7, 8}), boundaries(iterator));
  iterator.reset(ascii.data(), static_cast<int32_t>(ascii.size()));
  ASSERT_TRUE(iterator.ok());
  EXPECT_TRUE(iterator.is_ascii());
}

TEST(EgcResetTest, AsciiPairsMatchIcu) {
  for (int first = 0; first < 128; ++first) {
    for (int second = 0; second < 128; ++second) {
      const char source[] = {static_cast<char>(first), static_cast<char>(second)};
      EGCIterator iterator(source, 2);
      ASSERT_TRUE(iterator.ok());
      const auto offsets = boundaries(iterator);
      if (first == '\r' && second == '\n') {
        EXPECT_EQ((std::vector<int32_t>{0, 2}), offsets);
      } else {
        EXPECT_EQ((std::vector<int32_t>{0, 1, 2}), offsets);
      }
    }
  }
}

}  // namespace
