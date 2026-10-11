#include "packages/sockets/socket_text.h"

#include <gtest/gtest.h>

#include <string_view>

TEST(SocketTextAccumulatorTest, ReassemblesIncompleteUtf8Suffixes) {
  SocketTextAccumulator accumulator;

  EXPECT_EQ(accumulator.consume(std::string_view("prefix \xe4", 8)), "prefix ");
  EXPECT_EQ(accumulator.pending_size(), 1u);
  EXPECT_EQ(accumulator.consume(std::string_view("\xb8", 1)), "");
  EXPECT_EQ(accumulator.pending_size(), 2u);
  EXPECT_EQ(accumulator.consume(std::string_view("\xad suffix", 8)), "\xe4\xb8\xad suffix");
  EXPECT_EQ(accumulator.pending_size(), 0u);

  EXPECT_EQ(accumulator.consume(std::string_view("\xc3", 1)), "");
  EXPECT_EQ(accumulator.consume(std::string_view("\xa9", 1)), "\xc3\xa9");
  EXPECT_EQ(accumulator.consume(std::string_view("\xf0\x9f", 2)), "");
  EXPECT_EQ(accumulator.consume(std::string_view("\x98\x80", 2)), "\xf0\x9f\x98\x80");
}

TEST(SocketTextAccumulatorTest, InvalidInputIsSanitizedWithoutCreatingTail) {
  SocketTextAccumulator accumulator;

  EXPECT_EQ(accumulator.consume(std::string_view("\xe4(", 2)), "\xef\xbf\xbd(");
  EXPECT_EQ(accumulator.pending_size(), 0u);
  EXPECT_EQ(accumulator.consume(std::string_view("\x80", 1)), "\xef\xbf\xbd");
  EXPECT_EQ(accumulator.pending_size(), 0u);
  EXPECT_EQ(accumulator.consume(std::string_view("\xe0\x80", 2)), "\xef\xbf\xbd");
  EXPECT_EQ(accumulator.pending_size(), 0u);
}

TEST(SocketTextAccumulatorTest, FlushPreservesExistingReplacementBehavior) {
  SocketTextAccumulator accumulator;

  EXPECT_EQ(accumulator.consume(std::string_view("\xe4", 1)), "");
  EXPECT_EQ(accumulator.flush(), "\xef\xbf\xbd");
  EXPECT_EQ(accumulator.pending_size(), 0u);
  EXPECT_EQ(accumulator.flush(), "");

  EXPECT_EQ(accumulator.consume(std::string_view("\xf0\x9f", 2)), "");
  accumulator.clear();
  EXPECT_EQ(accumulator.flush(), "");
}
