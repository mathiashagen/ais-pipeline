#include "ais/decoder_stage.hpp"
#include "ais/tag_block.hpp"
#include "ais/ais_message.hpp"

namespace ais {
void DecoderStage::run(ThreadSafeQueue<std::string>& input, ThreadSafeQueue<AisMessage>& output, std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        std::optional<std::string> line = input.pop();
        if (line) {
            std::optional<std::string_view> view = strip_tag_block(*line);
            if (view) {
                Sentence sentence(*view);
                if(sentence.is_valid() && sentence.is_well_formed()) {
                    std::optional<std::string_view> source = tag_block_source(*line);
                    std::optional<Payload> payload = assembler_.add(sentence, source.value_or(std::string_view{}));
                    if (payload) {
                        auto result = decode_message(*payload);
                        if (result) {
                            output.push(std::move(*result));
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