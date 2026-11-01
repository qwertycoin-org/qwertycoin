// Copyright 2018 The Beam Team
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// Modified by the Qwertycoin developers, 2026.


#pragma once

#define BVMOp_Memcpy(macro, sep) \
	macro(void*, pDst) sep \
	macro(const void*, pSrc) sep \
	macro(uint32_t, size)

#define BVMOp_Memset(macro, sep) \
	macro(void*, pDst) sep \
	macro(uint8_t, val) sep \
	macro(uint32_t, size)

#define BVMOp_Memcmp(macro, sep) \
	macro(const void*, p1) sep \
	macro(const void*, p2) sep \
	macro(uint32_t, size)

#define BVMOp_Memis0(macro, sep) \
	macro(const void*, p) sep \
	macro(uint32_t, size)

#define BVMOp_Strlen(macro, sep) \
	macro(const char*, sz)

#define BVMOp_Strcmp(macro, sep) \
	macro(const char*, sz1) sep \
	macro(const char*, sz2)

#define BVMOp_StackAlloc(macro, sep) \
	macro(uint32_t, size)

#define BVMOp_StackFree(macro, sep) \
	macro(uint32_t, size)

#define BVMOp_Heap_Alloc(macro, sep) \
	macro(uint32_t, size)

#define BVMOp_Heap_Free(macro, sep) \
	macro(void*, pPtr)

#define BVMOp_LoadVar(macro, sep) \
	macro(const void*, pKey) sep \
	macro(uint32_t, nKey) sep \
	macro(void*, pVal) sep \
	macro(uint32_t, nVal) sep \
	macro(uint8_t, nType)

#define BVMOp_SaveVar(macro, sep) \
	macro(const void*, pKey) sep \
	macro(uint32_t, nKey) sep \
	macro(const void*, pVal) sep \
	macro(uint32_t, nVal) sep \
	macro(uint8_t, nType)

#define BVMOp_EmitLog(macro, sep) \
	macro(const void*, pKey) sep \
	macro(uint32_t, nKey) sep \
	macro(const void*, pVal) sep \
	macro(uint32_t, nVal) sep \
	macro(uint8_t, nType)

#define BVMOp_CallFar(macro, sep) \
	macro(const ContractID&, cid) sep \
	macro(uint32_t, iMethod) sep \
	macro(void*, pArgs) sep \
	macro(uint32_t, nArgs) sep \
	macro(uint32_t, nFlags)

#define BVMOp_get_CallDepth(macro, sep)

#define BVMOp_get_CallerCid(macro, sep) \
	macro(uint32_t, iCaller) sep \
	macro(ContractID&, cid)

#define BVMOp_Halt(macro, sep)

#define BVMOp_FundsLock(macro, sep) \
	macro(Amount, amount)

#define BVMOp_FundsUnlock(macro, sep) \
	macro(Amount, amount)

#define BVMOp_AddSig(macro, sep) \
	macro(const PubKey&, pubKey)

#define BVMOp_HashWrite(macro, sep) \
	macro(HashObj*, pHash) sep \
	macro(const void*, p) sep \
	macro(uint32_t, size)

#define BVMOp_HashGetValue(macro, sep) \
	macro(HashObj*, pHash) sep \
	macro(void*, pDst) sep \
	macro(uint32_t, size)

#define BVMOp_HashFree(macro, sep) \
	macro(HashObj*, pHash)

#define BVMOp_HashClone(macro, sep) \
	macro(HashObj*, pHash)

#define BVMOp_get_Height(macro, sep)

#define BVMOp_get_ParentHdr(macro, sep) \
	macro(ParentHeader&, hdr)

#define BVMOp_get_HdrInfo(macro, sep) \
	macro(ParentHeader&, hdr)

#define BVMOp_HashCreateSha256(macro, sep)

#define BVMOp_HashCreateBlake2b(macro, sep) \
	macro(const void*, pPersonal) sep \
	macro(uint32_t, nPersonal) sep \
	macro(uint32_t, nResultSize)

#define BVMOp_HashCreateKeccak(macro, sep) \
	macro(uint32_t, nBits)

#define BVMOp_CheckSignature(macro, sep) \
	macro(const HashValue&, hash) sep \
	macro(const PubKey&, pubKey) sep \
	macro(const Signature&, sig)

