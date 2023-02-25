// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2021 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "evm_fixture.hpp"
#include "evmone/eof.hpp"

using evmone::test::evm;

TEST_P(evm, eof1_execution)
{
    const auto code = eof1_bytecode(OP_STOP);

    rev = EVMC_SHANGHAI;
    execute(code);
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);

    rev = EVMC_CANCUN;
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
}

TEST_P(evm, eof1_execution_with_data_section)
{
    rev = EVMC_CANCUN;
    // data section contains ret(0, 1)
    const auto code = eof1_bytecode(mstore8(0, 1) + OP_STOP, ret(0, 1));

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(result.output_size, 0);
}

TEST_P(evm, eof1_pc)
{
    rev = EVMC_CANCUN;
    auto code = eof1_bytecode(OP_PC + mstore8(0) + ret(0, 1));

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 0);

    code = eof1_bytecode(4 * bytecode{OP_JUMPDEST} + OP_PC + mstore8(0) + ret(0, 1));

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 4);
}

TEST_P(evm, eof1_jump_inside_code_section)
{
    rev = EVMC_CANCUN;
    auto code = eof1_bytecode(jump(4) + OP_INVALID + OP_JUMPDEST + mstore8(0, 1) + ret(0, 1));

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 1);

    code =
        eof1_bytecode(jump(4) + OP_INVALID + OP_JUMPDEST + mstore8(0, 1) + ret(0, 1), "deadbeef");

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 1);
}

TEST_P(evm, eof1_jumpi_inside_code_section)
{
    rev = EVMC_CANCUN;
    auto code = eof1_bytecode(jumpi(6, 1) + OP_INVALID + OP_JUMPDEST + mstore8(0, 1) + ret(0, 1));

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 1);

    code = eof1_bytecode(
        jumpi(6, 1) + OP_INVALID + OP_JUMPDEST + mstore8(0, 1) + ret(0, 1), "deadbeef");

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 1);
}

TEST_P(evm, eof1_jump_into_data_section)
{
    rev = EVMC_CANCUN;
    // data section contains OP_JUMPDEST + mstore8(0, 1) + ret(0, 1)
    const auto code = eof1_bytecode(jump(4) + OP_STOP, OP_JUMPDEST + mstore8(0, 1) + ret(0, 1));

    execute(code);
    EXPECT_STATUS(EVMC_BAD_JUMP_DESTINATION);
}

TEST_P(evm, eof1_jumpi_into_data_section)
{
    rev = EVMC_CANCUN;
    // data section contains OP_JUMPDEST + mstore8(0, 1) + ret(0, 1)
    const auto code = eof1_bytecode(jumpi(6, 1) + OP_STOP, OP_JUMPDEST + mstore8(0, 1) + ret(0, 1));

    execute(code);
    EXPECT_STATUS(EVMC_BAD_JUMP_DESTINATION);
}

TEST_P(evm, eof1_push_byte_in_header)
{
    rev = EVMC_CANCUN;
    // data section is 0x65 bytes long, so header contains 0x65 (PUSH6) byte,
    // but it must not affect jumpdest analysis (OP_JUMPDEST stays valid)
    auto code = eof1_bytecode(
        jump(4) + OP_INVALID + OP_JUMPDEST + mstore8(0, 1) + ret(0, 1), bytes(0x65, '\0'));

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 1);
}

TEST_P(evm, eof1_codesize)
{
    rev = EVMC_CANCUN;
    auto code = eof1_bytecode(mstore8(0, OP_CODESIZE) + ret(0, 1));

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 16);

    code = eof1_bytecode(mstore8(0, OP_CODESIZE) + ret(0, 1), "deadbeef");

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 23);
}

TEST_P(evm, eof1_codecopy_full)
{
    rev = EVMC_CANCUN;
    auto code = eof1_bytecode(bytecode{19} + 0 + 0 + OP_CODECOPY + ret(0, 19));

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "ef000101000c006013600060003960136000f3"_hex);

    code = eof1_bytecode(bytecode{26} + 0 + 0 + OP_CODECOPY + ret(0, 26), "deadbeef");

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "ef000101000c02000400601a6000600039601a6000f3deadbeef"_hex);
}

TEST_P(evm, eof1_codecopy_header)
{
    rev = EVMC_CANCUN;
    auto code = eof1_bytecode(bytecode{7} + 0 + 0 + OP_CODECOPY + ret(0, 7));

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size), "ef000101000c00"_hex);

    code = eof1_bytecode(bytecode{10} + 0 + 0 + OP_CODECOPY + ret(0, 10), "deadbeef");

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size), "ef000101000c02000400"_hex);
}

TEST_P(evm, eof1_codecopy_code)
{
    rev = EVMC_CANCUN;
    auto code = eof1_bytecode(bytecode{12} + 7 + 0 + OP_CODECOPY + ret(0, 12));

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size), "600c6007600039600c6000f3"_hex);

    code = eof1_bytecode(bytecode{12} + 10 + 0 + OP_CODECOPY + ret(0, 12), "deadbeef");

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size), "600c600a600039600c6000f3"_hex);
}

TEST_P(evm, eof1_codecopy_data)
{
    rev = EVMC_CANCUN;

    const auto code = eof1_bytecode(bytecode{4} + 22 + 0 + OP_CODECOPY + ret(0, 4), "deadbeef");

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size), "deadbeef"_hex);
}

