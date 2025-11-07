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

			[[nodiscard]] virtual constexpr MagicNumber GetMagicNumber() const override { return MagicNumber::BNDL; }

			[[nodiscard]] virtual std::optional<Resource> GetResource(ResourceKey resourceKey) const override;

		private:
			std::map<ResourceID, std::vector<ImportEntry>> m_imports;

			virtual constexpr bool AppendsImportsToResource() const override { return false; }

			[[nodiscard]] std::optional<uint8_t> MapFileBlockToLibBlock(uint8_t block) const;
		};
	}
}
