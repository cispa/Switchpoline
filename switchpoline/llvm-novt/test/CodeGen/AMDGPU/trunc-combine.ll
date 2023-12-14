; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc < %s -march=amdgcn -mcpu=tahiti -verify-machineinstrs | FileCheck %s -enable-var-scope -check-prefixes=GCN,SI
; RUN: llc < %s -march=amdgcn -mcpu=fiji -verify-machineinstrs | FileCheck %s -enable-var-scope -check-prefixes=GCN,VI

; Make sure high constant 0 isn't pointlessly materialized
define i16 @trunc_bitcast_i64_lshr_32_i16(i64 %bar) {
; GCN-LABEL: trunc_bitcast_i64_lshr_32_i16:
; GCN:       ; %bb.0:
; GCN-NEXT:    s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
; GCN-NEXT:    v_mov_b32_e32 v0, v1
; GCN-NEXT:    s_setpc_b64 s[30:31]
  %srl = lshr i64 %bar, 32
  %trunc = trunc i64 %srl to i16
  ret i16 %trunc
}

define i32 @trunc_bitcast_i64_lshr_32_i32(i64 %bar) {
; GCN-LABEL: trunc_bitcast_i64_lshr_32_i32:
; GCN:       ; %bb.0:
; GCN-NEXT:    s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
; GCN-NEXT:    v_mov_b32_e32 v0, v1
; GCN-NEXT:    s_setpc_b64 s[30:31]
  %srl = lshr i64 %bar, 32
  %trunc = trunc i64 %srl to i32
  ret i32 %trunc
}

define i16 @trunc_bitcast_v2i32_to_i16(<2 x i32> %bar) {
; SI-LABEL: trunc_bitcast_v2i32_to_i16:
; SI:       ; %bb.0:
; SI-NEXT:    s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
; SI-NEXT:    s_mov_b32 s7, 0xf000
; SI-NEXT:    s_mov_b32 s6, -1
; SI-NEXT:    buffer_load_dword v0, off, s[4:7], 0
; SI-NEXT:    s_waitcnt vmcnt(0)
; SI-NEXT:    v_add_i32_e32 v0, vcc, 4, v0
; SI-NEXT:    s_setpc_b64 s[30:31]
;
; VI-LABEL: trunc_bitcast_v2i32_to_i16:
; VI:       ; %bb.0:
; VI-NEXT:    s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
; VI-NEXT:    flat_load_dword v0, v[0:1]
; VI-NEXT:    s_waitcnt vmcnt(0) lgkmcnt(0)
; VI-NEXT:    v_add_u16_e32 v0, 4, v0
; VI-NEXT:    s_setpc_b64 s[30:31]
  %load0 = load i32, i32 addrspace(1)* undef
  %load1 = load i32, i32 addrspace(1)* null
  %insert.0 = insertelement <2 x i32> undef, i32 %load0, i32 0
  %insert.1 = insertelement <2 x i32> %insert.0, i32 99, i32 1
  %bc = bitcast <2 x i32> %insert.1 to i64
  %trunc = trunc i64 %bc to i16
  %add = add i16 %trunc, 4
  ret i16 %add
}

; Make sure there's no crash if the source vector type is FP
define i16 @trunc_bitcast_v2f32_to_i16(<2 x float> %bar) {
; SI-LABEL: trunc_bitcast_v2f32_to_i16:
; SI:       ; %bb.0:
; SI-NEXT:    s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
; SI-NEXT:    s_mov_b32 s7, 0xf000
; SI-NEXT:    s_mov_b32 s6, -1
; SI-NEXT:    buffer_load_dword v0, off, s[4:7], 0
; SI-NEXT:    s_waitcnt vmcnt(0)
; SI-NEXT:    v_add_i32_e32 v0, vcc, 4, v0
; SI-NEXT:    s_setpc_b64 s[30:31]
;
; VI-LABEL: trunc_bitcast_v2f32_to_i16:
; VI:       ; %bb.0:
; VI-NEXT:    s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
; VI-NEXT:    flat_load_dword v0, v[0:1]
; VI-NEXT:    s_waitcnt vmcnt(0) lgkmcnt(0)
; VI-NEXT:    v_add_u16_e32 v0, 4, v0
; VI-NEXT:    s_setpc_b64 s[30:31]
  %load0 = load float, float addrspace(1)* undef
  %load1 = load float, float addrspace(1)* null
  %insert.0 = insertelement <2 x float> undef, float %load0, i32 0
  %insert.1 = insertelement <2 x float> %insert.0, float 4.0, i32 1
  %bc = bitcast <2 x float> %insert.1 to i64
  %trunc = trunc i64 %bc to i16
  %add = add i16 %trunc, 4
  ret i16 %add
}

