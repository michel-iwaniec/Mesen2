#pragma once
#include "stdafx.h"
#include "NES/BaseMapper.h"

class UnlPuzzle : public BaseMapper
{
protected:
	uint16_t GetPrgPageSize() override { return 0x8000; }
	uint16_t GetChrPageSize() override { return 0x2000; }
	uint16_t RegisterStartAddress() override { return 0x4100; }
	uint16_t RegisterEndAddress() override { return 0xFFFF; }

	void InitMapper() override
	{
		SelectPrgPage(0, 0);
		SelectChrPage(0, 0);
	}

	void WriteRegister(uint16_t addr, uint8_t value) override
	{
		SelectPrgPage(0, (value >> 3) & 0x01);
		SelectChrPage(0, value & 0x07);
	}
};