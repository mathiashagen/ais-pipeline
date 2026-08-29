#include "ais/decoder_stage.hpp"
#include "ais/tag_block.hpp"
#include "ais/position_report.hpp"

namespace ais {
void DecoderStage::run(ThreadSafeQueue<std::string>& input, ThreadSafeQueue<PositionReport>& output, std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        std::optional<std::string> line = input.pop();
        if (line) {
            std::optional<std::string_view> view = strip_tag_block(*line);
            if (view) {
                Sentence sentence(*view);
                if(sentence.is_valid() && sentence.is_well_formed()) {
                    std::optional<Payload> payload = assembler_.add(sentence);
                    if (payload) {
                        std::optional<PositionReport> report =decode_position_report(*payload);
                        if (report) {
                            output.push(std::move(*report));
                        }
                    }
                }
            }
        }
        else {
            break;
        }
    }
}
}