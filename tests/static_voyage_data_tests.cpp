#include <gtest/gtest.h>
#include <fstream>
#include "ais/static_voyage_data.hpp"
#include "ais/sentence_assembler.hpp"
#include "ais/tag_block.hpp"

TEST(StaticVoyageData, DecodesReferenceMessage) {
    ais::SentenceAssembler assembler;
    ASSERT_FALSE(assembler.add(ais::Sentence(
        "!AIVDM,2,1,1,A,55?MbV02;H;s<HtKR20EHE:0@T4@Dn2222222216L961O5Gf0NSQEp6ClRp8,0*1C")).has_value());
    auto payload = assembler.add(ais::Sentence("!AIVDM,2,2,1,A,88888888880,2*25"));
    ASSERT_TRUE(payload.has_value());

    auto data = ais::decode_static_voyage_data(*payload);
    ASSERT_TRUE(data.has_value());
    EXPECT_EQ(data->mmsi, 351759000u);
    ASSERT_TRUE(data->imo.has_value());
    EXPECT_EQ(data->imo, 9134270);
    EXPECT_EQ(data->call_sign, "3FOF8");
    EXPECT_EQ(data->name, "EVER DIADEM");
    EXPECT_EQ(data->ship_type, 70);
    EXPECT_EQ(data->to_bow, 225);
    EXPECT_EQ(data->to_stern, 70);
    EXPECT_EQ(data->to_port, 1);
    EXPECT_EQ(data->to_starboard, 31);
    EXPECT_EQ(data->eta_month, 5);
    EXPECT_EQ(data->eta_day, 15);
    EXPECT_EQ(data->eta_hour, 14);
    EXPECT_EQ(data->eta_minute, 0);
    EXPECT_EQ(data->draught, 12.2);
    EXPECT_EQ(data->destination, "NEW YORK");
}

TEST(StaticVoyageData, WrongType) {
    ais::Payload payload(
    "15?MbV02;H;s<HtKR20EHE:0@T4@Dn2222222216L961O5Gf0NSQEp6ClRp8"
    "88888888880", 2);
    EXPECT_EQ(payload.bit_count(), 424u);
    EXPECT_FALSE(ais::decode_static_voyage_data(payload));
}

TEST(StaticVoyageData, TooShort) {
    ais::Payload payload("55?MbV02;H;s<HtKR20EHE:0@T4@Dn2222222216L961O5Gf0NSQEp6ClRp8", 0);  
    EXPECT_EQ(payload.bit_count(), 360u);
    EXPECT_FALSE(ais::decode_static_voyage_data(payload));
}

TEST(StaticVoyageData, KystverketFixture) {
    std::ifstream file(std::string(FIXTURES_DIR) + "/kystverket_live_sample.nmea");
    ASSERT_TRUE(file.is_open());
    std::string line;
    ais::SentenceAssembler assembler;
    int decoded = 0;
    int missing_imo = 0;
    int missing_draught = 0;
    std::optional<ais::StaticVoyageData> sagaoy;
    while (std::getline(file, line)) {
        std::optional<std::string_view> view = ais::strip_tag_block(line);
        if (view) {
            ais::Sentence sentence(*view);
            if(sentence.is_valid() && sentence.is_well_formed()) {
                std::optional<ais::Payload> payload = assembler.add(sentence);
                if (payload) {
                    std::optional<ais::StaticVoyageData> data = ais::decode_static_voyage_data(*payload);
                    if (data) {
                        if (!data->imo.has_value()) {
                            missing_imo++;
                        }
                        if (!data->draught.has_value()) {
                            missing_draught++;
                        }
                        if (data->mmsi == 257201600) {
                            sagaoy = std::move(*data);
                        }
                        decoded++;
                    }
                }
            }
        }
    }
    EXPECT_EQ(decoded, 133);
    EXPECT_EQ(missing_imo, 56);
    EXPECT_EQ(missing_draught, 38);
    ASSERT_TRUE(sagaoy.has_value());
    EXPECT_EQ(sagaoy->name, "SAGAOY");
    EXPECT_EQ(sagaoy->destination, "FREDRIKSTAD");
}