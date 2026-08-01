#pragma once
#include "base.hpp"

namespace libbndl
{
	namespace Formats
	{
		class Bndl : public Base
		{
		public:
			using Base::Base;

			bool Load(binaryio::BinaryReader &reader) override;
			bool Save(binaryio::BinaryWriter &reader) override;

			[[nodiscard]] constexpr Magic GetMagic() const override { return Magic::Bndl; }

			[[nodiscard]] std::optional<Resource> GetResource(ResourceKey resourceKey) const override;

		private:
			std::map<ResourceID, std::vector<ImportEntry>> m_imports;

			[[nodiscard]] constexpr bool AppendsImportsToResource() const override { return false; }

			[[nodiscard]] std::optional<uint8_t> MapFileBlockToLibBlock(uint8_t block) const;
		};
	}
}
