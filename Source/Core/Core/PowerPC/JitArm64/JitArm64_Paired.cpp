// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Arm64Emitter.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"
#include "Core/PowerPC/JitArm64/JitAsm.h"

using namespace Arm64Gen;

void JitArm64::ps_mergeXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_REG);
	ARM64Reg VB = fpr.R(b, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);

	switch (inst.SUBOP10)
	{
	case 528: //00
		m_float_emit.TRN1(64, VD, VA, VB);
		break;
	case 560: //01
		m_float_emit.INS(64, VD, 0, VA, 0);
		m_float_emit.INS(64, VD, 1, VB, 1);
		break;
	case 592: //10
		if (d != a && d != b)
		{
			m_float_emit.INS(64, VD, 0, VA, 1);
			m_float_emit.INS(64, VD, 1, VB, 0);
		}
		else
		{
			ARM64Reg V0 = fpr.GetReg();
			m_float_emit.INS(64, V0, 0, VA, 1);
			m_float_emit.INS(64, V0, 1, VB, 0);
			m_float_emit.ORR(VD, V0, V0);
			fpr.Unlock(V0);
		}
		break;
	case 624: //11
		m_float_emit.TRN2(64, VD, VA, VB);
		break;
	default:
		_assert_msg_(DYNA_REC, 0, "ps_merge - invalid op");
		break;
	}
}

void JitArm64::ps_mulsX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);
	FALLBACK_IF(SConfig::GetInstance().bFPRF && js.op->wantsFPRF);

	u32 a = inst.FA, c = inst.FC, d = inst.FD;

	bool upper = inst.SUBOP5 == 13;

	ARM64Reg VA = fpr.R(a, REG_REG);
	ARM64Reg VC = fpr.R(c, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.DUP(64, V0, VC, upper ? 1 : 0);
	m_float_emit.FMUL(64, VD, VA, V0);
	fpr.FixSinglePrecision(d);
	fpr.Unlock(V0);
}

void JitArm64::ps_maddXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);
	FALLBACK_IF(SConfig::GetInstance().bFPRF && js.op->wantsFPRF);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;
	u32 op5 = inst.SUBOP5;

	ARM64Reg VA = fpr.R(a, REG_REG);
	ARM64Reg VB = fpr.R(b, REG_REG);
	ARM64Reg VC = fpr.R(c, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);
	ARM64Reg V0 = fpr.GetReg();

	switch (op5)
	{
	case 14: // ps_madds0
		m_float_emit.DUP(64, V0, VC, 0);
		m_float_emit.FMUL(64, V0, V0, VA);
		m_float_emit.FADD(64, VD, V0, VB);
		break;
	case 15: // ps_madds1
		m_float_emit.DUP(64, V0, VC, 1);
		m_float_emit.FMUL(64, V0, V0, VA);
		m_float_emit.FADD(64, VD, V0, VB);
		break;
	case 28: // ps_msub
		m_float_emit.FMUL(64, V0, VA, VC);
		m_float_emit.FSUB(64, VD, V0, VB);
		break;
	case 29: // ps_madd
		m_float_emit.FMUL(64, V0, VA, VC);
		m_float_emit.FADD(64, VD, V0, VB);
		break;
	case 30: // ps_nmsub
		m_float_emit.FMUL(64, V0, VA, VC);
		m_float_emit.FSUB(64, VD, V0, VB);
		m_float_emit.FNEG(64, VD, VD);
		break;
	case 31: // ps_nmadd
		m_float_emit.FMUL(64, V0, VA, VC);
		m_float_emit.FADD(64, VD, V0, VB);
		m_float_emit.FNEG(64, VD, VD);
		break;
	default:
		_assert_msg_(DYNA_REC, 0, "ps_madd - invalid op");
		break;
	}
	fpr.FixSinglePrecision(d);

	fpr.Unlock(V0);
}

void JitArm64::ps_res(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);
	FALLBACK_IF(SConfig::GetInstance().bFPRF && js.op->wantsFPRF);

	u32 b = inst.FB, d = inst.FD;

	ARM64Reg VB = fpr.R(b, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);

	m_float_emit.FRSQRTE(64, VD, VB);
	fpr.FixSinglePrecision(d);
}

void JitArm64::ps_sel(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	ARM64Reg VA = fpr.R(a, REG_REG);
	ARM64Reg VB = fpr.R(b, REG_REG);
	ARM64Reg VC = fpr.R(c, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);

	if (d != a && d != b && d != c)
	{
		m_float_emit.FCMGE(64, VD, VA);
		m_float_emit.BSL(VD, VC, VB);
	}
	else
	{
		ARM64Reg V0 = fpr.GetReg();
		m_float_emit.FCMGE(64, V0, VA);
		m_float_emit.BSL(V0, VC, VB);
		m_float_emit.ORR(VD, V0, V0);
		fpr.Unlock(V0);
	}
}

void JitArm64::ps_sumX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);
	FALLBACK_IF(SConfig::GetInstance().bFPRF && js.op->wantsFPRF);

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	bool upper = inst.SUBOP5 == 11;

	ARM64Reg VA = fpr.R(a, REG_REG);
	ARM64Reg VB = fpr.R(b, REG_REG);
	ARM64Reg VC = fpr.R(c, REG_REG);
	ARM64Reg VD = fpr.RW(d, REG_REG);
	ARM64Reg V0 = fpr.GetReg();

	m_float_emit.DUP(64, V0, upper ? VA : VB, upper ? 0 : 1);
	if (d != c)
	{
		m_float_emit.FADD(64, VD, V0, upper ? VB : VA);
		m_float_emit.INS(64, VD, upper ? 0 : 1, VC, upper ? 0 : 1);
	}
	else
	{
		m_float_emit.FADD(64, V0, V0, upper ? VB : VA);
		m_float_emit.INS(64, VD, upper ? 1 : 0, V0, upper ? 1 : 0);
	}
	fpr.FixSinglePrecision(d);

	fpr.Unlock(V0);
}
