#pragma once
#include "ais/sentence_assembler.hpp"
#include "ais/concurrent_queue.hpp"
#include "ais/position_report.hpp"
#include <stop_token>

namespace ais {

class DecoderStage {
public:
    void run(ThreadSafeQueue<std::string>& input, ThreadSafeQueue<PositionReport>& output, std::stop_token stop_token);
private:
    SentenceAssembler assembler_;
};

}