define amdgpu_kernel void @truncate_high_elt_extract_vector(<2 x i16> addrspace(1)* nocapture readonly %arg, <2 x i16> addrspace(1)* nocapture readonly %arg1, <2 x i16> addrspace(1)* nocapture %arg2) local_unnamed_addr {
; SI-LABEL: truncate_high_elt_extract_vector:
; SI:       ; %bb.0: ; %bb
; SI-NEXT:    s_load_dwordx4 s[4:7], s[0:1], 0x9
; SI-NEXT:    s_load_dwordx2 s[0:1], s[0:1], 0xd
; SI-NEXT:    s_mov_b32 s3, 0xf000
; SI-NEXT:    s_mov_b32 s2, -1
; SI-NEXT:    s_waitcnt lgkmcnt(0)
; SI-NEXT:    s_load_dword s4, s[4:5], 0x0
; SI-NEXT:    s_load_dword s5, s[6:7], 0x0
; SI-NEXT:    s_waitcnt lgkmcnt(0)
; SI-NEXT:    s_sext_i32_i16 s4, s4
; SI-NEXT:    s_sext_i32_i16 s5, s5
; SI-NEXT:    v_mov_b32_e32 v0, s4
; SI-NEXT:    v_mul_i32_i24_e32 v0, s5, v0
; SI-NEXT:    v_lshrrev_b32_e32 v0, 16, v0
; SI-NEXT:    buffer_store_dword v0, off, s[0:3], 0
; SI-NEXT:    s_endpgm
;
; VI-LABEL: truncate_high_elt_extract_vector:
; VI:       ; %bb.0: ; %bb
; VI-NEXT:    s_load_dwordx4 s[4:7], s[0:1], 0x24
; VI-NEXT:    s_load_dwordx2 s[0:1], s[0:1], 0x34
; VI-NEXT:    s_waitcnt lgkmcnt(0)
; VI-NEXT:    v_mov_b32_e32 v0, s0
; VI-NEXT:    v_mov_b32_e32 v1, s1
; VI-NEXT:    s_load_dword s0, s[4:5], 0x0
; VI-NEXT:    s_load_dword s1, s[6:7], 0x0
; VI-NEXT:    s_waitcnt lgkmcnt(0)
; VI-NEXT:    s_sext_i32_i16 s0, s0
; VI-NEXT:    s_sext_i32_i16 s1, s1
; VI-NEXT:    v_mov_b32_e32 v2, s0
; VI-NEXT:    v_mul_i32_i24_e32 v2, s1, v2
; VI-NEXT:    v_lshrrev_b32_e32 v2, 16, v2
; VI-NEXT:    flat_store_dword v[0:1], v2
; VI-NEXT:    s_endpgm
bb:
  %tmp = getelementptr inbounds <2 x i16>, <2 x i16> addrspace(1)* %arg, i64 undef
  %tmp3 = load <2 x i16>, <2 x i16> addrspace(1)* %tmp, align 4
  %tmp4 = getelementptr inbounds <2 x i16>, <2 x i16> addrspace(1)* %arg1, i64 undef
  %tmp5 = load <2 x i16>, <2 x i16> addrspace(1)* %tmp4, align 4
  %tmp6 = sext <2 x i16> %tmp3 to <2 x i32>
  %tmp7 = sext <2 x i16> %tmp5 to <2 x i32>
  %tmp8 = extractelement <2 x i32> %tmp6, i64 0
  %tmp9 = extractelement <2 x i32> %tmp7, i64 0
  %tmp10 = mul nsw i32 %tmp9, %tmp8
  %tmp11 = insertelement <2 x i32> undef, i32 %tmp10, i32 0
  %tmp12 = insertelement <2 x i32> %tmp11, i32 undef, i32 1
  %tmp13 = lshr <2 x i32> %tmp12, <i32 16, i32 16>
  %tmp14 = trunc <2 x i32> %tmp13 to <2 x i16>
  %tmp15 = getelementptr inbounds <2 x i16>, <2 x i16> addrspace(1)* %arg2, i64 undef
  store <2 x i16> %tmp14, <2 x i16> addrspace(1)* %tmp15, align 4
  ret void
}

define <2 x i16> @trunc_v2i64_arg_to_v2i16(<2 x i64> %arg0) #0 {
; SI-LABEL: trunc_v2i64_arg_to_v2i16:
; SI:       ; %bb.0:
; SI-NEXT:    s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
; SI-NEXT:    v_lshlrev_b32_e32 v1, 16, v2
; SI-NEXT:    v_and_b32_e32 v0, 0xffff, v0
; SI-NEXT:    v_or_b32_e32 v0, v0, v1
; SI-NEXT:    v_lshrrev_b32_e32 v1, 16, v0
; SI-NEXT:    s_setpc_b64 s[30:31]
;
; VI-LABEL: trunc_v2i64_arg_to_v2i16:
; VI:       ; %bb.0:
; VI-NEXT:    s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
; VI-NEXT:    v_lshlrev_b32_e32 v1, 16, v2
; VI-NEXT:    v_or_b32_sdwa v0, v0, v1 dst_sel:DWORD dst_unused:UNUSED_PAD src0_sel:WORD_0 src1_sel:DWORD
; VI-NEXT:    s_setpc_b64 s[30:31]
  %trunc = trunc <2 x i64> %arg0 to <2 x i16>
  ret <2 x i16> %trunc
}