#define BVMOpsAll_Common(macro) \
	macro(0x10, void*    , Memcpy) \
	macro(0x11, void*    , Memset) \
	macro(0x12, int32_t  , Memcmp) \
	macro(0x13, uint8_t  , Memis0) \
	macro(0x14, uint32_t , Strlen) \
	macro(0x15, int32_t  , Strcmp) \
	macro(0x18, void*    , StackAlloc) \
	macro(0x19, void     , StackFree) \
	macro(0x1A, void*    , Heap_Alloc) \
	macro(0x1B, void     , Heap_Free) \
	macro(0x20, uint32_t , LoadVar) \
	macro(0x21, uint32_t , SaveVar) \
	macro(0x28, void     , Halt) \
	macro(0x2B, void     , HashWrite) \
	macro(0x2D, void     , HashGetValue) \
	macro(0x2E, void     , HashFree) \
	macro(0x2F, HashObj* , HashClone) \
	macro(0x40, Height   , get_Height) \
	macro(0x45, void     , get_ParentHdr) \
	macro(0x46, uint8_t  , get_HdrInfo) \
	macro(0x48, HashObj* , HashCreateSha256) \
	macro(0x49, HashObj* , HashCreateBlake2b) \
	macro(0x4A, HashObj* , HashCreateKeccak) \
	macro(0x4B, uint8_t  , CheckSignature)

#define BVMOpsAll_Contract(macro) \
	macro(0x22, uint32_t , EmitLog) \
	macro(0x23, void     , CallFar) \
	macro(0x24, uint32_t , get_CallDepth) \
	macro(0x25, void     , get_CallerCid) \
	macro(0x29, void     , AddSig) \
	macro(0x30, void     , FundsLock) \
	macro(0x31, void     , FundsUnlock)

#define BVMOp_DocAddText(macro, sep) \
	macro(const char*, szId) sep \
	macro(const char*, szValue)

#define BVMOp_DocAddNum(macro, sep) \
	macro(const char*, szId) sep \
	macro(uint64_t, value)

#define BVMOp_DocAddBlob(macro, sep) \
	macro(const char*, szId) sep \
	macro(const void*, p) sep \
	macro(uint32_t, n)

#define BVMOp_DocOpenGroup(macro, sep) \
	macro(const char*, szId)

#define BVMOp_DocCloseGroup(macro, sep)

#define BVMOp_DocGetText(macro, sep) \
	macro(const char*, szId) sep \
	macro(char*, szOut) sep \
	macro(uint32_t, nOutMax)

#define BVMOp_DocGetNum(macro, sep) \
	macro(const char*, szId) sep \
	macro(uint64_t*, pOut)

#define BVMOp_VarRead(macro, sep) \
	macro(const void*, pKey) sep \
	macro(uint32_t, nKey) sep \
	macro(void*, pVal) sep \
	macro(uint32_t, nVal)

#define BVMOp_GenerateCall(macro, sep) \
	macro(const ContractID&, cid) sep \
	macro(uint32_t, iMethod) sep \
	macro(const void*, pArgs) sep \
	macro(uint32_t, nArgs) sep \
	macro(Amount, deposit)

#define BVMOp_GenerateDeploy(macro, sep) \
	macro(const void*, pCode) sep \
	macro(uint32_t, nCode) sep \
	macro(const void*, pArgs) sep \
	macro(uint32_t, nArgs) sep \
	macro(Amount, deposit)

#define BVMOp_GenerateCallSigned(macro, sep) \
	macro(const ContractID&, cid) sep \
	macro(uint32_t, iMethod) sep \
	macro(const void*, pArgs) sep \
	macro(uint32_t, nArgs) sep \
	macro(Amount, deposit) sep \
	macro(const PubKey*, pSigners) sep \
	macro(uint32_t, nSigners)

#define BVMOp_GenerateDeploySigned(macro, sep) \
	macro(const void*, pCode) sep \
	macro(uint32_t, nCode) sep \
	macro(const void*, pArgs) sep \
	macro(uint32_t, nArgs) sep \
	macro(Amount, deposit) sep \
	macro(const PubKey*, pSigners) sep \
	macro(uint32_t, nSigners)

#define BVMOp_DerivePk(macro, sep) \
	macro(const ContractID&, cid) sep \
	macro(const HashValue&, keyId) sep \
	macro(PubKey&, pubKey)

#define BVMOpsAll_App(macro) \
	macro(0x80, void     , DocAddText) \
	macro(0x81, void     , DocAddNum) \
	macro(0x82, void     , DocAddBlob) \
	macro(0x83, void     , DocOpenGroup) \
	macro(0x84, void     , DocCloseGroup) \
	macro(0x85, uint32_t , DocGetText) \
	macro(0x86, uint8_t  , DocGetNum) \
	macro(0x88, uint32_t , VarRead) \
	macro(0x89, void     , GenerateCall) \
	macro(0x8A, void     , GenerateDeploy) \
	macro(0x8B, void     , GenerateCallSigned) \
	macro(0x8C, void     , GenerateDeploySigned) \
	macro(0x8D, void     , DerivePk)
