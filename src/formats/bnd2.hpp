#pragma once
#include "base.hpp"

namespace libbndl
{
	namespace Formats
	{
		class BND2 : public Base
		{
		public:
			using Base::Base;

			virtual bool Load(binaryio::BinaryReader &reader) override;
			virtual bool Save(binaryio::BinaryWriter &reader) override;

			[[nodiscard]] virtual constexpr Bundle::MagicVersion GetMagicVersion() const override { return Bundle::MagicVersion::BND2; }

			[[nodiscard]] virtual std::optional<Bundle::Resource> GetResource(uint32_t resourceID) const override;

		protected:
			virtual constexpr bool AppendsImportsToResource() const override { return false; }
		};
	}
}
