#include "ais/decoder_stage.hpp"
#include "ais/position_report.hpp"
#include "ais/concurrent_queue.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <optional>

TEST(DecoderStageTests, BasicTest) {
    // Arrange
    ais::ThreadSafeQueue<std::string> input(100);
    ais::ThreadSafeQueue<ais::AisMessage> output(100);
    ais::DecoderStage decoder;
    std::stop_source stop_source;

    // Act
    std::jthread decoder_thread([&] {
        decoder.run(input, output, stop_source.get_token());
    });

    // Push a test sentence
    input.push("!AIVDM,1,1,,B,15M67FC000G?ufbE`FepT@3n00Sa,0*5C");

    // Assert
    std::optional<ais::AisMessage> result = output.pop();
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<ais::PositionReport>(*result));
    EXPECT_EQ(std::get<ais::PositionReport>(*result).mmsi, 366053209u);

    input.close();
    decoder_thread.join();
}

TEST(DecoderStageTests, MalformedLine) {
    // Arrange
    ais::ThreadSafeQueue<std::string> input(100);
    ais::ThreadSafeQueue<ais::AisMessage> output(100);
    ais::DecoderStage decoder;
    std::stop_source stop_source;

    // Act
    std::jthread decoder_thread([&] {
        decoder.run(input, output, stop_source.get_token());
    });

    // Push a malformed test sentence
    input.push("!AIVDM,1,1,,B,INVALID,0*00");
    input.push("!AIVDM,1,1,,B,15M67FC000G?ufbE`FepT@3n00Sa,0*5C");

    // Assert
    std::optional<ais::AisMessage> result = output.pop();
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<ais::PositionReport>(*result));
    EXPECT_EQ(std::get<ais::PositionReport>(*result).mmsi, 366053209u);

    input.close();
    decoder_thread.join();
}

TEST(DecoderStageTests, MultiFragment) {
    // Arrange
    ais::ThreadSafeQueue<std::string> input(100);
    ais::ThreadSafeQueue<ais::AisMessage> output(100);
    ais::DecoderStage decoder;
    std::stop_source stop_source;

    // Act
    std::jthread decoder_thread([&] {
        decoder.run(input, output, stop_source.get_token());
    });

    // Push multi-fragment test sentences
    input.push("!AIVDM,2,1,5,B,15M67FC000G?uf,0*05");
    input.push("!AIVDM,2,2,5,B,bE`FepT@3n00Sa,0*7F");

    // Assert
    std::optional<ais::AisMessage> result = output.pop();
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<ais::PositionReport>(*result));
    EXPECT_EQ(std::get<ais::PositionReport>(*result).mmsi, 366053209u);

    input.close();
    decoder_thread.join();
}

TEST(DecoderStageTests, Type5Test) {
    // Arrange
    ais::ThreadSafeQueue<std::string> input(100);
    ais::ThreadSafeQueue<ais::AisMessage> output(100);
    ais::DecoderStage decoder;
    std::stop_source stop_source;

    // Act
    std::jthread decoder_thread([&] {
        decoder.run(input, output, stop_source.get_token());
    });

    // Push a test sentence
    input.push("!AIVDM,2,1,1,A,55?MbV02;H;s<HtKR20EHE:0@T4@Dn2222222216L961O5Gf0NSQEp6ClRp8,0*1C");
    input.push("!AIVDM,2,2,1,A,88888888880,2*25");

    // Assert
    std::optional<ais::AisMessage> result = output.pop();
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<ais::StaticVoyageData>(*result));
    EXPECT_EQ(std::get<ais::StaticVoyageData>(*result).name, "EVER DIADEM");

    input.close();
    decoder_thread.join();
}