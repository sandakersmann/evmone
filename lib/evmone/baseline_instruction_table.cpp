// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2020 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "baseline_instruction_table.hpp"
#include "instructions_traits.hpp"

namespace evmone::baseline
{
namespace
{
constexpr auto eof_cost_tables = []() noexcept {
    std::array<CostTable, EVMC_MAX_REVISION + 1> tables{};
    for (size_t r = EVMC_FRONTIER; r <= EVMC_MAX_REVISION; ++r)
    {
        auto& table = tables[r];
        for (size_t i = 0; i < table.size(); ++i)
        {
            table[i] = instr::gas_costs[r][i];  // Include instr::undefined in the table.
        }
    }
    return tables;
}();

constexpr auto legacy_cost_tables = []() noexcept {
    auto tables = eof_cost_tables;
    tables[EVMC_SHANGHAI][OP_RJUMP] = instr::undefined;
    tables[EVMC_SHANGHAI][OP_RJUMPI] = instr::undefined;
    return tables;
}();

}  // namespace

const CostTable& get_baseline_cost_table(evmc_revision rev, uint8_t eof_version) noexcept
{
    const auto& tables = (eof_version == 0) ? legacy_cost_tables : eof_cost_tables;
    return tables[rev];
}
}  // namespace evmone::baseline
