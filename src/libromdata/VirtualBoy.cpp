/***************************************************************************
 * ROM Properties Page shell extension. (libromdata)                       *
 * DMG.hpp: Virtual Boy ROM reader.                                        *
 *                                                                         *
 * Copyright (c) 2016 by David Korth.                                      *
 * Copyright (c) 2016 by Egor.                                             *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published by the   *
 * Free Software Foundation; either version 2 of the License, or (at your  *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it will be useful, but     *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.           *
 ***************************************************************************/

#include "VirtualBoy.hpp"
#include "RomData_p.hpp"

#include "NintendoPublishers.hpp"
#include "vb_structs.h"

#include "common.h"
#include "byteswap.h"
#include "TextFuncs.hpp"
#include "file/IRpFile.hpp"

// C includes. (C++ namespace)
#include <cassert>
#include <cstring>
#include <cctype>

// C++ includes.
#include <algorithm>
#include <string>
#include <vector>
using std::string;
using std::vector;

namespace LibRomData {

class VirtualBoyPrivate
{
	public:
		VirtualBoyPrivate() { }

	private:
		VirtualBoyPrivate(const VirtualBoyPrivate &other);
		VirtualBoyPrivate &operator=(const VirtualBoyPrivate &other);
	public:
		/** RomFields **/
		static const struct RomFields::Desc vb_fields[];
	
	public:
		/**
		 * Is character a valid JIS X 0201 codepoint?
		 * @param c The character
		 * @return Wether or not character is valid
		 */
		static bool inline isJISX0201(unsigned char c);
		
		/**
		 * Is character a valid Publisher ID character?
		 * @param c The character
		 * @return Wether or not character is valid
		 */
		static bool inline isPublisherID(char c);

		/**
		 * Is character a valid Game ID character?
		 * @param c The character
		 * @return Wether or not character is valid
		 */
		static bool inline isGameID(char c);
	
