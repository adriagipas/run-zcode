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
    INSTRUCTION_NAME_ART_SHIFT,
    INSTRUCTION_NAME_BUFFER_MODE,
    INSTRUCTION_NAME_CALL,
    INSTRUCTION_NAME_CATCH,
    INSTRUCTION_NAME_CHECK_ARG_COUNT,
    INSTRUCTION_NAME_CLEAR_ATTR,
    INSTRUCTION_NAME_COPY_TABLE,
    INSTRUCTION_NAME_DEC,
    INSTRUCTION_NAME_DEC_CHK,
    INSTRUCTION_NAME_DIV,
    INSTRUCTION_NAME_ERASE_WINDOW,
    INSTRUCTION_NAME_GET_PARENT,
    INSTRUCTION_NAME_GET_PROP,
    INSTRUCTION_NAME_GET_PROP_ADDR,
    INSTRUCTION_NAME_GET_PROP_LEN,
    INSTRUCTION_NAME_INC,
    INSTRUCTION_NAME_INC_CHK,
    INSTRUCTION_NAME_JE,
    INSTRUCTION_NAME_JG,
    INSTRUCTION_NAME_JIN,
    INSTRUCTION_NAME_JL,
    INSTRUCTION_NAME_JUMP,
    INSTRUCTION_NAME_JZ,
    INSTRUCTION_NAME_LOAD,
    INSTRUCTION_NAME_LOADB,
    INSTRUCTION_NAME_LOADW,
    INSTRUCTION_NAME_LOG_SHIFT,
    INSTRUCTION_NAME_MOD,
    INSTRUCTION_NAME_MUL,
    INSTRUCTION_NAME_NEW_LINE,
    INSTRUCTION_NAME_NOT,
    INSTRUCTION_NAME_OR,
    INSTRUCTION_NAME_OUTPUT_STREAM,
    INSTRUCTION_NAME_PRINT,
    INSTRUCTION_NAME_PRINT_ADDR,
    INSTRUCTION_NAME_PRINT_CHAR,
    INSTRUCTION_NAME_PRINT_NUM,
    INSTRUCTION_NAME_PRINT_OBJ,
    INSTRUCTION_NAME_PRINT_PADDR,
    INSTRUCTION_NAME_PRINT_RET,
    INSTRUCTION_NAME_PRINT_TABLE,
    INSTRUCTION_NAME_PULL,
    INSTRUCTION_NAME_PUSH,
    INSTRUCTION_NAME_PUT_PROP,
    INSTRUCTION_NAME_QUIT,
    INSTRUCTION_NAME_READ,
    INSTRUCTION_NAME_READ_CHAR,
    INSTRUCTION_NAME_RESTORE_UNDO,
    INSTRUCTION_NAME_RET,
    INSTRUCTION_NAME_RET_POPPED,
    INSTRUCTION_NAME_RFALSE,
    INSTRUCTION_NAME_RTRUE,
    INSTRUCTION_NAME_SAVE_UNDO,
    INSTRUCTION_NAME_SCAN_TABLE,
    INSTRUCTION_NAME_SET_ATTR,
    INSTRUCTION_NAME_SET_COLOUR,
    INSTRUCTION_NAME_SET_CURSOR,
    INSTRUCTION_NAME_SET_TEXT_STYLE,
    INSTRUCTION_NAME_SET_TRUE_COLOUR,
    INSTRUCTION_NAME_SET_WINDOW,
    INSTRUCTION_NAME_SPLIT_WINDOW,
    INSTRUCTION_NAME_STORE,
    INSTRUCTION_NAME_STOREB,
    INSTRUCTION_NAME_STOREW,
    INSTRUCTION_NAME_SUB,
    INSTRUCTION_NAME_TEST_ATTR,
    INSTRUCTION_NAME_THROW
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
    INSTRUCTION_OP_TYPE_REF_TOP_STACK,
    INSTRUCTION_OP_TYPE_REF_LOCAL_VARIABLE,
    INSTRUCTION_OP_TYPE_REF_GLOBAL_VARIABLE,
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
