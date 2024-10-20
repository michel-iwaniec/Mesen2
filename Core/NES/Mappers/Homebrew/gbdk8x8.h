#pragma once
#include "pch.h"
#include "NES/BaseMapper.h"
#include "NES/Mappers/Homebrew/FlashSST39SF040.h"
#include "Shared/BatteryManager.h"
#include "Utilities/Patches/IpsPatcher.h"
#include "Utilities/Serializer.h"

class GBDK8x8: public BaseMapper
{
private:
	unique_ptr<FlashSST39SF040> _flash;
	bool _enableMirroringBit = false;
	uint8_t _prgBank = 0;
	vector<uint8_t> _orgPrgRom;
	uint8_t _attribute_fine_x = 0;
	uint8_t _attribute_fine_y = 0;
	bool _irqEnabled = true;
	uint8_t a12_r = 0;
	uint8_t a12_r_old = 0;
	uint8_t chr_a12 = 0;
	uint8_t chr_a13 = 0;
	uint8_t a14 = 0;
	bool swap_spr_nt = false;
	bool a13_d, a13_dd, a13_ddd;
	bool at_read_next;

protected:
	uint16_t GetPrgPageSize() override { return 0x4000; }
	uint16_t GetChrPageSize() override { return 0x1000; }
	uint32_t GetSaveRamSize() override { return 0; }
	uint16_t RegisterStartAddress() override { return 0x8000; }
	uint16_t RegisterEndAddress() override { return 0xFFFF; }
	uint32_t GetChrRamSize() override { return 0x8000; }
	bool HasBusConflicts() override { return false; }
	bool AllowRegisterRead() override { return true; }
	bool EnableCustomVramRead() override { return true; }
	bool EnableVramAddressHook() override { return true; }

	void InitMapper() override
	{
		_flash.reset(new FlashSST39SF040(_prgRom, _prgSize));
		SelectPrgPage(0, 0);
		SelectPrgPage(1, -1);

		_enableMirroringBit = false;
		if(GetMirroringType() == MirroringType::ScreenAOnly || GetMirroringType() == MirroringType::ScreenBOnly) {
			SetMirroringType(MirroringType::ScreenAOnly);
			_enableMirroringBit = true;
		} else {
			switch(_romInfo.Header.Byte6 & 0x09) {
				case 0: SetMirroringType(MirroringType::Horizontal); break;
				case 1: SetMirroringType(MirroringType::Vertical); break;
				case 8: SetMirroringType(MirroringType::ScreenAOnly); _enableMirroringBit = true; break;
				case 9: SetMirroringType(MirroringType::FourScreens); break;
			}
		}

		if(true) {
			AddRegisterRange(0x8000, 0xFFFF, MemoryOperation::Read);
			_orgPrgRom = vector<uint8_t>(_prgRom, _prgRom + _prgSize);
			ApplySaveData();
		}
	}

	void Serialize(Serializer& s) override
	{
		BaseMapper::Serialize(s);

		SV(_flash);
		SV(_prgBank);

		if(s.IsSaving()) {
			vector<uint8_t> prgRom = vector<uint8_t>(_prgRom, _prgRom + _prgSize);
			vector<uint8_t> ipsData = IpsPatcher::CreatePatch(_orgPrgRom, prgRom);
			SVVector(ipsData);
		} else {
			vector<uint8_t> ipsData;
			SVVector(ipsData);

			vector<uint8_t> patchedPrgRom;
			if(IpsPatcher::PatchBuffer(ipsData, _orgPrgRom, patchedPrgRom)) {
				memcpy(_prgRom, patchedPrgRom.data(), _prgSize);
			}
		}
	}

	void ApplySaveData()
	{
		if(_console->GetNesConfig().DisableFlashSaves) {
			return;
		}

		//Apply save data (saved as an IPS file), if found
		vector<uint8_t> ipsData = _emu->GetBatteryManager()->LoadBattery(".ips");
		if(!ipsData.empty()) {
			vector<uint8_t> patchedPrgRom;
			if(IpsPatcher::PatchBuffer(ipsData, _orgPrgRom, patchedPrgRom)) {
				memcpy(_prgRom, patchedPrgRom.data(), _prgSize);
			}
		}
	}

	void SaveBattery() override
	{
		if(_console->GetNesConfig().DisableFlashSaves) {
			return;
		}

		if(true) {
			vector<uint8_t> prgRom = vector<uint8_t>(_prgRom, _prgRom + _prgSize);
			vector<uint8_t> ipsData = IpsPatcher::CreatePatch(_orgPrgRom, prgRom);
			if(ipsData.size() > 8) {
				_emu->GetBatteryManager()->SaveBattery(".ips", ipsData.data(), (uint32_t)ipsData.size());
			}
		}
		_irqEnabled = false;
	}

