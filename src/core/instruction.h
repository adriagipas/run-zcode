/*
 * Copyright 2023 Adrià Giménez Pastor.
 *
 * This file is part of adriagipas/run-zcode.
 *
 * adriagipas/run-zcode is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * adriagipas/run-zcode is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with adriagipas/run-zcode.  If not, see
 * <https://www.gnu.org/licenses/>.
 */
/*
 *  instruction.h - Definició del tipus Instruction.
 *
 */

#ifndef __CORE__INSTRUCTION_H__
#define __CORE__INSTRUCTION_H__

typedef enum
  {
    INSTRUCTION_NAME_UNK= 0,
    INSTRUCTION_NAME_ADD,
    INSTRUCTION_NAME_AND,
    INSTRUCTION_NAME_CALL,
    INSTRUCTION_NAME_CHECK_ARG_COUNT,
    INSTRUCTION_NAME_JE,
    INSTRUCTION_NAME_JG,
    INSTRUCTION_NAME_JL,
    INSTRUCTION_NAME_JZ,
    INSTRUCTION_NAME_SUB
  } InstructionName;

typedef enum
  {
    INSTRUCTION_OP_TYPE_NONE=0,
    INSTRUCTION_OP_TYPE_TOP_STACK,
    INSTRUCTION_OP_TYPE_LOCAL_VARIABLE,
    INSTRUCTION_OP_TYPE_GLOBAL_VARIABLE,
    INSTRUCTION_OP_TYPE_LARGE_CONSTANT,
    INSTRUCTION_OP_TYPE_SMALL_CONSTANT,
    INSTRUCTION_OP_TYPE_ROUTINE,
    INSTRUCTION_OP_TYPE_BRANCH_IF_TRUE,
    INSTRUCTION_OP_TYPE_BRANCH_IF_FALSE,
    INSTRUCTION_OP_TYPE_RETURN_TRUE_IF_TRUE,
    INSTRUCTION_OP_TYPE_RETURN_TRUE_IF_FALSE,
    INSTRUCTION_OP_TYPE_RETURN_FALSE_IF_TRUE,
    INSTRUCTION_OP_TYPE_RETURN_FALSE_IF_FALSE
  } InstructionOpType;

typedef struct
{
  InstructionOpType type;
  int               ind; // Local Variable/Global Variable
  uint8_t           u8;  // Small constant
  uint16_t          u16; // Large constant/Routine packed
  uint32_t          u32; // Routine unpacked/Branch offset (amb el -2 inclòs)
} InstructionOp;

typedef struct
{
  uint32_t         addr;
  InstructionName  name;
  uint8_t         *bytes;
  size_t           nbytes;
  size_t           bytes_size;
  InstructionOp    ops[8];
  int              nops;
  bool             store;
  InstructionOp    store_op;
  bool             branch;
  InstructionOp    branch_op;
  // branch offset
  
} Instruction;

#endif // __CORE__INSTRUCTION_H__