TEST_P(evm, eof1_codecopy_out_of_bounds)
{
    // 4 bytes out of container bounds - result is implicitly 0-padded
    rev = EVMC_CANCUN;
    auto code = eof1_bytecode(bytecode{23} + 0 + 0 + OP_CODECOPY + ret(0, 23));

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "ef000101000c006017600060003960176000f300000000"_hex);

    code = eof1_bytecode(bytecode{30} + 0 + 0 + OP_CODECOPY + ret(0, 30), "deadbeef");

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "ef000101000c02000400601e6000600039601e6000f3deadbeef00000000"_hex);
}

TEST_P(evm, eof2_rjump)
{
    // Relative jumps are not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_SHANGHAI;
    auto code = eof1_bytecode(rjump(1) + OP_INVALID + mstore8(0, 1) + ret(0, 1));

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 1);

    code = eof1_bytecode(rjump(1) + OP_INVALID + mstore8(0, 1) + ret(0, 1), "deadbeef");

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 1);
}

TEST_P(evm, eof2_rjump_backward)
{
    // Relative jumps are not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_SHANGHAI;
    auto code =
        eof1_bytecode(rjump(11) + OP_INVALID + mstore8(0, 1) + ret(0, 1) + rjump(-13) + OP_STOP);

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 1);

    code = eof1_bytecode(
        rjump(11) + OP_INVALID + mstore8(0, 1) + ret(0, 1) + rjump(-13) + OP_STOP, "deadbeef");

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 1);
}

TEST_P(evm, eof2_rjump_0_offset)
{
    // Relative jumps are not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_SHANGHAI;
    auto code = eof1_bytecode(rjump(0) + mstore8(0, 1) + ret(0, 1));

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 1);
}

TEST_P(evm, eof2_rjumpi)
{
    // Relative jumps are not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_SHANGHAI;
    auto code = eof1_bytecode(
        rjumpi(10, calldataload(0)) + mstore8(0, 2) + ret(0, 1) + mstore8(0, 1) + ret(0, 1));

    // RJUMPI condition is true
    execute(code, "01"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 1);

    // RJUMPI condition is false
    execute(code, "00"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 2);
}

TEST_P(evm, eof2_rjumpi_backwards)
{
    // Relative jumps are not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_SHANGHAI;
    auto code = eof1_bytecode(rjump(11) + OP_INVALID + mstore8(0, 1) + ret(0, 1) +
                              rjumpi(-16, calldataload(0)) + mstore8(0, 2) + ret(0, 1));

    // RJUMPI condition is true
    execute(code, "01"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 1);

    // RJUMPI condition is false
    execute(code, "00"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 2);
}

TEST_P(evm, eof2_rjumpi_0_offset)
{
    // Relative jumps are not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_SHANGHAI;
    auto code = eof1_bytecode(rjumpi(0, calldataload(0)) + mstore8(0, 1) + ret(0, 1));

    // RJUMPI condition is true
    execute(code, "01"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 1);

    // RJUMPI condition is false
    execute(code, "00"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 1);
}


TEST_P(evm, relative_jumps_undefined_in_legacy)
{
    rev = EVMC_SHANGHAI;
    auto code = rjump(1) + OP_INVALID + mstore8(0, 1) + ret(0, 1);

    execute(code);
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);

    code = rjumpi(10, 1) + mstore8(0, 2) + ret(0, 1) + mstore8(0, 1) + ret(0, 1);

    execute(code);
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, eof_function_example1)
{
    // Relative jumps are not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_SHANGHAI;
    const auto code =
        "EF00 01 010008 020002 000f 0002 00"
        "00000005 02010000"
        "6001 6008 b00001 " +
        ret_top() + "03b1";

    ASSERT_EQ((int)evmone::validate_eof(rev, code), (int)evmone::EOFValidationError{});

    execute(code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 32);
    EXPECT_OUTPUT_INT(7);
}

TEST_P(evm, eof_function_example2)
{
    // Relative jumps are not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_SHANGHAI;
    const auto code =
        "ef0001 01000c 020003 003b 0017 001d 00 00000400 01010400 01010400"
        "60043560003560e01c63c766526781145d001c63c6c2ea1781145d00065050600080fd50b00002600052602060"
        "00f350b0000160005260206000f3"
        "600181115d0004506001b160018103b0000181029050b1"
        "600281115d0004506001b160028103b0000260018203b00002019050b1"_hex;

    ASSERT_EQ((int)evmone::validate_eof(rev, code), (int)evmone::EOFValidationError{});

    // Call fac(5)
    const auto calldata_fac =
        "c76652670000000000000000000000000000000000000000000000000000000000000005"_hex;
    execute(bytecode{code}, calldata_fac);
    EXPECT_GAS_USED(EVMC_SUCCESS, 246);
    EXPECT_EQ(output, "0000000000000000000000000000000000000000000000000000000000000078"_hex);

    // Call fib(15)
    const auto calldata_fib =
        "c6c2ea17000000000000000000000000000000000000000000000000000000000000000f"_hex;
    execute(bytecode{code}, calldata_fib);
    EXPECT_GAS_USED(EVMC_SUCCESS, 44544);
    EXPECT_EQ(output, "0000000000000000000000000000000000000000000000000000000000000262"_hex);
}