	uint8_t MapperReadVram(uint16_t addr, MemoryOperationType memoryOperationType) override
	{
		bool ppu_a13 = bool(addr & 0x2000);
		bool ppu_a12 = bool(addr & 0x1000);
		bool spr_or_nt_fetch = ppu_a12 || ppu_a13;
		//bool vram_a12 = spr_or_nt_fetch ? ppu_a12 : chr_a12;
		bool vram_a12 = spr_or_nt_fetch ? 0 : chr_a12;
		bool vram_a13 = spr_or_nt_fetch ? swap_spr_nt : chr_a13;
		bool vram_a14 = spr_or_nt_fetch ? bool(!a14) : a14;
		bool read_ciram = false;
		bool at_read_normal = ppu_a13 && (!a13_ddd && !a13_dd);
		bool at_read_dummy = ppu_a13 && (a13_ddd && a13_dd);
		at_read_next = at_read_normal || at_read_dummy;
		if(memoryOperationType == MemoryOperationType::PpuRenderingRead || memoryOperationType == MemoryOperationType::Read) {
			if(ppu_a13 && !at_read_next) {
				//Nametable fetches
				_attribute_fine_x = addr & 1;
				_attribute_fine_y = (addr >> 5) & 1;
				read_ciram = true;
			}
			else if(ppu_a13 && at_read_next) {
				// Attribute fetches
				uint16_t n_a14 = (~a14) & 1;
				addr = (n_a14 << 14) | (_attribute_fine_y << 13) | (_attribute_fine_x << 12) | (addr & 0x3F) | 0x0FC0; // 0x1FC0;
			}
			else {
				addr = (uint16_t(vram_a14 & 1) << 14) | (uint16_t(vram_a13 & 1) << 13) | (uint16_t(vram_a12 & 1) << 12) | (addr & 0xFFF);
			}
			a13_ddd = a13_dd;
			a13_dd = a13_d;
			a13_d = ppu_a13;
			if(ppu_a12) {
				if(_irqEnabled) {
					_console->GetCpu()->SetIrqSource(IRQSource::External);
				}
			} 

			if(read_ciram)
				return BaseMapper::MapperReadVram(addr, memoryOperationType);
			else
				return _chrRam[addr];
		}
		return BaseMapper::MapperReadVram(addr, memoryOperationType);
	}

	uint8_t ReadRegister(uint16_t addr) override
	{
		int16_t value = _flash->Read(addr);
		if(value >= 0) {
			return (uint8_t)value;
		}

		return BaseMapper::InternalReadRam(addr);
	}

	void WriteRegister(uint16_t addr, uint8_t value) override
	{
		if(addr >= 0xC000) {
			SelectPrgPage(0, value);
			_prgBank = value;
		}
		else if(addr >= 0x8000) {
			chr_a12 = value & 1;
			chr_a13 = (value >> 1) & 1;
			a14 = (value >> 2) & 1;
			SelectChrPage(0, value & 0x07);
			SelectChrPage(1, value & 0x07 ^ 0x4 ^ (a14 << 2));
			SetMirroringType(value & 0x10 ? MirroringType::ScreenBOnly : MirroringType::ScreenAOnly);
			swap_spr_nt = bool((value >> 4) & 1);
			_irqEnabled = (value >> 7) & 1;
			_console->GetCpu()->ClearIrqSource(IRQSource::External);
			_flash->Write((addr & 0x3FFF) | (_prgBank << 14), value);
		}
	}

public:
/*
	void NotifyVramAddressChange(uint16_t addr) override
	{
		if(addr & 0x1000) {
		   if(_irqEnabled) {
				_console->GetCpu()->SetIrqSource(IRQSource::External);
			}
		}
		else {
			_console->GetCpu()->ClearIrqSource(IRQSource::External);
		}
	}
*/

/*
	void NotifyVramAddressChange(uint16_t addr) override
	{
		//if(addr & 0x1000) {
		uint8_t a12 = (addr >> 12) & 1;
		if(a12 | a12_r | a12_r_old) {
			if(_irqEnabled) {
				_console->GetCpu()->SetIrqSource(IRQSource::External);
			}
		} else {
			_console->GetCpu()->ClearIrqSource(IRQSource::External);
		}
	}
*/

};
