#pragma once
#include "base.hpp"

namespace libbndl
{
	namespace Formats
	{
		class BNDL : public Base
		{
		public:
			using Base::Base;

			virtual bool Load(binaryio::BinaryReader &reader) override;
			virtual bool Save(binaryio::BinaryWriter &reader) override;

			[[nodiscard]] virtual constexpr Bundle::MagicNumber GetMagicNumber() const override { return Bundle::MagicNumber::BNDL; }

			[[nodiscard]] virtual std::optional<Bundle::Resource> GetResource(uint32_t resourceID) const override;

		private:
			std::map<uint32_t, std::vector<ImportEntry>> m_imports;

			virtual constexpr bool AppendsImportsToResource() const override { return false; }

			[[nodiscard]] int8_t MapBNDLBlockToBND2(uint8_t block) const;
		};
	}
}