	public:
		// ROM header.
		VB_RomHeader romHeader;
};

/** VirtualBoyPrivate **/

// ROM fields.
const struct RomFields::Desc VirtualBoyPrivate::vb_fields[] = {
	{_RP("Title"), RomFields::RFT_STRING, nullptr},
	{_RP("Game ID"), RomFields::RFT_STRING, nullptr},
	{_RP("Publisher"), RomFields::RFT_STRING, nullptr},
	{_RP("Revision"), RomFields::RFT_STRING, nullptr},
	{_RP("Region"), RomFields::RFT_STRING, nullptr},
};

/**
 * Is character a valid JIS X 0201 codepoint?
 * @param c The character
 * @return Wether or not character is valid
 */
bool inline VirtualBoyPrivate::isJISX0201(unsigned char c){
	return (c >= ' ' && c <= '~') || (c > 0xA0 && c < 0xE0);
}

/**
 * Is character a valid Game ID character?
 * @param c The character
 * @return Wether or not character is valid
 */
bool inline VirtualBoyPrivate::isPublisherID(char c){
	// Valid characters:
	// - Uppercase letters
	// - Digits
	return (isupper(c) || isdigit(c));
}

/**
 * Is character a valid Game ID character?
 * @param c The character
 * @return Wether or not character is valid
 */
bool inline VirtualBoyPrivate::isGameID(char c){
	// Valid characters:
	// - Uppercase letters
	// - Digits
	// - Space (' ')
	// - Hyphen ('-')
	return (isupper(c) || isdigit(c) || c == ' ' || c == '-');
}

/** VirtualBoy **/

/**
 * Read a Virtual Boy ROM.
 *
 * A ROM file must be opened by the caller. The file handle
 * will be dup()'d and must be kept open in order to load
 * data from the ROM.
 *
 * To close the file, either delete this object or call close().
 *
 * NOTE: Check isValid() to determine if this is a valid ROM.
 *
 * @param file Open ROM file.
 */
VirtualBoy::VirtualBoy(IRpFile *file)
	: super(file, VirtualBoyPrivate::vb_fields, ARRAY_SIZE(VirtualBoyPrivate::vb_fields))
	, d(new VirtualBoyPrivate())
	
{
	if (!d_ptr->file) {
		// Could not dup() the file handle.
		return;
	}

	// Seek to the beginning of the header.
	const int64_t filesize = d_ptr->file->fileSize();
	// File must be at least 0x220 bytes,
	// and cannot be larger than 16 MB.
	if (filesize < 0x220 || filesize > (16*1024*1024)) {
		// File size is out of range.
		return;
	}

	// Read the ROM header.
	const unsigned int header_addr = (unsigned int)(filesize - 0x220);
	d_ptr->file->seek(header_addr);
	size_t size = d_ptr->file->read(&d->romHeader, sizeof(d->romHeader));
	if (size != sizeof(d->romHeader))
		return;

	// Make sure this is actually a Virtual Boy ROM.
	DetectInfo info;
	info.header.addr = header_addr;
	info.header.size = sizeof(d->romHeader);
	info.header.pData = reinterpret_cast<const uint8_t*>(&d->romHeader);
	info.ext = nullptr;	// Not needed for Virtual Boy.
	info.szFile = filesize;
	d_ptr->isValid = (isRomSupported(&info) >= 0);
}

VirtualBoy::~VirtualBoy()
{
	delete d;
}

/** ROM detection functions. **/

/**
 * Is a ROM image supported by this class?
 * @param info DetectInfo containing ROM detection information.
 * @return Class-specific system ID (>= 0) if supported; -1 if not.
 */
int VirtualBoy::isRomSupported_static(const DetectInfo *info)
{
	assert(info != nullptr);
	assert(info->header.pData != nullptr);
	if (!info || !info->header.pData)
		return -1;

	// File size constraints:
	// - Must be at least 16 KB.
	// - Cannot be larger than 16 MB.
	// - Must be a power of two.
	// NOTE: The only retail ROMs were 512 KB, 1 MB, and 2 MB,
	// but the system supports up to 16 MB, and some homebrew
	// is less than 512 KB.
	if (info->szFile < 16*1024 ||
	    info->szFile > 16*1024*1024 ||
	    ((info->szFile & (info->szFile - 1)) != 0))
	{
		// File size is not valid.
		return -1;
	}

	// Virtual Boy header is located at
	// 0x220 before the end of the file.
	if (info->szFile < 0x220)
		return -1;
	const uint32_t header_addr_expected = (uint32_t)(info->szFile - 0x220);
	if (info->header.addr > header_addr_expected)
		return -1;
	else if (info->header.addr + info->header.size < header_addr_expected + 0x20)
		return -1;

	// Determine the offset.
	const unsigned int offset = header_addr_expected - info->header.addr;

	// Get the ROM header.
	const VB_RomHeader *romHeader =
		reinterpret_cast<const VB_RomHeader*>(&info->header.pData[offset]);

	// NOTE: The following is true for every Virtual Boy ROM:
	// 1) First 20 bytes of title are non-control JIS X 0201 characters (padded with space if needed)
	// 2) 21th byte is a null
	// 3) Game ID is either VxxJ (for Japan) or VxxE (for USA) (xx are alphanumeric uppercase characters)
	// 4) ROM version is always 0, but let's not count on that.
	// 5) And, obviously, the publisher is always valid, but again let's not rely on this
	// NOTE: We're supporting all no-intro ROMs except for "Space Pinball (Unknown) (Proto).vb" as it doesn't have a valid header at all
	if (romHeader->title[20] != 0) {
		// title[20] is not NULL.
		return -1;
	}

	// Make sure the title is valid JIS X 0201.
	for (int i = ARRAY_SIZE(romHeader->title)-2-1; i >= 0; i--) {
		if (!VirtualBoyPrivate::isJISX0201(romHeader->title[i])) {
			// Invalid title character.
			return -1;
		}
	}

	// NOTE: Game ID is VxxJ or VxxE for retail ROMs,
	// but homebrew ROMs can have anything here.
	// Valid characters:
	// - Uppercase letters
	// - Digits
	// - Space (' ') [not for publisher]
	// - Hyphen ('-') [not for publisher]
	if (!VirtualBoyPrivate::isPublisherID(romHeader->publisher[0]) ||
	    !VirtualBoyPrivate::isPublisherID(romHeader->publisher[1]))
	{
		// Invalid publisher ID.
		return -1;
	}

	for (int i = ARRAY_SIZE(romHeader->gameid)-1; i >= 0; i--) {
		if (!VirtualBoyPrivate::isGameID(romHeader->gameid[i])) {
			// Invalid game ID.
			return -1;
		}
	}

	// Looks like a Virtual Boy ROM.
	return 0;
}

/**
 * Is a ROM image supported by this object?
 * @param info DetectInfo containing ROM detection information.
 * @return Class-specific system ID (>= 0) if supported; -1 if not.
 */
int VirtualBoy::isRomSupported(const DetectInfo *info) const
{
	return isRomSupported_static(info);
}

/**
 * Get the name of the system the loaded ROM is designed for.
 * @return System name, or nullptr if not supported.
 */
const rp_char *VirtualBoy::systemName(uint32_t type) const
{
	if (!d_ptr->isValid || !isSystemNameTypeValid(type))
		return nullptr;

	static_assert(SYSNAME_TYPE_MASK == 3,
		"VirtualBoy::systemName() array index optimization needs to be updated.");
	
	static const rp_char *const sysNames[4] = {
		_RP("Nintendo Virtual Boy"), _RP("Virtual Boy"), _RP("VB"), nullptr,
	};

	return sysNames[type & SYSNAME_TYPE_MASK];
}

/**
 * Get a list of all supported file extensions.
 * This is to be used for file type registration;
 * subclasses don't explicitly check the extension.
 *
 * NOTE: The extensions include the leading dot,
 * e.g. ".bin" instead of "bin".
 *
 * NOTE 2: The strings in the std::vector should *not*
 * be freed by the caller.
 *
 * @return List of all supported file extensions.
 */
vector<const rp_char*> VirtualBoy::supportedFileExtensions_static(void)
{
	// FIXME: ".vb" can't be associated until we
	// implement fallbacks, since it conflicts
	// with VB.NET source files.
#if 0
	static const rp_char *const exts[] = {
		_RP(".vb")
	};
	return vector<const rp_char*>(exts, exts + ARRAY_SIZE(exts));
#endif
	return vector<const rp_char*>();
}

/**
 * Get a list of all supported file extensions.
 * This is to be used for file type registration;
 * subclasses don't explicitly check the extension.
 *
 * NOTE: The extensions include the leading dot,
 * e.g. ".bin" instead of "bin".
 *
 * NOTE 2: The strings in the std::vector should *not*
 * be freed by the caller.
 *
 * @return List of all supported file extensions.
 */
vector<const rp_char*> VirtualBoy::supportedFileExtensions(void) const
{
	return supportedFileExtensions_static();
}

/**
 * Load field data.
 * Called by RomData::fields() if the field data hasn't been loaded yet.
 * @return Number of fields read on success; negative POSIX error code on error.
 */
int VirtualBoy::loadFieldData(void)
{
	if (d_ptr->fields->isDataLoaded()) {
		// Field data *has* been loaded...
		return 0;
	} else if (!d_ptr->file || !d_ptr->file->isOpen()) {
		// File isn't open.
		return -EBADF;
	} else if (!d_ptr->isValid) {
		// ROM image isn't valid.
		return -EIO;
	}

	// Virtual Boy ROM header, excluding the vector table.
	const VB_RomHeader *const romHeader = &d->romHeader;

	// Title
	d_ptr->fields->addData_string(cp1252_sjis_to_rp_string(romHeader->title,sizeof(romHeader->title)));

	// Game ID and publisher.
	string id6(romHeader->gameid, sizeof(romHeader->gameid));
	id6 += string(romHeader->publisher, sizeof(romHeader->publisher));
	d_ptr->fields->addData_string(latin1_to_rp_string(id6.data(), (int)id6.size()));

	// Look up the publisher.
	const rp_char* publisher = NintendoPublishers::lookup(romHeader->publisher);
	d_ptr->fields->addData_string(publisher?publisher:_RP("Unknown"));

	// Revision
	d_ptr->fields->addData_string_numeric(romHeader->version, RomFields::FB_DEC, 2);

	// Region
	const rp_char* region;
	switch(romHeader->gameid[3]){
		case 'J':
			region = _RP("Japan");
			break;
		case 'E':
			region = _RP("USA");
			break;
		default:
			region = _RP("Unknown");
			break;
	}
	d_ptr->fields->addData_string(region);

	return (int)d_ptr->fields->count();
}

}
