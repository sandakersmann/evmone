// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2022 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "statetest.hpp"
#include <nlohmann/json.hpp>
#include <cassert>
#include <fstream>
#include <sstream>
#include <string>

namespace evmone::test
{
namespace json = nlohmann;
using evmc::from_hex;

template <>
uint8_t from_json<uint8_t>(const json::json& j)
{
    const auto ret = std::stoul(j.get<std::string>(), nullptr, 16);
    if (ret > std::numeric_limits<uint8_t>::max())
        throw std::out_of_range("from_json<uint8_t>: value > 0xFF");

    return static_cast<uint8_t>(ret);
}

template <>
int64_t from_json<int64_t>(const json::json& j)
{
    return static_cast<int64_t>(std::stoll(j.get<std::string>(), nullptr, 16));
}

template <>
uint64_t from_json<uint64_t>(const json::json& j)
{
    return static_cast<uint64_t>(std::stoull(j.get<std::string>(), nullptr, 16));
}

template <>
bytes from_json<bytes>(const json::json& j)
{
    return from_hex(j.get<std::string>()).value();
}

template <>
address from_json<address>(const json::json& j)
{
    return evmc::from_hex<address>(j.get<std::string>()).value();
}

template <>
hash256 from_json<hash256>(const json::json& j)
{
    return evmc::from_hex<hash256>(j.get<std::string>()).value();
}

template <>
intx::uint256 from_json<intx::uint256>(const json::json& j)
{
    const auto s = j.get<std::string>();
    if (s.starts_with("0x:bigint "))
        return std::numeric_limits<intx::uint256>::max();  // Fake it
    return intx::from_string<intx::uint256>(s);
}

template <>
state::AccessList from_json<state::AccessList>(const json::json& j)
{
    state::AccessList o;
    for (const auto& a : j)
    {
        std::vector<bytes32> storage_access_list;
        for (const auto& storage_key : a.at("storageKeys"))
            storage_access_list.emplace_back(from_json<bytes32>(storage_key));
        o.emplace_back(from_json<address>(a.at("address")), std::move(storage_access_list));
    }
    return o;
}

// Based on calculateEIP1559BaseFee from ethereum/retesteth
inline uint64_t calculate_current_base_fee_eip1559(
    uint64_t parent_gas_used, uint64_t parent_gas_limit, uint64_t parent_base_fee)
{
    // TODO: Make sure that 64-bit precision is good enough.
    static const auto BASE_FEE_MAX_CHANGE_DENOMINATOR = 8;
    static const auto ELASTICITY_MULTIPLIER = 2;

    uint64_t base_fee = 0;

    uint64_t parent_gas_target = parent_gas_limit / ELASTICITY_MULTIPLIER;

    if (parent_gas_used == parent_gas_target)
        base_fee = parent_base_fee;
    else if (parent_gas_used > parent_gas_target)
    {
        uint64_t gas_used_delta = parent_gas_used - parent_gas_target;
        uint64_t formula =
            parent_base_fee * gas_used_delta / parent_gas_target / BASE_FEE_MAX_CHANGE_DENOMINATOR;
        uint64_t base_fee_per_gas_delta = formula > 1 ? formula : 1;
        base_fee = parent_base_fee + base_fee_per_gas_delta;
    }
    else
    {
        uint64_t gas_used_delta = parent_gas_target - parent_gas_used;

        auto base_fee_per_gas_delta_u128 = intx::uint128(parent_base_fee, 0) *
                                           intx::uint128(gas_used_delta, 0) / parent_gas_target /
                                           BASE_FEE_MAX_CHANGE_DENOMINATOR;

        uint64_t base_fee_per_gas_delta = base_fee_per_gas_delta_u128[0];
        if (parent_base_fee > base_fee_per_gas_delta)
            base_fee = parent_base_fee - base_fee_per_gas_delta;
        else
            base_fee = 0;
    }
    return base_fee;
}

template <>
state::BlockInfo from_json<state::BlockInfo>(const json::json& j)
{
    evmc::bytes32 difficulty;
    const auto prev_randao_it = j.find("currentRandom");
    const auto current_difficulty_it = j.find("currentDifficulty");
    const auto parent_difficulty_it = j.find("parentDifficulty");
    if (prev_randao_it != j.end())
        difficulty = from_json<evmc::bytes32>(*prev_randao_it);
    else if (current_difficulty_it != j.end())
        difficulty = from_json<evmc::bytes32>(*current_difficulty_it);
    else if (parent_difficulty_it != j.end())
        difficulty = from_json<evmc::bytes32>(*parent_difficulty_it);

    uint64_t base_fee = 0;
    if (j.contains("currentBaseFee"))
        base_fee = from_json<uint64_t>(j.at("currentBaseFee"));
    else if (j.contains("parentBaseFee"))
    {
        base_fee = calculate_current_base_fee_eip1559(from_json<uint64_t>(j.at("parentGasUsed")),
            from_json<uint64_t>(j.at("parentGasLimit")),
            from_json<uint64_t>(j.at("parentBaseFee")));
    }
    return {from_json<int64_t>(j.at("currentNumber")), from_json<int64_t>(j.at("currentTimestamp")),
        from_json<int64_t>(j.at("currentGasLimit")),
        from_json<evmc::address>(j.at("currentCoinbase")), difficulty, base_fee};
}

template <>
state::State from_json<state::State>(const json::json& j)
{
    state::State o;
    for (const auto& [j_addr, j_acc] : j.items())
    {
        auto& acc = o.insert(from_json<address>(j_addr),
            {.nonce = from_json<uint64_t>(j_acc.at("nonce")),
                .balance = from_json<intx::uint256>(j_acc.at("balance")),
                .code = from_json<bytes>(j_acc.at("code"))});

        for (const auto& [j_key, j_value] : j_acc.at("storage").items())
        {
            const auto value = from_json<bytes32>(j_value);
            acc.storage.insert({from_json<bytes32>(j_key), {.current = value, .original = value}});
        }
    }
    return o;
}

evmc_revision to_rev(std::string_view s)
{
    if (s == "Frontier")
        return EVMC_FRONTIER;
    if (s == "Homestead")
        return EVMC_HOMESTEAD;
    if (s == "EIP150")
        return EVMC_TANGERINE_WHISTLE;
    if (s == "EIP158")
        return EVMC_SPURIOUS_DRAGON;
    if (s == "Byzantium")
        return EVMC_BYZANTIUM;
    if (s == "Constantinople")
        return EVMC_CONSTANTINOPLE;
    if (s == "ConstantinopleFix")
        return EVMC_PETERSBURG;
    if (s == "Istanbul")
        return EVMC_ISTANBUL;
    if (s == "Berlin")
        return EVMC_BERLIN;
    if (s == "London")
        return EVMC_LONDON;
    if (s == "Merge")
        return EVMC_PARIS;
    if (s == "Merge+3855")  // PUSH0
        return EVMC_SHANGHAI;
    if (s == "Shanghai")
        return EVMC_SHANGHAI;
    if (s == "Cancun")
        return EVMC_CANCUN;
    if (s == "Prague")
        return EVMC_PRAGUE;
    throw std::invalid_argument{"unknown revision: " + std::string{s}};
}

/// Load common parts of Transaction or TestMultiTransaction.
static void from_json_tx_common(const json::json& j, state::Transaction& o)
{
    o.sender = from_json<evmc::address>(j.at("sender"));

    if (const auto& to = j.at("to"); !to.get<std::string>().empty())
        o.to = from_json<evmc::address>(to);

    if (const auto gas_price_it = j.find("gasPrice"); gas_price_it != j.end())
    {
        o.kind = state::Transaction::Kind::legacy;
        o.max_gas_price = from_json<intx::uint256>(*gas_price_it);
        o.max_priority_gas_price = o.max_gas_price;
        if (j.contains("maxFeePerGas") || j.contains("maxPriorityFeePerGas"))
        {
            throw std::invalid_argument(
                "Misformatted transaction -- contains both legacy and 1559 fees");
        }
    }
    else
    {
        o.kind = state::Transaction::Kind::eip1559;
        o.max_gas_price = from_json<intx::uint256>(j.at("maxFeePerGas"));
        o.max_priority_gas_price = from_json<intx::uint256>(j.at("maxPriorityFeePerGas"));
    }
}

template <>
state::Transaction from_json<state::Transaction>(const json::json& j)
{
    state::Transaction o;
    from_json_tx_common(j, o);
    o.data = from_json<bytes>(j.at("input"));
    o.gas_limit = from_json<int64_t>(j.at("gas"));
    o.value = from_json<intx::uint256>(j.at("value"));

    if (const auto ac_it = j.find("accessList"); ac_it != j.end())
        o.access_list = from_json<state::AccessList>(*ac_it);

    o.nonce = from_json<uint64_t>(j.at("nonce"));
    o.r = from_json<intx::uint256>(j.at("r"));
    o.s = from_json<intx::uint256>(j.at("s"));
    o.v = from_json<uint8_t>(j.at("v"));

    return o;
}

static void from_json(const json::json& j, TestMultiTransaction& o)
{
    from_json_tx_common(j, o);

    for (const auto& j_data : j.at("data"))
        o.inputs.emplace_back(from_json<bytes>(j_data));

    if (j.contains("accessLists"))
    {
        for (const auto& j_access_list : j["accessLists"])
            o.access_lists.emplace_back(from_json<state::AccessList>(j_access_list));
    }

    for (const auto& j_gas_limit : j.at("gasLimit"))
        o.gas_limits.emplace_back(from_json<int64_t>(j_gas_limit));

    for (const auto& j_value : j.at("value"))
        o.values.emplace_back(from_json<intx::uint256>(j_value));
}

static void from_json(const json::json& j, TestMultiTransaction::Indexes& o)
{
    o.input = j.at("data").get<size_t>();
    o.gas_limit = j.at("gas").get<size_t>();
    o.value = j.at("value").get<size_t>();
}

static void from_json(const json::json& j, StateTransitionTest::Case::Expectation& o)
{
    o.indexes = j.at("indexes").get<TestMultiTransaction::Indexes>();
    o.state_hash = from_json<hash256>(j.at("hash"));
    o.logs_hash = from_json<hash256>(j.at("logs"));
    o.exception = j.contains("expectException");
}

static void from_json(const json::json& j, StateTransitionTest& o)
{
    const auto& j_t = j.begin().value();  // Content is in a dict with the test name.

    o.pre_state = from_json<state::State>(j_t.at("pre"));

    o.multi_tx = j_t.at("transaction").get<TestMultiTransaction>();

    o.block = from_json<state::BlockInfo>(j_t.at("env"));

    if (const auto& info = j_t.at("_info"); info.contains("labels"))
    {
        for (const auto& [j_id, j_label] : info.at("labels").items())
            o.input_labels.emplace(from_json<uint64_t>(j_id), j_label);
    }

    for (const auto& [rev_name, expectations] : j_t.at("post").items())
    {
        // TODO(c++20): Use emplace_back with aggregate initialization.
        o.cases.push_back({to_rev(rev_name),
            expectations.get<std::vector<StateTransitionTest::Case::Expectation>>()});
    }
}

StateTransitionTest load_state_test(const fs::path& test_file)
{
    return json::json::parse(std::ifstream{test_file}).get<StateTransitionTest>();
}
}  // namespace evmone::test
